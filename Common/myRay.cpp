#include "myRay.h"

#include "d3dApp.h"
#include "d3dUtil.h"

myRay::myRay(ID3D12Device* pDevice)
{
    mD3dDevice = pDevice;
    bufferByteSize = mPickingResultNum * sizeof(PickingResult);

    rayCB = std::make_unique<UploadBuffer<Ray>>(pDevice,1,true);
    planeInfoCB = std::make_unique<UploadBuffer<PlaneInfo>>(pDevice,1,true);

    BuildBuffers();

    BuildRootSignature();
}

myRay::~myRay()
{
    mReadBackBuffer->Unmap(0,nullptr);
}

void myRay::BuildBuffers()
{
    ThrowIfFailed(mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferByteSize,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mOutputBuffer)));

    ThrowIfFailed(mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferByteSize),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mReadBackBuffer)));

    ThrowIfFailed(mReadBackBuffer->Map(0,nullptr,reinterpret_cast<void**>(&mIntersectMappedData)));
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

    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE srvTable1;
    srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    
    CD3DX12_DESCRIPTOR_RANGE srvTable2;
    srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    
    CD3DX12_ROOT_PARAMETER slotRootParameter[6];
    // height Map srv
    slotRootParameter[0].InitAsDescriptorTable(1,&srvTable);
    // vertex buffer
    slotRootParameter[1].InitAsDescriptorTable(1,&srvTable1);
    
    // index buffer
    slotRootParameter[2].InitAsDescriptorTable(1,&srvTable2);

    // ray info
    slotRootParameter[3].InitAsConstantBufferView(0);
    // plane info
    slotRootParameter[4].InitAsConstantBufferView(1);
    // uav
    slotRootParameter[5].InitAsDescriptorTable(1,&uavTable);

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

// void myRay::BuildDescriptorHeaps()
// {
//     D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
//     
//     heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
//     heapDesc.NumDescriptors = 1;
//     heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//     
//     ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mUavDescriptorHeap)));
//
// }

void myRay::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize, UINT vertexCount, UINT indexCount)
{
    mIntersectCpuUav = hCpuDescriptor;
    mIntersectGpuUav = hGpuDescriptor;

    mVertexCpuSrv = hCpuDescriptor.Offset(1,descriptorSize);
    mVertexGpuSrv = hGpuDescriptor.Offset(1,descriptorSize);

    mIndexCpuSrv = hCpuDescriptor.Offset(1,descriptorSize);
    mIndexGpuSrv = hGpuDescriptor.Offset(1,descriptorSize);
    
    BuildDescriptors(vertexCount,indexCount);
}

void myRay::BuildDescriptors(UINT vertexCount, UINT indexCount)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer = {0,1,sizeof(PickingResult)};
        
    mD3dDevice->CreateUnorderedAccessView(
        mOutputBuffer.Get(), nullptr, &uavDesc, mIntersectCpuUav);

    // Create Vertex, Index Shader resource view
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1{};
    srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc1.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc1.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc1.Buffer.FirstElement = 0;
    srvDesc1.Buffer.StructureByteStride = sizeof(Vertex); 
    srvDesc1.Buffer.NumElements = vertexCount;
    mD3dDevice->CreateShaderResourceView(mVertexBuffer.Get(),&srvDesc1,mVertexCpuSrv);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc2.Buffer.FirstElement = 0;
    srvDesc2.Buffer.StructureByteStride = sizeof(uint32_t); 
    srvDesc2.Buffer.NumElements = indexCount;
    mD3dDevice->CreateShaderResourceView(mIndexBuffer.Get(),&srvDesc2,mIndexCpuSrv);
}

void myRay::InitBuffer(ID3D12GraphicsCommandList* pCmdList, ID3D12CommandQueue* pCommandQueue, ID3D12CommandAllocator* pCommandAllocator)
{
    ID3D12Resource* uploadBuffer = nullptr;
    
    // 업로드 버퍼 생성
    mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    PickingResult initData ={};
    initData.Distance = 1000;

    void* mappedData = nullptr;
    D3D12_RANGE readRange = {0,0};
    uploadBuffer->Map(0,nullptr,&mappedData);
    memcpy(mappedData, &initData,sizeof(PickingResult));
    uploadBuffer->Unmap(0,nullptr);

    // pCmdList->Reset(pCommandAllocator,nullptr);

    pCmdList->CopyBufferRegion(
        mOutputBuffer.Get(),0,
        uploadBuffer,0,
        sizeof(PickingResult));

    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    // pCmdList->Close();
    // ID3D12CommandList* cmdLists[]={pCmdList};
    // pCommandQueue->ExecuteCommandLists(1,cmdLists);
    //
    // // Wait For Gpu
    // Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence;
    // UINT64 fenceValue = 1;
    // mD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence));
    //
    // HANDLE fenceEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);
    //
    //     
    // pCommandQueue->Signal(uploadFence.Get(),fenceValue);
    //
    //
    // // GPU가 펜스 값에 도달했는지 확인
    // if (uploadFence->GetCompletedValue() < fenceValue) {
    //     uploadFence->SetEventOnCompletion(fenceValue, fenceEvent);
    //     WaitForSingleObject(fenceEvent, INFINITE);
    // }
    
}

void myRay::SetNewHeightMap(CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapSrv)
{
    mHeightMapGpuSrv = pHeightMapSrv;
}


void myRay::Execute(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* pHeightMap, RenderItem* pPlane)
{
    pCmdList->SetComputeRootSignature(mRayRootSignature.Get());

    pCmdList->SetPipelineState(mRayIntersectPipelineState.Get());

    // set descriptor heap
    
    pCmdList->SetComputeRootDescriptorTable(0,mHeightMapGpuSrv);

    
    // plane 정보 넣어주기
    pCmdList->SetComputeRootDescriptorTable(1,mVertexGpuSrv);
    pCmdList->SetComputeRootDescriptorTable(2,mIndexGpuSrv);

    // 상수 값 넣어주기
    pCmdList->SetComputeRootConstantBufferView(3,rayCB->Resource()->GetGPUVirtualAddress());
    pCmdList->SetComputeRootConstantBufferView(4,planeInfoCB->Resource()->GetGPUVirtualAddress());

    pCmdList->SetComputeRootDescriptorTable(5,mIntersectGpuUav);
    // pCmdList->SetComputeRootUnorderedAccessView(5,mOutputBuffer->GetGPUVirtualAddress());
    
    // pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mVertexBuffer.Get(),
    //             D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    //
    // pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mIndexBuffer.Get(),
    //         D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_GENERIC_READ));

    // const UINT ThreadGroups = (triangleNums + 63)/64;
    const UINT ThreadGroups = 1;
    pCmdList->Dispatch(ThreadGroups,1,1);
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COMMON));
    
    // pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mVertexBuffer.Get(),
    //             D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON));
    //
    // pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mIndexBuffer.Get(),
    //         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON));
    // ReadBack 버퍼에 결과 outputBuffer 값을 넣어준다 한다.
    
    pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

    pCmdList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());

    pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

   
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

void myRay::UpdateRayCBs(UINT pWidth, UINT pHeight, UINT pNumTriangles)
{
    Ray tmpRay;
    tmpRay.gRayDir = mRayDir;
    tmpRay.gRayOrigin = mRayOrigin;

    PlaneInfo tmpPlaneInfo;
    tmpPlaneInfo.Height = pHeight;
    tmpPlaneInfo.Width = pWidth;
    tmpPlaneInfo.NumTriangles = pNumTriangles;

    triangleNums = pNumTriangles;
    
    rayCB->CopyData(0,tmpRay);
    planeInfoCB->CopyData(0,tmpPlaneInfo);
}


    
