#pragma once
#include <d3d12.h>
#include <DirectXMath.h>

#include "MathHelper.h"

using namespace DirectX;

class myRay
{
public:
    myRay() {};
    ~myRay() {};

public:
    // inline void SetRayOrigin(float x, float y, float z);
    // inline void SetRayOrigin(const XMFLOAT3& pNewOrigin);
    //
    // inline void SetRayDirection(float x, float y ,float z);
    // inline void SetRayDirection(const XMFLOAT3& pNewDir); 

    XMFLOAT3 GetRayOrigin() const { return mRayOrigin;}

    XMFLOAT3 GetRayDirection() const {return mRayDir;}

    POINT GetStartPos() const {return mStartPos;}
    
    void SetStartPos(POINT pNewStartPos) { mStartPos = pNewStartPos;}
    
    void SetMatrix(XMMATRIX pViewMat, XMMATRIX pProjMat);

    void SetViewport(const D3D12_VIEWPORT& pViewport) {mD3DViewport = pViewport;}

    // Screen space -> clip space -> view space -> world space
    // Update mRayOrigin & mRayDir
    void UpdateRay();
    
    XMVECTOR PlaneLineIntersectVect(XMVECTOR pPoint, XMVECTOR pNormal);
    
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
};
