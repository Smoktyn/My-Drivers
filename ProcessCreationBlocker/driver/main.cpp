#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include "Common.h"



void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DispathCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispathIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);

Global_t global;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispathCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispathCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispathIoControl;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\RmtThrdDtc");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\rmtthrd");
	PDEVICE_OBJECT DeviceObject = nullptr;
	NTSTATUS status = STATUS_SUCCESS;
	bool bDeviceCreated = false, bSymbolicLinkCreated = false, bSetProcessNotify = false;
	UNREFERENCED_PARAMETER(bSetProcessNotify);
	do {
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create device failed 0x%08X\n", status));
			break;
		}
		bDeviceCreated = true;

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create symbolic link failed 0x%08X\n", status));
			break;
		}
		bSymbolicLinkCreated = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Set process notify failed 0x%08X\n", status));
			break;
		}
		bSetProcessNotify = true;

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (bDeviceCreated)
			IoDeleteDevice(DeviceObject);
		if (bSymbolicLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (bSetProcessNotify)
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	}

	global.pForbiddenPaths = (UNICODE_STRING*)ExAllocatePoolWithTag(NonPagedPool, (sizeof(UNICODE_STRING) * 20), 'jtmc');
	if (global.pForbiddenPaths != 0)
		::RtlZeroMemory(global.pForbiddenPaths, (sizeof(UNICODE_STRING) * 20));
	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	IoDeleteDevice(DriverObject->DeviceObject);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\rmtthrd");
	IoDeleteSymbolicLink(&symLink);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	ExFreePoolWithTag(global.pForbiddenPaths, 'jtmc');
	for (int i = 0; i < global.iPathsCount; ++i) {
		RtlFreeUnicodeString(global.pForbiddenPaths + i * sizeof(UNICODE_STRING));
	}
	
}

NTSTATUS DispathCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}

NTSTATUS DispathIoControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case CTL_ADD_PATH:
	{
		if (stack->Parameters.DeviceIoControl.InputBufferLength < 8) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		auto buffer = (WCHAR*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		if (buffer == nullptr) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		for (int i = 0; i < 20; ++i) {
			if ((global.pForbiddenPaths + i * sizeof(UNICODE_STRING))->Buffer == 0) {
				RtlCreateUnicodeString((global.pForbiddenPaths + i * sizeof(UNICODE_STRING)), buffer);
				global.iPathsCount++;
				break;
			}
		}

		break;
	}
	default:
		break;
	}
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, 0);
	return status;
}

void OnProcessNotify(PEPROCESS, HANDLE, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	if (global.iPathsCount && (CreateInfo) && (CreateInfo->ImageFileName != NULL)) {
		for (int i = 0; i < global.iPathsCount; ++i) {
			if (::wcsstr(CreateInfo->ImageFileName->Buffer, (global.pForbiddenPaths + i * sizeof(UNICODE_STRING))->Buffer)) {
				CreateInfo->CreationStatus = STATUS_UNABLE_TO_DECOMMIT_VM;
			}
		}
	}
}