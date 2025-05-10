#pragma once
#include "FrameResource.h"

const int NormalMapBufferSize = 10;

class NormalMapGenerator
{
public:
    NormalMapGenerator(ID3D12Device* device);
    ~NormalMapGenerator();


    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
        UINT descriptorSize);

    void Execute(
        ID3D12GraphicsCommandList* pCmdList,
        ID3D12RootSignature* pRootSig,
        ID3D12PipelineState* pNormalMappingPso,
        ID3D12Resource* input);

    void SetNewNormalMap(UINT width, UINT height, CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapGpuHandle);

    int GetDirty() const { return mNumNormalDirty; }

    Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentNormalMap() { return mNormalMapBuffer[mCurrentBufferIndex]; }
    
    UINT GetWidth() const {return mWidth;}
    UINT GetHeight() const {return mHeight;}

    ID3D12Resource* GetCurrentNormalMap(int& retBufferIndex){ retBufferIndex = mCurrentBufferIndex; return mNormalMapBuffer[mCurrentBufferIndex].Get();}
private:
    void BuildDescriptors();
    void BuildResources();

    void AddNormalMap(ID3D12Resource* pNewNormalMap);


private:
    ID3D12Device* mD3DDevice = nullptr;

    UINT mWidth = 0;
    UINT mHeight =0;
    DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mNormalCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mNormalGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mNormalCpuUav;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mNormalGpuUav;

    Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mNormalMapBuffer;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE mHeightMapGpuSrv;

    int mCurrentBufferIndex = -1;
    
    int mNumNormalDirty = 1;
};
