#include <iostream>
#include <windows.h>
#define _DEVICE_TYPE 0x8000
#define READ_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)
#define WRITE_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x811, METHOD_NEITHER, FILE_ANY_ACCESS)

int main() {
	HANDLE hDevice = CreateFile(L"\\\\.\\rmtthrd", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		std::cout << "File creation error\n";
		return 1;
	}
	uintptr_t* pThreadAddress{ nullptr };
	DWORD returned{ 0 };
	BOOL status = DeviceIoControl(hDevice, READ_THREAD_ADDRESS, &pThreadAddress, sizeof(uintptr_t*), nullptr, 0, &returned, nullptr);
	if (status && pThreadAddress != nullptr) {
		DWORD ProcId{ 0 };
		std::cout << "Enetr ProcessId \n";
		std::cin >> ProcId;
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, ProcId);
		if (hProcess != INVALID_HANDLE_VALUE) {
			HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pThreadAddress, nullptr, NULL, nullptr);
			if (hThread == INVALID_HANDLE_VALUE) {
				std::cout << "Create thread error\n";
			}
			while (true) {
				::Sleep(1000);
			}
			CloseHandle(hProcess);
			CloseHandle(hThread);
		}

	}
	else {
		std::cout << "Read address error\n";
	}
	CloseHandle(hDevice);
	return 0;
}