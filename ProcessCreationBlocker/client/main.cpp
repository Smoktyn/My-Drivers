#include <iostream>
#include <windows.h>
#define DEVICE 0x8000
#define CTL_ADD_PATH CTL_CODE(DEVICE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)

int main() {
	HANDLE hDevice = CreateFile(L"\\\\.\\rmtthrd", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) {
		std::cout << "Create file error\n";
		return 1;
	}
	wchar_t* str = (wchar_t*)malloc(0x100);
	std::cout << "sizeof(wchar_t) = " << sizeof(wchar_t) << std::endl;
	for (int i = 0; i < 3; ++i) {
		::memset(str, 0x0, 0x100);
		_getws_s(str, 0x80);
		DWORD bytes{ 0 };
		DeviceIoControl(hDevice, CTL_ADD_PATH, str, 0x8, nullptr, 0, &bytes, nullptr);
	}
	::Sleep(2000);
	CloseHandle(hDevice);
	free(str);
	//delete[] str;
	return 0;
}