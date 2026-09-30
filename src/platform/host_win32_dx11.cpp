#include "platform/host.hpp"

#include <windows.h>
#include <commdlg.h>
#include <d3d11.h>

#include <iterator>
#include <string>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace cypherarx::gui::platform {

namespace {

struct State {
    HWND hwnd = nullptr;
    WNDCLASSEXW wc{};
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapchain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    float dpi = 1.0f;
    bool quit = false;
};

State g;

void create_render_target() {
    ID3D11Texture2D* backbuffer = nullptr;
    g.swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    if (backbuffer) {
        g.device->CreateRenderTargetView(backbuffer, nullptr, &g.rtv);
        backbuffer->Release();
    }
}

void release_render_target() {
    if (g.rtv) {
        g.rtv->Release();
        g.rtv = nullptr;
    }
}

bool create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, 2, D3D11_SDK_VERSION, &sd, &g.swapchain,
                                               &g.device, &got, &g.context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g.swapchain, &g.device, &got,
                                           &g.context);
    }
    if (hr != S_OK) {
        return false;
    }
    create_render_target();
    return true;
}

void release_device() {
    release_render_target();
    if (g.swapchain) {
        g.swapchain->Release();
        g.swapchain = nullptr;
    }
    if (g.context) {
        g.context->Release();
        g.context = nullptr;
    }
    if (g.device) {
        g.device->Release();
        g.device = nullptr;
    }
}

LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return 1;
    }
    switch (msg) {
        case WM_SIZE:
            if (g.device != nullptr && wparam != SIZE_MINIMIZED) {
                release_render_target();
                g.swapchain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam), DXGI_FORMAT_UNKNOWN,
                                           0);
                create_render_target();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

std::string to_utf8(const wchar_t* w) {
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string out(static_cast<std::size_t>(n - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring to_wide(const char* s) {
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(n - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

std::optional<std::string> file_dialog(bool save, const wchar_t* filter,
                                       const char* default_extension) {
    wchar_t buf[MAX_PATH * 4] = L"";
    const std::wstring ext = to_wide(default_extension[0] == '.' ? default_extension + 1
                                                                 : default_extension);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
    ofn.lpstrDefExt = ext.c_str();
    ofn.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER |
                (save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
    const BOOL ok = save ? ::GetSaveFileNameW(&ofn) : ::GetOpenFileNameW(&ofn);
    if (!ok) {
        return std::nullopt;
    }
    return to_utf8(buf);
}

} // namespace

bool init(const WindowConfig& window) {
    if (window.dpi_aware) {
        ImGui_ImplWin32_EnableDpiAwareness();
    }
    g.wc = {sizeof(g.wc), CS_CLASSDC, wnd_proc, 0L, 0L, ::GetModuleHandleW(nullptr), nullptr,
            nullptr, nullptr, nullptr, window.class_name, nullptr};
    ::RegisterClassExW(&g.wc);
    g.dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(
        ::MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
    const int w = static_cast<int>(static_cast<float>(window.width) * g.dpi);
    const int h = static_cast<int>(static_cast<float>(window.height) * g.dpi);
    g.hwnd = ::CreateWindowW(g.wc.lpszClassName, window.title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                             CW_USEDEFAULT, w, h, nullptr, nullptr, g.wc.hInstance, nullptr);
    if (!g.hwnd || !create_device(g.hwnd)) {
        shutdown();
        return false;
    }
    ::ShowWindow(g.hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g.hwnd);
    return true;
}

void attach_imgui() {
    ImGui_ImplWin32_Init(g.hwnd);
    ImGui_ImplDX11_Init(g.device, g.context);
}

void detach_imgui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
}

void shutdown() {
    release_device();
    if (g.hwnd) {
        ::DestroyWindow(g.hwnd);
        g.hwnd = nullptr;
    }
    if (g.wc.lpszClassName) {
        ::UnregisterClassW(g.wc.lpszClassName, g.wc.hInstance);
        g.wc = {};
    }
}

float dpi_scale() {
    return g.dpi;
}

bool pump_events() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            g.quit = true;
        }
    }
    return !g.quit;
}

void begin_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void end_frame(const ImVec4& clear_color) {
    ImGui::Render();
    const float clear[4] = {clear_color.x, clear_color.y, clear_color.z, clear_color.w};
    g.context->OMSetRenderTargets(1, &g.rtv, nullptr);
    g.context->ClearRenderTargetView(g.rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g.swapchain->Present(1, 0);
}

std::optional<std::string> pick_open_file(const wchar_t* filter, const char* default_extension) {
    return file_dialog(false, filter, default_extension);
}

std::optional<std::string> pick_save_file(const wchar_t* filter, const char* default_extension) {
    return file_dialog(true, filter, default_extension);
}

} // namespace cypherarx::gui::platform
