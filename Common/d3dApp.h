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
#include "QuadTreeNode.h"

class ShadowMap;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

const UINT Descriptors_Per_Frame =30;

enum class RenderType
{
	Opaque = 0,
	OpaqueTri = 1,
	Sky = 2,
	Debug = 3,
	Count
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
	
	// void UpdateObjectCBs(const GameTimer& gt);
	// void UpdateMaterialCBs(const GameTimer& gt);
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
	
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	// Update Scene by moving Camera
	void UpdateScene(float dt);

	void InitImGui();

	void CreateShaderResourceView(myTexture* pInTexture, ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset, UINT pDescriptorSize, bool bIsTypeless = false);

	void BuildPostProcessRootSignature();

	void BuildRayRootSignature();
	
	void UpdateHeightMap(myTexture* pTexture);

	RenderItem* GetPlane() const;

	INT GetCurrentHeightMapOffset() const { return mMaxSrvCount + mMaxNormalCount + mHeightMapBuffer.mCurrentUsingIndex;}

	void InitRay();

	void CalcMouseRay();

	void CalcHeightMod();

	void OnMouseInput(const GameTimer& gt);

	void BuildShapeGeometry();

	void SaveMapFile();

	void DrawSceneToShadowMap();

	void AnimateLights(const GameTimer& gt);

	void UpdateShadowPassCB(const GameTimer& gt);

	void UpdateShadowTransform(const GameTimer& gt);

	void UpdateInstanceBuffer(const GameTimer& gt);

	void UpdateMaterialBuffer(const GameTimer& gt);

	void InitializeQuadTree(UINT width, UINT height);

	void CreateRenderItem(RenderItem* pRI, XMFLOAT3 worldPos,XMFLOAT3 worldScale,XMFLOAT3 worldRot);

	void EraseRenderItem(XMFLOAT3 worldPos, float pRange);
	
	void UpdateObjectCursur(const GameTimer& gt);
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
	PassConstants mShadowPassCB;

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

	UINT mShadowMapHeapIndex = 0;
	
	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;
	
	std::wstring mHeightMapDirectory;
	
	std::unique_ptr<myTexture> mNewHeightMap = nullptr;
	
	bool bIsHeightMapDirty = false;

	UINT mCurrentUAVDescriptorOffset = Descriptors_Per_Frame;

	std::unique_ptr<ShadowMap> mShadowMap;

	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	BoundingFrustum mCamFrustum;

	bool mFrustumCullingEnabled = true;

	bool bIsDebugging = false;

	bool bIsMouseWidgetHovering = false;

	std::vector<QuadTreeNode*> mQuadTree;

	RenderItem* mBox;

	RenderItem* mSphere;

	const float createCooldown = 0.01f;

	float creationTimer = 0.0f;
};

