#include "myTexture.h"

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
        // check md3d12device
        result = DirectX::CreateTexture(pD3D12Device,image.GetMetadata(),&Resource);

        if(FAILED(result))
            return result;

        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        
        result = DirectX::PrepareUpload(pD3D12Device,image.GetImages(),image.GetImageCount(),image.GetMetadata(),subresources);

        if(FAILED(result))
            return result;

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
            IID_PPV_ARGS(UploadHeap.GetAddressOf()));
        if (FAILED(result))
            return result;
            // error!

        UpdateSubresources(pD3D12CommandList,
                    Resource.Get(), UploadHeap.Get(),
                    0, 0, static_cast<unsigned int>(subresources.size()),
                    subresources.data());
    }

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


