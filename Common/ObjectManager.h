#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Vertex;
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

    RenderItem* GetObj() {return mObj;}
    
    // void BuildRenderItem(const ID3D12Device* md3dDevice, RenderType pRenderType);

    void BuildPlaneGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList,float width = 10, float depth = 10, uint32_t m = 2, uint32_t n = 2);
    
    void BuildShapeGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList);

    bool LoadOBJ(const std::wstring& filename, 
             std::vector<Vertex>& outVertices, 
             std::vector<uint32_t>& outIndices);

    bool CreateBufferAndUpload(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList,std::vector<Vertex>& outVertices, 
             std::vector<uint32_t>& outIndices);

    void BuildRenderItem(ID3D12Device* md3dDevice);

    void CreateOBJ(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList,const std::wstring& filename);

    void ModifyScale(float var);

    DirectX::XMFLOAT3 GetScale() const {return mObjScale;}

    void SetScale(const float x, const float y, const float z) { mObjScale.x =x; mObjScale.y = y; mObjScale.z = z;}

    void SetMaterialIndex(UINT pMat) {mMaterialIndex = pMat;}

    UINT GetMaterialIndex() const {return mMaterialIndex;}
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

    RenderItem* mObj = nullptr;

    DirectX::XMFLOAT3 mObjScale = DirectX::XMFLOAT3(1.0f,1.0f,1.0f);

    UINT mMaterialIndex=0;
};
