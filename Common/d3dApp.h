//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "wrl/client.h"
#include "d3dUtil.h"
#include "GameTimer.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include "GeometryGenerator.h"
#include "FrameResource.h"
#include "Camera.h"


#include "myImGui.h"
#include "myTexture.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

const UINT Descriptors_Per_Frame =30;

enum class RenderType
{
	Opaque =0,
	Sky = 1,
	Count
};

struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

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

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
	
};

struct HeightMapBuffer
{
	int mMaxHeightMapCount=10;
	int mCurrentUsingIndex=-1;
	int numDirty = 0;
	std::vector<std::unique_ptr<myTexture>> mTextureBuffer;

	HeightMapBuffer()
	{
		for(int i=0;i<mMaxHeightMapCount;++i)
		{
			mTextureBuffer.push_back(nullptr);
		}
	}
	void UpdateHeightMap(std::unique_ptr<myTexture> pInTex)
	{
		// mAvailableIndex쓸일 없으면 이 로컬변수로 대체 
		int availableIndex = (mCurrentUsingIndex+1)%mMaxHeightMapCount;
		mTextureBuffer[availableIndex] = std::move(pInTex);
		mCurrentUsingIndex = (mCurrentUsingIndex+1)%mMaxHeightMapCount;
		numDirty = 1;
	}
	myTexture* GetCurrentUsingHeightmap() const { return mTextureBuffer[mCurrentUsingIndex].get();}

	
};

class myRay;
class NormalMapGenerator;

class D3DApp
{
public:

    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:

    static D3DApp* GetApp();
    
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

	int Run();
 
    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize(); 
	virtual void Update(const GameTimer& gt);
    virtual void Draw(const GameTimer& gt);

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);

protected:

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
    void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void CalculateFrameStats();

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

private:
	void OnKeyboardInput(const GameTimer& gt);
	
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildPlaneGeometry(float width = 10, float depth = 10, uint32_t m = 2, uint32_t n = 2);
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList,const std::vector<RenderItem*>& ritems);
	
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	// Update Scene by moving Camera
	void UpdateScene(float dt);

	void InitImGui();

	void CreateShaderResourceView(myTexture* pInTexture, ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset, UINT pDescriptorSize);

	void BuildPostProcessRootSignature();

	void BuildRayRootSignature();
	
	void UpdateHeightMap(myTexture* pTexture);

	RenderItem* GetPlane() const;

	INT GetCurrentHeightMapOffset() const { return mMaxSrvCount + mMaxNormalCount + mHeightMapBuffer.mCurrentUsingIndex;}

	void InitRay();

	void CalcMouseRay();

	void CalcHeightMod();

	void OnMouseInput();

	void BuildSphereGeometry();
protected:

    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr; // application instance handle
    HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
    bool      mFullscreenState = false;// fullscreen enabled

	// Set true to use 4X MSAA (?.1.8).  The default is false.
    bool      m4xMsaaState = false;    // 4X MSAA enabled
    UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	// Used to keep track of the �delta-time?and game time (?.4).
	GameTimer mTimer;
	
    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;
	
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;


    D3D12_VIEWPORT mScreenViewport; 
    D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;



private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	// UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	
	// RootSignature for normal Compute Shader
	ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;

	// RootSignature for Ray Compute Shader
	ComPtr<ID3D12RootSignature> mRayRootSignature = nullptr;
	
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	// Use this count instead of indivisual input
	INT mSrvDescriptorHeapObjCount=0;

	ComPtr<ID3D12DescriptorHeap> mHeightMapDescriptorHeap = nullptr;
	INT mHeightMapDescriptorHeapObjCount=0;

	
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	// std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, std::unique_ptr<myTexture>> myTextures;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// Cache render items of interest.
	RenderItem* mSkullRitem = nullptr;
	RenderItem* mReflectedSkullRitem = nullptr;
	RenderItem* mShadowedSkullRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderType::Count];

	PassConstants mMainPassCB;
	PassConstants mReflectedPassCB;

	XMFLOAT3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	// float mTheta = 1.24f*XM_PI;
	// float mPhi = 0.42f*XM_PI;
	// float mRadius = 12.0f;

	POINT mLastMousePos;

	Camera mCam;
	
	const float mCameraSpeed = 50.0f;

	// Vector3 for Mouse Position raytraced on plane
	XMFLOAT3 mMousePosOnPlane = {0.0f,0.0f,0.0f};

	// Srv Descriptor Heap for ImGui textures
	ComPtr<ID3D12DescriptorHeap> mImGuiSrvDescriptorHeap = nullptr;

	myImGui* mImGui = nullptr;

	myRay* mMouseRay = nullptr;

	UINT mTextureIndex=1;

	bool mIsWireFrameMode = false;

	NormalMapGenerator* mNormalMapGenerator = nullptr;

	ID3D12Resource* mNormalMap = nullptr;

	// height map's descriptor heap index for ring buffer
	HeightMapBuffer mHeightMapBuffer;

	// 20 shader textures
	UINT mMaxSrvCount= 20;
	
	// srv 1개 uav 1개
	
	UINT mMaxNormalCount = 2;

	std::wstring mHeightMapDirectory;
	
	std::unique_ptr<myTexture> mNewHeightMap = nullptr;
	
	bool bIsHeightMapDirty = false;

	UINT mCurrentUAVDescriptorOffset = Descriptors_Per_Frame;
};

