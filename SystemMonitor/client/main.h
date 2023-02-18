#pragma once
#include <iostream>
#include <Windows.h>
#include "../SysMon/Common.h"

struct cGlobals {
	HANDLE hConsole;
	bool bShowThreads;
	bool bShowProcess;
	bool bShowHelp;
};