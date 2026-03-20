#include "pch.h"
#include <windows.h>

// -- Compile DLL: g++ -shared -o output/ArchipelagoGUI.dll dllmain.cpp -luser32 -lgdi32 -lcomctl32

// --- LUA FUNCTION POINTERS ---
typedef void* (__cdecl* t_luaL_newstate)(void);
typedef void(__cdecl* t_lua_createtable)(void* L, int narr, int nrec);
typedef void(__cdecl* t_lua_pushstring)(void* L, const char* s);
typedef void(__cdecl* t_lua_setfield)(void* L, int idx, const char* k);
typedef void(__cdecl* t_luaL_setfuncs)(void* L, const void* l, int nup);
t_lua_createtable   p_lua_createtable = nullptr;
t_lua_pushstring    p_lua_pushstring = nullptr;
t_lua_setfield      p_lua_setfield = nullptr;
t_luaL_setfuncs     p_luaL_setfuncs = nullptr;

// --- DATA & STATE ---
char g_host[256] = "archipelago.gg:";
char g_slot[256] = "";
char g_pass[256] = "";
bool g_pending = false;
bool g_threadStarted = false;
HINSTANCE hInst;
HWND hostBox, slotBox, passBox;

// --- GUI LOGIC ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND && LOWORD(wParam) == 1) {
        GetWindowTextA(hostBox, g_host, 256);
        GetWindowTextA(slotBox, g_slot, 256);
        GetWindowTextA(passBox, g_pass, 256);
        g_pending = true;
        ShowWindow(hwnd, SW_HIDE);
    }
    if (msg == WM_CLOSE) {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

DWORD WINAPI GuiThread(LPVOID lpParam) {
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "ArchipelagoGuiClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    HWND mainHwnd = CreateWindowA("ArchipelagoGuiClass", "Archipelago Connection",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        100, 100, 360, 260, NULL, NULL, hInst, NULL);
    CreateWindowA("STATIC", "Host:", WS_VISIBLE | WS_CHILD,
        20, 20, 80, 20, mainHwnd, NULL, hInst, NULL);
    hostBox = CreateWindowA("EDIT", g_host, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, 20, 200, 20, mainHwnd, NULL, hInst, NULL);
    CreateWindowA("STATIC", "Slot Name:", WS_VISIBLE | WS_CHILD,
        20, 60, 80, 20, mainHwnd, NULL, hInst, NULL);
    slotBox = CreateWindowA("EDIT", g_slot, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        100, 60, 200, 20, mainHwnd, NULL, hInst, NULL);
    CreateWindowA("STATIC", "Password:", WS_VISIBLE | WS_CHILD,
        20, 100, 80, 20, mainHwnd, NULL, hInst, NULL);
    passBox = CreateWindowA("EDIT", g_pass, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD | ES_AUTOHSCROLL,
        100, 100, 200, 20, mainHwnd, NULL, hInst, NULL);
    CreateWindowA("BUTTON", "Connect", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        120, 150, 100, 35, mainHwnd, (HMENU)1, hInst, NULL);
    bool lastState = false;
    while (true) {
        bool currentState = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (currentState && !lastState) {
            bool isVisible = IsWindowVisible(mainHwnd);
            ShowWindow(mainHwnd, isVisible ? SW_HIDE : SW_SHOW);
            if (!isVisible) {
                SetForegroundWindow(mainHwnd);
                SetFocus(hostBox);
            }
        }
        lastState = currentState;
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(10);
    }
    return 0;
}

// --- MODULE EXPORTS ---
// Returns data only if "Connect" was clicked
extern "C" int l_get_data(void* L) {
    if (g_pending && p_lua_pushstring) {
        p_lua_createtable(L, 0, 3);
        p_lua_pushstring(L, g_host); p_lua_setfield(L, -2, "host");
        p_lua_pushstring(L, g_slot); p_lua_setfield(L, -2, "slot");
        p_lua_pushstring(L, g_pass); p_lua_setfield(L, -2, "password");
        g_pending = false;
        return 1;
    }
    return 0;
}
// Returns current box contents immediately (Perfect for _OnInit)
extern "C" int l_peek_data(void* L) {
    if (p_lua_pushstring && hostBox && slotBox && passBox) {
        GetWindowTextA(hostBox, g_host, 256);
        GetWindowTextA(slotBox, g_slot, 256);
        GetWindowTextA(passBox, g_pass, 256);
        p_lua_createtable(L, 0, 3);
        p_lua_pushstring(L, g_host); p_lua_setfield(L, -2, "host");
        p_lua_pushstring(L, g_slot); p_lua_setfield(L, -2, "slot");
        p_lua_pushstring(L, g_pass); p_lua_setfield(L, -2, "password");
        return 1;
    }
    return 0;
}

struct luaL_Reg { const char* name; void* func; };

const luaL_Reg guilib[] = {
    {"get_data", (void*)l_get_data},
    {"peek_data", (void*)l_peek_data},
    {NULL, NULL}
};

extern "C" __declspec(dllexport) int luaopen_ArchipelagoGUI(void* L) {
    HMODULE hLua = GetModuleHandleA("lua54.dll");
    if (hLua && !p_lua_createtable) {
        p_lua_createtable = (t_lua_createtable)GetProcAddress(hLua, "lua_createtable");
        p_lua_pushstring = (t_lua_pushstring)GetProcAddress(hLua, "lua_pushstring");
        p_lua_setfield = (t_lua_setfield)GetProcAddress(hLua, "lua_setfield");
        p_luaL_setfuncs = (t_luaL_setfuncs)GetProcAddress(hLua, "luaL_setfuncs");
    }
    if (!g_threadStarted) {
        CreateThread(NULL, 0, GuiThread, NULL, 0, NULL);
        g_threadStarted = true;
    }
    p_lua_createtable(L, 0, 2);
    p_luaL_setfuncs(L, guilib, 0);
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        hInst = hModule;
    }
    return TRUE;
}