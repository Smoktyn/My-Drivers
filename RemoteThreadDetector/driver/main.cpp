#include "Core.h"



void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DispathCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispathCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispathCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\RmtThrdDtc");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\rmtthrd");
	PDEVICE_OBJECT DeviceObject = nullptr;
	NTSTATUS status = STATUS_SUCCESS;
	bool bDeviceCreated = false, bSymbolicLinkCreated = false, bSetCreateProcessNotify = false, bSetCreateThreadNotify = false;
	do {
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Device creation fail 0x%08X", status));
			break;
		}
		bDeviceCreated = true;
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Symbolic link creation fail 0x%08X", status));
			break;
		}
		bSymbolicLinkCreated = true;
		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status)) {
			KdPrint(("Symbolic link creation fail 0x%08X", status));
			break;
		}
		bSetCreateThreadNotify = true;
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (bDeviceCreated)
			IoDeleteDevice(DeviceObject);
		if (bSymbolicLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if (bSetCreateThreadNotify)
			PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	}

	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	IoDeleteDevice(DriverObject->DeviceObject);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\rmtthrd");
	IoDeleteSymbolicLink(&symLink);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);

}

NTSTATUS DispathCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
	UNREFERENCED_PARAMETER(ProcessId);
	if (Create && ExGetPreviousMode() != KernelMode) {
		PETHREAD pEthread;
		auto status = STATUS_SUCCESS;
		status = PsLookupThreadByThreadId(ThreadId, &pEthread);
		if (NT_SUCCESS(status)) {
			__try {
				LOG_INFO("ProcessId -> %d\n", ProcessId);
				LOG_INFO("ThreadId -> %d\n", ThreadId);
				LOG_INFO("Unique ProcessId -> %d \n", ((_CLIENT_ID*)((char*)pEthread + 0x638))->UniqueProcess);
				LOG_INFO("Unique ThreadId -> %d \n", ((_CLIENT_ID*)((char*)pEthread + 0x638))->UniqueThread);
				LOG_INFO("###############\n");
				if (ProcessId != PsGetCurrentProcessId()) {
					LOG_INFO("***REMOTE THREAD DETECTED***\n");
				}
			}
			__finally {
				ObDereferenceObject(pEthread);
			}
		}
		else {
			ObDereferenceObject(pEthread);
		}
	}
	return;
}

ULONG64* pThread = 0;
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
	case READ_THREAD_ADDRESS:
		__try {
			*((ULONG64**)(stack->Parameters.DeviceIoControl.Type3InputBuffer)) = pThread;
		}
		__except (1) {
			LOG_ERROR("Somethings went wrong\n");
		}
		break;
	case WRITE_THREAD_ADDRESS:
		LOG_INFO("Thread address -> %p\n", (ULONG64*)stack->Parameters.DeviceIoControl.Type3InputBuffer);
		__try {
			pThread = (ULONG64*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
		}
		__except (1) {
			LOG_ERROR("Somethings went wrong\n");
		}
		break;
	}
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, 0);
	return Irp->IoStatus.Status;
}