#include "NormalMapGenerator.h"



NormalMapGenerator::NormalMapGenerator(ID3D12Device* device)
        : mD3DDevice(device)
{

}

void NormalMapGenerator::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
                                          CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
    mNormalCpuSrv = hCpuDescriptor;
    mNormalGpuSrv = hGpuDescriptor;
    mNormalCpuUav = hCpuDescriptor.Offset(1,descriptorSize);
    mNormalGpuUav = hGpuDescriptor.Offset(1,descriptorSize);
    
    BuildDescriptors();
}

void NormalMapGenerator::Execute(ID3D12GraphicsCommandList* pCmdList, ID3D12RootSignature* pRootSig,
                                 ID3D12PipelineState* pNormalMappingPso, ID3D12Resource* input)
{
    pCmdList->SetComputeRootSignature(pRootSig);

    pCmdList->SetPipelineState(pNormalMappingPso);

    // input(height map)의 gpuSrv descriptor Handle을 받아와야한다.
    pCmdList->SetComputeRootDescriptorTable(0,mHeightMapGpuSrv);
    pCmdList->SetComputeRootDescriptorTable(1,mNormalGpuUav);
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(input,
        D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_GENERIC_READ));

    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    // 8,8,1 is defined in Compute Shader
    UINT numGroupsX = (UINT)ceilf(mWidth/8.0f);
    UINT numGroupsY = (UINT)ceilf(mHeight/8.0f);

    pCmdList->Dispatch(numGroupsX,numGroupsY,1);

    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(input,
        D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COMMON));
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap.Get(),
       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON));

    // renewal done
    bNormalDirty = false;
}

void NormalMapGenerator::SetNewNormalMap(UINT width, UINT height, CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapGpuHandle)
{
    mWidth = width;
    mHeight = height;
    mHeightMapGpuSrv = pHeightMapGpuHandle;

    BuildResources();
}


void NormalMapGenerator::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    
    mD3DDevice->CreateShaderResourceView(mNormalMap.Get(), &srvDesc, mNormalCpuSrv);
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    uavDesc.Format = mFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    
    mD3DDevice->CreateUnorderedAccessView(mNormalMap.Get(), nullptr, &uavDesc, mNormalCpuUav);
}

void NormalMapGenerator::BuildResources()
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(mD3DDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mNormalMap)));
}
