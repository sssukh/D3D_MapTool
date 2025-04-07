#pragma once
#include "FrameResource.h"

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

    void GetNormalMap(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* rNormalMap);

    ID3D12Resource* GetNormalMap(){ return mNormalMap.Get();}

    void SetNewNormalMap(UINT width, UINT height, CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapGpuHandle);

    bool GetDirty() const { return bNormalDirty; }

    ID3D12Resource* GetNormalMap() const { return mNormalMap.Get(); }
private:
    void BuildDescriptors();
    void BuildResources();

    

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

    CD3DX12_GPU_DESCRIPTOR_HANDLE mHeightMapGpuSrv;

    bool bNormalDirty = true;
};
