﻿#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl/client.h>

#include "d3dx12.h"
#include "MathHelper.h"
#include "myTexture.h"
#include "UploadBuffer.h"

struct RenderItem;
using namespace DirectX;

enum RayMode 
{
    HeightModification = 0,
    ObjectPlacing = 1,
    ObjectErase = 2,
    Counts
};

struct PickingResult
{
    bool Hit;
    XMFLOAT3 IntersectPos;
    float Distance;
};

struct Ray 
{
    XMFLOAT3 gRayOrigin;
    float padding1;
    XMFLOAT3 gRayDir;
    float padding2;
};

struct PlaneInfo
{
    UINT Width;
    UINT Height;
    UINT NumTriangles;
};

struct HeightModifyingInfo
{
    UINT IntersectRange;

    UINT MaxStrengthRange;

    float ModifingStrength;
    
    float padding1;
    
    XMFLOAT3 IntersectPos;
};

class myRay
{
public:
    myRay(ID3D12Device* pDevice);
    ~myRay(); 

public:

    XMFLOAT3 GetRayOrigin() const { return mRayOrigin;}

    XMFLOAT3 GetRayDirection() const {return mRayDir;}

    POINT GetStartPos() const {return mStartPos;}
    
    void SetStartPos(POINT pNewStartPos) { mStartPos = pNewStartPos;}
    
    void SetMatrix(XMMATRIX pViewMat, XMMATRIX pProjMat);

    void SetViewport(const D3D12_VIEWPORT& pViewport) {mD3DViewport = pViewport;}

    // Screen space -> clip space -> view space -> world space
    // Update mRayOrigin & mRayDir
    void UpdateRay();
    
    // dummy before CS
    XMVECTOR PlaneLineIntersectVect(XMVECTOR pPoint, XMVECTOR pNormal);


    // *********************************************
    // ray CS
    // *********************************************
    void BuildBuffers();

    // Build Root Signature for Ray CS
    void BuildRootSignature();

    void BuildModRootSignature();

    // void BuildDescriptorHeaps();

    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize, UINT vertexCount, UINT indexCount);
    
    void BuildDescriptors(UINT vertexCount, UINT indexCount);

    void BuildIntersectPso();

    void BuildModPso();

    void BuildModResource();
    
    void InitBuffer(ID3D12GraphicsCommandList* pCmdList, ID3D12CommandQueue* pCommandQueue, ID3D12CommandAllocator* pCommandAllocator);
    
    // Empty now
    XMFLOAT3 GetIntersectionPos() { return mIntersectMappedData[0].IntersectPos; };

    bool IsRayIntersectPlane() {return mIntersectMappedData[0].Hit;}
    
    // Initialize
    void SetNewHeightMap(CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapSrv, ID3D12Resource* pHeightMap); 

    void SetIntersectShader(ID3DBlob* pShader) { mRayIntersectShader = pShader;}

    void SetModShader(ID3DBlob* pShader) { mRayModShader = pShader;}

    void SetVertexIndexResource(ID3D12Resource* pVertexBuffer, ID3D12Resource* pIndexBuffer) { mVertexBuffer = pVertexBuffer; mIndexBuffer = pIndexBuffer;}

    void SetFormat(const DXGI_FORMAT pFormat) { mTexFormat = pFormat;}
    
    // dispatch
    void ExecuteRayIntersectTriangle(
        ID3D12GraphicsCommandList* pCmdList,
        ID3D12Resource* pHeightMap);

    void ExecuteRayMod(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* pHeightMap);


    // TODO : update constant buffer
    void UpdateRayCBs(UINT pWidth, UINT pHeight, UINT pNumTriangles);

    ID3D12PipelineState* GetRayIntersectPSO() const { return mRayIntersectPipelineState.Get();}

    ID3D12PipelineState* GetRayModPSO() const { return mRayModPipelineState.Get();}


    void SetIntersectRange(UINT pIntersectRange) { mIntersectRange = pIntersectRange;}

    void SetStrengthRange(UINT pStrengthRange) { mMaxStrengthRange = pStrengthRange;}
  
    void SetModStrength(float pModStrength) {mModifingStrength = pModStrength;}
    
    UINT GetIntersectRange() { return mIntersectRange;}

    UINT GetStrengthRange() { return mMaxStrengthRange;}
  
    float GetModStrength() {return mModifingStrength;}

    UINT GetRayMode() {return (UINT)mRayMode;}

    void ChangeRayMode(UINT param);
private:
    XMFLOAT3 mRayOrigin = XMFLOAT3(0.0f,0.0f,0.0f);

    XMFLOAT3 mRayDir = XMFLOAT3(0.0f,0.0f,0.0f);

    POINT mStartPos = {0,0};

    // viewport
    D3D12_VIEWPORT mD3DViewport = {};
    
    // projectionMatrix
    XMFLOAT4X4 mD3DProjMatrix = MathHelper::Identity4x4();
    
    // viewMatrix
    XMFLOAT4X4 mD3DViewMatrix = MathHelper::Identity4x4();
    
    // world matrix
    XMFLOAT4X4 mD3DWorldMatrix = MathHelper::Identity4x4();
    
    std::vector<PickingResult> mRayPickingResults;

    UINT mPickingResultNum = 2;

    UINT64 bufferByteSize;

    Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> mReadBackBuffer = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12Device> mD3dDevice=nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRayIntersectSignature = nullptr;

    CD3DX12_GPU_DESCRIPTOR_HANDLE mHeightMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mIntersectCpuUav;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE mIntersectGpuUav;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mVertexCpuSrv;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE mVertexGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mIndexCpuSrv;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE mIndexGpuSrv;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> mRayIntersectPipelineState = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> mRayIntersectShader = nullptr;

    std::unique_ptr<UploadBuffer<Ray>> rayCB = nullptr;

    std::unique_ptr<UploadBuffer<PlaneInfo>> planeInfoCB = nullptr;

    PickingResult* mIntersectMappedData = nullptr;

    UINT triangleNums = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
    
    
    // Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mUavDescriptorHeap = nullptr;

    // Modifying variables

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRayModRootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> mRayModPipelineState = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> mModifiedHeight = nullptr;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mModCpuUav;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE mModGpuUav;

    Microsoft::WRL::ComPtr<ID3DBlob> mRayModShader = nullptr;
    
    UINT mIntersectRange = 15;

    UINT mMaxStrengthRange = 5;

    float mModifingStrength = 0.0026f;

    std::unique_ptr<UploadBuffer<HeightModifyingInfo>> modHeightCB = nullptr;

    UINT mTexWidth = 0;
    
    UINT mTexHeight =0;
    
    DXGI_FORMAT mTexFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    RayMode mRayMode = RayMode::HeightModification;
};
