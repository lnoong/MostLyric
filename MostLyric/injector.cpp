#include "injector.h"
#include "config_io.h"
#include <TlHelp32.h>
#include <Psapi.h>

static DWORD FindExplorerPID()
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return pid;
}

static BOOL GetDllPath(WCHAR* szDllPath, SIZE_T cchDllPath)
{
    GetModuleFileNameW(NULL, szDllPath, (DWORD)cchDllPath);
    WCHAR* pSlash = wcsrchr(szDllPath, L'\\');
    if (pSlash) {
        wcscpy_s(pSlash + 1, (size_t)(szDllPath + cchDllPath - pSlash - 1), L"MostLyricHook.dll");
    } else {
        wcscpy_s(szDllPath, cchDllPath, L"MostLyricHook.dll");
    }
    return GetFileAttributesW(szDllPath) != INVALID_FILE_ATTRIBUTES;
}

BOOL InjectDLL()
{
    WCHAR szDllPath[MAX_PATH];
    if (!GetDllPath(szDllPath, MAX_PATH)) return FALSE;

    DWORD pid = FindExplorerPID();
    if (!pid) return FALSE;

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        FALSE, pid);
    if (!hProcess) return FALSE;

    SIZE_T pathLen = (wcslen(szDllPath) + 1) * sizeof(WCHAR);
    LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathLen, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemotePath) { CloseHandle(hProcess); return FALSE; }

    if (!WriteProcessMemory(hProcess, pRemotePath, szDllPath, pathLen, NULL)) {
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    FARPROC pLoadLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, NULL);

    if (!hThread) {
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return TRUE;
}

BOOL EjectDLL()
{
    if (pipe_send_stop())
    {
        for (int i = 0; i < 30; ++i)
        {
            Sleep(100);
            if (!IsDLLInjected())
                return TRUE;
        }
    }

    WCHAR szDllPath[MAX_PATH];
    if (!GetDllPath(szDllPath, MAX_PATH)) return FALSE;

    DWORD pid = FindExplorerPID();
    if (!pid) return FALSE;

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProcess) return FALSE;

    HMODULE hMods[1024];
    DWORD cbNeeded = 0;
    HMODULE hOurDll = NULL;

    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        DWORD count = cbNeeded / sizeof(HMODULE);
        WCHAR modName[MAX_PATH];
        for (DWORD i = 0; i < count; i++) {
            if (GetModuleFileNameExW(hProcess, hMods[i], modName, MAX_PATH)) {
                if (_wcsicmp(modName, szDllPath) == 0) {
                    hOurDll = hMods[i];
                    break;
                }
            }
        }
    }

    if (!hOurDll) { CloseHandle(hProcess); return FALSE; }

    FARPROC pFreeLibrary = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pFreeLibrary, hOurDll, 0, NULL);

    if (!hThread) { CloseHandle(hProcess); return FALSE; }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return TRUE;
}

BOOL IsDLLInjected()
{
    WCHAR szDllPath[MAX_PATH];
    if (!GetDllPath(szDllPath, MAX_PATH)) return FALSE;

    DWORD pid = FindExplorerPID();
    if (!pid) return FALSE;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) return FALSE;

    HMODULE hMods[1024];
    DWORD cbNeeded = 0;
    BOOL found = FALSE;

    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        DWORD count = cbNeeded / sizeof(HMODULE);
        WCHAR modName[MAX_PATH];
        for (DWORD i = 0; i < count; i++) {
            if (GetModuleFileNameExW(hProcess, hMods[i], modName, MAX_PATH)) {
                if (_wcsicmp(modName, szDllPath) == 0) {
                    found = TRUE;
                    break;
                }
            }
        }
    }

    CloseHandle(hProcess);
    return found;
}
