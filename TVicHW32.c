/*
 * TVicHW32.dll shim
 *
 * Replaces the real TVicHW32 hardware I/O driver with stubs,
 * and patches version APIs to report Windows 2000 so that
 * DirectSound / waveOut initialization takes the legacy path.
 */

#include <windows.h>
#include <string.h>

/* ================================================================
 * Version spoofing - report Windows 2000 SP4 (5.0.2195)
 * ================================================================ */

static DWORD WINAPI Fake_GetVersion(void)
{
    /*
     * Return value format:
     *   Low byte  = major version (5)
     *   Next byte = minor version (0)
     *   High word = build number (2195 = 0x0893)
     *   Bit 31 clear = Windows NT
     */
    return (DWORD)0x08930005;
}

static BOOL WINAPI Fake_GetVersionExA(LPOSVERSIONINFOA lpvi)
{
    if (!lpvi)
        return FALSE;

    if (lpvi->dwOSVersionInfoSize >= sizeof(OSVERSIONINFOEXA)) {
        LPOSVERSIONINFOEXA lpex = (LPOSVERSIONINFOEXA)lpvi;
        ZeroMemory(lpex, sizeof(OSVERSIONINFOEXA));
        lpex->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
        lpex->dwMajorVersion    = 5;
        lpex->dwMinorVersion    = 0;
        lpex->dwBuildNumber     = 2195;
        lpex->dwPlatformId      = VER_PLATFORM_WIN32_NT;
        lstrcpyA(lpex->szCSDVersion, "Service Pack 4");
        lpex->wServicePackMajor = 4;
        lpex->wServicePackMinor = 0;
        lpex->wProductType      = VER_NT_WORKSTATION;
        return TRUE;
    }

    if (lpvi->dwOSVersionInfoSize >= sizeof(OSVERSIONINFOA)) {
        ZeroMemory(lpvi, sizeof(OSVERSIONINFOA));
        lpvi->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        lpvi->dwMajorVersion    = 5;
        lpvi->dwMinorVersion    = 0;
        lpvi->dwBuildNumber     = 2195;
        lpvi->dwPlatformId      = VER_PLATFORM_WIN32_NT;
        lstrcpyA(lpvi->szCSDVersion, "Service Pack 4");
        return TRUE;
    }

    return FALSE;
}

/* ================================================================
 * IAT patching - walk import table and redirect version APIs
 * ================================================================ */

static void PatchModuleIAT(HMODULE hMod)
{
    PIMAGE_DOS_HEADER pDos;
    PIMAGE_NT_HEADERS pNT;
    PIMAGE_IMPORT_DESCRIPTOR pImp;
    DWORD importRVA;
    HMODULE hK32;
    FARPROC pfnGetVersion, pfnGetVersionExA;

    if (!hMod)
        return;

    pDos = (PIMAGE_DOS_HEADER)hMod;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
        return;

    pNT = (PIMAGE_NT_HEADERS)((BYTE *)hMod + pDos->e_lfanew);
    if (pNT->Signature != IMAGE_NT_SIGNATURE)
        return;

    importRVA = pNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRVA == 0)
        return;

    hK32 = GetModuleHandleA("kernel32.dll");
    pfnGetVersion   = GetProcAddress(hK32, "GetVersion");
    pfnGetVersionExA = GetProcAddress(hK32, "GetVersionExA");

    pImp = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)hMod + importRVA);

    while (pImp->Name) {
        const char *dllName = (const char *)((BYTE *)hMod + pImp->Name);

        if (_stricmp(dllName, "KERNEL32.dll") == 0) {
            PIMAGE_THUNK_DATA pThunk;
            pThunk = (PIMAGE_THUNK_DATA)((BYTE *)hMod + pImp->FirstThunk);

            while (pThunk->u1.Function) {
                DWORD oldProt;

                if ((FARPROC)pThunk->u1.Function == pfnGetVersion) {
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD),
                                   PAGE_READWRITE, &oldProt);
                    pThunk->u1.Function = (DWORD_PTR)Fake_GetVersion;
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD),
                                   oldProt, &oldProt);
                }
                else if ((FARPROC)pThunk->u1.Function == pfnGetVersionExA) {
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD),
                                   PAGE_READWRITE, &oldProt);
                    pThunk->u1.Function = (DWORD_PTR)Fake_GetVersionExA;
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD),
                                   oldProt, &oldProt);
                }

                pThunk++;
            }
        }

        pImp++;
    }
}

/* ================================================================
 * TVicHW32 stub exports
 *
 * Only the 10 functions actually imported by MK6Emu.exe.
 * All I/O port functions are no-ops since hardware access
 * is not needed.
 * ================================================================ */

/* ordinal 1 - Initialize driver (return fake handle) */
DWORD __stdcall Shim_OpenTVicHW32(HWND hWnd, const char *szService, const char *szDisplay)
{
    (void)hWnd; (void)szService; (void)szDisplay;
    return 0x12345678;
}

/* ordinal 2 - Close driver */
void __stdcall Shim_CloseTVicHW32(void)
{
}

/* ordinal 3 - Return active handle */
DWORD __stdcall Shim_GetActiveHW(void)
{
    return 0x12345678;
}

/* ordinal 12 - Read byte from I/O port */
BYTE __stdcall Shim_GetPortByte(WORD wPort)
{
    (void)wPort;
    return 0;
}

/* ordinal 13 - Write byte to I/O port */
void __stdcall Shim_SetPortByte(WORD wPort, BYTE bValue)
{
    (void)wPort; (void)bValue;
}

/* ordinal 14 - Read word from I/O port */
WORD __stdcall Shim_GetPortWord(WORD wPort)
{
    (void)wPort;
    return 0;
}

/* ordinal 15 - Write word to I/O port */
void __stdcall Shim_SetPortWord(WORD wPort, WORD wValue)
{
    (void)wPort; (void)wValue;
}

/* ordinal 16 - Read dword from I/O port */
DWORD __stdcall Shim_GetPortLong(WORD wPort)
{
    (void)wPort;
    return 0;
}

/* ordinal 17 - Write dword to I/O port */
void __stdcall Shim_SetPortLong(WORD wPort, DWORD dwValue)
{
    (void)wPort; (void)dwValue;
}

/* ordinal 30 - Map physical memory to process space */
LPVOID __stdcall Shim_MapPhysToLinear(DWORD dwPhysAddr, DWORD dwSize)
{
    (void)dwPhysAddr;
    if (dwSize == 0)
        dwSize = 4096;
    return VirtualAlloc(NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

/* ================================================================
 * DLL entry point
 * ================================================================ */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL; (void)lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        /* Patch the main EXE so GetVersion/GetVersionExA report Win2000 */
        PatchModuleIAT(GetModuleHandleA(NULL));
    }
    return TRUE;
}
