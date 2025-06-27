#include "QuadTreeNode.h"
#include "RenderItem.h"


QuadTreeNode::QuadTreeNode(DirectX::XMFLOAT3 pCenter, DirectX::XMFLOAT3 pExtents)
{
    XMStoreFloat3(&mbound.Center,XMLoadFloat3(&pCenter));
    XMStoreFloat3(&mbound.Extents,XMLoadFloat3(&pExtents));
}

QuadTreeNode::QuadTreeNode(DirectX::XMVECTOR pCenter, DirectX::XMVECTOR pExtents)
{
    XMStoreFloat3(&mbound.Center,pCenter);
    XMStoreFloat3(&mbound.Extents,pExtents);
}

QuadTreeNode::~QuadTreeNode()
{
   
}

bool QuadTreeNode::CheckMouseCursurContain(DirectX::BoundingSphere pBS)
{
    return mbound.Contains(pBS)!= DirectX::DISJOINT;
}

bool QuadTreeNode::CheckObjectContain(InstanceRef pIR)
{
    // bound 외부에 있는 것이 아니라면 true
    if(mbound.Contains(pIR.WorldBounds) != DirectX::DISJOINT)
    {
        return true;
    }

    return false;
}

void QuadTreeNode::AddObject(InstanceRef pIR)
{
    // 리스트에 존재하지 않으면 추가
    if(!CheckObjectAlreadyOnList(pIR))
        mObjects.push_back(pIR);
}

bool QuadTreeNode::CheckObjectAlreadyOnList(InstanceRef pIR)
{
    for(InstanceRef ir : mObjects)
    {
        if(pIR == ir)
        {
            return true;
        }
    }

    return false;
}

bool QuadTreeNode::CheckObjectIntersectNodeObjects(InstanceRef pIR)
{
    for(InstanceRef ir : mObjects)
    {
        if(ir.WorldBounds.Contains(pIR.WorldBounds) != DirectX::DISJOINT)
        {
            return true;
        }
    }
    return false;
}

std::vector<InstanceRef> QuadTreeNode::GetIntersectedObject(DirectX::XMFLOAT3 WorldPos, float pRange)
{
    std::vector<InstanceRef> vIR;
    
    DirectX::BoundingSphere bs;
    bs.Center = WorldPos;
    bs.Radius = pRange;
    
    for(InstanceRef ir : mObjects)
    {
        if(ir.WorldBounds.Contains(bs) !=DirectX::DISJOINT)
        {
            vIR.push_back(ir);
        }
    }

    return vIR;
}

void QuadTreeNode::RemoveInstance(UINT instanceID)
{
    auto it = std::find_if(mObjects.begin(), mObjects.end(),
           [&](const InstanceRef& ref) {
               return ref.InstanceID == instanceID;
           });
        
    if (it != mObjects.end()) {
        // Swap-and-Pop 적용
        *it = mObjects.back();
        mObjects.pop_back();
    }
}
