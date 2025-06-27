#pragma once
#include <d3d12.h>

#include "UploadBuffer.h"

struct InstanceData
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex;
    UINT InstanceID;
    UINT InstancePad0;
    UINT InstancePad1;
    UINT InstancePad2;
};

class RenderItem
{
public:
    // RenderItem() = default;
    RenderItem(ID3D12Device* device, UINT maxInstanceCount)
    {
        InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCount, false);
    }

    bool IsUsingBB() { return bUsingBB;}
    void SetUsingBB(bool bUseBB) { bUsingBB = bUseBB; }

    UINT AddInstance(const InstanceData& data);

    void RemoveInstanceByID(UINT instanceID);
public:
    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DirectX::BoundingBox Bounds;
    std::vector<InstanceData> Instances;
	
    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT InstanceCount=0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
	
    std::unique_ptr<UploadBuffer<InstanceData>> InstanceBuffer = nullptr;
private:
    bool bUsingBB = false;

    std::unordered_map<UINT, UINT> InstanceIDToIndexMap;
    UINT NextInstanceID = 0;  // ID 발급기

    
};
