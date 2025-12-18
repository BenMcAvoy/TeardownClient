#pragma once

#include <Windows.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3d12.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include "sdk.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void Render();

// TODO: ResizeBuffers hook should be added
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

namespace RenderHook {
    inline PresentFn oPresent = nullptr;
    inline WNDPROC g_oWndProc = nullptr;
    inline bool g_bMenuOpen = true;

    inline bool initialized = false;

    inline ID3D12Device* device = nullptr;
    inline ID3D12CommandQueue* commandQueue = nullptr;
    inline ID3D12DescriptorHeap* imguiSrvHeap = nullptr;
    inline ID3D12CommandAllocator* cmdAlloc = nullptr;
    inline ID3D12GraphicsCommandList* cmdList = nullptr;
    inline ID3D12Fence* fence = nullptr;
    inline HANDLE fenceEvent = nullptr;
    inline UINT64 fenceValue = 0;
    inline ID3D12DescriptorHeap* rtvHeap = nullptr;
    inline UINT rtvDescriptorSize = 0;
    inline ID3D12Resource** backBuffers = nullptr;
    inline UINT bufferCount = 0;

    inline LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
            g_bMenuOpen = !g_bMenuOpen;
            return true;
        }

        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) || g_bMenuOpen)
            return true;

        return CallWindowProc(g_oWndProc, hWnd, uMsg, wParam, lParam);
    }

    inline HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
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

        ::Render();

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
}
