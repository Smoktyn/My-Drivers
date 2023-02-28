#include <Windows.h>
#include <iostream>
#define _DEVICE_TYPE 0x8000
#define READ_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)
#define WRITE_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x811, METHOD_NEITHER, FILE_ANY_ACCESS)


void ThreadRoutine() {
	while (true) {
		std::cout << "Thread was called\n";
		::Sleep(1000);
	}
	return;
}

int main() {
	HANDLE hDevice = CreateFile(L"\\\\.\\rmtthrd", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		std::cout << "File creation error\n";
		return 1;
	}
	void (*pFunction)() { ThreadRoutine };
	std::cout << "Thread addres -> 0x" << ThreadRoutine << " pFunction -> " << pFunction << std::endl;
	DWORD returned{ 0 };
	BOOL status{ DeviceIoControl(hDevice, WRITE_THREAD_ADDRESS, pFunction, 8, nullptr, 0, &returned, nullptr) };
	if (!status) {
		std::cout << "Write file error\n";
		CloseHandle(hDevice);
	}
	while (true) {
		::Sleep(500);
	}
	CloseHandle(hDevice);
	return 0;
}