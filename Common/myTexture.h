#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"
#include "DirectXTex.h"

// dds랑 bmp 로드
class myTexture
{
public:
    myTexture(){};
    myTexture(std::string pName, std::wstring pFileName);
    ~myTexture(){};

public:
    HRESULT CreateDDSTextureFromFile(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList);

    HRESULT CreateTextureFromFileName(ID3D12Device* pD3D12Device, ID3D12GraphicsCommandList* pD3D12CommandList);

    HRESULT GetScratchImage(std::wstring pFilename, DirectX::ScratchImage* sImage);
    
    void CreateShaderResourceView(ID3D12Device* pD3D12Device,ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset, UINT pDescriptorSize);
public:
    // Unique material name for lookup.
    std::string Name;

    std::wstring Filename;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mD3D12DescriptorHeap = nullptr;
    INT mHandleOffset;

    // Initialized needed
    // Microsoft::WRL::ComPtr<ID3D12Device> mD3D12Device = nullptr;
    // Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mD3D12CommandList = nullptr;

};
