/** @file
*  File managing the MMU for ARMv8 architecture in S-EL0
*
*  Copyright (c) 2017 - 2021, Arm Limited. All rights reserved.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Uefi.h>
#include <IndustryStandard/ArmMmSvc.h>
#include <IndustryStandard/ArmFfaSvc.h>

#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

STATIC
EFI_STATUS
GetMemoryPermissions (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  OUT UINT32                    *MemoryAttributes
  )
{
  INT32         Ret;
  ARM_SVC_ARGS  GetMemoryPermissionsSvcArgs;
  BOOLEAN       FfaEnabled;

  ZeroMem (&GetMemoryPermissionsSvcArgs, sizeof (ARM_SVC_ARGS));

  FfaEnabled = FeaturePcdGet (PcdFfaEnable);
  if (FfaEnabled) {
    GetMemoryPermissionsSvcArgs.Arg0 = ARM_SVC_ID_FFA_MSG_SEND_DIRECT_REQ_AARCH64;
    GetMemoryPermissionsSvcArgs.Arg1 = ARM_FFA_DESTINATION_ENDPOINT_ID;
    GetMemoryPermissionsSvcArgs.Arg2 = 0;
    GetMemoryPermissionsSvcArgs.Arg3 = ARM_SVC_ID_SP_GET_MEM_ATTRIBUTES_AARCH64;
    GetMemoryPermissionsSvcArgs.Arg4 = BaseAddress;
  } else {
    GetMemoryPermissionsSvcArgs.Arg0 = ARM_SVC_ID_SP_GET_MEM_ATTRIBUTES_AARCH64;
    GetMemoryPermissionsSvcArgs.Arg1 = BaseAddress;
    GetMemoryPermissionsSvcArgs.Arg2 = 0;
    GetMemoryPermissionsSvcArgs.Arg3 = 0;
  }

  ArmCallSvc (&GetMemoryPermissionsSvcArgs);
  if (FfaEnabled) {
    if (GetMemoryPermissionsSvcArgs.Arg0 !=
        ARM_SVC_ID_FFA_MSG_SEND_DIRECT_RESP_AARCH64) {
      // If Arg0 is not a Direct Response, that means we
      // have an FF-A error. We need to check Arg2 for the
      // FF-A error code.
      Ret = GetMemoryPermissionsSvcArgs.Arg2;
      switch (Ret) {
      case ARM_FFA_SPM_RET_INVALID_PARAMETERS:
        *MemoryAttributes = 0;
        return EFI_INVALID_PARAMETER;

      case ARM_FFA_SPM_RET_DENIED:
        *MemoryAttributes = 0;
        return EFI_NOT_READY;

      case ARM_FFA_SPM_RET_NOT_SUPPORTED:
        *MemoryAttributes = 0;
        return EFI_UNSUPPORTED;

      case ARM_FFA_SPM_RET_BUSY:
        *MemoryAttributes = 0;
        return EFI_NOT_READY;

      case ARM_FFA_SPM_RET_ABORTED:
        *MemoryAttributes = 0;
        return EFI_ABORTED;
      }
    } else if (GetMemoryPermissionsSvcArgs.Arg0 ==
               ARM_SVC_ID_FFA_MSG_SEND_DIRECT_RESP_AARCH64) {
      // A Direct Response means FF-A success
      // Now check the payload for errors
      // The callee sends back the return value
      // in Arg3
      Ret = GetMemoryPermissionsSvcArgs.Arg3;
      if (Ret & BIT31) {
        // Bit 31 set means there is an error retured
        *MemoryAttributes = 0;
        switch (Ret) {
        case ARM_SVC_SPM_RET_INVALID_PARAMS:
          return EFI_INVALID_PARAMETER;

        case ARM_SVC_SPM_RET_NOT_SUPPORTED:
          return EFI_UNSUPPORTED;
        }
      } else {
        *MemoryAttributes = Ret;
      }
    }
  } else {
    Ret = GetMemoryPermissionsSvcArgs.Arg0;

    switch (Ret) {
    case ARM_SVC_SPM_RET_INVALID_PARAMS:
      *MemoryAttributes = 0;
      return EFI_INVALID_PARAMETER;

    case ARM_SVC_SPM_RET_NOT_SUPPORTED:
      *MemoryAttributes = 0;
      return EFI_UNSUPPORTED;
    }

    *MemoryAttributes = Ret;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
RequestMemoryPermissionChange (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  IN  UINT64                    Length,
  IN  UINTN                     Permissions
  )
{
  INT32         Ret;
  BOOLEAN       FfaEnabled;
  ARM_SVC_ARGS  ChangeMemoryPermissionsSvcArgs;

  ZeroMem (&ChangeMemoryPermissionsSvcArgs, sizeof (ARM_SVC_ARGS));

  FfaEnabled = FeaturePcdGet (PcdFfaEnable);

  if (FfaEnabled) {
    ChangeMemoryPermissionsSvcArgs.Arg0 = ARM_SVC_ID_FFA_MSG_SEND_DIRECT_REQ_AARCH64;
    ChangeMemoryPermissionsSvcArgs.Arg1 = ARM_FFA_DESTINATION_ENDPOINT_ID;
    ChangeMemoryPermissionsSvcArgs.Arg2 = 0;
    ChangeMemoryPermissionsSvcArgs.Arg3 = ARM_SVC_ID_SP_SET_MEM_ATTRIBUTES_AARCH64;
    ChangeMemoryPermissionsSvcArgs.Arg4 = BaseAddress;
    ChangeMemoryPermissionsSvcArgs.Arg5 = EFI_SIZE_TO_PAGES (Length);
    ChangeMemoryPermissionsSvcArgs.Arg6 = Permissions;
  } else {
    ChangeMemoryPermissionsSvcArgs.Arg0 = ARM_SVC_ID_SP_SET_MEM_ATTRIBUTES_AARCH64;
    ChangeMemoryPermissionsSvcArgs.Arg1 = BaseAddress;
    ChangeMemoryPermissionsSvcArgs.Arg2 = EFI_SIZE_TO_PAGES (Length);
    ChangeMemoryPermissionsSvcArgs.Arg3 = Permissions;
  }

  ArmCallSvc (&ChangeMemoryPermissionsSvcArgs);

  if (FfaEnabled) {
    if (ChangeMemoryPermissionsSvcArgs.Arg0 !=
        ARM_SVC_ID_FFA_MSG_SEND_DIRECT_RESP_AARCH64) {
      // If Arg0 is not a Direct Response, that means we
      // have an FF-A error. We need to check Arg2 for the
      // FF-A error code.
      Ret = ChangeMemoryPermissionsSvcArgs.Arg2;
      switch (Ret) {
      case ARM_FFA_SPM_RET_INVALID_PARAMETERS:
        return EFI_INVALID_PARAMETER;

      case ARM_FFA_SPM_RET_DENIED:
        return EFI_NOT_READY;

      case ARM_FFA_SPM_RET_NOT_SUPPORTED:
        return EFI_UNSUPPORTED;

      case ARM_FFA_SPM_RET_BUSY:
        return EFI_NOT_READY;

      case ARM_FFA_SPM_RET_ABORTED:
        return EFI_ABORTED;
      }
    } else if (ChangeMemoryPermissionsSvcArgs.Arg0 ==
               ARM_SVC_ID_FFA_MSG_SEND_DIRECT_RESP_AARCH64) {
      // A Direct Response means FF-A success
      // Now check the payload for errors
      // The callee sends back the return value
      // in Arg3
      Ret = ChangeMemoryPermissionsSvcArgs.Arg3;
      switch (Ret) {
      case ARM_SVC_SPM_RET_NOT_SUPPORTED:
        return EFI_UNSUPPORTED;

      case ARM_SVC_SPM_RET_INVALID_PARAMS:
        return EFI_INVALID_PARAMETER;

      case ARM_SVC_SPM_RET_DENIED:
        return EFI_ACCESS_DENIED;

      case ARM_SVC_SPM_RET_NO_MEMORY:
        return EFI_BAD_BUFFER_SIZE;
      }
    }
  } else {
    Ret = ChangeMemoryPermissionsSvcArgs.Arg0;
    switch (Ret) {

    case ARM_SVC_SPM_RET_NOT_SUPPORTED:
      return EFI_UNSUPPORTED;

    case ARM_SVC_SPM_RET_INVALID_PARAMS:
      return EFI_INVALID_PARAMETER;

    case ARM_SVC_SPM_RET_DENIED:
      return EFI_ACCESS_DENIED;

    case ARM_SVC_SPM_RET_NO_MEMORY:
      return EFI_BAD_BUFFER_SIZE;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
ArmSetMemoryRegionNoExec (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  IN  UINT64                    Length
  )
{
  EFI_STATUS    Status;
  UINT32 MemoryAttributes;
  UINT32 CodePermission;

  Status = GetMemoryPermissions (BaseAddress, &MemoryAttributes);
  if (Status != EFI_INVALID_PARAMETER) {
    CodePermission = SET_MEM_ATTR_CODE_PERM_XN << SET_MEM_ATTR_CODE_PERM_SHIFT;
    return RequestMemoryPermissionChange (
             BaseAddress,
             Length,
             MemoryAttributes | CodePermission
             );
  }
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
ArmClearMemoryRegionNoExec (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  IN  UINT64                    Length
  )
{
  EFI_STATUS    Status;
  UINT32 MemoryAttributes;
  UINT32 CodePermission;

  Status = GetMemoryPermissions (BaseAddress, &MemoryAttributes);
  if (Status != EFI_INVALID_PARAMETER) {
    CodePermission = SET_MEM_ATTR_CODE_PERM_XN << SET_MEM_ATTR_CODE_PERM_SHIFT;
    return RequestMemoryPermissionChange (
             BaseAddress,
             Length,
             MemoryAttributes & ~CodePermission
             );
  }
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
ArmSetMemoryRegionReadOnly (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  IN  UINT64                    Length
  )
{
  EFI_STATUS    Status;
  UINT32 MemoryAttributes;
  UINT32 DataPermission;

  Status = GetMemoryPermissions (BaseAddress, &MemoryAttributes);
  if (Status != EFI_INVALID_PARAMETER) {
    DataPermission = SET_MEM_ATTR_DATA_PERM_RO << SET_MEM_ATTR_DATA_PERM_SHIFT;
    return RequestMemoryPermissionChange (
             BaseAddress,
             Length,
             MemoryAttributes | DataPermission
             );
  }
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
ArmClearMemoryRegionReadOnly (
  IN  EFI_PHYSICAL_ADDRESS      BaseAddress,
  IN  UINT64                    Length
  )
{
  EFI_STATUS    Status;
  UINT32 MemoryAttributes;
  UINT32 PermissionRequest;

  Status = GetMemoryPermissions (BaseAddress, &MemoryAttributes);
  if (Status != EFI_INVALID_PARAMETER) {
    PermissionRequest = SET_MEM_ATTR_MAKE_PERM_REQUEST (SET_MEM_ATTR_DATA_PERM_RW,
                                                        MemoryAttributes);
    return RequestMemoryPermissionChange (
             BaseAddress,
             Length,
             PermissionRequest
             );
  }
  return EFI_INVALID_PARAMETER;
}
