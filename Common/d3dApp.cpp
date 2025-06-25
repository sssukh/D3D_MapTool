

#include "d3dApp.h"
#include <WindowsX.h>
#include "pix3.h"
#include "myRay.h"
#include "NormalMapGenerator.h"
#include "RenderItem.h"
#include "ShadowMap.h"

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
	if(md3dDevice != nullptr)
		FlushCommandQueue();
	mImGui->ReleaseImGui();
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

				mImGui->ResetMouseHovering();

				
				if(mImGui->DrawChangeRayModeWindow())
				{
					mMouseRay->ChangeRayMode();
				}
				
				if(mImGui->DrawTextureOpenWindow(mHeightMapDirectory))
				{
					mNewHeightMap = std::make_unique<myTexture>("heightTex",mHeightMapDirectory);
					if(FAILED(mNewHeightMap->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get(),mCommandQueue.Get(),mCurrFrameResource->CmdListAlloc.Get())))
					{
						MessageBoxA(nullptr,"Can't create height map texture from file","DirectXError",MB_OK | MB_ICONERROR);

					}

					mHeightMapBuffer.UpdateHeightMap(std::move(mNewHeightMap));
				}

				if(mImGui->DrawSaveMapWindow())
				{
					SaveMapFile();
				}
				
				// temporary
				mImGui->DrawMousePlanePosWindow(mMousePosOnPlane);
				
				mImGui->DrawPlaneTextureListWindow(mTextureIndex);

				mImGui->DrawWireFrameModeWindow(mIsWireFrameMode);

				{
					UINT tmpIntRange = mMouseRay->GetIntersectRange();
					UINT tmpMaxStrRange = mMouseRay->GetStrengthRange();
					float tmpModStrength = mMouseRay->GetModStrength();
				
					mImGui->DrawHeightModVarWindow(tmpIntRange,tmpMaxStrRange,tmpModStrength);

					mMouseRay->SetIntersectRange(tmpIntRange);
					mMouseRay->SetStrengthRange(tmpMaxStrRange);
					mMouseRay->SetModStrength(tmpModStrength);
				}

				bIsDebugging = mImGui->DrawDebugWindow(bIsDebugging);
				
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

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(),2048,2048);

	
	
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

	UINT halfWid = width/2;
	UINT halfHeight = height/2;
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(halfWid*halfWid + halfHeight*halfHeight);

	InitializeQuadTree(width,height);
	
	BuildPlaneGeometry(width,height,100,100);
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Initialize mMouseRay
	InitRay();
	
	
	
	
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


	// render target 1 , shadow map 1
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
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

	BoundingFrustum::CreateFromMatrix(mCamFrustum,mCam.GetProj());
}

void D3DApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	OnMouseInput();
	
	
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

	// update plane width, height in RayCB for CS
	D3D12_RESOURCE_DESC tmpDesc = mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource->GetDesc();
	UINT tmpVertexByteSize = GetPlane()->Geo->VertexBufferByteSize;
	UINT tmpVertexByteStride = GetPlane()->Geo->VertexByteStride;
	mMouseRay->UpdateRayCBs(tmpDesc.Width,tmpDesc.Height,mGeometries["planeGeo"]->DrawArgs["plane"].IndexCount/4);

	// Delete
	// UpdateObjectCBs(gt);
	// UpdateMaterialCBs(gt);
	
	UpdateInstanceBuffer(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
	
	AnimateLights(gt);
	UpdateShadowTransform(gt);
	UpdateShadowPassCB(gt);
	
	
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

	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps),descriptorHeaps);

	// renew normal map
	if(mNormalMapGenerator->GetDirty()>0)
	{
		mNormalMapGenerator->Execute(mCommandList.Get(),mPostProcessRootSignature.Get(),
			mPSOs["normalMapping"].Get(),mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get());
	}
	
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Mat Buffer 
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

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
	INT normalOffset;
	mNormalMapGenerator->GetCurrentNormalMap(normalOffset);
	normalOffset+=mMaxSrvCount;
	normalTex.Offset(normalOffset,mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(5,normalTex);

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	INT skyOffset = myTextures["skyCubeMapTex"]->mHandleOffset;
	skyTex.Offset(skyOffset,mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(6,skyTex);

	DrawSceneToShadowMap();
	
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

	

	// different passCB has set at shadowMap Drawing.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2,passCB->GetGPUVirtualAddress());

	// draw objects
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	if(mIsWireFrameMode)
		mCommandList->SetPipelineState(mPSOs["opaqueWireFrame"].Get());
	DrawRenderItems(mCommandList.Get(),mRitemLayer[(int)RenderType::Opaque]);

	mCommandList->SetPipelineState(mPSOs["opaqueTri"].Get());
	DrawRenderItems(mCommandList.Get(),mRitemLayer[(int)RenderType::OpaqueTri]);

	if(bIsDebugging)
	{
		mCommandList->SetPipelineState(mPSOs["debug"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderType::Debug]);
	}

	// draw sky sphere
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderType::Sky]);
	
	
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

	// frustum culling 설정
	if(GetAsyncKeyState('1') & 0x8000)
		mFrustumCullingEnabled = true;

	if(GetAsyncKeyState('2') & 0x8000)
		mFrustumCullingEnabled = false;

	
}


// void D3DApp::UpdateObjectCBs(const GameTimer& gt)
// {
// 	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
// 	for(auto& e : mAllRitems)
// 	{
// 		// Only update the cbuffer data if the constants have changed.  
// 		// This needs to be tracked per frame resource.
// 		if(e->NumFramesDirty > 0)
// 		{
// 			XMMATRIX world = XMLoadFloat4x4(&e->World);
// 			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
//
// 			ObjectConstants objConstants;
// 			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
// 			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
//
// 			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
//
// 			// Next FrameResource need to be updated too.
// 			e->NumFramesDirty--;
// 		}
// 	}
// }
//
// void D3DApp::UpdateMaterialCBs(const GameTimer& gt)
// {
// 	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
// 	for(auto& e : mMaterials)
// 	{
// 		// Only update the cbuffer data if the constants have changed.  If the cbuffer
// 		// data changes, it needs to be updated for each FrameResource.
// 		Material* mat = e.second.get();
// 		if(mat->NumFramesDirty > 0)
// 		{
// 			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);
//
// 			MaterialConstants matConstants;
// 			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
// 			matConstants.FresnelR0 = mat->FresnelR0;
// 			matConstants.Roughness = mat->Roughness;
// 			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));
//
// 			// update and upload to uploadheap
// 			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
//
// 			// Next FrameResource need to be updated too.
// 			mat->NumFramesDirty--;
// 		}
// 	}
// }

void D3DApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCam.GetView();
	XMMATRIX proj = mCam.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);
	
	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = mCam.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = mCam.GetNearZ();
	mMainPassCB.FarZ = mCam.GetFarZ();
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	// Update current texture index
	mMainPassCB.TextureIndex = mTextureIndex;
	
	// Update ray information

	mMouseRay->SetMatrix(view,proj);
	mMouseRay->SetStartPos(mLastMousePos);

	mMouseRay->UpdateRay();

	{
	// if(mMouseRay->IsRayIntersectPlane())
	// 	CalcHeightMod();
		CalcMouseRay();
		mMousePosOnPlane = mMouseRay->GetIntersectionPos();
	}
	
	// XMVECTOR point = XMVectorZero();
	// XMVECTOR normal = XMLoadFloat3(&XMFLOAT3(0.0f,1.0f,0.0f));
	//
	// XMStoreFloat3(&mMousePosOnPlane,mMouseRay->PlaneLineIntersectVect(point,normal));
	
	mMainPassCB.MouseProjPos = mMousePosOnPlane;

	mMainPassCB.IntersectRange = mMouseRay->GetIntersectRange();
	mMainPassCB.MaxStrengthRange = mMouseRay->GetStrengthRange();

	
	// Main pass stored in index 2
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void D3DApp::LoadTextures()
{
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

	auto mySkyCubeMapTex = std::make_unique<myTexture>("skyCubeMapTex",L"../../Textures/snowcube1024.dds",true);
	mySkyCubeMapTex->CreateDDSTextureFromFile(md3dDevice.Get(),mCommandList.Get());
	// mySkyCubeMapTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get());

	myTextures[mySkyCubeMapTex->Name] = std::move(mySkyCubeMapTex);
	
	std::wstring texPath = mImGui->OpenFileDialog();
	auto myHeightTex = std::make_unique<myTexture>("heightTex",texPath);

// 	auto myHeightTex = std::make_unique<myTexture>("heightTex",L"../../Textures/gray7.jpg");
	myHeightTex->CreateTextureFromFileName(md3dDevice.Get(),mCommandList.Get(),true);
	
	
	mHeightMapBuffer.UpdateHeightMap(std::move(myHeightTex));
}

void D3DApp:: BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 0);

	CD3DX12_DESCRIPTOR_RANGE heightTable;
	heightTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,10);
	
	CD3DX12_DESCRIPTOR_RANGE normalTable;
	normalTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11);
	
	CD3DX12_DESCRIPTOR_RANGE skyTexTable;
	skyTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 12);

	// 0 - instancebuffer - srv
	// 1 - matbuffer - srv
	// 2 - passbuffer - cb
	// 3 - srvDesHeap - destable
	
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[7];

	// Perfomance TIP: Order from most frequent to least frequent.
	// InstanceBuffer
	slotRootParameter[0].InitAsShaderResourceView(0, 1);
	// MatBuffer
	slotRootParameter[1].InitAsShaderResourceView(1, 1);
	// PassBuffer
	slotRootParameter[2].InitAsConstantBufferView(0);
	// diffuseMapBuffer(Texture)
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	// HeightMap
	slotRootParameter[4].InitAsDescriptorTable(1,&heightTable,D3D12_SHADER_VISIBILITY_DOMAIN);
	// NormalMap
	slotRootParameter[5].InitAsDescriptorTable(1,&normalTable,D3D12_SHADER_VISIBILITY_DOMAIN);
	// SkyMap
	slotRootParameter[6].InitAsDescriptorTable(1,&skyTexTable,D3D12_SHADER_VISIBILITY_PIXEL);



	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
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
	// 30 + 1(ray UAV) + 1(vertex srv) + 1(index srv) + 1(heightMap mod uav)
	srvHeapDesc.NumDescriptors = Descriptors_Per_Frame + 4;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));
	
	CreateShaderResourceView(myTextures["iceTex"].get(),mSrvDescriptorHeap.Get(),mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	
	++mSrvDescriptorHeapObjCount;
	
	CreateShaderResourceView(myTextures["bricksTex"].get(),mSrvDescriptorHeap.Get(),mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	
	++mSrvDescriptorHeapObjCount;
	
	
	CreateShaderResourceView(myTextures["checkboardTex"].get(),mSrvDescriptorHeap.Get(),mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	
	++mSrvDescriptorHeapObjCount;
	
	
	CreateShaderResourceView(myTextures["whiteTex"].get(),mSrvDescriptorHeap.Get(),mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	
	++mSrvDescriptorHeapObjCount;
	
	CreateShaderResourceView(myTextures["skyCubeMapTex"].get(),mSrvDescriptorHeap.Get(),mSrvDescriptorHeapObjCount,mCbvSrvUavDescriptorSize);
	
	++mSrvDescriptorHeapObjCount;

	
	
	mShadowMapHeapIndex = mSrvDescriptorHeapObjCount++;

	mNullCubeSrvIndex = mShadowMapHeapIndex + 1;

	mNullTexSrvIndex = mNullCubeSrvIndex + 1;

	auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	
	auto nullSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);
	mNullSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mNullCubeSrvIndex, mCbvSrvUavDescriptorSize);

	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// nullTexSrv 생성
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	mShadowMap->BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));
	
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
	const D3D_SHADER_MACRO alphaTestDefines[] =
		{
		"ALPHA_TEST", "1",
		NULL, NULL
	};
	
	mShaders["VS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["HS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl",nullptr,"HS","hs_5_1");
	mShaders["DS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl",nullptr,"DS","ds_5_1");
	mShaders["PS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShader.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["triVS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShaderTriangle.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["triPS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\myShaderTriangle.hlsl", nullptr, "PS", "ps_5_1");

	
	// computing shaders
	mShaders["normalCS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\NormalMapCS.hlsl",nullptr,"NormalCS","cs_5_0");
	mShaders["rayIntersectCS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\RayIntersectCS.hlsl",nullptr,"IntersectCS","cs_5_0");
	mShaders["rayModCS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\RayModCS.hlsl",nullptr,"ModCS","cs_5_0");

	// Sky Box
	mShaders["skyVS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	// Shadow Map
	mShaders["shadowVS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");
	mShaders["shadowHS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Shadows.hlsl", nullptr, "HS", "hs_5_1");
	mShaders["shadowDS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\Shadows.hlsl", nullptr, "DS", "ds_5_1");

	mShaders["shadowTriVS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\ShadowsTriangle.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowOpaqueTriPS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\ShadowsTriangle.hlsl", nullptr, "PS", "ps_5_1");
	
	// Debug
	mShaders["debugVS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"C:\\MapTool\\Shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");
	
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

	// sky pass
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
	mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	skyPsoDesc.DS = {};
	skyPsoDesc.HS = {};

	skyPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	
	// PSO for shadow map pass.
	// Just for now, change primitive topology into patchlist(because only patchlist obj exist)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = {};
	ZeroMemory(&smapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	smapPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	smapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	smapPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	smapPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.5f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};
	smapPsoDesc.DS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowDS"]->GetBufferPointer()),
			mShaders["shadowDS"]->GetBufferSize()
	};
	smapPsoDesc.HS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowHS"]->GetBufferPointer()),
			mShaders["shadowHS"]->GetBufferSize()
	};
	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	
	smapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	smapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	smapPsoDesc.SampleMask = UINT_MAX;
	smapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	smapPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	smapPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	smapPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	// PSO for shadow map pass.
	// Just for now, change primitive topology into patchlist(because only patchlist obj exist)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapTriPsoDesc = {};
	ZeroMemory(&smapTriPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	smapTriPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	smapTriPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	smapTriPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	smapTriPsoDesc.RasterizerState.DepthBias = 100;
	smapTriPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapTriPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapTriPsoDesc.pRootSignature = mRootSignature.Get();
	smapTriPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowTriVS"]->GetBufferPointer()),
		mShaders["shadowTriVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaqueTriPS"]->GetBufferPointer()),
		mShaders["shadowOpaqueTriPS"]->GetBufferSize()
	};
	
	// Shadow map pass does not have a render target.
	smapTriPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapTriPsoDesc.NumRenderTargets = 0;
	
	smapTriPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	smapTriPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	smapTriPsoDesc.SampleMask = UINT_MAX;
	smapTriPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	smapTriPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	smapTriPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	smapTriPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapTriPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque_tri"])));
	

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = {};
	ZeroMemory(&debugPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	debugPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	debugPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	
	// Shadow map pass does not have a render target.
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = mBackBufferFormat;
	
	debugPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	debugPsoDesc.SampleMask = UINT_MAX;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	debugPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	debugPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	debugPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueTriPsoDesc = {};

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaqueTriPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaqueTriPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaqueTriPsoDesc.pRootSignature = mRootSignature.Get();
	opaqueTriPsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["triVS"]->GetBufferPointer()), 
		mShaders["triVS"]->GetBufferSize()
	};
	opaqueTriPsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["triPS"]->GetBufferPointer()),
		mShaders["triPS"]->GetBufferSize()
	};
	opaqueTriPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaqueTriPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaqueTriPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaqueTriPsoDesc.SampleMask = UINT_MAX;
	opaqueTriPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaqueTriPsoDesc.NumRenderTargets = 1;
	opaqueTriPsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaqueTriPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaqueTriPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaqueTriPsoDesc.DSVFormat = mDepthStencilFormat;
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueTriPsoDesc, IID_PPV_ARGS(&mPSOs["opaqueTri"])));
}

void D3DApp::BuildFrameResources()
{
	for(int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2,  (UINT)mMaterials.size()));
	}
}

void D3DApp::BuildPlaneGeometry(float width, float depth, uint32_t m, uint32_t n)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData plane = geoGen.CreateGrid(width,depth,m,n);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vPlaneMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vPlaneMax = XMLoadFloat3(&vMaxf3);
	
	std::vector<Vertex> vertices(plane.Vertices.size());
	for(int i=0;i<vertices.size();++i)
	{
		vertices[i].Pos = plane.Vertices[i].Position;
		vertices[i].Normal = plane.Vertices[i].Normal;
		vertices[i].TexC = plane.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
		vPlaneMax = XMVectorMax(vPlaneMax, P);
		vPlaneMin = XMVectorMin(vPlaneMin, P);
	}

	BoundingBox planeBounds;
	XMStoreFloat3(&planeBounds.Center, 0.5f*(vPlaneMin + vPlaneMax));
	XMStoreFloat3(&planeBounds.Extents, 0.5f*(vPlaneMax - vPlaneMin));

	UINT indexCount;

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "planeGeo";
	
	// 임시로 32bit 강제
	if(true/*m*n>(UINT)1<<1*/)
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
	submesh.VertexCount = plane.Vertices.size();
	submesh.StartIndexLocation =0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = planeBounds;

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

	auto skyMat = std::make_unique<Material>();
	skyMat->Name = "skyMat";
	skyMat->MatCBIndex = 1;
	skyMat->DiffuseSrvHeapIndex = 1;
	skyMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skyMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	skyMat->Roughness = 0.5f;

	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 2;
	bricks->DiffuseSrvHeapIndex = 2;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks->Roughness = 0.5f;

	
	auto checkboard = std::make_unique<Material>();
	checkboard->Name = "checkboard";
	checkboard->MatCBIndex = 2;
	checkboard->DiffuseSrvHeapIndex = 2;
	checkboard->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkboard->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	checkboard->Roughness = 0.5f;

	
	// auto ice = std::make_unique<Material>();
	// ice->Name = "ice";
	// ice->MatCBIndex = 3;
	// ice->DiffuseSrvHeapIndex = 0;
	// ice->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	// ice->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	// ice->Roughness = 0.5f;
	
	mMaterials["planeMat"] = std::move(planeMat);
	mMaterials["skyMat"] = std::move(skyMat);
	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkboard"] = std::move(checkboard);
	// mMaterials["ice"] = std::move(ice);

}

void D3DApp::BuildRenderItems()
{
	auto planeRitem = std::make_unique<RenderItem>(md3dDevice.Get(),1);
	planeRitem->World = MathHelper::Identity4x4();
	planeRitem->TexTransform = MathHelper::Identity4x4();
	planeRitem->ObjCBIndex = 0;
	planeRitem->Mat = mMaterials["planeMat"].get();
	planeRitem->Geo = mGeometries["planeGeo"].get();
	planeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	planeRitem->IndexCount = planeRitem->Geo->DrawArgs["plane"].IndexCount;
	planeRitem->StartIndexLocation = planeRitem->Geo->DrawArgs["plane"].StartIndexLocation;
	planeRitem->BaseVertexLocation = planeRitem->Geo->DrawArgs["plane"].BaseVertexLocation;
	planeRitem->Bounds = planeRitem->Geo->DrawArgs["plane"].Bounds;
	
	// planeRitem->SetUsingBB(true);
	planeRitem->InstanceCount = 1;
	planeRitem->Instances.resize(planeRitem->InstanceCount);
	
	XMStoreFloat4x4(&planeRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&planeRitem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	planeRitem->Instances[0].MaterialIndex = 0 % mMaterials.size();
	
	mRitemLayer[(int)RenderType::Opaque].push_back(planeRitem.get());
	mAllRitems.push_back(std::move(planeRitem));

	// build sky sphere
	auto skyRitem = std::make_unique<RenderItem>(md3dDevice.Get(),1);
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = 1;
	skyRitem->Mat = mMaterials["skyMat"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	skyRitem->Bounds = skyRitem->Geo->DrawArgs["sphere"].Bounds;
	// skyRitem->SetUsingBB(true);

	skyRitem->InstanceCount = 1;
	skyRitem->Instances.resize(skyRitem->InstanceCount);

	XMStoreFloat4x4(&skyRitem->Instances[0].World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	XMStoreFloat4x4(&skyRitem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	skyRitem->Instances[0].MaterialIndex = 1 % mMaterials.size();
	
	mRitemLayer[(int)RenderType::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	auto quadRitem = std::make_unique<RenderItem>(md3dDevice.Get(),1);
	quadRitem->World = MathHelper::Identity4x4();
	quadRitem->TexTransform = MathHelper::Identity4x4();
	quadRitem->ObjCBIndex = 2;
	quadRitem->Mat = mMaterials["bricks"].get();
	quadRitem->Geo = mGeometries["shapeGeo"].get();
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->Bounds = quadRitem->Geo->DrawArgs["quad"].Bounds;
	
	quadRitem->InstanceCount = 1;
	quadRitem->Instances.resize(quadRitem->InstanceCount);

	XMStoreFloat4x4(&quadRitem->Instances[0].World, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	XMStoreFloat4x4(&quadRitem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	quadRitem->Instances[0].MaterialIndex = 2 % mMaterials.size();
	
	mRitemLayer[(int)RenderType::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	auto boxRitem = std::make_unique<RenderItem>(md3dDevice.Get(),1000);
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f)*XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 0.5f, 1.0f));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = mMaterials["checkboard"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->Bounds = boxRitem->Geo->DrawArgs["box"].Bounds;
	
	boxRitem->SetUsingBB(true);

	boxRitem->InstanceCount = 1;
	boxRitem->Instances.resize(boxRitem->InstanceCount);
	
	XMStoreFloat4x4(&boxRitem->Instances[0].World, XMMatrixTranslation(0.0f, 50.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->Instances[0].TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->Instances[0].MaterialIndex = 0 % mMaterials.size();

	mBox = boxRitem.get();
	mRitemLayer[(int)RenderType::OpaqueTri].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));
}

void D3DApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	// UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	// UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	//
	// auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	// auto matCB = mCurrFrameResource->MaterialCB->Resource();

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


		// D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		// D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		// cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		// cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);

		auto InstanceBuffer = ri->InstanceBuffer->Resource();

		// 0번째에 InstanceBuffer 묶기
		cmdList->SetGraphicsRootShaderResourceView(0,InstanceBuffer->GetGPUVirtualAddress());

		cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}





std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> D3DApp::GetStaticSamplers()
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

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);
	
	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp,
		shadow
	};
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
	UINT pDescriptorSize, bool bIsTypeless)
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
	
	pInTexture->CreateShaderResourceView(md3dDevice.Get(),pDescriptorHeap, pOffset,pDescriptorSize,bIsTypeless);
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
		CreateShaderResourceView(pTexture,mSrvDescriptorHeap.Get(),heightMapOffset,mCbvSrvUavDescriptorSize,true);
	
		D3D12_RESOURCE_DESC heightTex = pTexture->Resource->GetDesc();
		INT texOffset = pTexture->mHandleOffset;
		CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		hDescriptor.Offset(texOffset,mCbvSrvUavDescriptorSize);
	
		// call buildResource()
		mNormalMapGenerator->SetNewNormalMap(heightTex.Width,heightTex.Height,hDescriptor);

		INT normalMapGenOffset;

		mNormalMapGenerator->GetCurrentNormalMap(normalMapGenOffset);

		normalMapGenOffset+=mMaxSrvCount;
		
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

void D3DApp::InitRay()
{
	INT texOffset = GetCurrentHeightMapOffset();
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(texOffset,mCbvSrvUavDescriptorSize);
	
	mMouseRay->SetNewHeightMap(hDescriptor,mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get() );

	mMouseRay->SetIntersectShader(mShaders["rayIntersectCS"].Get());

	mMouseRay->SetModShader(mShaders["rayModCS"].Get());

	mMouseRay->BuildIntersectPso();

	mMouseRay->BuildModPso();

	mMouseRay->SetVertexIndexResource(mGeometries["planeGeo"]->VertexBufferGPU.Get(),mGeometries["planeGeo"]->IndexBufferGPU.Get());

	DXGI_FORMAT heightFormat = mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource->GetDesc().Format;
	mMouseRay->SetFormat(heightFormat);
	
	mMouseRay->BuildModResource();
	
	mMouseRay->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),mCurrentUAVDescriptorOffset,mCbvSrvUavDescriptorSize)
			,CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),mCurrentUAVDescriptorOffset,mCbvSrvUavDescriptorSize),mCbvSrvUavDescriptorSize,
			mGeometries["planeGeo"]->DrawArgs["plane"].VertexCount,mGeometries["planeGeo"]->DrawArgs["plane"].IndexCount);
	
	mMouseRay->InitBuffer(mCommandList.Get(),mCommandQueue.Get(),mDirectCmdListAlloc.Get());
}

void D3DApp::CalcMouseRay()
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// InitRay에서 RayPSO 초기화됨
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),mMouseRay->GetRayIntersectPSO() ));

	ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps),descriptorHeaps);
	
	// Dispatch
	mMouseRay->ExecuteRayIntersectTriangle(mCommandList.Get(),mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get());

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists),cmdsLists);
	

	Microsoft::WRL::ComPtr<ID3D12Fence> CalcMouseFence;
	UINT64 fenceValue = 1;
	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&CalcMouseFence));

	HANDLE fenceEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);

        
	mCommandQueue->Signal(CalcMouseFence.Get(),fenceValue);


	// GPU가 펜스 값에 도달했는지 확인
	if (CalcMouseFence->GetCompletedValue() < fenceValue) {
		CalcMouseFence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

void D3DApp::CalcHeightMod()
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// InitRay에서 RayPSO 초기화됨
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),mMouseRay->GetRayModPSO() ));

	ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps),descriptorHeaps);
	
	// Dispatch
	mMouseRay->ExecuteRayMod(mCommandList.Get(),mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get());

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists),cmdsLists);
	

	Microsoft::WRL::ComPtr<ID3D12Fence> CalcModFence;
	UINT64 fenceValue = 1;
	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&CalcModFence));

	HANDLE fenceEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);

        
	mCommandQueue->Signal(CalcModFence.Get(),fenceValue);


	// GPU가 펜스 값에 도달했는지 확인
	if (CalcModFence->GetCompletedValue() < fenceValue) {
		CalcModFence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	mNormalMapGenerator->SetDirty();
}

void D3DApp::OnMouseInput()
{
	if(GetKeyState(VK_LBUTTON)&0x8000 && mMouseRay->IsRayIntersectPlane()&&!mImGui->GetMouseIsHovering() )
	{
		if(mMouseRay->GetRayMode() == RayMode::HeightModification)
			CalcHeightMod();
		else if(mMouseRay->GetRayMode() == RayMode::ObjectPlacing)
		{
			CreateRenderItem( mBox, mMousePosOnPlane,XMFLOAT3(1.0f,1.0f,1.0f),XMFLOAT3(0.0f,0.0f,0.0f));
		}
	}
}

void D3DApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	GeometryGenerator::MeshData box = geoGen.CreateBox(10.0f,10.0f,10.0f,2);

	UINT sphereVertexOffset = 0;
	UINT quadVertexOffset = (UINT)sphere.Vertices.size();
	UINT boxVertexOffset = quadVertexOffset + (UINT)quad.Vertices.size();

	UINT sphereIndexOffset =0;
	UINT quadIndexOffset = (UINT)sphere.Indices32.size();
	UINT boxIndexOffset = quadIndexOffset + (UINT)quad.Indices32.size();
	
	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	auto totalVertexCount = sphere.Vertices.size() + quad.Vertices.size() + box.Vertices.size();
	
	std::vector<Vertex> vertices(totalVertexCount);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vSphereMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vSphereMax = XMLoadFloat3(&vMaxf3);
	
	UINT k=0;
	for(size_t i =0;i< sphere.Vertices.size(); ++i,++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vSphereMax = XMVectorMax(vSphereMax, P);
		vSphereMin = XMVectorMin(vSphereMin, P);
	}

	BoundingBox sphereBounds;
	XMStoreFloat3(&sphereBounds.Center, 0.5f*(vSphereMin + vSphereMax));
	XMStoreFloat3(&sphereBounds.Extents, 0.5f*(vSphereMax - vSphereMin));

	XMVECTOR vQuadMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vQuadMax = XMLoadFloat3(&vMaxf3);
	
	for(int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vQuadMax = XMVectorMax(vQuadMax, P);
		vQuadMin = XMVectorMin(vQuadMin, P);
	}

	BoundingBox quadBounds;
	XMStoreFloat3(&quadBounds.Center, 0.5f*(vQuadMin + vQuadMax));
	XMStoreFloat3(&quadBounds.Extents, 0.5f*(vQuadMax - vQuadMin));
	
	XMVECTOR vBoxMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vBoxMax = XMLoadFloat3(&vMaxf3);
	
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);
		vBoxMax = XMVectorMax(vBoxMax, P);
		vBoxMin = XMVectorMin(vBoxMin, P);
	}

	BoundingBox boxBounds;
	XMStoreFloat3(&boxBounds.Center, 0.5f*(vBoxMin + vBoxMax));
	XMStoreFloat3(&boxBounds.Extents, 0.5f*(vBoxMax - vBoxMin));
	
	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(),std::begin(sphere.Indices32),std::end(sphere.Indices32));
	indices.insert(indices.end(),std::begin(quad.Indices32),std::end(quad.Indices32));
	indices.insert(indices.end(),std::begin(box.Indices32),std::end(box.Indices32));


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(),vertices.data(),vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize,&geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(),indices.data(),ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize,geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(),indices.data(),ibByteSize,geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	sphereSubmesh.Bounds = sphereBounds;
	boxSubmesh.Bounds = boxBounds;
	quadSubmesh.Bounds = quadBounds;
	
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;
	geo->DrawArgs["box"] = boxSubmesh;
	
	mGeometries[geo->Name] = std::move(geo);
}

void D3DApp::SaveMapFile()
{
	ID3D12Resource* currentHeightMap = mHeightMapBuffer.GetCurrentUsingHeightmap()->Resource.Get();

	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	
	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));
	
	// 높이맵 상태 전환
	// mCommandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(
	// 	currentHeightMap,
	// 	D3D12_RESOURCE_STATE_COMMON,
	// 	D3D12_RESOURCE_STATE_COPY_SOURCE));

	// 크기에 맞는 버퍼를 생성하기 위해 높이맵의 정보 복사
	D3D12_RESOURCE_DESC desc = currentHeightMap->GetDesc();
	UINT64 bufferSize = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint;
	UINT64 rowPitch;
	UINT numRows;
	md3dDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footPrint, &numRows, &rowPitch, &bufferSize);

	// if (rowPitch != (desc.Width * bitsPerPixel / 8)) {
	// 	// 패딩 바이트 존재 → 메모리 재정렬 필요
	// }
	
	// 높이맵과 동일한 크기의 readback buffer 생성
	CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	ComPtr<ID3D12Resource> readbackBuffer;
	md3dDevice->CreateCommittedResource(
		&readbackHeap,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackBuffer));


	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = currentHeightMap;
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	dstLocation.pResource = readbackBuffer.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstLocation.PlacedFootprint = footPrint; // 이전 단계에서 계산값 사용

	mCommandList->CopyTextureRegion(
		&dstLocation, 0, 0, 0,
		&srcLocation, nullptr
	);

	// 복사 
	// mCommandList->CopyResource(readbackBuffer.Get(), currentHeightMap);
	mCommandList->Close();
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists);

	// 동기화 처리

	// Create fence for upload resource
	Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence;
	UINT64 fenceValue = 1;
	md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence));

	HANDLE fenceEvent = CreateEventEx(nullptr, nullptr,0,EVENT_MODIFY_STATE | SYNCHRONIZE);

        
	mCommandQueue->Signal(uploadFence.Get(),fenceValue);


	// GPU가 펜스 값에 도달했는지 확인
	if (uploadFence->GetCompletedValue() < fenceValue) {
		uploadFence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	void* mappedData = nullptr;
	D3D12_RANGE readRange{0, bufferSize};
	readbackBuffer->Map(0, &readRange, &mappedData);

	// DirectXTex 라이브러리 활용
	DirectX::Image image;
	image.width = desc.Width;
	image.height = desc.Height;
	image.format = desc.Format;
	image.pixels = static_cast<uint8_t*>(mappedData);
	image.rowPitch = rowPitch;
	
	HRESULT hr = DirectX::SaveToWICFile(
		image,
		DirectX::WIC_FLAGS_IGNORE_SRGB,
		GetWICCodec(DirectX::WIC_CODEC_PNG),
		L"output.png");

	if(FAILED(hr))
	{
		MessageBoxA(nullptr,"Can't save texture in png","DirectXError",MB_OK | MB_ICONERROR);
		
		HRESULT ddsHr = DirectX::SaveToDDSFile(
			image,
			DDS_FLAGS_FORCE_RGB,
			L"output.dds"
		);

		if(FAILED(ddsHr))
		{
			MessageBoxA(nullptr,"Can't save texture in dds","DirectXError",MB_OK | MB_ICONERROR);
		}
	}

	
	readbackBuffer->Unmap(0,nullptr);
}

void D3DApp::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), 
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Set null render target because we are only going to draw to
	// depth buffer.  Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	// Bind the pass constant buffer for the shadow map pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1*passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderType::Opaque]);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque_tri"].Get());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderType::OpaqueTri]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void D3DApp::AnimateLights(const GameTimer& gt)
{
	// Animate the lights (and hence shadows).

	mLightRotationAngle += 0.1f*gt.DeltaTime();

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for(int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}
}

void D3DApp::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void D3DApp::UpdateShadowTransform(const GameTimer& gt)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f*mSceneBounds.Radius*lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView*lightProj*T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void D3DApp::UpdateInstanceBuffer(const GameTimer& gt)
{
	XMMATRIX view = mCam.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	
	for(auto& e : mAllRitems)
	{
		auto currInstanceBuffer = e->InstanceBuffer.get();
		const auto& instanceData = e->Instances;

		int visibleInstanceCount = 0;

		for(UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
			XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

			// Case of Non BB Object
			if(!e->IsUsingBB())
			{
				InstanceData data;
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = instanceData[i].MaterialIndex;

				// Write the instance data to structured buffer for the visible objects.
				currInstanceBuffer->CopyData(visibleInstanceCount++, data);
				continue;
			}
			
			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);
 
			// Transform the camera frustum from view space to the object's local space.
			BoundingFrustum localSpaceFrustum;
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);
			
			// Perform the box/frustum intersection test in local space.
			if((localSpaceFrustum.Contains(e->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled==false))
			{
				InstanceData data;
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
				data.MaterialIndex = instanceData[i].MaterialIndex;

				// Write the instance data to structured buffer for the visible objects.
				currInstanceBuffer->CopyData(visibleInstanceCount++, data);
			}
		}

		e->InstanceCount = visibleInstanceCount;

		std::wostringstream outs;
		outs.precision(6);
		outs << L"Instancing and Culling Demo" <<
			L"    " << e->InstanceCount <<
			L" objects visible out of " << e->Instances.size();
		mMainWndCaption = outs.str();
	}
}


void D3DApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void D3DApp::InitializeQuadTree(UINT width, UINT height)
{
	float halfWid = width/2;
	float halfHeight = height/2;

	XMFLOAT3 centerLT = XMFLOAT3(-halfWid/2,0.0f,+halfHeight/2);
	XMFLOAT3 centerLB = XMFLOAT3(-halfWid/2,0.0f,-halfHeight/2);
	XMFLOAT3 centerRT = XMFLOAT3(+halfWid/2,0.0f,+halfHeight/2);
	XMFLOAT3 centerRB = XMFLOAT3(+halfWid/2,0.0f,-halfHeight/2);

	XMFLOAT3 extents = XMFLOAT3(halfWid,50.0f,halfHeight);

	XMVECTOR ev = XMLoadFloat3(&extents);
	XMVECTOR c1 = XMLoadFloat3(&centerLT);
	XMVECTOR c2 = XMLoadFloat3(&centerLB);
	XMVECTOR c3 = XMLoadFloat3(&centerRT);
	XMVECTOR c4 = XMLoadFloat3(&centerRB);
	
	QuadTreeNode* qt1 = new QuadTreeNode(c1,ev);
	QuadTreeNode* qt2 = new QuadTreeNode(c2,ev);
	QuadTreeNode* qt3 = new QuadTreeNode(c3,ev);
	QuadTreeNode* qt4 = new QuadTreeNode(c4,ev);

	mQuadTree.push_back(qt1);
	mQuadTree.push_back(qt2);
	mQuadTree.push_back(qt3);
	mQuadTree.push_back(qt4);
}

void D3DApp::CreateRenderItem(RenderItem* pRI, XMFLOAT3 worldPos,XMFLOAT3 worldScale,XMFLOAT3 worldRot)
{
	BoundingBox nbb = pRI->Bounds;

	DirectX::XMMATRIX nWorld = XMMatrixTranslation(worldPos.x,worldPos.y,worldPos.z);

	nbb.Transform(nbb,nWorld);


	InstanceRef nIR;
	nIR.ParentItem = pRI;
	nIR.WorldBounds = nbb;
	nIR.InstanceIndex = pRI->Instances.size();
	
	for(QuadTreeNode* qt : mQuadTree)
	{
		// 바운드가 겹치면서 node 내부의 오브젝트들과 겹치지 않으면
		if(qt->CheckObjectContain(nIR) && !qt->CheckObjectIntersectNodeObjects(nIR))
		{
			 qt->AddObject(nIR);
			 pRI->InstanceCount++;

			InstanceData nID;
			XMStoreFloat4x4(&nID.World,XMMatrixRotationRollPitchYaw(worldRot.x,worldRot.y,worldRot.z)*XMMatrixScaling(worldScale.x,worldScale.y,worldScale.z) * XMMatrixTranslation(worldPos.x,worldPos.y,worldPos.z));
			XMStoreFloat4x4(&nID.TexTransform,XMMatrixScaling(1.0f,1.0f,1.0f));
			nID.MaterialIndex = 1 % mMaterials.size();

			pRI->Instances.push_back(nID);
		}
	}
}











