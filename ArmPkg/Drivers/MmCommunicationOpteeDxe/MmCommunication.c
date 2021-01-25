/** @file

  Copyright 2020 NXP

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MmCommunicate.h"

extern VOID EFIAPI ArmEnableInterrupts (VOID);
extern UINTN EFIAPI ArmDisableInterrupts (VOID);

EFI_GUID MmUuid = OPTEE_TA_MM_UUID;

STATIC
EFI_STATUS
OpteeStatusToEfi (
  IN  UINT64        OpteeStatus
  )
{
  switch (OpteeStatus) {
  case ARM_SMC_MM_RET_SUCCESS:
    return EFI_SUCCESS;

  case ARM_SMC_MM_RET_INVALID_PARAMS:
    return EFI_INVALID_PARAMETER;

  case ARM_SMC_MM_RET_DENIED:
    return EFI_ACCESS_DENIED;

  case ARM_SMC_MM_RET_NO_MEMORY:
    DEBUG ((DEBUG_ERROR, "Byeeee %s, %d ARM_SMC_MM_RET_NO_MEMORY \n", __func__, __LINE__));
    return EFI_OUT_OF_RESOURCES;

  default:
    return EFI_ACCESS_DENIED;
  }
}

STATIC
UINT32
NewSession ()
{
  OPTEE_OPEN_SESSION_ARG        OpenArg;
  EFI_STATUS                    Status;

  SetMem (&OpenArg, 0, sizeof (OpenArg));
  CopyGuid (&OpenArg.Uuid, &MmUuid);

  Status = OpteeOpenSession (&OpenArg);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "OpteeOpenSession failed !EFI_SUCCESS\n"));
    return 0xFFFF;
  }

  if (OpenArg.Return != OPTEE_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "OpteeOpenSession failed OpenArg.Return != OPTEE_SUCCESS = %x\n", OpenArg.Return));
    return 0xFFFF;
  }

  return OpenArg.Session;
}

STATIC
EFI_STATUS
EFIAPI
MmCommunicate (
  IN CONST EFI_MM_COMMUNICATION2_PROTOCOL   *This,
  IN OUT VOID                               *CommBufferPhysical,
  IN OUT VOID                               *CommBufferVirtual,
  IN OUT UINTN                              *CommSize OPTIONAL
  )
{
  EFI_STATUS                    Status;
  OPTEE_MM_SESSION              *OpteeMm;
  OPTEE_INVOKE_FUNCTION_ARG     InvokeArg;
  STATIC UINTN	num_open_session = 0;

  if (This == NULL || CommBufferPhysical == NULL ||
      CommBufferVirtual == NULL || CommSize == NULL ||
      *CommSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (num_open_session) {
	  DEBUG ((DEBUG_ERROR, "Session already open num_open_session= %u\n",
			num_open_session));
	  Status = EFI_DEVICE_ERROR;
	  goto CLOSE_SESSION;
  }

  OpteeMm = OPTEE_MM_SESSION_FROM_MM_COMMUNICATION_PROTOCOL_THIS (This);
try_again:
  OpteeMm->Session = NewSession ();
  if (OpteeMm->Session == 0xFFFF) {
    DEBUG ((DEBUG_ERROR, "%s, %d  NewSession Failed\n", __func__, __LINE__));
    goto try_again;
//    return EFI_ACCESS_DENIED;
  }
  num_open_session++;

  ZeroMem (&InvokeArg, sizeof (InvokeArg));
  InvokeArg.Function = OPTEE_TA_MM_FUNC_COMMUNICATE;
  InvokeArg.Session = OpteeMm->Session;
  InvokeArg.Params[0].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  InvokeArg.Params[0].Union.Memory.BufferAddress = (UINTN) CommBufferVirtual;
  InvokeArg.Params[0].Union.Memory.Size = *CommSize;
  InvokeArg.Params[1].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT;

  Status = OpteeInvokeFunction (&InvokeArg);
  if ((Status != EFI_SUCCESS) ||
      (InvokeArg.Return != OPTEE_SUCCESS)) {
    DEBUG ((DEBUG_ERROR, "OP-TEE Invoke Function failed with "
            "return: %x and return origin: %d\n",
            InvokeArg.Return, InvokeArg.ReturnOrigin));
    Status = EFI_DEVICE_ERROR;
    goto CLOSE_SESSION;
  }

  Status = OpteeStatusToEfi (InvokeArg.Params[1].Union.Value.A);
  if (Status == EFI_SUCCESS) {
    *CommSize = InvokeArg.Params[0].Union.Memory.Size;
  }

CLOSE_SESSION:
  if (OpteeCloseSession (OpteeMm->Session))
	  DEBUG ((DEBUG_ERROR, "%s, %d, CloseSession Failed \n"));
  num_open_session--;
  return Status;
}

STATIC
EFI_STATUS
GetMmCompatibility ()
{
  UINT32      Session;

  Session = NewSession ();
  if (Session == 0) {
    return EFI_ACCESS_DENIED;
  }

  return OpteeCloseSession (Session);
}

STATIC OPTEE_MM_SESSION mOpteeMm = {
  OPTEE_MM_SESSION_SIGNATURE,         // Signature
  NULL,                               // Handle
  {
    MmCommunicate,
  },                                  // Mm
  0,                                  // Session
};

STATIC EFI_GUID* CONST mGuidedEventGuid[] = {
  &gEfiEndOfDxeEventGroupGuid,
  &gEfiEventExitBootServicesGuid,
  &gEfiEventReadyToBootGuid,
};

STATIC EFI_EVENT mGuidedEvent[ARRAY_SIZE (mGuidedEventGuid)];

STATIC
VOID
EFIAPI
MmGuidedEventNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_MM_COMMUNICATE_HEADER   Header;
  UINTN                       Size;

  //
  // Use Guid to initialize EFI_SMM_COMMUNICATE_HEADER structure
  //
  CopyGuid (&Header.HeaderGuid, Context);
  Header.MessageLength = 1;
  Header.Data[0] = 0;

  Size = sizeof (Header);
  MmCommunicate (&mOpteeMm.Mm, &Header, &Header, &Size);
}

EFI_STATUS
EFIAPI
MmCommunicationInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS      Status;
  UINTN           Index;

  Status = OpteeInit ();
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Status = GetMmCompatibility ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MM: Incompatible MM version\n"));
    goto Exit;
  }

  for (Index = 0; Index < ARRAY_SIZE (mGuidedEventGuid); Index++) {
    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                    MmGuidedEventNotify, mGuidedEventGuid[Index],
                    mGuidedEventGuid[Index], &mGuidedEvent[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "MM: Failed to create GUIDed event[%d]\n", Index));
      goto CloseGuidedEvents;
    }
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mOpteeMm.Handle,
                  &gEfiMmCommunication2ProtocolGuid,
                  &mOpteeMm.Mm,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MM: Failed to install communication protocol\n"));
    goto CloseGuidedEvents;
  }

  return Status;

CloseGuidedEvents:
  for (Index = 0; Index < ARRAY_SIZE (mGuidedEventGuid); Index++) {
    gBS->CloseEvent (&mGuidedEvent[Index]);
  }
Exit:
  return Status;
}
