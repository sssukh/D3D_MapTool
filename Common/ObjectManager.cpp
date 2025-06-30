#include "ObjectManager.h"

#include <d3dcommon.h>

#include "MathHelper.h"
#include "RenderItem.h"
#include "GeometryGenerator.h"
#include "d3dUtil.h"
#include "FrameResource.h"

using namespace DirectX;

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
}

void ObjectManager::InitializeRenderItems(ID3D12Device* md3dDevice)
{
    
	auto planeRitem =std::make_unique<RenderItem>(md3dDevice,1);
	planeRitem->World = MathHelper::Identity4x4();
	planeRitem->TexTransform = MathHelper::Identity4x4();
	planeRitem->ObjCBIndex = 0;
	planeRitem->Mat = mMaterials["planeMat"].get();
	planeRitem->Geo = mGeometries["planeGeo"].get();
	planeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	planeRitem->IndexCount = planeRitem->Geo->DrawArgs["plane"].IndexCount;
	planeRitem->StartIndexLocation = planeRitem->Geo->DrawArgs["plane"].StartIndexLocation;
	planeRitem->BaseVertexLocation = planeRitem->Geo->DrawArgs["plane"].BaseVertexLocation;
	planeRitem->Bounds = planeRitem->Geo->DrawArgs["plane"].Bounds;
	
	// planeRitem->SetUsingBB(true);
	// planeRitem->Instances.resize(1);

	InstanceData planeID;
	
	XMStoreFloat4x4(&planeID.World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&planeID.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	planeID.MaterialIndex = 0 % mMaterials.size();

	planeRitem->AddInstance(planeID);

	mPlane = planeRitem.get();
	
	mRitemLayer[(int)RenderType::Opaque].push_back(planeRitem.get());
	mAllRitems.push_back(std::move(planeRitem));

	// build sky sphere
	auto skyRitem = std::make_unique<RenderItem>(md3dDevice,1);
	DirectX::XMStoreFloat4x4(&skyRitem->World, DirectX::XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = 1;
	skyRitem->Mat = mMaterials["skyMat"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	skyRitem->Bounds = skyRitem->Geo->DrawArgs["sphere"].Bounds;
	// skyRitem->SetUsingBB(true);

	// skyRitem->Instances.resize(1);

	InstanceData skyID;
	
	XMStoreFloat4x4(&skyID.World, DirectX::XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	XMStoreFloat4x4(&skyID.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	skyID.MaterialIndex = 1 % mMaterials.size();

	skyRitem->AddInstance(skyID);
	
	mRitemLayer[(int)RenderType::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	// sphere
	auto sphereRitem = std::make_unique<RenderItem>(md3dDevice,1);
	DirectX::XMStoreFloat4x4(&sphereRitem->World, DirectX::XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	sphereRitem->TexTransform = MathHelper::Identity4x4();
	sphereRitem->ObjCBIndex = 4;
	sphereRitem->Mat = mMaterials["red"].get();
	sphereRitem->Geo = mGeometries["shapeGeo"].get();
	sphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	sphereRitem->Bounds = sphereRitem->Geo->DrawArgs["sphere"].Bounds;
	// skyRitem->SetUsingBB(true);

	// skyRitem->Instances.resize(1);

	InstanceData sphereID;
	
	XMStoreFloat4x4(&sphereID.World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&sphereID.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	sphereID.MaterialIndex = 4 % mMaterials.size();

	sphereRitem->AddInstance(sphereID);

	mSphere = sphereRitem.get();
	
	mRitemLayer[(int)RenderType::OpaqueTri].push_back(sphereRitem.get());
	mAllRitems.push_back(std::move(sphereRitem));
	

	auto quadRitem = std::make_unique<RenderItem>(md3dDevice,1);
	quadRitem->World = MathHelper::Identity4x4();
	quadRitem->TexTransform = MathHelper::Identity4x4();
	quadRitem->ObjCBIndex = 2;
	quadRitem->Mat = mMaterials["bricks"].get();
	quadRitem->Geo = mGeometries["shapeGeo"].get();
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->Bounds = quadRitem->Geo->DrawArgs["quad"].Bounds;
	
	// quadRitem->Instances.resize(1);

	InstanceData quadID;
	
	XMStoreFloat4x4(&quadID.World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&quadID.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	quadID.MaterialIndex = 2 % mMaterials.size();

	quadRitem->AddInstance(quadID);
	
	mRitemLayer[(int)RenderType::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	auto boxRitem = std::make_unique<RenderItem>(md3dDevice,1000);
	DirectX::XMStoreFloat4x4(&boxRitem->World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f)* DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	DirectX::XMStoreFloat4x4(&boxRitem->TexTransform, DirectX::XMMatrixScaling(1.0f, 0.5f, 1.0f));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = mMaterials["checkboard"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->Bounds = boxRitem->Geo->DrawArgs["box"].Bounds;
	
	boxRitem->SetUsingBB(true);

	// boxRitem->Instances.resize(1);

	InstanceData boxID;
	
	XMStoreFloat4x4(&boxID.World, DirectX::XMMatrixTranslation(0.0f, 50.0f, 0.0f));
	XMStoreFloat4x4(&boxID.TexTransform, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxID.MaterialIndex = 3 % mMaterials.size();

	boxRitem->AddInstance(boxID);
	
	mBox = boxRitem.get();
	mRitemLayer[(int)RenderType::OpaqueTri].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));
}

void ObjectManager::BuildPlaneGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList, float width, float depth, uint32_t m, uint32_t n)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData plane = geoGen.CreateGrid(width,depth,m,n);

	DirectX::XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	DirectX::XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	DirectX::XMVECTOR vPlaneMin = XMLoadFloat3(&vMinf3);
	DirectX::XMVECTOR vPlaneMax = XMLoadFloat3(&vMaxf3);
	
	std::vector<Vertex> vertices(plane.Vertices.size());
	for(int i=0;i<vertices.size();++i)
	{
		vertices[i].Pos = plane.Vertices[i].Position;
		vertices[i].Normal = plane.Vertices[i].Normal;
		vertices[i].TexC = plane.Vertices[i].TexC;

		DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vPlaneMax = DirectX::XMVectorMax(vPlaneMax, P);
		vPlaneMin = DirectX::XMVectorMin(vPlaneMin, P);
	}

	DirectX::BoundingBox planeBounds;
	XMStoreFloat3(&planeBounds.Center, 0.5f*(vPlaneMin + vPlaneMax));
	XMStoreFloat3(&planeBounds.Extents, 0.5f*(vPlaneMax - vPlaneMin));

	UINT indexCount;

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "planeGeo";
	
	
	std::vector<std::uint32_t> indices = plane.Indices32;
	indexCount = (UINT)indices.size();
	
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(),vertices.data(),vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(),indices.data(),ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,mCommandList
		,vertices.data(),vbByteSize,geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,mCommandList
		,indices.data(),ibByteSize,geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;
	
	
	
	SubmeshGeometry submesh;
	submesh.IndexCount = indexCount;
	submesh.VertexCount = plane.Vertices.size();
	submesh.StartIndexLocation =0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = planeBounds;

	geo->DrawArgs["plane"] = submesh;

	mGeometries["planeGeo"] = std::move(geo);
}

void ObjectManager::BuildShapeGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList)
{
	
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	GeometryGenerator::MeshData box = geoGen.CreateBox(10.0f,10.0f,10.0f,2);

	UINT sphereVertexOffset = 0;
	UINT quadVertexOffset = (UINT)sphere.Vertices.size();
	UINT boxVertexOffset = quadVertexOffset + (UINT)quad.Vertices.size();

	UINT sphereIndexOffset =0;
	UINT quadIndexOffset = (UINT)sphere.Indices32.size();
	UINT boxIndexOffset = quadIndexOffset + (UINT)quad.Indices32.size();
	
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	auto totalVertexCount = sphere.Vertices.size() + quad.Vertices.size() + box.Vertices.size();
	
	std::vector<Vertex> vertices(totalVertexCount);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vSphereMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vSphereMax = XMLoadFloat3(&vMaxf3);
	
	UINT k=0;
	for(size_t i =0;i< sphere.Vertices.size(); ++i,++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vSphereMax = XMVectorMax(vSphereMax, P);
		vSphereMin = XMVectorMin(vSphereMin, P);
	}

	BoundingBox sphereBounds;
	XMStoreFloat3(&sphereBounds.Center, 0.5f*(vSphereMin + vSphereMax));
	XMStoreFloat3(&sphereBounds.Extents, 0.5f*(vSphereMax - vSphereMin));

	XMVECTOR vQuadMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vQuadMax = XMLoadFloat3(&vMaxf3);
	
	for(int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vQuadMax = XMVectorMax(vQuadMax, P);
		vQuadMin = XMVectorMin(vQuadMin, P);
	}

	BoundingBox quadBounds;
	XMStoreFloat3(&quadBounds.Center, 0.5f*(vQuadMin + vQuadMax));
	XMStoreFloat3(&quadBounds.Extents, 0.5f*(vQuadMax - vQuadMin));
	
	XMVECTOR vBoxMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vBoxMax = XMLoadFloat3(&vMaxf3);
	
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vBoxMax = XMVectorMax(vBoxMax, P);
		vBoxMin = XMVectorMin(vBoxMin, P);
	}

	BoundingBox boxBounds;
	XMStoreFloat3(&boxBounds.Center, 0.5f*(vBoxMin + vBoxMax));
	XMStoreFloat3(&boxBounds.Extents, 0.5f*(vBoxMax - vBoxMin));
	
	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(),std::begin(sphere.Indices32),std::end(sphere.Indices32));
	indices.insert(indices.end(),std::begin(quad.Indices32),std::end(quad.Indices32));
	indices.insert(indices.end(),std::begin(box.Indices32),std::end(box.Indices32));


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(),vertices.data(),vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(),indices.data(),ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, vertices.data(), vbByteSize,geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList,indices.data(),ibByteSize,geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	sphereSubmesh.Bounds = sphereBounds;
	boxSubmesh.Bounds = boxBounds;
	quadSubmesh.Bounds = quadBounds;
	
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;
	geo->DrawArgs["box"] = boxSubmesh;
	
	mGeometries[geo->Name] = std::move(geo);
}
