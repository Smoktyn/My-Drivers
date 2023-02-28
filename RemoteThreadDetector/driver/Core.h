#pragma once
#include <ntifs.h>
#include <ntddk.h>
#pragma warning(push)
#pragma warning(disable:4201)
#pragma warning(disable:4311)
#pragma warning(disable:4302)

/*
* Logging Utilities
*/

#define _LOG_STR(VAL) #VAL
#define LOG_STR(VAL) _LOG_STR(VAL)

#define LOG_INFO(FMT, ...) DbgPrintEx(0, 0, "[+] " FMT, __VA_ARGS__)
#define LOG_ERROR(FMT, ...) DbgPrintEx(0, 0, "[-] " FMT, __VA_ARGS__)

#define LOG_ENTER_FUNCTION(CLASS, FUNCTION) DbgPrintEx(0, 0, "[>] " LOG_STR(CLASS) "::" LOG_STR(FUNCTION) "\n")
#define LOG_LEAVE_FUNCTION(CLASS, FUNCTION) DbgPrintEx(0, 0, "[<] " LOG_STR(CLASS) "::" LOG_STR(FUNCTION) "\n")

/*
* IOCTL
*/

#define _DEVICE_TYPE 0x8000
#define FUNCTION_BASE 0x800
#define CTL_HIDE(i) CTL_CODE(_DEVICE_TYPE, FUNCTION_BASE + i, METHOD_NEITHER, FILE_ANY_ACCESS)
#define READ_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x800, METHOD_NEITHER, FILE_ANY_ACCESS)
#define WRITE_THREAD_ADDRESS CTL_CODE(_DEVICE_TYPE, 0x811, METHOD_NEITHER, FILE_ANY_ACCESS)

/*
* Undocumented structures
*/

//0x5f0 bytes (sizeof)
struct _KTHREAD
{
    struct _DISPATCHER_HEADER Header;                                       //0x0
};

//0x810 bytes (sizeof)
struct _ETHREAD
{
    struct _KTHREAD Tcb;                                                    //0x0
    union _LARGE_INTEGER CreateTime;                                        //0x5f0
    union
    {
        union _LARGE_INTEGER ExitTime;                                      //0x5f8
        struct _LIST_ENTRY KeyedWaitChain;                                  //0x5f8
    };
    union
    {
        struct _LIST_ENTRY PostBlockList;                                   //0x608
        struct
        {
            VOID* ForwardLinkShadow;                                        //0x608
            VOID* StartAddress;                                             //0x610
        };
    };
    union
    {
        struct _TERMINATION_PORT* TerminationPort;                          //0x618
        struct _ETHREAD* ReaperLink;                                        //0x618
        VOID* KeyedWaitValue;                                               //0x618
    };
    ULONGLONG ActiveTimerListLock;                                          //0x620
    struct _LIST_ENTRY ActiveTimerListHead;                                 //0x628
    struct _CLIENT_ID Cid;                                                  //0x638
};