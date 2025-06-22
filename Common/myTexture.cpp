#include "myTexture.h"

#include "d3dUtil.h"

myTexture::myTexture(std::string pName,
                     std::wstring pFileName,
                     bool pIsCubeMap)
        :Name(pName)
        ,Filename(pFileName)
        ,bIsCubeMap(pIsCubeMap)
{
}

HRESULT myTexture::CreateDDSTextureFromFile(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList)
{
    if(pD3D12Device==nullptr || pD3D12CommandList == nullptr)
        return E_FAIL;

    return DirectX::CreateDDSTextureFromFile12(pD3D12Device, pD3D12CommandList,Filename.c_str(),Resource,UploadHeap);
}

HRESULT myTexture::CreateTextureFromFileName(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList, bool bFmtTypeless)
{
     DirectX::ScratchImage image;
    HRESULT result = GetScratchImage(Filename,&image);

    if(SUCCEEDED(result))
    {
        // 포맷을 typeless로 설정이 필요하면 
        // if(bFmtTypeless)
        // {
        //     image = SetTypelessFormat(image);
        // }
        
        // resource 받아오기
        result = DirectX::CreateTexture(pD3D12Device,image.GetMetadata(),&Resource);
            
        if(FAILED(result))
            return result;

        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        
        result = DirectX::PrepareUpload(pD3D12Device,image.GetImages(),image.GetImageCount(),image.GetMetadata(),subresources);

        if(FAILED(result))
        {
            MessageBoxA(nullptr,"Can't Get Subresource from texture","DirectXError",MB_OK | MB_ICONERROR);

            return result;
        }
        
        // upload is implemented by application developer. Here's one solution using <d3dx12.h>
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(Resource.Get(),
            0, static_cast<unsigned int>(subresources.size()));

        
        // Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
        result = pD3D12Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&UploadHeap));
        
        if (FAILED(result))
        {
            MessageBoxA(nullptr,"Can't create upload heap","DirectXError",MB_OK | MB_ICONERROR);

            return result;
            // error!
        }

        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
        
        UpdateSubresources(pD3D12CommandList,
                    Resource.Get(), UploadHeap.Get(),
                    0, 0, static_cast<unsigned int>(subresources.size()),
                    subresources.data());
        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
			
        return result;
    }

    MessageBoxA(nullptr,"Can't create texture from file","DirectXError",MB_OK | MB_ICONERROR);
    return result;
}

HRESULT myTexture::CreateTextureFromFileName(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList, ID3D12CommandQueue* pD3D12CommandQueue, ID3D12CommandAllocator* pD3D12ComAlloc , bool bFmtTypeless)
{
    DirectX::ScratchImage image;
    HRESULT result = GetScratchImage(Filename,&image);

    if(SUCCEEDED(result))
    {
        // 포맷을 typeless로 설정이 필요하면 
        // if(bFmtTypeless)
        // {
        //     image = SetTypelessFormat(image);
        // }
        
        // resource 받아오기
        result = DirectX::CreateTexture(pD3D12Device,image.GetMetadata(),&Resource);
            
        if(FAILED(result))
            return result;
        
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        
        result = DirectX::PrepareUpload(pD3D12Device,image.GetImages(),image.GetImageCount(),image.GetMetadata(),subresources);

        if(FAILED(result))
        {
            MessageBoxA(nullptr,"Can't Get Subresource from texture","DirectXError",MB_OK | MB_ICONERROR);

            return result;
        }

        
        // upload is implemented by application developer. Here's one solution using <d3dx12.h>
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(Resource.Get(),
            0, static_cast<unsigned int>(subresources.size()));

        
        // Microsoft::WRL::ComPtr<ID3D12Resource> textureUploadHeap;
        result = pD3D12Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&UploadHeap));
        
        if (FAILED(result))
        {
            MessageBoxA(nullptr,"Can't create upload heap","DirectXError",MB_OK | MB_ICONERROR);

            return result;
            // error!
        }
	
        // Reuse the memory associated with command recording.
        // We can only reset when the associated command lists have finished execution on the GPU.
	
        ThrowIfFailed(pD3D12ComAlloc->Reset());

        // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
        // Reusing the command list reuses memory.
        ThrowIfFailed(pD3D12CommandList->Reset(pD3D12ComAlloc,nullptr));
        
        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
        
        UpdateSubresources(pD3D12CommandList,
                    Resource.Get(), UploadHeap.Get(),
                    0, 0, static_cast<unsigned int>(subresources.size()),
                    subresources.data());
        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));


        pD3D12CommandList->Close();

        ID3D12CommandList* cmdsLists[] = { pD3D12CommandList };
        pD3D12CommandQueue->ExecuteCommandLists(_countof(cmdsLists),cmdsLists);

        // Create fence for upload resource
        Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence;
        UINT64 fenceValue = 1;
        pD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence));

        HANDLE fenceEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);

        
        pD3D12CommandQueue->Signal(uploadFence.Get(),fenceValue);


        // GPU가 펜스 값에 도달했는지 확인
        if (uploadFence->GetCompletedValue() < fenceValue) {
            uploadFence->SetEventOnCompletion(fenceValue, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        
        return result;
    }

    MessageBoxA(nullptr,"Can't create texture from file","DirectXError",MB_OK | MB_ICONERROR);
    return result;
}

HRESULT myTexture::GetScratchImage(std::wstring pFilename, DirectX::ScratchImage* sImage)
{
    wchar_t ext[_MAX_EXT] = {};
    _wsplitpath_s( pFilename.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT );

    if ( _wcsicmp( ext, L".dds" ) == 0 )
    {
        return LoadFromDDSFile( pFilename.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *sImage );
    }
    if ( _wcsicmp( ext, L".tga" ) == 0 )
    {
        return LoadFromTGAFile( pFilename.c_str(), nullptr, *sImage );
    }
    if ( _wcsicmp( ext, L".hdr" ) == 0 )
    {
        return LoadFromHDRFile( pFilename.c_str(), nullptr, *sImage );
    }
    
    return LoadFromWICFile( pFilename.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, *sImage );
 
}

void myTexture::CreateShaderResourceView(ID3D12Device* pD3D12Device, ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset, UINT pDescriptorSize, bool bIsTypeless)
{
    mD3D12DescriptorHeap = pDescriptorHeap;
    mHandleOffset = pOffset;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mD3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    
    
    hDescriptor.Offset(mHandleOffset,pDescriptorSize);
    
    // auto bricksTex = mTextures["bricksTex"]->Resource;
    // auto checkboardTex = mTextures["checkboardTex"]->Resource;
    // auto iceTex = mTextures["iceTex"]->Resource;
    // auto white1x1Tex = mTextures["white1x1Tex"]->Resource;

    DXGI_FORMAT format = Resource->GetDesc().Format;

    
    if(bIsTypeless)
    {
        DXGI_FORMAT unormFmt = DirectX::MakeTypelessUNORM(Resource->GetDesc().Format);
        format = unormFmt;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = format;
    srvDesc.ViewDimension = bIsCubeMap?D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = Resource->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    
    pD3D12Device->CreateShaderResourceView(Resource.Get(), &srvDesc, hDescriptor);
}

void myTexture::Release()
{
    Resource->Release();
    UploadHeap->Release();
    mD3D12DescriptorHeap->Release();
}

DirectX::ScratchImage myTexture::SetTypelessFormat(DirectX::ScratchImage& pImage)
{
    DXGI_FORMAT originalFormat = pImage.GetMetadata().format;
    DXGI_FORMAT typelessFormat = DirectX::MakeTypeless(originalFormat);
    
    DirectX::ScratchImage typelessImage;
    Convert(*pImage.GetImage(0,0,0),typelessFormat,
        DirectX::TEX_FILTER_DEFAULT,0.5f,typelessImage);
    
    return typelessImage;
}




