#pragma once
#include <DirectXCollision.h>
#include <vector>
#include <wrl/client.h>

struct InstanceData;
class RenderItem;

struct InstanceRef
{
    RenderItem* ParentItem;  // 소속 RenderItem 포인터
    DirectX::BoundingBox WorldBounds; // 월드 공간 바운딩 박스 (계산된 값)
    UINT InstanceID;

    bool operator==(InstanceRef rhs)
    {
        if(this->ParentItem == rhs.ParentItem&&
            this->InstanceID == rhs.InstanceID)
            return true;

        return false;
    }
};


class QuadTreeNode
{
public:
    QuadTreeNode(DirectX::XMFLOAT3 pCenter, DirectX::XMFLOAT3 pExtents);
    QuadTreeNode(DirectX::XMVECTOR pCenter, DirectX::XMVECTOR pExtents);

    ~QuadTreeNode();

    bool CheckMouseCursurContain(DirectX::BoundingSphere pBS);
    
    // bound에 존재하는지 check
    bool CheckObjectContain(InstanceRef pIR);

    // object가 이미 존재하는지 확인한 후 add
    void AddObject(InstanceRef pIR);

    // object가 이미 존재하는지 check
    bool CheckObjectAlreadyOnList(InstanceRef pIR);

    // Node 내부의 object들과 겹치는지 확인
    bool CheckObjectIntersectNodeObjects(InstanceRef pIR);

    std::vector<InstanceRef> GetIntersectedObject(DirectX::XMFLOAT3 WorldPos, float pRange);

    void RemoveInstance(UINT instanceID);
    
    DirectX::BoundingBox GetBound() { return mbound; }
    
private:
    std::vector<InstanceRef> mObjects;
    DirectX::BoundingBox mbound;
};
