#pragma once
#include <ntddk.h>
#include "Common.h"
#include "AutoLock.h"
#include "SysMon.h"

void SysMonUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObjct, PIRP Irp);
NTSTATUS SysMonRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
void PushItem(LIST_ENTRY* entry);

Globals g_Globals;
extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverObject->DriverUnload = SysMonUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;
	auto status = STATUS_SUCCESS;

	InitializeListHead(&g_Globals.ItemsHead);
	g_Globals.Mutex.Init();

	bool symLinkCreated = false;
	bool processCallback = false, threadCallback = false;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\SysMonitor");
	PDEVICE_OBJECT DeviceObject = nullptr;
	do {
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create device error\n"));
			break;
		}

		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create symLink error\n"));
			break;
		}
		symLinkCreated = true;
		DeviceObject->Flags |= DO_DIRECT_IO;
		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create process notify error  (0x%08X)\n", status));
			break;
		}
		processCallback = true;
		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Create thread notify error  (0x%08X)\n", status));
			break;
		}
		threadCallback = true;

	} while (false);

	if (!NT_SUCCESS(status)) {
		if (threadCallback)
			PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
		if (processCallback)
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
	}
	return status;
}

NTSTATUS SysMonCreateClose(PDEVICE_OBJECT DeviceObjct, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObjct);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

void SysMonUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);
	while (!IsListEmpty(&g_Globals.ItemsHead)) {
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	UNREFERENCED_PARAMETER(Process);
	if (CreateInfo)
	{
		USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
		USHORT commandLineSize = 0, imageNameSize = 0;
		if (CreateInfo->CommandLine) {
			commandLineSize = CreateInfo->CommandLine->Length;
			allocSize += commandLineSize;
		}
		if (CreateInfo->ImageFileName) {
			imageNameSize = CreateInfo->ImageFileName->Length;
			allocSize += imageNameSize;
		}
		auto info = (FullItem<ProcessCreateInfo>*)ExAllocatePoolWithTag(PagedPool, allocSize, DRIVER_TAG);
		if (info == nullptr) {
			KdPrint(("failed allocation\n"));
			return;
		}
		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessCreate;
		item.Size = sizeof(ProcessCreateInfo) + commandLineSize + imageNameSize;
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		if (commandLineSize > 0) {
			::memcpy((UCHAR*)&item + sizeof(item), CreateInfo->CommandLine->Buffer, commandLineSize);
			item.CommandLineLength = commandLineSize / sizeof(WCHAR);
			item.CommandLineOffset = sizeof(item);
		}
		else {
			item.CommandLineLength = 0;
		}
		if (imageNameSize > 0) {
			::memcpy((UCHAR*)&item + sizeof(item) + commandLineSize, CreateInfo->ImageFileName->Buffer, imageNameSize);
			item.ImageNameLenght = imageNameSize / sizeof(WCHAR);
			item.ImageNameOffset = sizeof(item) + commandLineSize;
		}
		else {
			item.ImageNameLenght = 0;
		}
		PushItem(&info->Entry);
	}
	else {
		auto info = (FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool, sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (info == nullptr) {
			KdPrint(("failed allocation\n"));
			return;
		}
		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.Time);
		item.Type = ItemType::ProcessExit;
		item.ProcessId = HandleToULong(ProcessId);
		item.Size = sizeof(ProcessExitInfo);
		PushItem(&info->Entry);
	}
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
	auto size = sizeof(FullItem<ThreadCreateExitInfo>);
	auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
	if (info == nullptr) {
		KdPrint(("Allocation failed\n"));
	}
	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.Time);
	item.Size = sizeof(item);
	item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;
	item.ProcessId = HandleToULong(ProcessId);
	item.ThreadId = HandleToULong(ThreadId);
	PushItem(&info->Entry);
}

NTSTATUS SysMonRead(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto lenght = stack->Parameters.Read.Length;
	auto status = STATUS_SUCCESS;
	auto count = 0;
	NT_ASSERT(Irp->MdlAddress);

	auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (!buffer) {
		status = STATUS_INSUFFICIENT_RESOURCES;
	}
	else {
		AutoLock<FastMutex> lock(g_Globals.Mutex);
		while (true) {
			if (IsListEmpty(&g_Globals.ItemsHead))
				break;
			auto entry = RemoveHeadList(&g_Globals.ItemsHead);
			auto info = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
			auto size = info->Data.Size;
			if (lenght < size) {
				InsertHeadList(&g_Globals.ItemsHead, entry);
				break;
			}
			g_Globals.ItemCount--;
			::memcpy(buffer, &info->Data, size);
			lenght -= size;
			buffer += size;
			count += size;
			ExFreePool(info);
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = count;
	IoCompleteRequest(Irp, 0);
	return status;
}

void PushItem(LIST_ENTRY* entry) {
	AutoLock<FastMutex> lock(g_Globals.Mutex);
	if (g_Globals.ItemCount > 1024) {
		auto head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;
		auto item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
		ExFreePool(item);
	}
	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}