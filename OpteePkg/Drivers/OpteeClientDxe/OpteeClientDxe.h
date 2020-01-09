/** @file
  Optee Client

  Copyright (c) 2020, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _OPTEE_CLIENT_DXE_H_
#define _OPTEE_CLIENT_DXE_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/OpteeClient.h>

/*
 * The 'Trusted OS Call UID' is supposed to return the following UUID for
 * OP-TEE OS. This is a 128-bit value.
 */
#define OPTEE_OS_UID0          0x384fb3e0
#define OPTEE_OS_UID1          0xe7f811e3
#define OPTEE_OS_UID2          0xaf630002
#define OPTEE_OS_UID3          0xa5d5c51b

BOOLEAN
EFIAPI
IsOpteePresent (
  VOID
  );

EFI_STATUS
EFIAPI
OpteeInit (
  VOID
  );

EFI_STATUS
EFIAPI
OpteeClose (
  VOID
  );

EFI_STATUS
ArchSetVirtualAddressMap (
  VOID
  );

EFI_STATUS
EFIAPI
OpteeOpenSession (
  IN     OPTEE_CLIENT_PROTOCOL        *This,
  IN OUT OPTEE_OPEN_SESSION_ARG       *OpenSessionArg
  );

EFI_STATUS
EFIAPI
OpteeCloseSession (
  IN OPTEE_CLIENT_PROTOCOL        *This,
  IN UINT32                       Session
  );

EFI_STATUS
EFIAPI
OpteeInvokeFunction (
  IN     OPTEE_CLIENT_PROTOCOL        *This,
  IN OUT OPTEE_INVOKE_FUNCTION_ARG    *InvokeFunctionArg
  );

#endif /* _OPTEE_CLIENT_DXE_H_ */
