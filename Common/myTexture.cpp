#include "myTexture.h"

#include "d3dUtil.h"

myTexture::myTexture(std::string pName,
                     std::wstring pFileName)
        :Name(pName)
        ,Filename(pFileName)
{
}

HRESULT myTexture::CreateDDSTextureFromFile(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList)
{
    if(pD3D12Device==nullptr || pD3D12CommandList == nullptr)
        return E_FAIL;

    return DirectX::CreateDDSTextureFromFile12(pD3D12Device, pD3D12CommandList,Filename.c_str(),Resource,UploadHeap);
}

HRESULT myTexture::CreateTextureFromFileName(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList)
{
     DirectX::ScratchImage image;
    HRESULT result = GetScratchImage(Filename,&image);

    if(SUCCEEDED(result))
    {
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
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			
        return result;
    }

    MessageBoxA(nullptr,"Can't create texture from file","DirectXError",MB_OK | MB_ICONERROR);
    return result;
}

HRESULT myTexture::CreateTextureFromFileName(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList, ID3D12CommandQueue* pD3D12CommandQueue, ID3D12CommandAllocator* pD3D12ComAlloc)
{
    DirectX::ScratchImage image;
    HRESULT result = GetScratchImage(Filename,&image);

    if(SUCCEEDED(result))
    {
        // ID3D12Resource* tmpTex = nullptr;
        // resource 받아오기
        result = DirectX::CreateTexture(pD3D12Device,image.GetMetadata(),&Resource);
            
        if(FAILED(result))
            return result;

        // D3D12_RESOURCE_DESC resource_desc = tmpTex->GetDesc();
        //
        // pD3D12Device->CreateCommittedResource(
        // &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        // D3D12_HEAP_FLAG_NONE,
        // &resource_desc,
        // D3D12_RESOURCE_STATE_COPY_DEST,
        // nullptr,
        // IID_PPV_ARGS(&Resource));
        
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
        //
        // D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        // UINT64 totalBytes;
        // pD3D12Device->GetCopyableFootprints(&tmpTex->GetDesc(),0,1,0,&footprint,nullptr,nullptr,&totalBytes);
        //
        // // i put uploadBufferSize just for now, but i need to change it to exact texture size
        // void* mappedData;
        // UploadHeap->Map(0,nullptr,&mappedData);
        // memcpy(mappedData, tmpTex,uploadBufferSize);
        //
        // // for (UINT row = 0; row < footprint.Footprint.Height; row++) {
        // //     memcpy(
        // //         (BYTE*)mappedData + footprint.Offset + row * footprint.Footprint.RowPitch,
        // //         (BYTE*)tmpTex + row * (footprint.Footprint.Width), // 원본 데이터 (픽셀 당 4바이트)
        // //         footprint.Footprint.Width
        // //     );
        // // }
        // UploadHeap->Unmap(0,nullptr);

        

       
	
        // Reuse the memory associated with command recording.
        // We can only reset when the associated command lists have finished execution on the GPU.
	
        ThrowIfFailed(pD3D12ComAlloc->Reset());

        // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
        // Reusing the command list reuses memory.
        ThrowIfFailed(pD3D12CommandList->Reset(pD3D12ComAlloc,nullptr));
        
        // pD3D12CommandList->CopyTextureRegion(
        //     &CD3DX12_TEXTURE_COPY_LOCATION(Resource.Get(),0),
        //     0,0,0,
        //     &CD3DX12_TEXTURE_COPY_LOCATION(UploadHeap.Get(),footprint),nullptr);
        //
        // pD3D12CommandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(
        //     Resource.Get(),
        //     D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


        // 이부분을 나중에 해야한다?
      

        // 이후 uploadHeap 재사용 또는 해제 가능

        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
        
        UpdateSubresources(pD3D12CommandList,
                    Resource.Get(), UploadHeap.Get(),
                    0, 0, static_cast<unsigned int>(subresources.size()),
                    subresources.data());
        
        pD3D12CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Resource.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));


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

void myTexture::CreateShaderResourceView(ID3D12Device* pD3D12Device, ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset, UINT pDescriptorSize)
{
    mD3D12DescriptorHeap = pDescriptorHeap;
    mHandleOffset = pOffset;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mD3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    
    
    hDescriptor.Offset(mHandleOffset,pDescriptorSize);
    
    // auto bricksTex = mTextures["bricksTex"]->Resource;
    // auto checkboardTex = mTextures["checkboardTex"]->Resource;
    // auto iceTex = mTextures["iceTex"]->Resource;
    // auto white1x1Tex = mTextures["white1x1Tex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = Resource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
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




