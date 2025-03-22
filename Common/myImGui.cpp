﻿#include "myImGui.h"


myImGui::myImGui(HWND pHwnd, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, int pframeNums,
    ID3D12DescriptorHeap* pSrvHeap)
        :mHwnd(pHwnd)
        ,mFrameNums(pframeNums)
{
    md3dDevice.Attach(pDevice);
    mCommandQueue.Attach(pCommandQueue);
    mSrvDescHeap.Attach(pSrvHeap);
}

myImGui::~myImGui()
{
    
}

bool myImGui::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

     // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(mHwnd);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = APP_SRV_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (md3dDevice.Get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mSrvDescHeap)) != S_OK)
        return false;
    g_d3dSrvDescHeapAlloc.Create(md3dDevice.Get(), mSrvDescHeap.Get());
    
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = md3dDevice.Get();
    init_info.CommandQueue = mCommandQueue.Get();
    init_info.NumFramesInFlight = mFrameNums;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    init_info.SrvDescriptorHeap = mSrvDescHeap.Get();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_d3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)            { return g_d3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    return true;
}

void myImGui::DrawImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    DrawMyWindow();
}

void myImGui::Render(ID3D12GraphicsCommandList* pCommandList)
{
    ImGui::Render();
		
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
}

void myImGui::DrawMyWindow()
{
}

void myImGui::ReleaseImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}



