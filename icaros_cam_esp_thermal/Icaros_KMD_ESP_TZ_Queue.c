/*++

Module Name:

	queue.c

Abstract:

	This file contains the queue entry points and callbacks.

Environment:

	Kernel-mode Driver Framework

--*/

#include "Device.h"
#include "Debug.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, CameraESPTZQueueInitialize)
#endif

VOID
CameraESPTZEvtIoInternalDeviceControl(
	WDFQUEUE Queue,
	WDFREQUEST Request,
	size_t OutputBufferLength,
	size_t InputBufferLength,
	ULONG IoControlCode)

	/*++

	Description:

		The system uses IoInternalDeviceControl requests to communicate with the
		ACPI driver on the device stack. For proper operation of thermal zones,
		these requests must be forwarded unless the driver knows how to handle
		them.

	--*/

{
	WDF_REQUEST_SEND_OPTIONS RequestSendOptions;
	BOOLEAN Return;
	NTSTATUS Status;

	UNREFERENCED_PARAMETER(IoControlCode);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Queue.c",
		348,
		"CameraESPTZEvtIoInternalDeviceControl");

	WdfRequestFormatRequestUsingCurrentType(Request);

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&RequestSendOptions,
		WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

	Return = WdfRequestSend(
		Request,
		WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
		&RequestSendOptions);

	if (Return == FALSE) {
		Status = WdfRequestGetStatus(Request);
		EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestSend() Failed. Request Status=0x%x\n", Status);

		WdfRequestComplete(Request, Status);
	}

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Queue.c",
		370,
		"CameraESPTZEvtIoInternalDeviceControl");
}

NTSTATUS
CameraESPTZQueueInitialize(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	 The I/O dispatch callbacks for the frameworks device object
	 are configured in this function.

	 A single default I/O Queue is configured for parallel request
	 processing, and a driver context memory allocation is created
	 to hold our structure QUEUE_CONTEXT.

Arguments:

	Device - Handle to a framework device object.

Return Value:

	VOID

--*/
{
	WDFQUEUE queue;
	NTSTATUS status;
	PFDO_DATA DevExt;
	WDF_IO_QUEUE_CONFIG queueConfig;
	WDF_IO_QUEUE_CONFIG PendingRequestQueueConfig;

	PAGED_CODE();

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Queue.c",
		55,
		"CameraESPTZQueueInitialize");

	DevExt = GetDeviceExtension(Device);

	//
	// Configure a default queue so that requests that are not
	// configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
	// other queues get dispatched here.
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&queueConfig,
		WdfIoQueueDispatchParallel
	);


	queueConfig.EvtIoDeviceControl = CameraESPTZEvtIoDeviceControl;

	//
	// The system uses IoInternalDeviceControl requests to communicate with the
	// ACPI driver on the device stack. For proper operation of thermal zones,
	// these requests must be forwarded unless the driver knows how to handle
	// them.
	//

	queueConfig.EvtIoInternalDeviceControl = CameraESPTZEvtIoInternalDeviceControl;

	status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
	);

	if (!NT_SUCCESS(status)) {
		EspDbgPrintlEx(0, "ESP KMD TZ", "WdfIoQueueCreate() failed, status = 0x%x", status);
		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Queue.c",
			87,
			"CameraESPTZQueueInitialize");
		return status;
	}

	//
	// Configure a manual dispatch queue for pending requests. This queue
	// stores requests to read the sensor state which can't be retired
	// immediately.
	//

	WDF_IO_QUEUE_CONFIG_INIT(&PendingRequestQueueConfig,
		WdfIoQueueDispatchManual);

	PendingRequestQueueConfig.EvtIoStop = CameraESPTZEvtIoStop;

	status = WdfIoQueueCreate(Device,
		&PendingRequestQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&DevExt->PendingRequestQueue);

	if (!NT_SUCCESS(status)) {
		EspDbgPrintlEx(0, "ESP KMD TZ", "Pending request WdfIoQueueCreate() failed. 0x%x", status);
		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Queue.c",
			110,
			"CameraESPTZQueueInitialize");

		return status;
	}

	status = WdfWaitLockCreate(NULL, &DevExt->QueueLock);

	if (!NT_SUCCESS(status)) {
		EspDbgPrintlEx(0, "ESP KMD TZ", "Queue lock: WdfWaitLockCreate() failed. 0x%x", status);
		EspDbgPrintlEx(
			9,
			"ESP KMD TZ",
			"File: %s, Line: %d, Function: %s <<<",
			"Icaros_KMD_ESP_TZ_Queue.c",
			121,
			"CameraESPTZQueueInitialize");

		return status;
	}

	return status;
}

VOID
CameraESPTZEvtIoDeviceControl(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t OutputBufferLength,
	_In_ size_t InputBufferLength,
	_In_ ULONG IoControlCode
)
/*++

Routine Description:

	This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

	Queue -  Handle to the framework queue object that is associated with the
			 I/O request.

	Request - Handle to a framework request object.

	OutputBufferLength - Size of the output buffer in bytes

	InputBufferLength - Size of the input buffer in bytes

	IoControlCode - I/O control code.

Return Value:

	VOID

--*/
{
	WDFDEVICE Device;
	int Status;
	WDFIOTARGET Target;
	WDF_REQUEST_SEND_OPTIONS Options;

	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Queue.c",
		172,
		"CameraESPTZEvtIoDeviceControl");
	Device = WdfIoQueueGetDevice(Queue);
	if (IoControlCode > 0x222408)
	{
		if (IoControlCode == 2703504)
		{
			CameraESPTZAddReadRequest(Device, Request);
			goto LABEL_13;
		}
	}
	else
	{
		switch (IoControlCode)
		{
		case 0x222408u:
			CameraESPTZCameraOffNotification(Device, Request);
			goto LABEL_13;
		case 0x222400u:
			CameraESPTZSetTemperature(Device, Request);
			goto LABEL_13;
		case 0x222404u:
			CameraESPTZCameraOnNotification(Device, Request);
			goto LABEL_13;
		}
	}
	WDF_REQUEST_SEND_OPTIONS_INIT(&Options, 8u);
	WdfRequestFormatRequestUsingCurrentType(Request);
	Target = WdfDeviceGetIoTarget(Device);
	if (WdfRequestSend(Request, Target, &Options) == FALSE)
	{
		Status = WdfRequestGetStatus(Request);
		EspDbgPrintlEx(0, "ESP KMD TZ", "WdfRequestSend() Failed. Request Status = 0x%x\n", Status);
		WdfRequestComplete(Request, Status);
	}
LABEL_13:
	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Queue.c",
		224,
		"CameraESPTZEvtIoDeviceControl");
}

VOID
CameraESPTZEvtIoStop(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG ActionFlags
)
/*++

Routine Description:

	This routine is called when the framework is stopping the request's I/O
	queue.

Arguments:

	Queue - Supplies handle to the framework queue object that is associated
			with the I/O request.

	Request - Supplies handle to a framework request object.

	ActionFlags - Supplies the reason that the callback is being called.

Return Value:

	None.

--*/

{

	NTSTATUS Status;

	UNREFERENCED_PARAMETER(Queue);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Queue.c",
		264,
		"CameraESPTZEvtIoStop");

	if (ActionFlags & WdfRequestStopRequestCancelable) {
		Status = WdfRequestUnmarkCancelable(Request);
		if (Status == STATUS_CANCELLED) {
			goto CameraESPTZQueueIoStopEnd;
		}

		NT_ASSERT(NT_SUCCESS(Status));
	}

	WdfRequestStopAcknowledge(Request, FALSE);

CameraESPTZQueueIoStopEnd:

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Queue.c",
		307,
		"CameraESPTZEvtIoStop");

	return;
}
