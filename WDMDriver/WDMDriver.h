#pragma once
#include <wdm.h>

typedef struct {
	PDEVICE_OBJECT LowerDeviceObject;
	PDEVICE_OBJECT PhysicalDeviceObject;
	UNICODE_STRING SymbolicLinkName;
} WDMDRIVER_EXTENSION, * PWDMDRIVER_EXTENSION;


VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS DispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);


VOID DebugViewPrint(const char* str)
{
#ifdef DBG
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[WDMDRV] %s\n", str);
#else
	UNREFERENCED_PARAMETER(str);
#endif
}
