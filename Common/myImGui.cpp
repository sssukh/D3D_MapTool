#include "myImGui.h"

#include <DirectXMath.h>
#include <string>


myImGui::myImGui(HWND pHwnd, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, int pframeNums,
                 ID3D12DescriptorHeap* pSrvHeap)
        :mHwnd(pHwnd)
        ,mFrameNums(pframeNums)
{
    md3dDevice.Attach(pDevice);
    mCommandQueue.Attach(pCommandQueue);
    mSrvDescHeap.Attach(pSrvHeap);
}

myImGui::~myImGui()
{
    
}

bool myImGui::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

     // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(mHwnd);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = APP_SRV_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (md3dDevice.Get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mSrvDescHeap)) != S_OK)
        return false;
    g_d3dSrvDescHeapAlloc.Create(md3dDevice.Get(), mSrvDescHeap.Get());
    
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = md3dDevice.Get();
    init_info.CommandQueue = mCommandQueue.Get();
    init_info.NumFramesInFlight = mFrameNums;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    init_info.SrvDescriptorHeap = mSrvDescHeap.Get();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_d3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)            { return g_d3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    return true;
}

void myImGui::DrawImGui()
{
    if(isDebug)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        // ImGui::ShowDemoWindow();
    }
}

void myImGui::Render(ID3D12GraphicsCommandList* pCommandList)
{
    if(isDebug)
    {
        ImGui::Render();

        ID3D12DescriptorHeap* heaps[] = {mSrvDescHeap.Get()};
        pCommandList->SetDescriptorHeaps(1,heaps);
        
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
    }
}

void myImGui::DrawMousePlanePosWindow(DirectX::XMFLOAT3 pValue)
{
    if(isDebug)
    {
        ImGui::Begin("Mouse Pos On Plane");
        CheckMouseHovering();
        ImGui::Text("x : %f   y: %f   z : %f",pValue.x, pValue.y, pValue.z);
    
        ImGui::End();
    }
}

void myImGui::DrawPlaneTextureListWindow(UINT& pTexIndex)
{
    // UINT tmp = gFrameNum * pDescriptorNumPerFrame;
    ImGui::Begin("Plane Texture List");

    CheckMouseHovering();
    
    ImGui::Text("Click the button and change the plane texture");
    if(ImGui::Button("Texture0"))
        pTexIndex=0;
    ImGui::SameLine();
    
    if(ImGui::Button("Texture1"))
        pTexIndex=1;
    ImGui::SameLine();

    if(ImGui::Button("Texture2"))
        pTexIndex=2;
    ImGui::SameLine();
    
    if(ImGui::Button("Texture3"))
        pTexIndex=3;
    
    if(ImGui::Button("Normal"))
        pTexIndex=20;
    // if(ImGui::Button("NormalUav"))
    //     pTexIndex=5;
    //
    if(ImGui::Button("height1"))
        pTexIndex=22;
    if(ImGui::Button("height2"))
        pTexIndex=23;
    if(ImGui::Button("height3"))
        pTexIndex=24;
    ImGui::End();
}

void myImGui::DrawWireFrameModeWindow(bool& bIsWireFrameMode)
{
    ImGui::Begin("WireFrameMode");

    CheckMouseHovering();
    
    std::string text;
    if(bIsWireFrameMode)
    {
        text = "SolidMode";
    }
    else
    {
        text = "WireFrameMode";
    }
    if(ImGui::Button(text.c_str()))
    {
        bIsWireFrameMode = bIsWireFrameMode^1;
    }

    ImGui::End();
}

bool myImGui::DrawTextureOpenWindow(std::wstring& rFileDirectory)
{
    ImGui::Begin("Select Height Map");

    CheckMouseHovering();
    
    std::wstring tmpString = L""; 
    if(ImGui::Button("Open"))
    {
        // 파일 열기
        tmpString = OpenFileDialog();
    }

    ImGui::End();
    
    if( tmpString!=L"")
    {
        
        rFileDirectory = tmpString;
        return true;
    }

    return false;
}

void myImGui::DrawHeightModVarWindow(UINT& retIntersectRange, UINT& retMaxStrengthRange, float& retModStrength)
{
    ImGui::Begin("Select Cursur Range");

    CheckMouseHovering();
    
    int tmpIntRange = retIntersectRange;
    int tmpMaxStrRange = retMaxStrengthRange;
    float tmpModStrength = retModStrength;
    ImGui::SliderInt("Intersect Range",&tmpIntRange,1,50);
    ImGui::SliderInt("Max Strength Range",&tmpMaxStrRange,1,tmpIntRange);
    ImGui::SliderFloat("Modifying Strength",&tmpModStrength,0.0025,0.01);
    
    ImGui::End();

    retIntersectRange = tmpIntRange;
    retMaxStrengthRange = tmpMaxStrRange;
    retModStrength = tmpModStrength;
    
}



std::wstring myImGui::OpenFileDialog()
{
    OPENFILENAME ofn;       // struct initialize
    WCHAR szFile[260] = { 0 }; // file directory buffer

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = mHwnd;  // 
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"All Files\0*.*\0Text Files\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        return std::wstring(ofn.lpstrFile); // return file directory
    }

    return L""; // 취소 시 빈 문자열 반환
}

bool myImGui::DrawSaveMapWindow()
{
    bool result = false;
    
    ImGui::Begin("Save the Map");

    CheckMouseHovering();
    
    if(ImGui::Button("Save"))
    {
        // 파일 열기
        result = true;
    }
    ImGui::End();
    return result;
}

bool myImGui::DrawDebugWindow(bool IsWindowOpened)
{
    bool result = IsWindowOpened;
    
    ImGui::Begin("Open/Close DebugWindow");

    CheckMouseHovering();
    
    if(ImGui::Button("Open/Close"))
    {
        result = !IsWindowOpened;
    }
    ImGui::End();
    return result;
}

UINT myImGui::DrawChangeRayModeWindow()
{
    UINT result  = 0;
    ImGui::Begin("Change Mode");

    CheckMouseHovering();
    
    if(ImGui::Button("Height Mod"))
    {
        result = 1;
    }
    if(ImGui::Button("Object Placing"))
    {
        result = 2;
    }
    if(ImGui::Button("Object Erasing"))
    {
        result = 3;
    }
    ImGui::End();
    
    return result;
}

void myImGui::CheckMouseHovering()
{
    bool flag = false;
    if(ImGui::IsWindowHovered()||ImGui::IsAnyItemHovered())
        flag = true;

    bIsMouseHovering |=flag;
}


void myImGui::ReleaseImGui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}



