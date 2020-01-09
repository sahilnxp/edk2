/** @file
  OP-TEE Client API header file.

  Copyright (c) 2018-2020, Linaro Ltd. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _OPTEE_CLIENT_PROTOCOL_H_
#define _OPTEE_CLIENT_PROTOCOL_H_

#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_NONE                0x0
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INPUT         0x1
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_OUTPUT        0x2
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_VALUE_INOUT         0x3
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INPUT        0x9
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_OUTPUT       0xa
#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MEMORY_INOUT        0xb

#define OPTEE_MESSAGE_ATTRIBUTE_TYPE_MASK                0xff

#define OPTEE_SUCCESS                           0x00000000
#define OPTEE_ORIGIN_COMMUNICATION              0x00000002
#define OPTEE_ERROR_COMMUNICATION               0xFFFF000E

#define OPTEE_CLIENT_PROTOCOL_GUID \
  { 0x934251e6, 0x1891, 0x4833, { 0xbb, 0xd5, 0x29, 0xac, 0xd3, 0xaf, 0x2f, 0x80 } }

typedef struct _OPTEE_CLIENT_PROTOCOL OPTEE_CLIENT_PROTOCOL;

typedef struct {
  UINT64    BufferAddress;
  UINT64    Size;
  UINT64    SharedMemoryReference;
} OPTEE_MESSAGE_PARAM_MEMORY;

typedef struct {
  UINT64    A;
  UINT64    B;
  UINT64    C;
} OPTEE_MESSAGE_PARAM_VALUE;

typedef struct {
  UINT64 Attribute;
  union {
    OPTEE_MESSAGE_PARAM_MEMORY   Memory;
    OPTEE_MESSAGE_PARAM_VALUE    Value;
  } Union;
} OPTEE_MESSAGE_PARAM;

#define OPTEE_MAX_CALL_PARAMS       4

typedef struct {
  UINT32    Command;
  UINT32    Function;
  UINT32    Session;
  UINT32    CancelId;
  UINT32    Pad;
  UINT32    Return;
  UINT32    ReturnOrigin;
  UINT32    NumParams;

  // NumParams tells the actual number of element in Params
  OPTEE_MESSAGE_PARAM  Params[OPTEE_MAX_CALL_PARAMS];
} OPTEE_MESSAGE_ARG;

typedef struct {
  EFI_GUID  Uuid;           // [in] GUID/UUID of the Trusted Application
  UINT32    Session;        // [out] Session id
  UINT32    Return;         // [out] Return value
  UINT32    ReturnOrigin;   // [out] Origin of the return value
} OPTEE_OPEN_SESSION_ARG;

typedef struct {
  UINT32    Function;       // [in] Trusted Application function, specific to the TA
  UINT32    Session;        // [in] Session id
  UINT32    Return;         // [out] Return value
  UINT32    ReturnOrigin;   // [out] Origin of the return value
  OPTEE_MESSAGE_PARAM  Params[OPTEE_MAX_CALL_PARAMS]; // Params for function to be invoked
} OPTEE_INVOKE_FUNCTION_ARG;

typedef
EFI_STATUS
(*OPTEE_CLIENT_OPEN_SESSION)(
  IN     OPTEE_CLIENT_PROTOCOL        *This,
  IN OUT OPTEE_OPEN_SESSION_ARG       *OpenSessionArg
);

typedef
EFI_STATUS
(*OPTEE_CLIENT_CLOSE_SESSION)(
  IN OPTEE_CLIENT_PROTOCOL        *This,
  IN UINT32                       Session
);

typedef
EFI_STATUS
(*OPTEE_CLIENT_INVOKE_FUNCTION)(
  IN     OPTEE_CLIENT_PROTOCOL        *This,
  IN OUT OPTEE_INVOKE_FUNCTION_ARG    *InvokeFunctionArg
);

struct _OPTEE_CLIENT_PROTOCOL {
  OPTEE_CLIENT_OPEN_SESSION       OpenSession;
  OPTEE_CLIENT_CLOSE_SESSION      CloseSession;
  OPTEE_CLIENT_INVOKE_FUNCTION    InvokeFunc;
};

extern EFI_GUID gOpteeClientProtocolGuid;

#endif /* _OPTEE_CLIENT_PROTCOL_H_ */
