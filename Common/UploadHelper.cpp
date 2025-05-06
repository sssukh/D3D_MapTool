#include "UploadHelper.h"

#include "d3dUtil.h"



void UploadHelper::Begin()
{
    ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCmdAlloc.ReleaseAndGetAddressOf())));

    ThrowIfFailed(mDevice->CreateCommandList(1,D3D12_COMMAND_LIST_TYPE_DIRECT,mCmdAlloc.Get(),
        nullptr,IID_PPV_ARGS(mList.ReleaseAndGetAddressOf())));

    mInBeginEndBlock =  true;
}

void UploadHelper::Upload(ID3D12Resource* pResource, UINT pSubresourceIndexStart,
     D3D12_SUBRESOURCE_DATA* pSubRes, UINT numSubresources)
{
    if(!mInBeginEndBlock)
    {
        // error
        return;
    }
    const UINT64 uploadSize = GetRequiredIntermediateSize(
        pResource,
        pSubresourceIndexStart,
        numSubresources
        );

    const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource = nullptr;

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&scratchResource)));

    UpdateSubresources(mList.Get(),pResource,scratchResource.Get(),0,pSubresourceIndexStart,numSubresources,pSubRes);
}



void UploadHelper::Transition()
{
}

void UploadHelper::End(ID3D12CommandQueue* pCommandQueue)
{
    ThrowIfFailed(mList->Close());

    ID3D12CommandList* commandList[] = {mList.Get()};
    pCommandQueue->ExecuteCommandLists(_countof(commandList),commandList);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    ThrowIfFailed(mDevice->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(fence.GetAddressOf())));

    HANDLE gpuCompleteEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);

    // fence의 값을 1로 지정
    ThrowIfFailed(pCommandQueue->Signal(fence.Get(),1ULL));

    
    if(fence->GetCompletedValue()<1ULL)
    {
        // 지정한 값에 도달하면 이벤트 발생 
        ThrowIfFailed(fence->SetEventOnCompletion(1ULL,gpuCompleteEvent));
        WaitForSingleObject(gpuCompleteEvent,INFINITE);
    }

    // 마무리로 정리하기.
    
}
