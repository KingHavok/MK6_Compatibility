/*
 * TVicHW32.dll shim
 *
 * Replaces the real TVicHW32 hardware I/O driver with stubs,
 * patches version APIs to report Windows 2000, and provides
 * keyboard remapping (replaces KEYBOARDCONNECTOR.exe).
 *
 * Zero CRT dependency — only imports KERNEL32.dll and USER32.dll
 * so it runs on Windows 7 through 11 with no redistributables.
 */
#include <windows.h>
static HINSTANCE g_hInst;
/* ================================================================
 * Tiny CRT replacements (no libc dependency)
 * The compiler may emit calls to memset/memcpy for struct zeroing
 * and copies, so we must provide them.
 * ================================================================ */
void * __cdecl memset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}
void * __cdecl memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
/* Case-insensitive compare of two characters */
static __inline char toLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : c;
}
/* Case-insensitive substring search (replaces _strnicmp + strstr) */
static BOOL ContainsI(const char *hay, const char *needle)
{
    int i, j;
    if (!hay || !needle) return FALSE;
    for (i = 0; hay[i]; i++) {
        for (j = 0; needle[j]; j++) {
            if (toLower(hay[i + j]) != toLower(needle[j]))
                break;
        }
        if (!needle[j]) return TRUE;   /* full needle matched */
    }
    return FALSE;
}
/* Starts-with check (replaces strncmp) */
static BOOL StartsWithA(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return FALSE;
    }
    return TRUE;
}
/* ================================================================
 * Version spoofing - report Windows 2000 SP4 (5.0.2195)
 * ================================================================ */
static DWORD WINAPI Fake_GetVersion(void)
{
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
 * IAT patching
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
        if (lstrcmpiA(dllName, "KERNEL32.dll") == 0) {
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
 * TVicHW32 stub exports (unchanged)
 * ================================================================ */
DWORD __stdcall Shim_OpenTVicHW32(HWND hWnd, const char *szService, const char *szDisplay)
{
    (void)hWnd; (void)szService; (void)szDisplay;
    return 0x12345678;
}
void __stdcall Shim_CloseTVicHW32(void) {}
DWORD __stdcall Shim_GetActiveHW(void) { return 0x12345678; }
BYTE __stdcall Shim_GetPortByte(WORD wPort) { (void)wPort; return 0; }
void __stdcall Shim_SetPortByte(WORD wPort, BYTE bValue) { (void)wPort; (void)bValue; }
WORD __stdcall Shim_GetPortWord(WORD wPort) { (void)wPort; return 0; }
void __stdcall Shim_SetPortWord(WORD wPort, WORD wValue) { (void)wPort; (void)wValue; }
DWORD __stdcall Shim_GetPortLong(WORD wPort) { (void)wPort; return 0; }
void __stdcall Shim_SetPortLong(WORD wPort, DWORD dwValue) { (void)wPort; (void)dwValue; }
LPVOID __stdcall Shim_MapPhysToLinear(DWORD dwPhysAddr, DWORD dwSize)
{
    (void)dwPhysAddr;
    if (dwSize == 0) dwSize = 4096;
    return VirtualAlloc(NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
/* ================================================================
 * Keyboard remapping (replaces KEYBOARDCONNECTOR.exe)
 *
 * Active when "MK6 Emulator" or any "Roms Screen" is foreground.
 *
 * Key map:
 *   CapsLock → Button17 (Audit)
 *   f        → Button18 (Jackpot)
 *   a        → Button19 (Logic)
 *   s        → Button20 (Main-Opt)
 *   d        → Button21 (Main-Mec)
 *   Enter    → Button22 (Coins)
 *   g        → Payout combo (TakeWin/Collect + Jackpot x2)
 *   Escape   → Close app
 *   Shift    → Fullscreen toggle (Alt+Enter)
 *
 * When a "Roms Screen" window is foreground, native keys (1-8,
 * q-w-e-r-t-y-u-i, Space, F4) are forwarded to MK6 Emulator.
 * ================================================================ */
#define EMU_TITLE   "MK6 Emulator"
static HHOOK  g_hKeyHook;
static HANDLE g_hHookThread;
static DWORD  g_dwHookThreadId;
/* Button HWND cache (1-based index, Button1..Button36) */
static HWND g_hBtnCache[37];
static HWND g_hEmuCached;
static BOOL g_bBtnCacheValid;
static void RefreshButtonCache(HWND hEmu)
{
    HWND hChild;
    int n = 0;
    if (g_bBtnCacheValid && g_hEmuCached == hEmu && IsWindow(hEmu))
        return;
    ZeroMemory(g_hBtnCache, sizeof(g_hBtnCache));
    g_hEmuCached = hEmu;
    g_bBtnCacheValid = FALSE;
    if (!hEmu) return;
    hChild = FindWindowExA(hEmu, NULL, "Button", NULL);
    while (hChild && n < 36) {
        g_hBtnCache[++n] = hChild;
        hChild = FindWindowExA(hEmu, hChild, "Button", NULL);
    }
    g_bBtnCacheValid = (n > 0);
}
static void ClickEmuButton(HWND hEmu, int num)
{
    RefreshButtonCache(hEmu);
    if (num >= 1 && num <= 36 && g_hBtnCache[num])
        PostMessageA(g_hBtnCache[num], BM_CLICK, 0, 0);
}
static void SendKeyToEmu(HWND hEmu, UINT vk, char ch)
{
    PostMessageA(hEmu, WM_KEYDOWN, vk, 0x00000001);
    if (ch)
        PostMessageA(hEmu, WM_CHAR, (WPARAM)ch, 0x00000001);
    PostMessageA(hEmu, WM_KEYUP, vk, 0xC0000001);
}
/* --- Find a button whose text contains a keyword (case-insensitive) --- */
static HWND FindButtonByKeyword(HWND hEmu, const char *keyword)
{
    HWND hChild = FindWindowExA(hEmu, NULL, "Button", NULL);
    while (hChild) {
        char txt[128];
        if (GetWindowTextA(hChild, txt, sizeof(txt)) > 0) {
            if (ContainsI(txt, keyword))
                return hChild;
        }
        hChild = FindWindowExA(hEmu, hChild, "Button", NULL);
    }
    return NULL;
}
/* --- Payout combo: TakeWin/Collect → Jackpot × 2 --- */
static volatile LONG g_payoutActive;
static DWORD WINAPI PayoutThread(LPVOID lp)
{
    HWND hEmu = (HWND)lp;
    HWND hTakeWin;
    /* Find the Take Win / Collect button by label text */
    hTakeWin = FindButtonByKeyword(hEmu, "Collect");
    if (!hTakeWin) hTakeWin = FindButtonByKeyword(hEmu, "Take Win");
    if (hTakeWin)
        PostMessageA(hTakeWin, BM_CLICK, 0, 0);
    else
        SendKeyToEmu(hEmu, 'Q', 'q');   /* fallback: q key */
    Sleep(150);
    ClickEmuButton(hEmu, 18);
    Sleep(150);
    ClickEmuButton(hEmu, 18);
    InterlockedExchange(&g_payoutActive, 0);
    return 0;
}
/* --- Fullscreen toggle: Alt+Enter + move cursor --- */
static void ToggleFullscreen(void)
{
    INPUT inp[4];
    ZeroMemory(inp, sizeof(inp));
    inp[0].type = INPUT_KEYBOARD;  inp[0].ki.wVk = VK_MENU;
    inp[1].type = INPUT_KEYBOARD;  inp[1].ki.wVk = VK_RETURN;
    inp[2].type = INPUT_KEYBOARD;  inp[2].ki.wVk = VK_RETURN;
    inp[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inp[3].type = INPUT_KEYBOARD;  inp[3].ki.wVk = VK_MENU;
    inp[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inp, sizeof(INPUT));
    SetCursorPos(0, 479);
}
/* --- Native key check --- */
static BOOL IsNativeKey(DWORD vk)
{
    if (vk >= '1' && vk <= '8') return TRUE;
    switch (vk) {
        case 'Q': case 'W': case 'E': case 'R':
        case 'T': case 'Y': case 'U': case 'I':
        case VK_SPACE: case VK_F4:
            return TRUE;
    }
    return FALSE;
}
/* --- Non-native key → button number (0 = not mapped) --- */
static int ButtonForKey(DWORD vk)
{
    switch (vk) {
        case VK_CAPITAL: return 17;  /* Audit    */
        case 'F':        return 18;  /* Jackpot  */
        case 'A':        return 19;  /* Logic    */
        case 'S':        return 20;  /* Main-Opt */
        case 'D':        return 21;  /* Main-Mec */
        case VK_RETURN:  return 22;  /* Coins    */
    }
    return 0;
}
/* --- Low-level keyboard hook --- */
static LRESULT CALLBACK KbHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *pk;
    HWND hFg, hEmu;
    BOOL emuFg, romFg;
    char fgTitle[64];
    int  btn;
    if (nCode != HC_ACTION)
        goto pass;
    pk = (KBDLLHOOKSTRUCT *)lParam;
    /* Ignore our own synthetic keystrokes */
    if (pk->flags & LLKHF_INJECTED)
        goto pass;
    hFg  = GetForegroundWindow();
    hEmu = FindWindowA(NULL, EMU_TITLE);
    if (!hFg || !hEmu)
        goto pass;
    emuFg = (hFg == hEmu);
    /* Check foreground title for "Roms" + "Screen" (handles variable spacing) */
    romFg = FALSE;
    if (!emuFg) {
        GetWindowTextA(hFg, fgTitle, sizeof(fgTitle));
        romFg = (StartsWithA(fgTitle, "Roms") && ContainsI(fgTitle, "Screen"));
    }
    if (!emuFg && !romFg)
        goto pass;   /* Neither window active — don't hijack */
    /* === KEY DOWN === */
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        /* g → payout combo */
        if (pk->vkCode == 'G' && hEmu) {
            if (InterlockedCompareExchange(&g_payoutActive, 1, 0) == 0) {
                HANDLE h = CreateThread(NULL, 0, PayoutThread, (LPVOID)hEmu, 0, NULL);
                if (h) CloseHandle(h);
            }
            return 1;
        }
        /* Escape → close emulator */
        if (pk->vkCode == VK_ESCAPE && hEmu) {
            PostMessageA(hEmu, WM_SYSCOMMAND, SC_CLOSE, 0);
            return 1;
        }
        /* Shift → fullscreen toggle */
        if (pk->vkCode == VK_LSHIFT || pk->vkCode == VK_RSHIFT ||
            pk->vkCode == VK_SHIFT) {
            ToggleFullscreen();
            return 1;
        }
        /* Non-native key → button click */
        btn = ButtonForKey(pk->vkCode);
        if (btn && hEmu) {
            ClickEmuButton(hEmu, btn);
            return 1;
        }
        /* Forward native keys from Roms Screen → Emulator */
        if (romFg && hEmu && IsNativeKey(pk->vkCode)) {
            char ch = 0;
            DWORD vk = pk->vkCode;
            if (vk >= 'A' && vk <= 'Z')
                ch = (char)(vk | 0x20);   /* lowercase */
            else if (vk >= '1' && vk <= '8')
                ch = (char)vk;
            else if (vk == VK_SPACE)
                ch = ' ';
            SendKeyToEmu(hEmu, vk, ch);
            return 1;
        }
    }
    /* === KEY UP — eat for keys we consumed on down === */
    if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        DWORD vk = pk->vkCode;
        if (vk == 'G' || vk == VK_ESCAPE ||
            vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT)
            return 1;
        if (ButtonForKey(vk))
            return 1;
        if (romFg && IsNativeKey(vk))
            return 1;
    }
pass:
    return CallNextHookEx(g_hKeyHook, nCode, wParam, lParam);
}
/* --- Hook thread (needs its own message pump for LL hooks) --- */
static DWORD WINAPI KbHookThread(LPVOID lp)
{
    MSG msg;
    (void)lp;
    g_hKeyHook = SetWindowsHookExA(WH_KEYBOARD_LL, KbHookProc,
                                    g_hInst, 0);
    if (!g_hKeyHook)
        return 1;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    UnhookWindowsHookEx(g_hKeyHook);
    g_hKeyHook = NULL;
    return 0;
}
/* ================================================================
 * DLL entry point
 * ================================================================ */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        PatchModuleIAT(GetModuleHandleA(NULL));
        g_hHookThread = CreateThread(NULL, 0, KbHookThread, NULL, 0,
                                     &g_dwHookThreadId);
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_dwHookThreadId)
            PostThreadMessageA(g_dwHookThreadId, WM_QUIT, 0, 0);
        if (g_hHookThread) {
            WaitForSingleObject(g_hHookThread, 2000);
            CloseHandle(g_hHookThread);
        }
    }
    return TRUE;
}
