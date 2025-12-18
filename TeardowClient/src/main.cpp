// CreateDXGIFactory1
#pragma comment(lib, "dxgi.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "sdk/sdk.h"

#include <string>
#include <format>
#include <print>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <dxgi1_4.h>

#include <memcury.h>

#include "sdk/lua.h"
#include "sdk/render.h"

static Tear::Script* g_pCurrentScript = nullptr;

static std::vector<std::string> logMessages{};

void CopyButton(uintptr_t pData) {
    ImGui::SameLine();
    ImGui::PushID(reinterpret_cast<void*>(pData));
    if (ImGui::SmallButton("Copy")) {
        std::string addrStr = std::format("{:p}", reinterpret_cast<void*>(pData));
		ImGui::SetClipboardText(addrStr.c_str());
	}
    ImGui::PopID();
}

int LogFunc(lua_State* L) {
    const char* msg = Lua::ToString(L, 1);
	std::string text = std::format("[Lua] {}\n", msg);
	logMessages.push_back(text);
    return 0;
}

void Render() {
    if (RenderHook::g_bMenuOpen) {
        Tear::Context* pContext = Tear::Context::Get();
        Tear::PlayerDataWrapper* localPlayer = pContext->GetLocalPlayer();

        /*
        ImGui::Text("Players:");

        ImGui::Indent();
        for (Tear::PlayerDataWrapper* pData : pContext->players) {
            ImGui::BulletText("Player %d @ 0x%p", pData->data->id, pData);

            if (pData == localPlayer) {
                ImGui::SameLine();
                ImGui::Text("(Local)");
			}

            CopyButton(reinterpret_cast<uintptr_t>(pData));
        }
        ImGui::Unindent();

		ImGui::Separator();
        */

        ImGui::Begin("Script selector");

        if (ImGui::TreeNodeEx("Scripts Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
            Tear::Vector<Tear::Script*>& scripts = *pContext->scene->scripts;
            for (Tear::Script* script : scripts) {
				ImGui::PushID(reinterpret_cast<void*>(script));
                if (ImGui::TreeNodeEx(script->name.c_str())) {
					ImGui::BeginChild("ScriptDetails", ImVec2(0, 126), true);

                    ImGui::Text("Path: %s", script->filePath.c_str());
					ImGui::Text("Server core: 0x%p", script->serverCore);
                    ImGui::Text("Client core: 0x%p", script->clientCore);
					ImGui::Text("Script object: 0x%p", script);

                    ImGui::Spacing();

                    const char* message = (g_pCurrentScript == script) ? "Already selected" : "Select";
					ImGui::BeginDisabled(g_pCurrentScript == script);
					if (ImGui::Button(message, ImVec2(-1.0f, 0.0f))) {
                        g_pCurrentScript = script;
					}
					ImGui::EndDisabled();

					ImGui::EndChild();

                    ImGui::TreePop();
				}
				ImGui::PopID();
            }
			ImGui::TreePop();
        }
        ImGui::End();

        ImGui::Begin("Executor");
		ImGui::BeginDisabled(g_pCurrentScript == nullptr);

		float windowHeight = ImGui::GetWindowHeight();

		static char buffer[8192] = {};
		ImGui::InputTextMultiline("##source", buffer, sizeof(buffer), ImVec2(-1.0f, windowHeight - 66.0f), ImGuiInputTextFlags_AllowTabInput);

		if (ImGui::Button("Execute", ImVec2(-1.0f, 0.0f))) {
            if (g_pCurrentScript) {
                Tear::ScriptCore* clientCore = g_pCurrentScript->clientCore;
                if (clientCore) {
                    lua_State* L = *clientCore->L;

                    int safeTop = Lua::GetTop(L);

					// Bind tearprint function
					Lua::GetField(L, Lua::GLOBALS_INDEX, "_G");
					Lua::PushCClosure(L, &LogFunc, 0);
					Lua::SetField(L, -2, "print");
					Lua::Pop(L, 1); // pop _G

					// Compile the script
                    Lua::GetField(L, Lua::GLOBALS_INDEX, "loadstring");
                    Lua::PushString(L, buffer);
                    Lua::PCall(L, 1, 1, 0);

                    if (GetLuaType(L, -1) != LuaType::FUNCTION) {
						const char* errMsg = Lua::ToString(L, -1);
						std::string text = std::format("[Lua] Error: {}\n", errMsg);
						logMessages.push_back(text);
						Lua::Pop(L, 1); // pop error message
                    }

					// Call the loaded function
					int res = Lua::PCall(L, 0, 0, 0);
					if (res != 0) {
						MessageBoxA(NULL, "Error executing Lua script.", "Lua Error", MB_OK | MB_ICONERROR);
                        Lua::Pop(L, 1);
                    }

					Lua::SetTop(L, safeTop); // clean up the stack
                }
            }
        }

        ImGui::EndDisabled();
        ImGui::End();

        ImGui::Begin("Console");

        for (const std::string& msg : logMessages) {
            ImGui::TextWrapped(msg.c_str());
		}

        ImGui::End();
    }
}

void SetupConsole() {
    AllocConsole();

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);

    HANDLE handles[] = {
        GetStdHandle(STD_OUTPUT_HANDLE),
        GetStdHandle(STD_ERROR_HANDLE)
    };

    for (HANDLE h : handles) {
        if (h == INVALID_HANDLE_VALUE) continue;

        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) continue;

        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        mode |= ENABLE_PROCESSED_OUTPUT;
        SetConsoleMode(h, mode);
    }
}

DWORD MainThread(HINSTANCE hinstDLL) {
	Tear::Context* context = Tear::Context::Get();
	IDXGISwapChain* swapChain = context->renderer->swapchain;

    SetupConsole();

	PVOID* vtable = *reinterpret_cast<PVOID**>(swapChain);
	PVOID* presentFunc = &vtable[8];

	RenderHook::oPresent = reinterpret_cast<PresentFn>(*presentFunc);

	DWORD oldProtect;
	VirtualProtect(presentFunc, sizeof(PVOID), PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<PVOID*>(presentFunc) = reinterpret_cast<PVOID>(RenderHook::hkPresent);
	VirtualProtect(presentFunc, sizeof(PVOID), oldProtect, &oldProtect);

    HWND hWnd = FindWindowA(NULL, "Teardown");
    RenderHook::g_oWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RenderHook::WndProc);

	auto scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 4D 8B D0 48 8B D9 E8 ? ? ? ? 48 8B F8 49 C7 C0 ? ? ? ? 90 49 FF C0 43 80 3C 02 ? 75 ? 49 8B D2 48 8B CB E8 ? ? ? ? 4C 8B 4B ? 4C 8D 44 24 ? 48 8B D7");
	Lua::GetField = reinterpret_cast<GetField_t>(scanner.Get());
	scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 4C 8B 49 ? 49 8B F8");
	Lua::PushString = reinterpret_cast<PushString_t>(scanner.Get());
	scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 41 8B F8 44 8B D2");
	Lua::PCall = reinterpret_cast<PCall_t>(scanner.Get());
	scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 49 8B D8 8B F2 48 8B F9 E8");
	Lua::ToString = reinterpret_cast<ToString_t>(scanner.Get());
    scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 4C 8B 49 ? 48 8B F2");
	Lua::PushCClosure = reinterpret_cast<PushCClosure_t>(scanner.Get());
	scanner = Memcury::Scanner::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 4D 8B D0 48 8B D9 E8 ? ? ? ? 48 8B F8 49 C7 C0 ? ? ? ? 90 49 FF C0 43 80 3C 02 ? 75 ? 49 8B D2 48 8B CB E8 ? ? ? ? 4C 8B 4B ? 4C 8D 44 24 ? 49 83 E9");
	Lua::SetField = reinterpret_cast<SetField_t>(scanner.Get());

    auto scripts = context->scene->scripts;
    for (auto script : *scripts) {
        std::println("ScriptCore @ {:p}", (PVOID)script->clientCore);
    }

	while (!GetAsyncKeyState(VK_END)) {
		Sleep(100);
	}

    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RenderHook::g_oWndProc);

	VirtualProtect(presentFunc, sizeof(PVOID), PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<PVOID*>(presentFunc) = RenderHook::oPresent;
	VirtualProtect(presentFunc, sizeof(PVOID), oldProtect, &oldProtect);

	Sleep(1000); // Ensure no threads are in hooked functions

    FreeConsole();

	FreeLibraryAndExitThread(hinstDLL, 0);
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);

		HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hinstDLL, 0, nullptr);
        if (hThread) CloseHandle(hThread);
	}

	return TRUE;
}