#pragma once
#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Material;
struct MeshGeometry;

enum class RenderType
{
    Opaque = 0,
    OpaqueTri = 1,
    Sky = 2,
    Debug = 3,
    Count
};

class RenderItem;

class ObjectManager
{
public:
    ObjectManager();
    ~ObjectManager();

    void InitializeRenderItems(ID3D12Device* md3dDevice);

    std::unordered_map<std::string, std::unique_ptr<Material>>& GetMaterials() {return mMaterials;}

    std::unordered_map<std::string,std::unique_ptr<MeshGeometry>>& GetGeometries() {return mGeometries;}

    std::vector<RenderItem*> GetRenderItemByRenderType(RenderType pRenderType) { return mRitemLayer[(UINT)pRenderType];}

    std::vector<std::unique_ptr<RenderItem>>& GetAllRenderItems() { return mAllRitems;}

    RenderItem* GetPlane() {return mPlane;}

    RenderItem* GetBox() {return mBox;}

    RenderItem* GetSphere() {return mSphere;}
    // void BuildRenderItem(const ID3D12Device* md3dDevice, RenderType pRenderType);

    void BuildPlaneGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList,float width = 10, float depth = 10, uint32_t m = 2, uint32_t n = 2);
    
    void BuildShapeGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList);
private:
    // List of all the render items.
    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem*> mRitemLayer[(int)RenderType::Count];

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    
    RenderItem* mSphere = nullptr;

    RenderItem* mBox = nullptr;

    RenderItem* mPlane = nullptr;

};
