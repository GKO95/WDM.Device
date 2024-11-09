#include "WDMDriver.h"
#include "WDMDriverAPI.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DebugViewPrint("= Fn:DriverEntry:entry =");
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload							= DriverUnload;
	DriverObject->DriverExtension->AddDevice			= AddDevice;
	DriverObject->MajorFunction[IRP_MJ_PNP]				= DispatchPnP;
	DriverObject->MajorFunction[IRP_MJ_CREATE]			= DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]			= DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= DispatchDeviceControl;

	DebugViewPrint("= Fn:DriverEntry:return =");
	return STATUS_SUCCESS;
}


VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DebugViewPrint("= Fn:DriverUnload:entry =");
	UNREFERENCED_PARAMETER(DriverObject);
}


NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	// Reference documents:
	// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/adddevice-routines-in-function-or-filter-drivers

	DebugViewPrint("= Fn:DriverExtension.AddDevice:entry =");

	PDEVICE_OBJECT DeviceObject = NULL;
	PWDMDRIVER_EXTENSION DeviceExtension = NULL;
	NTSTATUS Status = -1;

	// Create and initialize a device object
	Status = IoCreateDevice(DriverObject, sizeof(WDMDRIVER_EXTENSION), NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (NT_SUCCESS(Status))
	{
		DeviceExtension = (PWDMDRIVER_EXTENSION)DeviceObject->DeviceExtension;
		RtlZeroMemory(DeviceExtension, sizeof(WDMDRIVER_EXTENSION));
	}
	else
	{
		return Status;
	}

	// Registers a device interface for the driver, allowing user-mode applications to communicate with the driver using a standardized interface.
	Status = IoRegisterDeviceInterface(DeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject, &GUID_DEVINTERFACE_WDMDRIVER, NULL, &DeviceExtension->SymbolicLinkName);
	if (NT_SUCCESS(Status))
	{
		// Set "Buffered I/O" and "Power Pagable" flags for the device object:
		// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-buffered-i-o
		// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/setting-device-object-flags-for-power-management
		DeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;
	}
	else
	{
		IoDeleteDevice(DeviceObject);
		return Status;
	}

	// Attach the device object to the device stack
	DeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (DeviceExtension->LowerDeviceObject == NULL)
	{
		IoDeleteDevice(DeviceObject);
		return STATUS_NO_SUCH_DEVICE;
	}

	// Clear the initializing flag: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/clearing-the-do-device-initializing-flag
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	
	DebugViewPrint("= Fn:DriverExtension.AddDevice:return =");
	return STATUS_SUCCESS;
}


NTSTATUS DispatchPnPStartDeviceIoCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	DebugViewPrint("= Cb:DispatchPnPStartDeviceIoCompletionRoutine:entry =");
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	// Signal the event to indicate that the IRP is complete.
	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);

	DebugViewPrint("= Cb:DispatchPnPStartDeviceIoCompletionRoutine:return =");
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS DispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugViewPrint("= Fn:DispatchPNP:entry =");

	NTSTATUS Status = -1;
	KEVENT Event = { 0 };

	PWDMDRIVER_EXTENSION DeviceExtension = (PWDMDRIVER_EXTENSION)DeviceObject->DeviceExtension;

	// IRP structure includes IO_STACK_LOCATION structure that contains the major and minor function codes, as well as the parameters for the function.
	// * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_stack_location
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG MinorFunction = IrpStack->MinorFunction;

	switch (MinorFunction)
	{
		case IRP_MN_START_DEVICE:
		// This IRP must be handled first by the parent bus driver for a device and then by each higher driver in the device stack.
		// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-start-device
		// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/postponing-pnp-irp-processing-until-lower-drivers-finish

			DebugViewPrint(">> IRP_MN_START_DEVICE");

			// Initialize the event for the completion routine
			KeInitializeEvent(&Event, NotificationEvent, FALSE);

			// Copy the current IRP stack location to the next-lower driver.
			IoCopyCurrentIrpStackLocationToNext(Irp);

			// Set the completion routine for the IRP and call the lower driver.
			IoSetCompletionRoutine(Irp, DispatchPnPStartDeviceIoCompletionRoutine, &Event, TRUE, TRUE, TRUE);
			Status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);

			// Wait for the completion routine to signal the event if the lower driver has not yet completed the IRP.
			if (Status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
				Status = Irp->IoStatus.Status;
			}

			// If the lower driver returns STATUS_SUCCESS, set the device interface state to TRUE.
			if (NT_SUCCESS(Status))
			{
				Irp->IoStatus.Status = Status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				IoSetDeviceInterfaceState(&DeviceExtension->SymbolicLinkName, TRUE);
			}
			// If the lower driver returns an error, complete the IRP and delete the device.
			else
			{
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				IoDeleteDevice(DeviceObject);
			}
			break;

		case IRP_MN_REMOVE_DEVICE:
		// This IRP is sent to the drivers for a device when the device is being removed from the system.
		// * https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-remove-device

			DebugViewPrint(">> IRP_MN_REMOVE_DEVICE");

			// Set the device interface state to FALSE and free the symbolic link name.
			IoSetDeviceInterfaceState(&DeviceExtension->SymbolicLinkName, FALSE);
			RtlFreeUnicodeString(&DeviceExtension->SymbolicLinkName);

			// Detach the device object from the device stack and delete the device.
			IoDetachDevice(DeviceExtension->LowerDeviceObject);
			IoDeleteDevice(DeviceObject);

			// Complete the IRP and return STATUS_SUCCESS.
			Irp->IoStatus.Status = STATUS_SUCCESS;

			// The driver must call IoCompleteRequest to complete the IRP.
			IoSkipCurrentIrpStackLocation(Irp);
			Status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
			break;

		default:
		// This case handles all other PnP IRPs that the driver does not explicitly handle.

			DebugViewPrint(">> Others");

			// The driver must call IoCompleteRequest to complete the IRP.
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			Status = STATUS_SUCCESS;
			break;
	}

	DebugViewPrint("= Fn:DispatchPNP:return =");
	return Status;
}


NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugViewPrint("= Fn:DispatchCreateClose:entry =");
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS Status = STATUS_SUCCESS;

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);

	switch (IrpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:

		DebugViewPrint(">> IRP_MJ_CREATE");

		// Set the information field of the IRP to zero to indicate that the IRP has been completed.
		Irp->IoStatus.Information = 0;
		break;

	case IRP_MJ_CLOSE:

		DebugViewPrint(">> IRP_MJ_CLOSE");

		// Set the information field of the IRP to zero to indicate that the IRP has been completed.
		Irp->IoStatus.Information = 0;
		break;

	default:
		DebugViewPrint(">> Others");
		break;
	}

	DebugViewPrint("= Fn:DispatchCreateClose:return =");
	return Status;
}


NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DebugViewPrint("= Fn:DispatchDeviceControl:entry =");
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS Status = STATUS_SUCCESS;

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG ControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;

	switch (ControlCode)
	{
	default:
		DebugViewPrint(">> Others");
		Status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DebugViewPrint("= Fn:DispatchDeviceControl:return =");
	return STATUS_SUCCESS;
}
