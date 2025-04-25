

#include "d3dApp.h"
#include <WindowsX.h>
#include "pix3.h"
#include "myRay.h"
#include "NormalMapGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

const int gNumFrameResources = 3;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp()
{
    return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
:	mhAppInst(hInstance)
{
    // Only one D3DApp can be constructed.
    assert(mApp == nullptr);
    mApp = this;

	bIsHeightMapDirty = false;
}

D3DApp::~D3DApp()
{
	mImGui->ReleaseImGui();
	if(md3dDevice != nullptr)
		FlushCommandQueue();
}

HINSTANCE D3DApp::AppInst()const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Get4xMsaaState()const
{
    return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
    if(m4xMsaaState != value)
    {
        m4xMsaaState = value;

        // Recreate the swapchain and buffers with new multisample settings.
        CreateSwapChain();
        OnResize();
    }
}

int D3DApp::Run()
{
	MSG msg = {0};
 
	mTimer.Reset();

	while(msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if(PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}
		// Otherwise, do animation/game stuff.
		else
        {	
			mTimer.Tick();

			if( !mAppPaused )
			{
				mImGui->DrawImGui();
				
				CalculateFrameStats();

				
				
				Update(mTimer);

				if(mImGui->DrawTextureOpenWindow(mHeightMapDirectory))
				{
					mNewHeightMap = std::make_unique<myTexture>("heightTex",mHeightMapDirectory);
					if(FAILED(mNewHeightMap->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get(),mCommandQueue.Get(),mCurrFrameResource->CmdListAlloc.Get())))
					{
						MessageBoxA(nullptr,"Can't create height map texture from file","DirectXError",MB_OK | MB_ICONERROR);

					}

					mHeightMapBuffer.UpdateHeightMap(std::move(mNewHeightMap));
					// UpdateHeightMap(mHeightMapBuffer.GetCurrentUsingHeightmap());

					// TODO : Change plane's vertex and index buffer depends on User's setting
					// BuildPlaneGeometry(heightResource.Width,heightResource.Height,heightResource.Width,heightResource.Height);
				}
				
				// temporary
				mImGui->DrawMousePlanePosWindow(mMousePosOnPlane);
				
				mImGui->DrawPlaneTextureListWindow(mTextureIndex, mCurrFrameResourceIndex, Descriptors_Per_Frame);

				mImGui->DrawWireFrameModeWindow(mIsWireFrameMode);
				
                Draw(mTimer);

			}
			else
			{
				Sleep(100);
			}
        }
    }

	

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	PIXLoadLatestWinPixGpuCapturerLibrary();
	if(!InitMainWindow())
		return false;
	

	
	if(!InitDirect3D())
		return false;

	// mouse ray 
	mMouseRay = new myRay(md3dDevice.Get());
	
    // Do the initial resize code.
    OnResize();

	// 명령목록 리셋
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),nullptr));

	// 힙타입(cbv_srv_uav)에 따른 서술자 크기를 저장
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	
	
	// 카메라 위치 설정
	mCam.SetPosition(0.0f,150.0f,-70.0f);
	mCam.Pitch(0.9f);
	
	mNormalMapGenerator = new NormalMapGenerator(md3dDevice.Get());

	// Init ImGui
	InitImGui();
	
	LoadTextures();
	BuildRootSignature();
	BuildPostProcessRootSignature();
	// ImGui추가했음
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();

	// mNormalMapGenerator need to be initialized by SetNewHeightMap() at BuildDescriptorHeaps().
	UINT width = mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource->GetDesc().Width;
	UINT height = mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource->GetDesc().Height;
	
	BuildPlaneGeometry(width,height,10,10);
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	

	// Init normal map generator
	// need to initialized before load Textures and build descriptor heaps
	// it need to be set after heignt map set and descriptor heap builded. 
	
	
	// mNormalMapGenerator = new NormalMapGenerator(md3dDevice.Get(),heightTex.Width,heightTex.Height,heightTex.Format,
	// 	hDescriptor.Offset(texOffset,mCbvSrvUavDescriptorSize)); 
	
	
	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
	
	return true;
}
 
void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));


    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	
	
}

void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
    assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
    mDepthStencilBuffer.Reset();
	
	// Resize the swap chain.
    ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount, 
		mClientWidth, mClientHeight, 
		mBackBufferFormat, 
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;
 
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	
    // Execute the resize commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width    = static_cast<float>(mClientWidth);
	mScreenViewport.Height   = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

    mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	
	mCam.SetLens(0.25f*MathHelper::Pi,AspectRatio(),1.0f,1000.0f);

	mMouseRay->SetViewport(mScreenViewport);
}

void D3DApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	
	
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	// 현재 프레임의 fence가 0이 아니고(프로그램 동작중) mFence(이 프로그램의 fence값)이 현재 프레임의 fence값보다 작으면 
	if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		// mFence가 현재 프레임의 fence값과 같아질 때 까지 대기.
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	
	
	// 리소스 변경
	
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	
	// 지금 사용하는 heightMap으로 업데이트
	UpdateHeightMap(mHeightMapBuffer.GetCurrentUsingHeightmap());
}

void D3DApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	
	// Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
	
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);
	
	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	
    // Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	
    // Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	
	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps),descriptorHeaps);
	
	// need renewal
	if(mNormalMapGenerator->GetDirty()>0)
	{
		mNormalMapGenerator->Execute(mCommandList.Get(),mPostProcessRootSignature.Get(),
			mPSOs["normalMapping"].Get(),mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get());
	}
	
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	// 
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	
	if(mIsWireFrameMode)
		mCommandList->SetPipelineState(mPSOs["opaqueWireFrame"].Get());
	// UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1,passCB->GetGPUVirtualAddress());

	// bind srv Textures
	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	mCommandList->SetGraphicsRootDescriptorTable(3, tex);
	
	
	// bind height map
	CD3DX12_GPU_DESCRIPTOR_HANDLE heightTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	INT heightOffset = GetCurrentHeightMapOffset();
	heightTex.Offset(heightOffset,mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4,heightTex);

	// bind normal map
	CD3DX12_GPU_DESCRIPTOR_HANDLE normalTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	INT normalOffset = mCurrFrameResourceIndex * Descriptors_Per_Frame + mMaxSrvCount;
	normalTex.Offset(normalOffset,mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(5,normalTex);
	
	DrawRenderItems(mCommandList.Get(),mRitemLayer[(int)RenderType::Opaque]);
	

	// ImGui Render
	mImGui->Render(mCommandList.Get());
	
	
    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
	ThrowIfFailed(mCommandList->Close());
	
    // Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	
	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3DApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void D3DApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void D3DApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.25 angle in the scene.
		float yawAngle = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float pitchAngle = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		// Update the camera radius based on input.
		mCam.Pitch(pitchAngle);
		mCam.RotateY(yawAngle);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// ImGui
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(MainWnd(), msg, wParam, lParam))
		return true;
	
	switch( msg )
	{
	// WM_ACTIVATE is sent when the window is activated or deactivated.  
	// We pause the game when the window is deactivated and unpause it 
	// when it becomes active.  
	case WM_ACTIVATE:
		if( LOWORD(wParam) == WA_INACTIVE )
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth  = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if( md3dDevice )
		{
			if( wParam == SIZE_MINIMIZED )
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if( wParam == SIZE_MAXIMIZED )
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if( wParam == SIZE_RESTORED )
			{
				
				// Restoring from minimized state?
				if( mMinimized )
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if( mMaximized )
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if( mResizing )
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing  = true;
		mTimer.Stop();
		return 0;

	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing  = false;
		mTimer.Start();
		OnResize();
		return 0;
 
	// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// The WM_MENUCHAR message is sent when a menu is active and the user presses 
	// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

	// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
		return 0;

	case WM_LBUTTONDOWN:
		
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
    case WM_KEYUP:
        if(wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if((int)wParam == VK_F2)
            Set4xMsaaState(!m4xMsaaState);

        return 0;

	}
	

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = MainWndProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = mhAppInst;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = L"MainWnd";

	if( !RegisterClass(&wc) )
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width  = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), 
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0); 
	if( !mhMainWnd )
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
{
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
}
#endif

	

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	// Fallback to WARP device.
	if(FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render 
    // target formats, so we only need to check quality support.

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
	
#ifdef _DEBUG
    LogAdapters();
#endif

	CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
    // Release the previous swapchain we will be recreating.
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;
    sd.BufferDesc.Height = mClientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = SwapChainBufferCount;
    sd.OutputWindow = mhMainWnd;
    sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
    ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd, 
		mSwapChain.GetAddressOf()));
}

void D3DApp::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
    mCurrentFence++;

    // Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
    if(mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

        // Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.
    
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if( (mTimer.TotalTime() - timeElapsed) >= 1.0f )
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

        wstring fpsStr = to_wstring(fps);
        wstring mspfStr = to_wstring(mspf);

		wstring mouseXPos = to_wstring(mLastMousePos.x);
		wstring mouseYPos = to_wstring(mLastMousePos.y);
		
		
        wstring windowText = mMainWndCaption +
            L"    fps: " + fpsStr +
            L"   mspf: " + mspfStr + L"   x : " + mouseXPos + L"   y : " + mouseYPos;

        SetWindowText(mhMainWnd, windowText.c_str());
		
		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void D3DApp::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while(mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());

        adapterList.push_back(adapter);
        
        ++i;
    }

    for(size_t i = 0; i < adapterList.size(); ++i)
    {
        LogAdapterOutputs(adapterList[i]);
        ReleaseCom(adapterList[i]);
    }
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while(adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);
        
        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());

        LogOutputDisplayModes(output, mBackBufferFormat);

        ReleaseCom(output);

        ++i;
    }
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;

    // Call with nullptr to get list count.
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for(auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";

        ::OutputDebugString(text.c_str());
    }
}

void D3DApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	UpdateScene(dt);
}



void D3DApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void D3DApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			// update and upload to uploadheap
			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void D3DApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCam.GetView();
	XMMATRIX proj = mCam.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCam.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = mCam.GetNearZ();
	mMainPassCB.FarZ = mCam.GetFarZ();
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	// Update current texture index
	mMainPassCB.TextureIndex = mTextureIndex;
	
	// Update ray information

	mMouseRay->SetMatrix(view,proj);
	mMouseRay->SetStartPos(mLastMousePos);

	mMouseRay->UpdateRay();

	XMVECTOR point = XMVectorZero();
	XMVECTOR normal = XMLoadFloat3(&XMFLOAT3(0.0f,1.0f,0.0f));

	XMStoreFloat3(&mMousePosOnPlane,mMouseRay->PlaneLineIntersectVect(point,normal));
	
	mMainPassCB.MouseProjPos = mMousePosOnPlane;
	
	// Main pass stored in index 2
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void D3DApp::LoadTextures()
{
	// TODO : textures should be take place in frame buffers 
	auto myiceTex = std::make_unique<myTexture>("iceTex",L"../../Textures/ice.dds");
	myiceTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());
	
	myTextures[myiceTex->Name] = std::move(myiceTex);

	auto mybricksTex = std::make_unique<myTexture>("bricksTex",L"../../Textures/bricks.dds");
	mybricksTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());

	myTextures[mybricksTex->Name] = std::move(mybricksTex);

	auto mycheckboardTex = std::make_unique<myTexture>("checkboardTex",L"../../Textures/checkboard.dds");
	mycheckboardTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());

	myTextures[mycheckboardTex->Name] = std::move(mycheckboardTex);

	auto mywhiteTex = std::make_unique<myTexture>("whiteTex",L"../../Textures/white1x1.dds");
	mywhiteTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());

	myTextures[mywhiteTex->Name] = std::move(mywhiteTex);

	std::wstring test = mImGui->OpenFileDialog();
	auto myHeightTex = std::make_unique<myTexture>("heightTex",test);

// 	auto myHeightTex = std::make_unique<myTexture>("heightTex",L"../../Textures/gray7.jpg");
	myHeightTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());

	mHeightMapBuffer.UpdateHeightMap(std::move(myHeightTex));
}

void D3DApp:: BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

	CD3DX12_DESCRIPTOR_RANGE heightTable;
	heightTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,4);
	
	CD3DX12_DESCRIPTOR_RANGE normalTable;
	normalTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
	
	
	
	

	
	
	
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[6];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1,&heightTable,D3D12_SHADER_VISIBILITY_DOMAIN);
	slotRootParameter[5].InitAsDescriptorTable(1,&normalTable,D3D12_SHADER_VISIBILITY_DOMAIN);


	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void D3DApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = Descriptors_Per_Frame * gNumFrameResources;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// // 널 SRV 디스크립터 생성
	// D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
	// nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// nullSrvDesc.Texture2D.MipLevels = 1;
	//
	// for (UINT i = 0; i < srvHeapDesc.NumDescriptors; ++i) {
	// 	md3dDevice->CreateShaderResourceView(
	// 		nullptr, 
	// 		&nullSrvDesc, 
	// 		CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),i,mCbvSrvUavDescriptorSize)
	// 	);
	// }

	
	for(int i=0;i<gNumFrameResources;++i)
	{
		CreateShaderResourceView(myTextures["iceTex"].get(),mSrvDescriptorHeap.Get(),i * Descriptors_Per_Frame + mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	}
	++mSrvDescriptorHeapObjCount;
	for(int i=0;i<gNumFrameResources;++i)
	{
		CreateShaderResourceView(myTextures["bricksTex"].get(),mSrvDescriptorHeap.Get(),i * Descriptors_Per_Frame +mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	}
	++mSrvDescriptorHeapObjCount;
	
	for(int i=0;i<gNumFrameResources;++i)
	{
		CreateShaderResourceView(myTextures["checkboardTex"].get(),mSrvDescriptorHeap.Get(),i * Descriptors_Per_Frame +mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	}
		++mSrvDescriptorHeapObjCount;
	
	for(int i=0;i<gNumFrameResources;++i)
	{
		CreateShaderResourceView(myTextures["whiteTex"].get(),mSrvDescriptorHeap.Get(),i * Descriptors_Per_Frame +mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	}
	++mSrvDescriptorHeapObjCount;
	
	
	// 이론상 update하고 draw하기 때문에 여기서 안하고 update에서 처리해줘도 되지 않나?
	// UpdateHeightMap(mHeightMapBuffer.GetCurrentUsingHeightmap());
	
	

	// For ImGui
	D3D12_DESCRIPTOR_HEAP_DESC imGuiSrvHeapDesc = {};
	imGuiSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imGuiSrvHeapDesc.NumDescriptors = 100;
	imGuiSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&imGuiSrvHeapDesc, IID_PPV_ARGS(&mImGuiSrvDescriptorHeap)));

	g_d3dSrvDescHeapAlloc.Create(md3dDevice.Get(),mImGuiSrvDescriptorHeap.Get());
}

void D3DApp::BuildShadersAndInputLayout()
{
	mShaders["VS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["HS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl",nullptr,"HS","hs_5_0");
	mShaders["DS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl",nullptr,"DS","ds_5_0");
	mShaders["normalCS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\NormalMapCS.hlsl",nullptr,"NormalCS","cs_5_0");
	mShaders["rayIntersectCS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\RayIntersectCS.hlsl",nullptr,"IntersectCS","cs_5_0");

	mShaders["PS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl", nullptr, "PS", "ps_5_1");
	
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"NORMAL",0, DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24 ,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};
}

void D3DApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["VS"]->GetBufferPointer()), 
		mShaders["VS"]->GetBufferSize()
	};
	opaquePsoDesc.HS =
	{
		reinterpret_cast<BYTE*>(mShaders["HS"]->GetBufferPointer()),
			mShaders["HS"]->GetBufferSize()
	};
	opaquePsoDesc.DS =
	{
		reinterpret_cast<BYTE*>(mShaders["DS"]->GetBufferPointer()),
			mShaders["DS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["PS"]->GetBufferPointer()),
		mShaders["PS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// same opaque Pso but wireframe mode
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireFramePsoDesc = opaquePsoDesc;
	opaqueWireFramePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireFramePsoDesc, IID_PPV_ARGS(&mPSOs["opaqueWireFrame"])));

	// compute pso for normal mapping
	D3D12_COMPUTE_PIPELINE_STATE_DESC normalPSO = {};
	normalPSO.pRootSignature = mPostProcessRootSignature.Get();
	normalPSO.CS =
		{
			reinterpret_cast<BYTE*>(mShaders["normalCS"]->GetBufferPointer()),
		mShaders["normalCS"]->GetBufferSize()
		};
	normalPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&normalPSO,IID_PPV_ARGS(&mPSOs["normalMapping"])));
	
}

void D3DApp::BuildFrameResources()
{
	for(int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void D3DApp::BuildPlaneGeometry(float width, float depth, uint32_t m, uint32_t n)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData plane = geoGen.CreateGrid(width,depth,m,n);

	std::vector<Vertex> vertices(plane.Vertices.size());
	for(int i=0;i<vertices.size();++i)
	{
		vertices[i].Pos = plane.Vertices[i].Position;
		vertices[i].Normal = plane.Vertices[i].Normal;
		vertices[i].TexC = plane.Vertices[i].TexC;
	}

	UINT indexCount;

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "planeGeo";
	
	if(m*n>(UINT)1<<16)
	{
		std::vector<std::uint32_t> indices = plane.Indices32;
		indexCount = (UINT)indices.size();
		
		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(),vertices.data(),vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(),indices.data(),ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),mCommandList.Get()
			,vertices.data(),vbByteSize,geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),mCommandList.Get()
			,indices.data(),ibByteSize,geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(Vertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R32_UINT;
		geo->IndexBufferByteSize = ibByteSize;
	}
	else
	{
		std::vector<std::uint16_t> indices = plane.GetIndices16();
		indexCount = (UINT)indices.size();

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
		CopyMemory(geo->VertexBufferCPU->GetBufferPointer(),vertices.data(),vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->IndexBufferCPU));
		CopyMemory(geo->IndexBufferCPU->GetBufferPointer(),indices.data(),ibByteSize);

		geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),mCommandList.Get()
			,vertices.data(),vbByteSize,geo->VertexBufferUploader);

		geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),mCommandList.Get()
			,indices.data(),ibByteSize,geo->IndexBufferUploader);

		geo->VertexByteStride = sizeof(Vertex);
		geo->VertexBufferByteSize = vbByteSize;
		geo->IndexFormat = DXGI_FORMAT_R16_UINT;
		geo->IndexBufferByteSize = ibByteSize;
	}
	
	SubmeshGeometry submesh;
	submesh.IndexCount = indexCount;
	submesh.StartIndexLocation =0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["plane"] = submesh;

	mGeometries["planeGeo"] = std::move(geo);
}

void D3DApp::BuildMaterials()
{
	auto planeMat = std::make_unique<Material>();
	planeMat->Name = "planeMat";
	planeMat->MatCBIndex = 0;
	planeMat->DiffuseSrvHeapIndex = 0;
	planeMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	planeMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	planeMat->Roughness = 0.5f;

	// auto bricks = std::make_unique<Material>();
	// bricks->Name = "bricks";
	// bricks->MatCBIndex = 1;
	// bricks->DiffuseSrvHeapIndex = 1;
	// bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	// bricks->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	// bricks->Roughness = 0.5f;
	//
	// auto checkboard = std::make_unique<Material>();
	// checkboard->Name = "checkboard";
	// checkboard->MatCBIndex = 2;
	// checkboard->DiffuseSrvHeapIndex = 2;
	// checkboard->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	// checkboard->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	// checkboard->Roughness = 0.5f;
	//
	// auto ice = std::make_unique<Material>();
	// ice->Name = "ice";
	// ice->MatCBIndex = 3;
	// ice->DiffuseSrvHeapIndex = 0;
	// ice->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	// ice->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	// ice->Roughness = 0.5f;
	
	mMaterials["planeMat"] = std::move(planeMat);
	// mMaterials["bricks"] = std::move(bricks);
	// mMaterials["checkboard"] = std::move(checkboard);
	// mMaterials["ice"] = std::move(ice);

}

void D3DApp::BuildRenderItems()
{
	auto planeRitem = std::make_unique<RenderItem>();
	planeRitem->World = MathHelper::Identity4x4();
	planeRitem->TexTransform = MathHelper::Identity4x4();
	planeRitem->ObjCBIndex = 0;
	planeRitem->Mat = mMaterials["planeMat"].get();
	planeRitem->Geo = mGeometries["planeGeo"].get();
	planeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	planeRitem->IndexCount = planeRitem->Geo->DrawArgs["plane"].IndexCount;
	planeRitem->StartIndexLocation = planeRitem->Geo->DrawArgs["plane"].StartIndexLocation;
	planeRitem->BaseVertexLocation = planeRitem->Geo->DrawArgs["plane"].BaseVertexLocation;
	mRitemLayer[(int)RenderType::Opaque].push_back(planeRitem.get());
	
	mAllRitems.push_back(std::move(planeRitem));
}

void D3DApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for(size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);
		// using button to change plane texture
		// tex.Offset(mTextureIndex, mCbvSrvUavDescriptorSize);


		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}




std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> D3DApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

void D3DApp::UpdateScene(float dt)
{
	if(GetAsyncKeyState('W') & 0x8000)
		mCam.Walk(mCameraSpeed * dt);
	if(GetAsyncKeyState('S') & 0x8000)
		mCam.Walk(-mCameraSpeed*dt);
	if(GetAsyncKeyState('A') & 0x8000)
		mCam.Strafe(-mCameraSpeed*dt);
	if(GetAsyncKeyState('D') & 0x8000)
		mCam.Strafe(mCameraSpeed*dt);

	mCam.UpdateViewMatrix();
}

void D3DApp::InitImGui()
{
	myImGui* tmpImGui = new myImGui(MainWnd(),md3dDevice.Get(),mCommandQueue.Get(),gNumFrameResources,mImGuiSrvDescriptorHeap.Get());

	mImGui = tmpImGui;

	mImGui->InitImGui();
}

void D3DApp::CreateShaderResourceView(myTexture* pInTexture, ID3D12DescriptorHeap* pDescriptorHeap, INT pOffset,
	UINT pDescriptorSize)
{
	if(pInTexture == nullptr)
	{
		// Error
		MessageBoxA(nullptr,"Texture is nullptr","DirectXError",MB_OK | MB_ICONERROR);
		return;
	}
	if(pDescriptorHeap==nullptr)
	{
		// Error
		MessageBoxA(nullptr,"pDescriptorHeap is nullptr","DirectXError",MB_OK | MB_ICONERROR);

		return;
	}
	
	pInTexture->CreateShaderResourceView(md3dDevice.Get(),pDescriptorHeap, pOffset,pDescriptorSize);
}

void D3DApp::BuildPostProcessRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0);

	CD3DX12_ROOT_PARAMETER slotRootParamter[2];

	slotRootParamter[0].InitAsDescriptorTable(1,&srvTable);
	slotRootParamter[1].InitAsDescriptorTable(1,&uavTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2,slotRootParamter
		,0,nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mPostProcessRootSignature.GetAddressOf())));


	
}

void D3DApp::BuildRayRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// vertex shader
	slotRootParameter[0].InitAsShaderResourceView(0);
	// Index shader
	slotRootParameter[1].InitAsShaderResourceView(1);
	// ray 정보
	slotRootParameter[2].InitAsConstantBufferView(0);
	// 결과
	slotRootParameter[3].InitAsUnorderedAccessView(0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4,slotRootParameter
		,0,nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRayRootSignature.GetAddressOf())));
}

void D3DApp::UpdateHeightMap(myTexture* pTexture)
{
	if(mHeightMapBuffer.numDirty>0)
	{
		// ring buffer index
		INT heightMapOffset = GetCurrentHeightMapOffset();

		// create srv for new height map
		CreateShaderResourceView(pTexture,mSrvDescriptorHeap.Get(),heightMapOffset,mCbvSrvUavDescriptorSize);
	
		D3D12_RESOURCE_DESC heightTex = pTexture->Resource->GetDesc();
		INT texOffset = pTexture->mHandleOffset;
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		hDescriptor.Offset(texOffset,mCbvSrvUavDescriptorSize);
	
		// call buildResource()
		mNormalMapGenerator->SetNewNormalMap(heightTex.Width,heightTex.Height,hDescriptor);

		INT normalMapGenOffset = mCurrFrameResourceIndex * Descriptors_Per_Frame + mMaxSrvCount;
		
		mNormalMapGenerator->BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),normalMapGenOffset,mCbvSrvUavDescriptorSize)
			,CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),normalMapGenOffset,mCbvSrvUavDescriptorSize)
			,mCbvSrvUavDescriptorSize);

		mHeightMapBuffer.numDirty--;
	}
}

RenderItem* D3DApp::GetPlane() const
{
	return mAllRitems[0].get();
}











