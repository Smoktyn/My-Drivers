#pragma once
#define _MAX_PATHS 20
#define _DRIVER_TAG 'jtmc'
#define DEVICE 0x8000
#define CTL_ADD_PATH CTL_CODE(DEVICE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)

struct Global_t {
	UNICODE_STRING* pForbiddenPaths = 0;
	int iPathsCount = 0;
};
