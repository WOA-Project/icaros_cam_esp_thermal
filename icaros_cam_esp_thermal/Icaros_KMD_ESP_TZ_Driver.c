/*++

Module Name:

	driver.c

Abstract:

	This file contains the driver entry points and callbacks.

Environment:

	Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "Debug.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, CameraESPTZEvtDeviceAdd)
#pragma alloc_text (PAGE, CameraESPTZEvtDriverContextCleanup)
#endif

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT  DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:
	DriverEntry initializes the driver and is the first routine called by the
	system after the driver is loaded. DriverEntry specifies the other entry
	points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

	DriverObject - represents the instance of the function driver that is loaded
	into memory. DriverEntry must initialize members of DriverObject before it
	returns to the caller. DriverObject is allocated by the system before the
	driver is loaded, and it is released by the system after the system unloads
	the function driver from memory.

	RegistryPath - represents the driver specific path in the Registry.
	The function driver can use the path to store driver related data between
	reboots. The path does not store hardware instance specific data.

Return Value:

	STATUS_SUCCESS if successful,
	STATUS_UNSUCCESSFUL otherwise.

--*/
{
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Driver.c",
		58,
		"DriverEntry");

	//
	// Register a cleanup callback so that we can call WPP_CLEANUP when
	// the framework driver object is deleted during driver unload.
	//
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.EvtCleanupCallback = CameraESPTZEvtDriverContextCleanup;
	attributes.SynchronizationScope = WdfSynchronizationScopeNone;
	config.DriverPoolTag = 'PSEL';

	WDF_DRIVER_CONFIG_INIT(&config,
		CameraESPTZEvtDeviceAdd
	);

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		WDF_NO_HANDLE
	);

	if (!NT_SUCCESS(status)) {
		EspDbgPrintlEx(0, "ESP KMD TZ", "WdfDriverCreate() failed, status =  0x%x", status);
		return status;
	}

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Driver.c",
		86,
		"DriverEntry");
	return status;
}

NTSTATUS
CameraESPTZEvtDeviceAdd(
	_In_    WDFDRIVER       Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
/*++
Routine Description:

	EvtDeviceAdd is called by the framework in response to AddDevice
	call from the PnP manager. We create and initialize a device object to
	represent a new instance of the device.

Arguments:

	Driver - Handle to a framework driver object created in DriverEntry

	DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

	NTSTATUS

--*/
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Driver.c",
		121,
		"CameraESPTZEvtDeviceAdd");

	status = CameraESPTZCreateDevice(DeviceInit);

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Driver.c",
		125,
		"CameraESPTZEvtDeviceAdd");

	return status;
}

VOID
CameraESPTZEvtDriverContextCleanup(
	_In_ WDFOBJECT DriverObject
)
/*++
Routine Description:

	Free all the resources allocated in DriverEntry.

Arguments:

	DriverObject - handle to a WDF Driver object.

Return Value:

	VOID.

--*/
{
	UNREFERENCED_PARAMETER(DriverObject);

	PAGED_CODE();

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s >>>",
		"Icaros_KMD_ESP_TZ_Driver.c",
		153,
		"CameraESPTZEvtDriverContextCleanup");

	EspDbgPrintlEx(
		9,
		"ESP KMD TZ",
		"File: %s, Line: %d, Function: %s <<<",
		"Icaros_KMD_ESP_TZ_Driver.c",
		156,
		"CameraESPTZEvtDriverContextCleanup");
}
