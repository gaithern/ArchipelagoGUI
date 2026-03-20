#include <windows.h>
#include <GL/gl.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GLFW/glfw3.h"

// --- To compile: g++ -shared -o output/ArchipelagoGUI.dll src/dllmain.cpp imgui/*.cpp -Iinclude -Iimgui -Llib -lglfw3 -lopengl32 -lgdi32 -lcomctl32 ---

// --- LUA FUNCTION POINTERS ---
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
bool g_showGui = false;
bool g_threadStarted = false;
HINSTANCE hInst;

// --- GUI THREAD (GLFW + ImGui) ---
DWORD WINAPI GuiThread(LPVOID lpParam) {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    GLFWwindow* window = glfwCreateWindow(360, 260, "Archipelago Connection", NULL, NULL);
    if (!window) return 1;

    // --- THE FIX: CLOSE CALLBACK ---
    // This intercepts the 'X' button click
    glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
        glfwSetWindowShouldClose(w, GLFW_FALSE); // Cancel the actual close
        glfwHideWindow(w);                       // Just hide the window
        g_showGui = false;                       // Sync our toggle variable
    });

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool lastState = false;
    // Changed while loop to true so the thread never dies
    while (true) {
        glfwPollEvents();

        // F4 Toggle Logic
        bool currentState = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (currentState && !lastState) {
            g_showGui = !g_showGui;
            if (g_showGui) {
                glfwShowWindow(window);
                glfwFocusWindow(window);
            } else {
                glfwHideWindow(window);
            }
        }
        lastState = currentState;

        if (g_showGui) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Archipelago Settings", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
            
            ImGui::InputText("Host", g_host, 256);
            ImGui::InputText("Slot Name", g_slot, 256);
            ImGui::InputText("Password", g_pass, 256, ImGuiInputTextFlags_Password);

            ImGui::Separator();

            if (ImGui::Button("Connect", ImVec2(120, 35))) {
                g_pending = true;
                g_showGui = false;
                glfwHideWindow(window);
            }
            
            ImGui::End();

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        } else {
            Sleep(16); // Idle when hidden to save CPU
        }
    }

    // These will technically never be reached now unless you add a break condition
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// --- MODULE EXPORTS ---
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

extern "C" int l_peek_data(void* L) {
    if (p_lua_pushstring) {
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
    if (hLua) {
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