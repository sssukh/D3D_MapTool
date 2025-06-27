#include "RenderItem.h"

UINT RenderItem::AddInstance(const InstanceData& data)
{
    const UINT newID = NextInstanceID++;
    Instances.push_back(data);
    Instances.back().InstanceID = newID;
    InstanceIDToIndexMap[newID] = static_cast<UINT>(Instances.size() - 1);
    return newID;
}

void RenderItem::RemoveInstanceByID(UINT instanceID)
{
    auto it = InstanceIDToIndexMap.find(instanceID);
    if (it == InstanceIDToIndexMap.end()) return;

    const UINT targetIndex = it->second;
    const UINT lastIndex = static_cast<UINT>(Instances.size() - 1);

    // 1. 마지막 요소와 스왑
    std::swap(Instances[targetIndex], Instances[lastIndex]);

    // 2. 스왑된 요소의 매핑 업데이트
    InstanceIDToIndexMap[Instances[targetIndex].InstanceID] = targetIndex;

    // 3. 마지막 요소 제거
    Instances.pop_back();
    InstanceIDToIndexMap.erase(instanceID);

    // 4. QuadTree 업데이트
    // 이 함수 호출전에 quadtree 정리
    // quadTree->RemoveInstance(instanceID);  // ID 기반 제거
}
