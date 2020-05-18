/** @file

  Copyright (c) 2020, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MmCommunicate.h"

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
    return EFI_OUT_OF_RESOURCES;

  default:
    return EFI_ACCESS_DENIED;
  }
}

STATIC
UINT32
NewSession (
  IN OPTEE_CLIENT_PROTOCOL   *Client
  )
{
  OPTEE_OPEN_SESSION_ARG        OpenArg;

  if (Client == NULL) {
    return 0;
  }

  SetMem (&OpenArg, 0, sizeof (OpenArg));
  CopyGuid (&OpenArg.Uuid, &MmUuid);
  Client->OpenSession (Client, &OpenArg);
  return OpenArg.Session;
}

STATIC
EFI_STATUS
EFIAPI
MmCommunicate (
  IN CONST EFI_MM_COMMUNICATION_PROTOCOL  *This,
  IN OUT VOID                             *CommBuffer,
  IN OUT UINTN                            *CommSize OPTIONAL
  )
{
  EFI_STATUS                    Status;
  OPTEE_MM_SESSION              *OpteeMm;
  OPTEE_CLIENT_PROTOCOL         *Client;
  OPTEE_INVOKE_FUNCTION_ARG     InvokeArg;

  if (This == NULL || CommBuffer == NULL ||
      CommSize == NULL || *CommSize == 0) {
    return EFI_INVALID_PARAMETER;
  }

  OpteeMm = OPTEE_MM_SESSION_FROM_MM_COMMUNICATION_PROTOCOL_THIS (This);
  Client = OpteeMm->Client;
  if (Client == NULL) {
    return EFI_NOT_READY;
  }
  if (OpteeMm->Session != 0) {
    return EFI_OUT_OF_RESOURCES;
  }

  OpteeMm->Session = NewSession (Client);
  if (OpteeMm->Session == 0) {
    return EFI_ACCESS_DENIED;
  }

  ZeroMem (&InvokeArg, sizeof (InvokeArg));
  InvokeArg.Function = OPTEE_TA_MM_FUNC_COMMUNICATE;
  InvokeArg.Session = OpteeMm->Session;
  InvokeArg.Params[0].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT;
  InvokeArg.Params[0].Union.Memory.BufferAddress = (UINTN) CommBuffer;
  InvokeArg.Params[0].Union.Memory.Size = *CommSize;
  InvokeArg.Params[1].Attribute = OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT;

  Status = Client->InvokeFunc (Client, &InvokeArg);
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
  Client->CloseSession (Client, OpteeMm->Session);
  OpteeMm->Session = 0;

  return Status;
}

STATIC
EFI_STATUS
GetMmCompatibility (
  IN OPTEE_CLIENT_PROTOCOL    *Client
  )
{
  UINT32      Session;

  Session = NewSession (Client);
  if (Session == 0) {
    return EFI_ACCESS_DENIED;
  }

  // TODO: check mm version once the OPTEE_TA_MM_FUNC_VERSION is supported

  Client->CloseSession (Client, Session);
  return EFI_SUCCESS;
}

STATIC OPTEE_MM_SESSION mOpteeMm = {
  OPTEE_MM_SESSION_SIGNATURE,         // Signature
  NULL,                               // Handle
  NULL,                               // AgentHandle
  NULL,                               // Client
  {
    MmCommunicate, 
  },                                  // Mm
  0,                                  // Session
};

STATIC EFI_EVENT  mSetVirtualAddressMapEvent;

STATIC
VOID
EFIAPI
NotifySetVirtualAddressMap (
  IN EFI_EVENT  Event,
  IN VOID      *Context
  )
{
  EFI_STATUS  Status;
#if 0
  DEBUG ((DEBUG_ERROR, "Handle = %p, AgentHandle = %p, Client = %p, Communicate = %p\n",
		&mOpteeMm.Handle, &mOpteeMm.AgentHandle, &mOpteeMm.Client, &mOpteeMm.Mm.Communicate));
  Status = EfiConvertPointer (0x0, (VOID **)&mOpteeMm.Handle);
  ASSERT_EFI_ERROR (Status);
  Status = EfiConvertPointer (0x0, (VOID **)&mOpteeMm.AgentHandle);
  ASSERT_EFI_ERROR (Status);
#endif
  Status = EfiConvertPointer (0x0, (VOID **)&mOpteeMm.Client);
  ASSERT_EFI_ERROR (Status);
  Status = EfiConvertPointer (0x0, (VOID **)&mOpteeMm.Mm.Communicate);
  ASSERT_EFI_ERROR (Status);
}

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
  MmCommunicate (&mOpteeMm.Mm, &Header, &Size);
}

EFI_STATUS
EFIAPI
MmCommunicationInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS      Status;
  EFI_HANDLE      *HandleBuffer;
  UINTN           HandleCount;
  UINTN           Index;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gOpteeClientProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  ASSERT_EFI_ERROR (Status);

  mOpteeMm.Handle = HandleBuffer[0];
  mOpteeMm.AgentHandle = ImageHandle;
  FreePool (HandleBuffer);

  Status = gBS->OpenProtocol (
                  mOpteeMm.Handle,
                  &gOpteeClientProtocolGuid,
                  (VOID **)&mOpteeMm.Client,
                  mOpteeMm.AgentHandle,
                  mOpteeMm.Handle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  ASSERT_EFI_ERROR (Status);

  Status = GetMmCompatibility (mOpteeMm.Client);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MM: Incompatible MM version\n"));
    goto CloseProtocol;
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

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  NotifySetVirtualAddressMap,
                  NULL,
                  &mSetVirtualAddressMapEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MM: Failed to create set virtual address event\n"));
    goto CloseGuidedEvents;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mOpteeMm.Handle,
                  &gEfiMmCommunicationProtocolGuid,
                  &mOpteeMm.Mm,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MM: Failed to install communication protocol\n"));
    goto CloseVirtAddressEvent;
  }
  
  return Status;

CloseVirtAddressEvent:
  gBS->CloseEvent (&mSetVirtualAddressMapEvent);
CloseGuidedEvents:
  for (Index = 0; Index < ARRAY_SIZE (mGuidedEventGuid); Index++) {
    gBS->CloseEvent (&mGuidedEvent[Index]);
  }
CloseProtocol:
  gBS->CloseProtocol (
          mOpteeMm.Handle,
          &gOpteeClientProtocolGuid,
          mOpteeMm.AgentHandle,
          mOpteeMm.Handle
          );

  return Status;
}
