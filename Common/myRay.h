#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl/client.h>

#include "MathHelper.h"

using namespace DirectX;

struct PickingResult
{
    bool Hit;
    XMFLOAT3 IntersectPos;
    float Distance;
};

class myRay
{
public:
    myRay(ID3D12Device* pDevice);
    ~myRay() {};

public:

    XMFLOAT3 GetRayOrigin() const { return mRayOrigin;}

    XMFLOAT3 GetRayDirection() const {return mRayDir;}

    POINT GetStartPos() const {return mStartPos;}
    
    void SetStartPos(POINT pNewStartPos) { mStartPos = pNewStartPos;}
    
    void SetMatrix(XMMATRIX pViewMat, XMMATRIX pProjMat);

    void SetViewport(const D3D12_VIEWPORT& pViewport) {mD3DViewport = pViewport;}

    void BuildBuffers();

    // Screen space -> clip space -> view space -> world space
    // Update mRayOrigin & mRayDir
    void UpdateRay();
    
    // dummy before CS
    XMVECTOR PlaneLineIntersectVect(XMVECTOR pPoint, XMVECTOR pNormal);

    
    void BuildRootSignature();

    // dispatch
    void GetIntersectionPos();
    
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

    Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> mReadBackBuffer;
    
    Microsoft::WRL::ComPtr<ID3D12Device> mD3dDevice=nullptr;

};
