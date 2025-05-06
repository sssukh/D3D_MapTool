#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class UploadHelper
{
public:
    UploadHelper();
    UploadHelper(ID3D12Device* pDevice);
    ~UploadHelper();

    void Begin();

    void Upload(ID3D12Resource* pResource, UINT pSubresourceIndexStart, D3D12_SUBRESOURCE_DATA* pSubRes, UINT numSubresources);

    void Transition();

    void End(ID3D12CommandQueue* pCommandQueue);

private:
    Microsoft::WRL::ComPtr<ID3D12Device>                mDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      mCmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   mList;

    bool mInBeginEndBlock = false;

    
};
