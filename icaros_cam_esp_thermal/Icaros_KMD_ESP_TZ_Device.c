/*++

Module Name:

	device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.

Environment:

	Kernel-mode Driver Framework

--*/

#include "Device.h"
#include "Debug.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, CameraESPTZCreateDevice)
#pragma alloc_text (PAGE, CameraESPTZAddReadRequest)
#pragma alloc_text (PAGE, CameraESPTZAreConstraintsSatisfied)
#pragma alloc_text (PAGE, CameraESPTZCheckQueuedRequest)
#pragma alloc_text (PAGE, CameraESPTZEvtExpiredRequestTimer)
#pragma alloc_text (PAGE, CameraESPTZScanPendingQueue)
#endif

ULONG
CameraESPTZReadTemperature(
	_In_ WDFDEVICE Device
)

/*++

Routine Description:

	This routine is invoked to read the current temperature of the device.

Arguments:

	Device - Supplies a handle to the device.

	LowerBound - Supplies the temperature below which the device should issue
		an interrupt.

	UpperBound - Supplies the temperature above which the device should issue
		an interrupt.

Return Value:

	NTSTATUS

--*/

{

	PFDO_DATA DevExt;
	ULONG Temperature;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		434,
		"CameraESPTZReadTemperature");

	DevExt = GetDeviceExtension(Device);
	WdfWaitLockAcquire(DevExt->Sensor.Lock, NULL);
	Temperature = DevExt->Sensor.Temperature;
	WdfWaitLockRelease(DevExt->Sensor.Lock);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		441,
		"CameraESPTZReadTemperature");

	return Temperature;
}

VOID
CameraESPTZEvtExpiredRequestTimer(
	WDFTIMER Timer
)

/*++

Routine Description:

	This routine is invoked when a request timer expires. A scan of the pending
	queue to complete expired and satisfied requests is initiated.

Arguments:

	Timer - Supplies a handle to the timer which expired.

--*/

{

	PFDO_DATA DevExt;
	WDFDEVICE Device;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		1044,
		"CameraESPTZEvtExpiredRequestTimer");

	PAGED_CODE();

	Device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
	DevExt = GetDeviceExtension(Device);
	WdfWaitLockAcquire(DevExt->QueueLock, NULL);
	CameraESPTZScanPendingQueue(Device);
	WdfWaitLockRelease(DevExt->QueueLock);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		1053,
		"CameraESPTZEvtExpiredRequestTimer");
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
CameraESPTZAddReadRequest(
	_In_ WDFDEVICE Device,
	_In_ WDFREQUEST ReadRequest
)

/*++

Routine Description:

	Handles IOCTL_THERMAL_READ_TEMPERATURE. If the request can be satisfied,
	it is completed immediately. Else, adds request to pending request queue.

Arguments:

	Device - Supplies a handle to the device that received the request.

	ReadRequest - Supplies a handle to the request.

--*/

{
	ULONG BytesReturned;
	PREAD_REQUEST_CONTEXT Context;
	WDF_OBJECT_ATTRIBUTES ContextAttributes;
	PFDO_DATA DevExt;
	LARGE_INTEGER ExpirationTime;
	size_t Length;
	BOOLEAN LockHeld;
	PULONG RequestTemperature;
	NTSTATUS Status;
	ULONG Temperature;
	WDFTIMER Timer;
	WDF_OBJECT_ATTRIBUTES TimerAttributes;
	WDF_TIMER_CONFIG TimerConfig;
	PTHERMAL_WAIT_READ ThermalWaitRead;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		210,
		"CameraESPTZAddReadRequest");

	PAGED_CODE();

	DevExt = GetDeviceExtension(Device);
	BytesReturned = 0;
	LockHeld = FALSE;
	Status = WdfRequestRetrieveInputBuffer(ReadRequest,
		sizeof(THERMAL_WAIT_READ),
		&ThermalWaitRead,
		&Length);

	if (!NT_SUCCESS(Status) || Length != sizeof(THERMAL_WAIT_READ)) {

		EspDbgPrintlEx(0, "ESP KMD TZ", "%s: This request is malformed, bail.", "CameraESPTZAddReadRequest");

		//
		// This request is malformed, bail.
		//

		WdfRequestCompleteWithInformation(ReadRequest, Status, BytesReturned);
		goto AddReadRequestEnd;
	}


	if (ThermalWaitRead->Timeout != -1 /* INFINITE */) {

		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"%s: if (ThermalWaitRead->Timeout != -1 /* INFINITE */ )",
			"CameraESPTZAddReadRequest");

		//
		// Estimate the system time this request will expire at.
		//

		KeQuerySystemTime(&ExpirationTime);
		ExpirationTime.QuadPart += (ULONGLONG)ThermalWaitRead->Timeout * 10000;

		EspDbgPrintlEx(9, "ESP KMD TZ", "ExpirationTime.QuadPart = %d", (int)ExpirationTime.QuadPart);
	}
	else {

		EspDbgPrintlEx(9, "ESP KMD TZ", "%s: Value which indicates the request never expires.", "CameraESPTZAddReadRequest");

		//
		// Value which indicates the request never expires.
		//

		ExpirationTime.QuadPart = -1LL /* INFINITE */;
	}

	//
	// Handle the immediate timeout case in the fast path.
	//

	Temperature = CameraESPTZReadTemperature(Device);
	if (CameraESPTZAreConstraintsSatisfied(Temperature,
		ThermalWaitRead->LowTemperature,
		ThermalWaitRead->HighTemperature,
		ExpirationTime)) {

		Status = WdfRequestRetrieveOutputBuffer(ReadRequest,
			sizeof(ULONG),
			&RequestTemperature,
			&Length);

		if (NT_SUCCESS(Status) && Length == sizeof(ULONG)) {
			EspDbgPrintlEx(9, "ESP KMD TZ", "%s: fast path, temperature %lu", "CameraESPTZAddReadRequest", Temperature);
			*RequestTemperature = Temperature;
			BytesReturned = sizeof(ULONG);

		}
		else {
			Status = STATUS_INVALID_PARAMETER;
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestRetrieveOutputBuffer() Failed. 0x%x", Status);
		}

		EspDbgPrintlEx(9, "ESP KMD TZ", "Completing fast path IOCTL_THERMAL_READ_TEMPERATURE");
		WdfRequestCompleteWithInformation(ReadRequest, Status, BytesReturned);
	}
	else {

		EspDbgPrintlEx(9, "ESP KMD TZ", "%s: Creating request and adding it to pending queue.", "CameraESPTZAddReadRequest");
		EspDbgPrintlEx(9, "ESP KMD TZ", "%s: DevExt->queueLock LOCK ON", "CameraESPTZAddReadRequest");

		WdfWaitLockAcquire(DevExt->QueueLock, NULL);
		LockHeld = TRUE;

		//
		// Create a context to store request-specific information.
		//

		WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&ContextAttributes,
			READ_REQUEST_CONTEXT);

		Status = WdfObjectAllocateContext(ReadRequest,
			&ContextAttributes,
			&Context);

		if (!NT_SUCCESS(Status)) {
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfObjectAllocateContext() Failed. 0x%x", Status);

			WdfRequestCompleteWithInformation(ReadRequest,
				Status,
				BytesReturned);

			goto AddReadRequestEnd;
		}

		Context->ExpirationTime.QuadPart = ExpirationTime.QuadPart;
		Context->LowTemperature = ThermalWaitRead->LowTemperature;
		Context->HighTemperature = ThermalWaitRead->HighTemperature;
		if (Context->ExpirationTime.QuadPart != -1LL /* INFINITE */) {

			//
			// This request eventually expires, create a timer to complete it.
			//

			WDF_TIMER_CONFIG_INIT(&TimerConfig, CameraESPTZEvtExpiredRequestTimer);
			WDF_OBJECT_ATTRIBUTES_INIT(&TimerAttributes);
			TimerAttributes.ExecutionLevel = WdfExecutionLevelPassive;
			TimerAttributes.SynchronizationScope = WdfSynchronizationScopeNone;
			TimerAttributes.ParentObject = Device;
			Status = WdfTimerCreate(&TimerConfig,
				&TimerAttributes,
				&Timer);

			if (!NT_SUCCESS(Status)) {
				EspDbgPrintlEx(0, "ESP KMD TZ", "WdfTimerCreate() Failed. 0x%x", Status);
				WdfRequestCompleteWithInformation(ReadRequest,
					Status,
					BytesReturned);

				goto AddReadRequestEnd;
			}

			EspDbgPrintlEx(
				9,
				"ESP KMD TZ",
				"%s:WdfTimerStart(), Timeout = %lu ms",
				"CameraESPTZAddReadRequest",
				ThermalWaitRead->Timeout);

			WdfTimerStart(Timer,
				WDF_REL_TIMEOUT_IN_MS(ThermalWaitRead->Timeout));

		}

		Status = WdfRequestForwardToIoQueue(ReadRequest,
			DevExt->PendingRequestQueue);

		if (!NT_SUCCESS(Status)) {
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestForwardToIoQueue() Failed. 0x%x", Status);

			WdfRequestCompleteWithInformation(ReadRequest,
				Status,
				BytesReturned);

			goto AddReadRequestEnd;
		}

		//
		// Force a rescan of the queue to update the interrupt thresholds.
		//

		CameraESPTZScanPendingQueue(Device);
	}

AddReadRequestEnd:

	if (LockHeld == TRUE) {
		WdfWaitLockRelease(DevExt->QueueLock);
		EspDbgPrintlEx(9, "ESP KMD TZ", "%s: DevExt->queueLock LOCK OFF", "CameraESPTZAddReadRequest");
	}

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		400,
		"CameraESPTZAddReadRequest");
}

VOID
CameraESPTZCameraOffNotification(
	WDFDEVICE Device,
	WDFREQUEST Request
)
{
	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		609,
		"CameraESPTZCameraOffNotification");

	GetDeviceExtension(Device)->Sensor.Temperature = 2940;
	WdfRequestComplete(Request, 0);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		617,
		"CameraESPTZCameraOffNotification");
}

VOID
CameraESPTZCameraOnNotification(
	WDFDEVICE Device,
	WDFREQUEST Request
)
{
	WDF_DEVICE_STATE PnpDeviceState;

	UNREFERENCED_PARAMETER(Device);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		591,
		"CameraESPTZCameraOnNotification");
	WDF_DEVICE_STATE_INIT(&PnpDeviceState);
	WdfRequestComplete(Request, 0);
	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		597,
		"CameraESPTZCameraOnNotification");
}

VOID
CameraESPTZTemperatureInterrupt(
	_In_ WDFDEVICE Device
)


/*++

Routine Description:

	This routine is invoked to simulate an interrupt from the virtual sensor
	device. It performs all the work a normal ISR would perform.

Arguments:

	Device - Supplies a handle to the device.

Return Value:

	None.

--*/

{

	PFDO_DATA DevExt;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		1012,
		"CameraESPTZTemperatureInterrupt");

	DevExt = GetDeviceExtension(Device);
	WdfWorkItemEnqueue(DevExt->InterruptWorker);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		1017,
		"CameraESPTZTemperatureInterrupt");

	return;
}

VOID
CameraESPTZSetTemperature(
	WDFDEVICE Device,
	WDFREQUEST ReadRequest
)
{
	NTSTATUS Status;
	PULONG Temperature;
	ULONG Value;
	FDO_DATA* DevExt;
	size_t Length;
	BOOLEAN Interrupt;

	Status = STATUS_SUCCESS;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		532,
		"CameraESPTZSetTemperature");

	PAGED_CODE();

	Value = 1;
	Temperature = &Value;
	Interrupt = FALSE;

	DevExt = GetDeviceExtension(Device);
	Status = WdfRequestRetrieveInputBuffer(ReadRequest, 0, &Temperature, &Length);

	if (NT_SUCCESS(Status))
	{
		if (Temperature != NULL)
		{
			WdfWaitLockAcquire(DevExt->Sensor.Lock, NULL);

			DevExt->Sensor.Temperature = *Temperature;

			EspDbgPrintlEx(9, "ESP KMD TZ", "%s : Temp %d", "CameraESPTZSetTemperature", DevExt->Sensor.Temperature);

			//
			// Check to see if the temperature has exceeded either of the thresholds
			// for noticing a temperature change. If so, the virtual interrupt will
			// need to be fired.
			//

			if ((DevExt->Sensor.Temperature <= DevExt->Sensor.LowerBound) ||
				(DevExt->Sensor.Temperature >= DevExt->Sensor.UpperBound)) {

				Interrupt = TRUE;

			}

			WdfWaitLockRelease(DevExt->Sensor.Lock);

		}

		WdfRequestComplete(ReadRequest, Status);

		//
		// Fire the virtual interrupt outside the lock, to avoid any locking issues.
		//

		if (Interrupt != FALSE) {
			CameraESPTZTemperatureInterrupt(Device);
		}

		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			577,
			"CameraESPTZSetTemperature");
	}
	else
	{
		EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestRetrieveInputBuffer() Failed. 0x%x", Status);
		WdfRequestCompleteWithInformation(ReadRequest, Status, 0);
		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			549,
			"CameraESPTZSetTemperature");
	}
}

VOID
CameraESPTZSetVirtualInterruptThresholds(
	_In_ WDFDEVICE Device,
	_In_ ULONG LowerBound,
	_In_ ULONG UpperBound
)

/*++

Routine Description:

	This routine is invoked to change the thresholds the virtual sensor driver
	uses to compare against for an interrupt to occur.

Arguments:

	Device - Supplies a handle to the device.

	LowerBound - Supplies the temperature below which the device should issue
		an interrupt.

	UpperBound - Supplies the temperature above which the device should issue
		an interrupt.

Return Value:

	NTSTATUS

--*/

{

	PFDO_DATA DevExt;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		951,
		"CameraESPTZSetVirtualInterruptThresholds");

	DevExt = GetDeviceExtension(Device);
	WdfWaitLockAcquire(DevExt->Sensor.Lock, NULL);

	DevExt->Sensor.LowerBound = LowerBound;
	DevExt->Sensor.UpperBound = UpperBound;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"%s : LowerBound = %lu, UpperBound = %lu",
		"CameraESPTZSetVirtualInterruptThresholds",
		LowerBound,
		UpperBound);

	WdfWaitLockRelease(DevExt->Sensor.Lock);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		985,
		"CameraESPTZSetVirtualInterruptThresholds");

	return;
}

_IRQL_requires_(PASSIVE_LEVEL)
BOOLEAN
CameraESPTZAreConstraintsSatisfied(
	_In_ ULONG Temperature,
	_In_ ULONG LowerBound,
	_In_ ULONG UpperBound,
	_In_ LARGE_INTEGER DueTime
)

/*++

Routine Description:

	Checks whether a request can be retired.

Arguments:

	Temperature - Supplies the device's current temperature.

	LowerBound - Supplies the request's lower temperature bound.

	UpperBound - Supplies the request's upper temperature bound.

	DueTime - Supplies when the request expires.

Return Value:

	TRUE - The request is retireable.

	FALSE - The request is not retireable.

--*/

{
	LARGE_INTEGER CurrentTime;

	PAGED_CODE();

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		482,
		"CameraESPTZAreConstraintsSatisfied");

	if (Temperature <= LowerBound || Temperature >= UpperBound) {

		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s, RETURN: 0x%x <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			489,
			"CameraESPTZAreConstraintsSatisfied",
			1);

		return TRUE;
	}

	//
	// Negative due times are meaningless, except for the special value -1,
	// which represents no timeout.
	//

	if (DueTime.QuadPart < 0) {

		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s, RETURN: 0x%x <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			499,
			"CameraESPTZAreConstraintsSatisfied",
			0);

		return FALSE;
	}

	KeQuerySystemTime(&CurrentTime);

	if ((CurrentTime.QuadPart - DueTime.QuadPart) >= 0) {

		//
		// This request expired in the past.
		//

		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s, RETURN: 0x%x <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			510,
			"CameraESPTZAreConstraintsSatisfied",
			1);

		return TRUE;
	}

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		514,
		"CameraESPTZAreConstraintsSatisfied");

	return FALSE;
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
CameraESPTZCheckQueuedRequest(
	_In_ WDFDEVICE Device,
	_In_ ULONG Temperature,
	_Inout_ PULONG LowerBound,
	_Inout_ PULONG UpperBound,
	_In_ WDFREQUEST Request
)

/*++

Routine Description:

	Examines a request and performs one of the following actions:

	* Retires the request if it is satisfied (the sensor temperature has
	  exceeded the bounds specified in the request)

	* Retires the request if it is expired (the timer due time is in the past)

	* Tightens the upper and lower bounds if the request remains in the queue.

Arguments:

	Device - Supplies a handle to the device which owns this request.

	Temperature - Supplies the current thermal zone temperature.

	LowerBound - Supplies the lower bound threshold to adjust.

	UpperBound - Supplies the upper bound threshold to adjust.

	Request - Supplies a handle to the request.

--*/

{
	ULONG BytesReturned;
	LARGE_INTEGER CurrentTime;
	PFDO_DATA DevExt;
	size_t Length;
	PREAD_REQUEST_CONTEXT Context;
	WDFREQUEST RetrievedRequest;
	PULONG RequestTemperature;
	NTSTATUS Status;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		818,
		"CameraESPTZCheckQueuedRequest");

	PAGED_CODE();

	KeQuerySystemTime(&CurrentTime);
	DevExt = GetDeviceExtension(Device);
	Context = WdfObjectGetTypedContext(Request, READ_REQUEST_CONTEXT);

	RetrievedRequest = NULL;

	//
	// Complete the request if:
	//
	// 1. The temperature has exceeded one of the request thresholds.
	// 2. The request timeout is in the past (but not negative).
	//

	if (CameraESPTZAreConstraintsSatisfied(Temperature,
		Context->LowTemperature,
		Context->HighTemperature,
		Context->ExpirationTime)) {

		Status = WdfIoQueueRetrieveFoundRequest(DevExt->PendingRequestQueue,
			Request,
			&RetrievedRequest);

		if (!NT_SUCCESS(Status)) {
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfIoQueueRetrieveFoundRequest() Failed. 0x%x", Status);

			//
			// Bail, likely because the request disappeared from the
			// queue.
			//

			goto CheckQueuedRequestEnd;
		}

		Status = WdfRequestRetrieveOutputBuffer(RetrievedRequest,
			sizeof(ULONG),
			&RequestTemperature,
			&Length);

		if (NT_SUCCESS(Status) && (Length == sizeof(ULONG))) {
			*RequestTemperature = Temperature;
			BytesReturned = sizeof(ULONG);

		}
		else {

			//
			// The request's return buffer is malformed.
			//

			BytesReturned = 0;
			Status = STATUS_INVALID_PARAMETER;
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestRetrieveOutputBuffer() Failed. 0x%x", -1073741811);

		}

		WdfRequestCompleteWithInformation(RetrievedRequest,
			Status,
			BytesReturned);

	}
	else {

		EspDbgPrintlEx(9, "ESP KMD TZ", "%s : request remains in queue.", "CameraESPTZCheckQueuedRequest");

		//
		// The request will remain in the queue. Update the bounds accordingly.
		//

		if (*LowerBound < Context->LowTemperature) {
			*LowerBound = Context->LowTemperature;
		}

		if (*UpperBound > Context->HighTemperature) {
			*UpperBound = Context->HighTemperature;
		}
	}

CheckQueuedRequestEnd:

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		912,
		"CameraESPTZCheckQueuedRequest");

	return;
}
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
CameraESPTZScanPendingQueue(
	_In_ WDFDEVICE Device
)

/*++

Routine Description:

	This routine scans the device's pending queue for retirable requests.

	N.B. This routine requires the QueueLock be held.

Arguments:

	Device - Supplies a handle to the device.

--*/

{
	WDFREQUEST CurrentRequest;
	PFDO_DATA DevExt;
	WDFREQUEST LastRequest;
	ULONG LowerBound;
	NTSTATUS Status;
	ULONG Temperature;
	ULONG UpperBound;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		684,
		"CameraESPTZScanPendingQueue");

	PAGED_CODE();

	DevExt = GetDeviceExtension(Device);

	Status = STATUS_SUCCESS;

	LastRequest = NULL;
	CurrentRequest = NULL;
	Temperature = CameraESPTZReadTemperature(Device);

	//
	// Prime the walk by finding the first request present. If there are no
	// requests, bail out immediately.
	//

	LowerBound = 0;
	UpperBound = (ULONG)-1;
	Status = WdfIoQueueFindRequest(DevExt->PendingRequestQueue,
		NULL,
		NULL,
		NULL,
		&CurrentRequest);

	//
	// Due to a technical limitation in SDV analysis engine, the following
	// analysis assume has to be inserted to supress a false defect for
	// the wdfioqueueretrievefoundrequest rule.
	//

	_Analysis_assume_(Status == STATUS_NOT_FOUND);

	while (NT_SUCCESS(Status)) {

		//
		// Walk past the current request. By walking past the current request
		// before checking it, the walk doesn't have to restart every time a
		// request is satisfied and removed form the queue.
		//

		LastRequest = CurrentRequest;
		Status = WdfIoQueueFindRequest(DevExt->PendingRequestQueue,
			LastRequest,
			NULL,
			NULL,
			&CurrentRequest);

		//
		// Process the last request.
		//

		CameraESPTZCheckQueuedRequest(Device,
			Temperature,
			&LowerBound,
			&UpperBound,
			LastRequest);

		WdfObjectDereference(LastRequest);

		if (Status == STATUS_NOT_FOUND) {

			EspDbgPrintlEx(9, "ESP KMD TZ", "LastRequest unexpectedly disappeared from the queue. Start over");

			//
			// LastRequest unexpectedly disappeared from the queue. Start over.
			//

			LowerBound = 0;
			UpperBound = (ULONG)-1;
			Status = WdfIoQueueFindRequest(DevExt->PendingRequestQueue,
				NULL,
				NULL,
				NULL,
				&CurrentRequest);

		}

	}

	//
	// Update the thresholds based on the latest contents of the queue.
	//

	CameraESPTZSetVirtualInterruptThresholds(Device, LowerBound, UpperBound);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s, RETURN: 0x%x <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		769,
		"CameraESPTZScanPendingQueue",
		Status);

	return Status;
}

VOID
CameraESPTZInterruptWorker(
	_In_ WDFWORKITEM WorkItem
)

/*++

Routine Description:

	This routine is invoked to call into the device to notify it of a
	temperature change.

Arguments:

	WorkItem - Supplies a handle to this work item.

Return Value:

	None.

--*/

{

	PFDO_DATA DevExt;
	WDFDEVICE Device;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		644,
		"CameraESPTZInterruptWorker");

	Device = (WDFDEVICE)WdfWorkItemGetParentObject(WorkItem);
	DevExt = GetDeviceExtension(Device);
	WdfWaitLockAcquire(DevExt->QueueLock, NULL);
	CameraESPTZScanPendingQueue(Device);
	WdfWaitLockRelease(DevExt->QueueLock);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Device.c",
		653,
		"CameraESPTZInterruptWorker");

	return;
}

NTSTATUS
CameraESPTZInitializeLocalParams(
	WDFDEVICE device
)
{
	PFDO_DATA DevExt;
	NTSTATUS Status;
	WDF_OBJECT_ATTRIBUTES WorkitemAttributes;
	WDF_WORKITEM_CONFIG WorkitemConfig;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Device.c",
		128,
		"CameraESPTZInitializeLocalParams");

	DevExt = GetDeviceExtension(device);

	//
	// Initilize the simulated sensor hardware.
	//

	DevExt->Sensor.LowerBound = 0;
	DevExt->Sensor.UpperBound = (ULONG)-1;
	DevExt->Sensor.Temperature = 2940; //TODO: VIRTUAL_SENSOR_RESET_TEMPERATURE
	Status = WdfWaitLockCreate(0, &DevExt->Sensor.Lock);

	if (NT_SUCCESS(Status))
	{
		//
		// Configure a workitem to process the simulated interrupt.
		//

		WDF_OBJECT_ATTRIBUTES_INIT(&WorkitemAttributes);
		WorkitemAttributes.ParentObject = device;
		WDF_WORKITEM_CONFIG_INIT(&WorkitemConfig,
			CameraESPTZInterruptWorker);

		Status = WdfWorkItemCreate(&WorkitemConfig,
			&WorkitemAttributes,
			&DevExt->InterruptWorker);

		if (NT_SUCCESS(Status))
		{
			EspDbgPrintlEx(
				9,
				"ESP KMD TZ",
				"File: %s, Line: %d, Function: %s <<<",
				"Icaros_KMD_ESP_TZ_Device.c",
				169,
				"CameraESPTZInitializeLocalParams");
		}
		else
		{
			EspDbgPrintlEx(0, "ESP KMD TZ", "WdfWorkItemCreate() Failed. 0x%x", Status);
			EspDbgPrintlEx(
				9,
				"ESP KMD TZ",
				"File: %s, Line: %d, Function: %s <<<",
				"Icaros_KMD_ESP_TZ_Device.c",
				163,
				"CameraESPTZInitializeLocalParams");
		}
	}
	else
	{
		EspDbgPrintlEx(0, "ESP KMD TZ", "Sensor.Lock WdfWaitLockCreate() failed. 0x%x", Status);
		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Device.c",
			141,
			"CameraESPTZInitializeLocalParams");
	}
	return Status;
}

NTSTATUS
CameraESPTZCreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
/*++

Routine Description:

	Worker routine called to create a device and its software resources.

Arguments:

	DeviceInit - Pointer to an opaque init structure. Memory for this
					structure will be freed by the framework when the WdfDeviceCreate
					succeeds. So don't access the structure after that point.

Return Value:

	NTSTATUS

--*/
{
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	WDFDEVICE Device;
	NTSTATUS status;
	UNICODE_STRING SymbolicLinkName;

	PAGED_CODE();

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, FDO_DATA);

	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &Device);

	if (NT_SUCCESS(status)) {
		//
		// Create a device interface so that applications can find and talk
		// to us.
		//
		status = WdfDeviceCreateDeviceInterface(
			Device,
			&GUID_DEVINTERFACE_Icaros_KMD_ESP_Thermal,
			NULL // ReferenceString
		);

		if (NT_SUCCESS(status)) {
			//
			// Initialize the I/O Package and any Queues
			//
			status = CameraESPTZQueueInitialize(Device);

			if (NT_SUCCESS(status)) {

				RtlInitUnicodeString(&SymbolicLinkName, L"\\??\\Icaros_KMD_ESP_Thermal");

				status = WdfDeviceCreateSymbolicLink(Device, &SymbolicLinkName);

				if (NT_SUCCESS(status))
				{
					status = CameraESPTZInitializeLocalParams(Device);
					if (!NT_SUCCESS(status))
						EspDbgPrintlEx(0, "ESP KMD TZ", "CameraESPTZInitializeLocalParams() failed. 0x%x", status);
				}
			}
		}
	}

	return status;
}
