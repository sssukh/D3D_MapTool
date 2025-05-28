#pragma once

#include <DirectXMath.h>
#include <ratio>
#include <string>
#include <wrl/event.h>

#include "../ImGui/imgui_impl_dx12.h"
#include "../ImGui/imgui_impl_win32.h"

static const int APP_SRV_HEAP_SIZE = 100;

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap*       Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    ImVector<int>               FreeIndices;

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n - 1);
    }
    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
};

static ExampleDescriptorHeapAllocator g_d3dSrvDescHeapAlloc;

class myImGui
{
public:
    myImGui(){};
    myImGui(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, int frameNums, ID3D12DescriptorHeap* srvHeap);
    ~myImGui();

public:
    bool InitImGui();
    
    void DrawImGui();
    
    void Render(ID3D12GraphicsCommandList* pCommandList );


    void ReleaseImGui();

    // Window for check mouse plane position
    void DrawMousePlanePosWindow(DirectX::XMFLOAT3 pValue);

    // Window for Plane Texture Change
    void DrawPlaneTextureListWindow(UINT& pTexIndex);

    void DrawWireFrameModeWindow(bool& bIsWireFrameMode);

    bool DrawTextureOpenWindow(std::wstring& rFileDirectory );

    void DrawHeightModVarWindow(UINT& retIntersectRange, UINT& retMaxStrengthRange, float& retModStrength);
    
    std::wstring OpenFileDialog();

    bool DrawSaveMapWindow();
    
private:
    HWND mHwnd = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice = nullptr;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue = nullptr;

    int mFrameNums=3;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescHeap = nullptr;

    bool isDebug = true;
};
