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
        if(ir.WorldBounds.Contains(pIR.WorldBounds))
        {
            return true;
        }
    }
    return false;
}
