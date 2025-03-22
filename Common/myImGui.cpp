#include "myImGui.h"
//
//
// bool myImGui::InitImGui(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, int frameNums,
//     ID3D12DescriptorHeap* srvHeap)
// {
//     IMGUI_CHECKVERSION();
//     ImGui::CreateContext();
//     ImGuiIO& io = ImGui::GetIO(); (void)io;
//     
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
//     io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
//
//      // Setup Dear ImGui style
//     ImGui::StyleColorsDark();
//     //ImGui::StyleColorsLight();
//
//     // Setup Platform/Renderer backends
//     ImGui_ImplWin32_Init(hwnd);
//
//     D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//     desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
//     desc.NumDescriptors = APP_SRV_HEAP_SIZE;
//     desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//     if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvHeap)) != S_OK)
//         return false;
//     md3dSrvDescHeapAlloc.Create(device, srvHeap);
//     
//     ImGui_ImplDX12_InitInfo init_info = {};
//     init_info.Device = device;
//     init_info.CommandQueue = commandQueue;
//     init_info.NumFramesInFlight = frameNums;
//     init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
//     init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
//     // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
//     // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
//     init_info.SrvDescriptorHeap = srvHeap;
//     init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return md3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle); };
//     init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)            { return md3dSrvDescHeapAlloc.Free(cpu_handle, gpu_handle); };
//     ImGui_ImplDX12_Init(&init_info);
//
//     return true;
//     // Before 1.91.6: our signature was using a single descriptor. From 1.92, specifying SrvDescriptorAllocFn/SrvDescriptorFreeFn will be required to benefit from new features.
//     //ImGui_ImplDX12_Init(g_pd3dDevice, APP_NUM_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap, g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
//
//     // Load Fonts
//     // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
//     // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
//     // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
//     // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
//     // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
//     // - Read 'docs/FONTS.md' for more instructions and details.
//     // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
//     //io.Fonts->AddFontDefault();
//     //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
//     //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
//     //IM_ASSERT(font != nullptr);
// }
//
// void myImGui::DrawImGui()
// {
//     ImGui_ImplDX12_NewFrame();
//     ImGui_ImplWin32_NewFrame();
//     ImGui::NewFrame();
//     ImGui::ShowDemoWindow();
//     ImGui::Render();
// }
//
// void myImGui::StepAfterDraw()
// {
//     
// }
