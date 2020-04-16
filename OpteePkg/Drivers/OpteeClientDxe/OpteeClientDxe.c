/** @file
  Optee Client

  Copyright (c) 2020, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "OpteeClientDxe.h"

STATIC OPTEE_CLIENT_PROTOCOL gOpteeClient = {
  OpteeOpenSession,     /* OpenSession */
  OpteeCloseSession,    /* CloseSession */
  OpteeInvokeFunction,  /* InvokeFunc */
};

STATIC EFI_EVENT mSetVirtualAddressMapEvent;

STATIC
VOID
EFIAPI
NotifySetVirtualAddressMap (
  IN EFI_EVENT  Event,
  IN VOID      *Context
  )
{
  EFI_STATUS    Status;

  DEBUG ((DEBUG_ERROR, "Before OpenSession = %p, CloseSession = %p, InvokeFunc = %p\n",
		&gOpteeClient.OpenSession, &gOpteeClient.CloseSession,
		&gOpteeClient.InvokeFunc));
  DEBUG ((DEBUG_INFO, "Optee: set virtual address map\n"));
  Status = EfiConvertPointer (0x0, (VOID **)&gOpteeClient.OpenSession);
  ASSERT_EFI_ERROR (Status);
  Status = EfiConvertPointer (0x0, (VOID **)&gOpteeClient.CloseSession);
  ASSERT_EFI_ERROR (Status);
  Status = EfiConvertPointer (0x0, (VOID **)&gOpteeClient.InvokeFunc);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_ERROR, "After OpenSession = %p, CloseSession = %p, InvokeFunc = %p\n",
		&gOpteeClient.OpenSession, &gOpteeClient.CloseSession,
		&gOpteeClient.InvokeFunc));
  Status = ArchSetVirtualAddressMap ();
//  ASSERT_EFI_ERROR (Status);
}

EFI_STATUS
EFIAPI
OpteeClientInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_HANDLE    Handle;

  DEBUG ((DEBUG_ERROR, "***** %a, %u *****\n", __FUNCTION__, __LINE__));
  Status = OpteeInit ();
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_ERROR, "***** %a, %u *****\n", __FUNCTION__, __LINE__));
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  NotifySetVirtualAddressMap,
                  NULL,
                  &mSetVirtualAddressMapEvent
                  );
  if (EFI_ERROR (Status)) {
    goto CloseOptee;
  }

  DEBUG ((DEBUG_ERROR, "***** %a, %u *****\n", __FUNCTION__, __LINE__));
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gOpteeClientProtocolGuid,
                  &gOpteeClient,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto CloseEvent;
  }

  return EFI_SUCCESS;

CloseEvent:
  gBS->CloseEvent (&mSetVirtualAddressMapEvent);
CloseOptee:
  OpteeClose ();

  return Status;
}
