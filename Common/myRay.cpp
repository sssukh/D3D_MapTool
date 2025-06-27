#include "myRay.h"

#include "d3dApp.h"
#include "d3dUtil.h"

myRay::myRay(ID3D12Device* pDevice)
{
    mD3dDevice = pDevice;
    bufferByteSize = mPickingResultNum * sizeof(PickingResult);

    rayCB = std::make_unique<UploadBuffer<Ray>>(pDevice,1,true);
    planeInfoCB = std::make_unique<UploadBuffer<PlaneInfo>>(pDevice,1,true);
    modHeightCB = std::make_unique<UploadBuffer<HeightModifyingInfo>>(pDevice,1,true);

    BuildBuffers();

    BuildRootSignature();

    BuildModRootSignature();
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
        IID_PPV_ARGS(mRayIntersectSignature.GetAddressOf())));
}

void myRay::BuildModRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsDescriptorTable(1,&srvTable);

    slotRootParameter[1].InitAsDescriptorTable(1,&uavTable);

    slotRootParameter[2].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
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
        IID_PPV_ARGS(mRayModRootSignature.GetAddressOf())));
}



void myRay::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize, UINT vertexCount, UINT indexCount)
{
    mIntersectCpuUav = hCpuDescriptor;
    mIntersectGpuUav = hGpuDescriptor;

    mVertexCpuSrv = hCpuDescriptor.Offset(1,descriptorSize);
    mVertexGpuSrv = hGpuDescriptor.Offset(1,descriptorSize);

    mIndexCpuSrv = hCpuDescriptor.Offset(1,descriptorSize);
    mIndexGpuSrv = hGpuDescriptor.Offset(1,descriptorSize);

    mModCpuUav = hCpuDescriptor.Offset(1,descriptorSize);
    mModGpuUav = hGpuDescriptor.Offset(1,descriptorSize);
    
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

    D3D12_UNORDERED_ACCESS_VIEW_DESC modUavDesc = {};

    modUavDesc.Format = mTexFormat;
    modUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    modUavDesc.Texture2D.MipSlice = 0;
    
    mD3dDevice->CreateUnorderedAccessView(mModifiedHeight.Get(), nullptr, &modUavDesc, mModCpuUav);
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

    
    
}

void myRay::SetNewHeightMap(CD3DX12_GPU_DESCRIPTOR_HANDLE pHeightMapSrv, ID3D12Resource* pHeightMap)
{
    mHeightMapGpuSrv = pHeightMapSrv;

    D3D12_RESOURCE_DESC heightDesc = pHeightMap->GetDesc();
    
    mTexWidth = heightDesc.Width;
    mTexHeight = heightDesc.Height;
    mTexFormat  = heightDesc.Format;
}



void myRay::ExecuteRayIntersectTriangle(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* pHeightMap)
{
    pCmdList->SetComputeRootSignature(mRayIntersectSignature.Get());

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
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_GENERIC_READ));

    // const UINT ThreadGroups = (triangleNums + 63)/64;
    const UINT ThreadGroups = 1;
    pCmdList->Dispatch(ThreadGroups,1,1);
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COMMON));
    
    // ReadBack 버퍼에 결과 outputBuffer 값을 넣어준다 한다.
    
    pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

    pCmdList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());

    pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

   
}

void myRay::ExecuteRayMod(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* pHeightMap)
{
    pCmdList->SetComputeRootSignature(mRayModRootSignature.Get());

    pCmdList->SetPipelineState(mRayModPipelineState.Get());

    // TODO : heightMap의 srv를 프레임당 하나씩 두었는데 사용하는 것은 하나 뿐이다. 수정요함. (프레임당 하나로 쓰던, 하나로 고정해서 쓰던 해야한다.) 
    pCmdList->SetComputeRootDescriptorTable(0,mHeightMapGpuSrv);

    pCmdList->SetComputeRootDescriptorTable(1,mModGpuUav);

    pCmdList->SetComputeRootConstantBufferView(2,modHeightCB->Resource()->GetGPUVirtualAddress());

    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_GENERIC_READ));

    D3D12_RESOURCE_DESC heightDesc = pHeightMap->GetDesc();

    UINT numGroupsX = (UINT)ceilf(heightDesc.Width/8.0f);
    UINT numGroupsY = (UINT)ceilf(heightDesc.Height/8.0f);
    
    pCmdList->Dispatch(numGroupsX,numGroupsY,1);

    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
            D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_STATE_COPY_DEST));
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mModifiedHeight.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE));
    
    pCmdList->CopyResource(pHeightMap,mModifiedHeight.Get());
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(pHeightMap,
          D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COMMON));
    
    pCmdList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(mModifiedHeight.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void myRay::BuildIntersectPso()
{
    // compute pso for computing intersect
    D3D12_COMPUTE_PIPELINE_STATE_DESC rayIntersectPSO = {};
    rayIntersectPSO.pRootSignature = mRayIntersectSignature.Get();
    rayIntersectPSO.CS =
        {
        reinterpret_cast<BYTE*>(mRayIntersectShader->GetBufferPointer()),
    mRayIntersectShader->GetBufferSize()
    };
    rayIntersectPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    ThrowIfFailed(mD3dDevice->CreateComputePipelineState(&rayIntersectPSO,IID_PPV_ARGS(&mRayIntersectPipelineState)));
}

void myRay::BuildModPso()
{
    // compute pso for computing height map modifying
    D3D12_COMPUTE_PIPELINE_STATE_DESC rayModPSO = {};
    rayModPSO.pRootSignature = mRayModRootSignature.Get();
    rayModPSO.CS =
    {
        reinterpret_cast<BYTE*>(mRayModShader->GetBufferPointer()),
    mRayModShader->GetBufferSize()
    };
    rayModPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    ThrowIfFailed(mD3dDevice->CreateComputePipelineState(&rayModPSO,IID_PPV_ARGS(&mRayModPipelineState)));
}

void myRay::BuildModResource()
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mTexWidth;
    texDesc.Height = mTexHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mTexFormat;
    // texDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    
    
    ThrowIfFailed(mD3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&mModifiedHeight)));
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

    HeightModifyingInfo tmpModInfo;
    tmpModInfo.IntersectPos = GetIntersectionPos();
    tmpModInfo.ModifingStrength = mModifingStrength;
    tmpModInfo.IntersectRange = mIntersectRange;
    tmpModInfo.MaxStrengthRange = mMaxStrengthRange;

    
    modHeightCB->CopyData(0,tmpModInfo);
    rayCB->CopyData(0,tmpRay);
    planeInfoCB->CopyData(0,tmpPlaneInfo);
}

void myRay::ChangeRayMode(UINT param)
{
    switch(param)
    {
    case 1:
        mRayMode = HeightModification;
        break;
    case 2:
        mRayMode = ObjectPlacing;
        break;
    case 3:
        mRayMode = ObjectErase;
        break;
    default:
        break;
    }
}


    
