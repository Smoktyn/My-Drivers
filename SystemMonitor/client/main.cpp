#pragma once
#include "main.h"
#include <iostream>
#include <Windows.h>
#include "../driver/Common.h"

cGlobals Globals;
void DisplayTime(const LARGE_INTEGER& time) {
	SYSTEMTIME st;
	::FileTimeToSystemTime((FILETIME*)&time, &st);
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void DisplayInfo(BYTE* buffer, DWORD size) {
	auto count = size;
	while (count > 0) {
		auto header = (ItemHeader*)buffer;
		switch (header->Type) {
		case ItemType::ProcessExit:
		{
			SetConsoleTextAttribute(Globals.hConsole, 12);
			DisplayTime(header->Time);
			auto info = (ProcessExitInfo*)buffer;
			printf("[!] Process");
			SetConsoleTextAttribute(Globals.hConsole, 14);
			printf(" %d ", info->ProcessId);
			SetConsoleTextAttribute(Globals.hConsole, 12);
			printf("Exited\n\n");
			SetConsoleTextAttribute(Globals.hConsole, 7);
			break;
		}

		case ItemType::ProcessCreate:
		{
			SetConsoleTextAttribute(Globals.hConsole, 11);
			DisplayTime(header->Time);
			auto info = (ProcessCreateInfo*)buffer;
			std::wstring commandline((WCHAR*)(buffer + info->CommandLineOffset), info->CommandLineLength);
			std::wstring imageName((WCHAR*)(buffer + info->ImageNameOffset), info->ImageNameLenght);
			printf("[+] Process");
			SetConsoleTextAttribute(Globals.hConsole, 14);
			printf(" %d ", info->ProcessId);
			SetConsoleTextAttribute(Globals.hConsole, 11);
			printf("Created.\n");
			printf("[+] Command line -> %ws\n", commandline.c_str());
			printf("[+] Image name -> %ws\n\n", imageName.c_str());
			SetConsoleTextAttribute(Globals.hConsole, 7);
			break;
		}

		case ItemType::ThreadCreate:
		{
			if (Globals.bShowThreads) {
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
				SetConsoleTextAttribute(Globals.hConsole, 9);
				printf("Thread");
				SetConsoleTextAttribute(Globals.hConsole, 14);
				printf(" %d ", info->ThreadId);
				SetConsoleTextAttribute(Globals.hConsole, 9);
				printf("Created in process");
				SetConsoleTextAttribute(Globals.hConsole, 14);
				printf(" %d\n\n", info->ProcessId);
				SetConsoleTextAttribute(Globals.hConsole, 7);
			}
			break;
		}

		case ItemType::ThreadExit:
		{
			if (Globals.bShowThreads) {
				DisplayTime(header->Time);
				auto info = (ThreadCreateExitInfo*)buffer;
				SetConsoleTextAttribute(Globals.hConsole, 13);
				printf("Thread");
				SetConsoleTextAttribute(Globals.hConsole, 14);
				printf(" %d ", info->ThreadId);
				SetConsoleTextAttribute(Globals.hConsole, 13);
				printf("Exited from process");
				SetConsoleTextAttribute(Globals.hConsole, 14);
				printf(" %d\n\n", info->ProcessId);
				SetConsoleTextAttribute(Globals.hConsole, 7);
			}
			break;
		}
		default:
			break;
		}
		buffer += header->Size;
		count -= header->Size;
	}

}

void ParseArgv(int argc, char* argv[]) {
	for (int i = 1; i < argc; ++i) {
		switch (*(argv[i] + 1)) {
		case 't':
			Globals.bShowThreads = true;
			break;
		case 'p':
			Globals.bShowProcess = true;
			break;
		case 'h':
			Globals.bShowHelp = true;
			break;
		default:
			Globals.bShowHelp = true;
			break;
		}
	}
	return;
}

void ShowHelp() {
	std::cout << "System Monitor v1.0\n-h for help\n-p show processes\n-t show threads\n";
}
int main(int argc, char* argv[]) {
	Globals.bShowHelp = false;
	if (argc > 1) ParseArgv(argc, argv);
	else {
		Globals.bShowProcess = true;
		Globals.bShowThreads = true;
	}
	if (Globals.bShowHelp) { ShowHelp(); return 1; }
	Globals.hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	auto hHandle = CreateFileW(L"\\\\.\\sysmon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hHandle == INVALID_HANDLE_VALUE) {
		std::cout << "Create File Error\n";
		return 1;
	}
	BYTE (*buffer) = new BYTE[(1 << 16)];
	if (buffer == nullptr) {
		printf("Buffer allocate error.\n");
	}
	while (true) {
		DWORD bytes;
		if (!::ReadFile(hHandle, buffer, (1 << 16), &bytes, nullptr)) {
			return 1;
		}
		if (bytes != 0) {
			DisplayInfo(buffer, bytes);
		}
		::Sleep(200);
	}
	return 0;
}
