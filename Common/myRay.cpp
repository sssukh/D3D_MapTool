#include "myRay.h"

#include "d3dUtil.h"

myRay::myRay(ID3D12Device* pDevice)
{
    mD3dDevice = pDevice;
    bufferByteSize = mPickingResultNum * sizeof(PickingResult);
}

void myRay::BuildBuffers()
{
    ThrowIfFailed(mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferByteSize,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&mOutputBuffer)));

    ThrowIfFailed(mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferByteSize),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mReadBackBuffer)));
}

void myRay::UpdateRay()
{
    // points on near Z-plane and far Z-plane
    const XMVECTOR vNear = XMVectorSet(mStartPos.x,mStartPos.y,0.0f,1.0f);
    const XMVECTOR vFar = XMVectorSet(mStartPos.x,mStartPos.y,1.0f,1.0f);
    
    XMMATRIX mxProj = XMLoadFloat4x4(&mD3DProjMatrix);
    XMMATRIX mxView = XMLoadFloat4x4(&mD3DViewMatrix);

    // For getting a world space pos, 
    XMMATRIX mxWorld = XMLoadFloat4x4(&MathHelper::Identity4x4());

    // Get start point (on near Z-plane in World space)
    XMVECTOR nearPoint = XMVector3Unproject(vNear, mD3DViewport.TopLeftX,mD3DViewport.TopLeftY,
        mD3DViewport.Width,mD3DViewport.Height,mD3DViewport.MinDepth,mD3DViewport.MaxDepth,mxProj,mxView,mxWorld);

    // Get destination (on far Z-plane in World space)
    XMVECTOR farPoint =  XMVector3Unproject(vFar, mD3DViewport.TopLeftX,mD3DViewport.TopLeftY,
        mD3DViewport.Width,mD3DViewport.Height,mD3DViewport.MinDepth,mD3DViewport.MaxDepth,mxProj,mxView,mxWorld);

    // Get ray direction
    XMVECTOR vDir = XMVector3Normalize(farPoint - nearPoint);

    XMStoreFloat3(&mRayDir,vDir);
    XMStoreFloat3(&mRayOrigin,nearPoint);
}




void myRay::SetMatrix(XMMATRIX pViewMat, XMMATRIX pProjMat)
{
    XMStoreFloat4x4(&mD3DViewMatrix , pViewMat);
    XMStoreFloat4x4(&mD3DProjMatrix, pProjMat);
}

XMVECTOR myRay::PlaneLineIntersectVect(XMVECTOR pPoint, XMVECTOR pNormal)
{
    
    XMVECTOR p =XMPlaneFromPointNormal(pPoint,pNormal);

    // temporary : 임시로 다음과 같이 했지만 필요하면 수정 필요할듯
    XMVECTOR RayDest = XMVectorMultiplyAdd(XMLoadFloat3(&mRayDir),XMVectorSet(10.0f,10.0f,10.0f,10.0f),XMLoadFloat3(&mRayOrigin));
    
    return XMPlaneIntersectLine(p,XMLoadFloat3(&mRayOrigin),RayDest);
}

void myRay::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER slotRootParameter[6];
    // height Map srv
    slotRootParameter[0].InitAsDescriptorTable(1,&srvTable);
    // vertex buffer
    slotRootParameter[1].InitAsShaderResourceView(0);
    // index buffer
    slotRootParameter[2].InitAsShaderResourceView(1);
    // ray info
    slotRootParameter[3].InitAsConstantBufferView(0);
    // plane info
    slotRootParameter[4].InitAsConstantBufferView(1);
    // uav
    slotRootParameter[5].InitAsUnorderedAccessView(0);

    
}

void myRay::GetIntersectionPos()
{
}
