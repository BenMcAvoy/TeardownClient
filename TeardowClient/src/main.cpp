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

// TODO: ResizeBuffers hook should be added
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
PresentFn oPresent = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <d3d12.h>
#include <dxgi.h>

static bool g_bMenuOpen = true;
static WNDPROC g_oWndProc = nullptr;
static Tear::Script* g_pCurrentScript = nullptr;

static std::vector<std::string> logMessages{};

typedef int (*lua_CFunction)(lua_State *L);

using GetField_t = int(__fastcall*)(void* L, int idx, const char* k);
using PushString_t = void(__fastcall*)(void* L, const char* s);
using PCall_t = int(__fastcall*)(void* L, int nargs, int nresults, int errfunc);
using ToString_t = const char* (__fastcall*)(void* L, int idx);
using PushCClosure_t = void(__fastcall*)(void*, lua_CFunction fn, int n);
using SetField_t = void(__fastcall*)(void* L, int idx, const char* k);

namespace Lua {
	GetField_t GetField = nullptr;
	PushString_t PushString = nullptr;
	PCall_t PCall = nullptr;
	ToString_t ToString = nullptr;
	PushCClosure_t PushCClosure = nullptr;
	SetField_t SetField = nullptr;

	constexpr int GLOBALS_INDEX = -10002;

    void Pop(lua_State* L, int n) {
        L->top -= n;
	}
}

LRESULT CALLBACK WndProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_bMenuOpen = !g_bMenuOpen;
        return true;
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) || g_bMenuOpen)
        return true;

    return CallWindowProc(g_oWndProc, hWnd, uMsg, wParam, lParam);
}

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

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8

enum class LuaType {
    NIL = LUA_TNIL,
    BOOLEAN = LUA_TBOOLEAN,
    LIGHTUSERDATA = LUA_TLIGHTUSERDATA,
    NUMBER = LUA_TNUMBER,
    STRING = LUA_TSTRING,
    TABLE = LUA_TTABLE,
    FUNCTION = LUA_TFUNCTION,
    USERDATA = LUA_TUSERDATA,
    THREAD = LUA_TTHREAD,
    UNKNOWN = -1
};

LuaType GetLuaType(lua_State* L, int idx) {
    LuaStackID addr;
    if (idx > 0) {
        addr = L->base + (idx - 1);
    }
    else if (idx > -10000) {
        addr = L->top + idx;
    }
    else {
        return LuaType::UNKNOWN;
    }

    if (addr >= L->top || addr < L->base)
        return LuaType::UNKNOWN;

    return (LuaType)addr->tt;
}

void Render() {
    if (g_bMenuOpen) {
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

					// Bind tearprint function
					Lua::GetField(L, Lua::GLOBALS_INDEX, "_G");
					Lua::PushCClosure(L, &LogFunc, 0);
					Lua::SetField(L, -2, "tearprint");
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
						const char* errMsg = Lua::ToString(L, -1);
                        if (!errMsg) errMsg = "Unknown Lua error";
						std::string text = std::format("[Lua] Error: {}\n", errMsg);
						logMessages.push_back(text);
						Lua::Pop(L, 1); // pop error message
                    }
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

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    static bool initialized = false;

    static ID3D12Device* device = nullptr;
    static ID3D12CommandQueue* commandQueue = nullptr;
    static ID3D12DescriptorHeap* imguiSrvHeap = nullptr;
    static ID3D12CommandAllocator* cmdAlloc = nullptr;
    static ID3D12GraphicsCommandList* cmdList = nullptr;
    static ID3D12Fence* fence = nullptr;
    static HANDLE fenceEvent = nullptr;
    static UINT64 fenceValue = 0;
    static ID3D12DescriptorHeap* rtvHeap = nullptr;
    static UINT rtvDescriptorSize = 0;
    static ID3D12Resource** backBuffers = nullptr;
    static UINT bufferCount = 0;

    if (!initialized) {
        // grab device/queue from whatever context you have
        Tear::Context* context = Tear::Context::Get();
        auto renderer = context->renderer;
        device = renderer->device;
        commandQueue = renderer->commandQueue;

        // swapchain desc and buffer count
        DXGI_SWAP_CHAIN_DESC desc{};
        pSwapChain->GetDesc(&desc);
        bufferCount = desc.BufferCount;

        // create ImGui context and Win32 integration
        HWND hwnd = desc.OutputWindow;
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        auto path = "C:/Users/Ben/AppData/Local/Microsoft/Windows/Fonts/JetBrainsMonoNerdFont-Regular.ttf";
		ImFont* font = io.Fonts->AddFontFromFileTTF(path, 16.0f);
        if (!font) {
            MessageBoxA(nullptr, "Failed to load font file for ImGui.", "Error", MB_OK | MB_ICONERROR);
        }

		io.FontDefault = font;
        io.Fonts->Build();

        ImGui_ImplWin32_Init(hwnd);

        // create SRV heap for ImGui (font texture)
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&imguiSrvHeap));

        // init ImGui DX12 backend with the heap we created
        ImGui_ImplDX12_Init(device, bufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM, imguiSrvHeap,
            imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
            imguiSrvHeap->GetGPUDescriptorHandleForHeapStart());

        ImGui_ImplDX12_CreateDeviceObjects();

        // create command allocator + command list (direct)
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList));
        // command lists start open; close it until we reset per-frame
        cmdList->Close();

        // create fence and event
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fenceValue = 1;

        // create an RTV heap for the swapchain back buffers
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = bufferCount;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // grab back buffers and create RTVs
        backBuffers = (ID3D12Resource**)malloc(sizeof(ID3D12Resource*) * bufferCount);
        for (UINT i = 0; i < bufferCount; ++i) {
            device; // no-op, just clarifying
            pSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));

            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            cpuHandle.ptr = SIZE_T(cpuHandle.ptr) + SIZE_T(i) * SIZE_T(rtvDescriptorSize);
            device->CreateRenderTargetView(backBuffers[i], nullptr, cpuHandle);
        }

        initialized = true;
    }

    // new frame ImGui calls
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Render();

    ImGui::Render();

    IDXGISwapChain3* swapChain3 = nullptr;
	pSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain3));
    UINT backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = backBuffers[backBufferIndex];
    cmdAlloc->Reset();
    cmdList->Reset(cmdAlloc, nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr = SIZE_T(rtvHandle.ptr) + SIZE_T(backBufferIndex) * SIZE_T(rtvDescriptorSize);
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { imguiSrvHeap };
    cmdList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();
    ID3D12CommandList* listsToExecute[] = { cmdList };
    commandQueue->ExecuteCommandLists(1, listsToExecute);
    const UINT64 signalVal = fenceValue++;
    commandQueue->Signal(fence, signalVal);
    if (fence->GetCompletedValue() < signalVal) {
        fence->SetEventOnCompletion(signalVal, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    return oPresent(pSwapChain, SyncInterval, Flags);
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

	oPresent = reinterpret_cast<PresentFn>(*presentFunc);

	DWORD oldProtect;
	VirtualProtect(presentFunc, sizeof(PVOID), PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<PVOID*>(presentFunc) = reinterpret_cast<PVOID>(hkPresent);
	VirtualProtect(presentFunc, sizeof(PVOID), oldProtect, &oldProtect);

    HWND hWnd = FindWindowA(NULL, "Teardown");
    g_oWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

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

    SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);

	VirtualProtect(presentFunc, sizeof(PVOID), PAGE_EXECUTE_READWRITE, &oldProtect);
	*reinterpret_cast<PVOID*>(presentFunc) = oPresent;
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