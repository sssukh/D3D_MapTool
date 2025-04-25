#include "myRay.h"

#include "d3dApp.h"
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

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(mD3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRayRootSignature.GetAddressOf())));
}

void myRay::GetIntersectionPos()
{
    // return PickingResult[0].IntersectPos;
    
    
}

void myRay::SetNewHeightMap(CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapSrv)
{
    mHeightMapGpuSrv = pHeightMapSrv;
}

void myRay::Execute(ID3D12GraphicsCommandList* pCmdList, ID3D12RootSignature* pRootSig,
    ID3D12PipelineState* pNormalMappingPso, ID3D12Resource* pHeightMap, RenderItem* pPlane)
{
    pCmdList->SetComputeRootSignature(mRayRootSignature.Get());

    // TODO : pso 생성해서 넣어주기
    pCmdList->SetPipelineState(mRayIntersectPipelineState.Get());
    
    pCmdList->SetComputeRootDescriptorTable(0,mHeightMapGpuSrv);
    
    // plane 정보 넣어주기
    pCmdList->SetComputeRootShaderResourceView(1, pPlane->Geo->VertexBufferGPU->GetGPUVirtualAddress() );
    pCmdList->SetComputeRootShaderResourceView(2, pPlane->Geo->IndexBufferGPU->GetGPUVirtualAddress() );

    // TODO : 상수 값 넣어주기
    // 버퍼뷰 만들어서 업데이트 해줘야한다.
    pCmdList->SetComputeRootConstantBufferView(3,)
    pCmdList->SetComputeRootConstantBufferView(4,)

    // TODO : UAV 구조적 버퍼 값 넣어주기.
    pCmdList->SetComputeRootUnorderedAccessView(5,mOutputBuffer->GetGPUVirtualAddress());
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_GENERIC_READ));
    
    pCmdList->Dispatch(1,1,1);
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COMMON));

    // TODO : ReadBack 버퍼에 결과 outputBuffer 값을 넣어주어야 한다.
}

void myRay::BuildIntersectPso()
{
    // compute pso for normal mapping
    D3D12_COMPUTE_PIPELINE_STATE_DESC rayIntersectPSO = {};
    rayIntersectPSO.pRootSignature = mRayRootSignature.Get();
    rayIntersectPSO.CS =
        {
        reinterpret_cast<BYTE*>(mRayIntersectShader->GetBufferPointer()),
    mRayIntersectShader->GetBufferSize()
    };
    rayIntersectPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    ThrowIfFailed(mD3dDevice->CreateComputePipelineState(&rayIntersectPSO,IID_PPV_ARGS(&mRayIntersectPipelineState)));
}
    
