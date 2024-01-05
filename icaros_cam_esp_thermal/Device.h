/*++/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    simsensor.h

Abstract:

    This is the header file for the simulated sensor driver.

@@BEGIN_DDKSPLIT
Author:

    Nicholas Brekhus (NiBrekhu) 25-Jul-2011

Revision History:

@@END_DDKSPLIT
--*/

//--------------------------------------------------------------------- Pragmas

#pragma once

//-------------------------------------------------------------------- Includes

#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdmguid.h>
#include <poclass.h>
#include "Public.h"

//----------------------------------------------------------------- Definitions

typedef struct {
    WDFQUEUE    PendingRequestQueue;
    WDFWAITLOCK QueueLock;
    WDFWORKITEM InterruptWorker;

    //
    // Virtual temperature sensor internal state. This portion of the context
    // should be opaque to most of the driver, except the portion implementing
    // the virtual sensor hardware.
    //

    struct {
        PVOID       PolicyHandle;
        BOOLEAN     Enabled;
        ULONG       LowerBound;
        ULONG       UpperBound;
        ULONG       Temperature;
        WDFWAITLOCK Lock;
    } Sensor;
} FDO_DATA, * PFDO_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_DATA, GetDeviceExtension);

typedef struct {
    LARGE_INTEGER ExpirationTime;
    ULONG HighTemperature;
    ULONG LowTemperature;
} READ_REQUEST_CONTEXT, * PREAD_REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(READ_REQUEST_CONTEXT);

EXTERN_C_START

//
// Function to initialize the device and its callbacks
//
NTSTATUS
CameraESPTZCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

_IRQL_requires_(PASSIVE_LEVEL)
VOID
CameraESPTZAddReadRequest(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST ReadRequest
);

_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
CameraESPTZAreConstraintsSatisfied(
    _In_ ULONG Temperature,
    _In_ ULONG LowerBound,
    _In_ ULONG UpperBound,
    _In_ LARGE_INTEGER DueTime
);

_IRQL_requires_(PASSIVE_LEVEL)
VOID
CameraESPTZCheckQueuedRequest(
    _In_ WDFDEVICE Device,
    _In_ ULONG Temperature,
    _Inout_ PULONG LowerBound,
    _Inout_ PULONG UpperBound,
    _In_ WDFREQUEST Request
);

VOID
CameraESPTZEvtExpiredRequestTimer(
    WDFTIMER Timer
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
CameraESPTZScanPendingQueue(
    _In_ WDFDEVICE Device
);

VOID
CameraESPTZCameraOffNotification(
    WDFDEVICE Device,
    WDFREQUEST Request
);

VOID
CameraESPTZCameraOnNotification(
    WDFDEVICE Device,
    WDFREQUEST Request
);

VOID
CameraESPTZSetTemperature(
    WDFDEVICE Device,
    WDFREQUEST ReadRequest
);

EXTERN_C_END
