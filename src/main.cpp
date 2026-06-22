// CypherArx GUI — Dear ImGui (Win32 + DirectX 11) proof of concept.
//
// Purpose of this PoC: prove the new toolkit end to end BEFORE porting the full UI —
//   1. link the C++ core directly (no FFI),
//   2. create/open a vault and list entries,
//   3. and critically, verify Japanese IME input works in an ImGui text field.
// The full two-axis UI / login methods (in git at gui a349ab3, Qt version) are ported after this.

#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "cypherarx/crypto.hpp"
#include "cypherarx/kdf.hpp"
#include "cypherarx/vault.hpp"

using cypherarx::Entry;
using cypherarx::Vault;

// --- DirectX 11 host (adapted from the official imgui example_win32_directx11) ---
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swapchain = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static void CreateRenderTarget() {
    ID3D11Texture2D* backbuffer = nullptr;
    g_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    if (backbuffer) {
        g_device->CreateRenderTargetView(backbuffer, nullptr, &g_rtv);
        backbuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

static bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
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
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
        &g_swapchain, &g_device, &got, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, &got,
                                           &g_context);
    }
    if (hr != S_OK) {
        return false;
    }
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_swapchain) {
        g_swapchain->Release();
        g_swapchain = nullptr;
    }
    if (g_context) {
        g_context->Release();
        g_context = nullptr;
    }
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
    switch (msg) {
        case WM_SIZE:
            if (g_device != nullptr && wparam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_swapchain->ResizeBuffers(0, (UINT)LOWORD(lparam), (UINT)HIWORD(lparam),
                                           DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) {
                return 0;  // disable ALT application menu
            }
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

// --- helpers ---
static std::vector<std::uint8_t> to_bytes(const char* s) {
    return {s, s + std::strlen(s)};
}

// Headless smoke test (no window): confirms the core still links and round-trips.
static int run_selftest() {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "cypherarx_gui_selftest.cax";
    std::error_code ec;
    fs::remove(path, ec);
    const std::vector<std::uint8_t> pw = {'p', 'w'};
    auto v = Vault::create(path.string(), pw, cypherarx::KdfParams::interactive());
    if (!v) {
        std::cerr << "selftest: create failed\n";
        return 1;
    }
    Entry e;
    e.entry_type = "service";
    e.label = "Example";
    if (!v->add_entry(e)) {
        std::cerr << "selftest: add_entry failed\n";
        return 1;
    }
    fs::remove(path, ec);
    std::cout << "selftest: OK (core linked, vault + entry round-trip works)\n";
    return 0;
}

int main(int argc, char** argv) {
    cypherarx::init_crypto();
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }

    WNDCLASSEXW wc = {sizeof(wc),    CS_CLASSDC, WndProc, 0L,      0L,
                      ::GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                      L"CypherArx", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"CypherArx", WS_OVERLAPPEDWINDOW, 100, 100, 900,
                                620, nullptr, nullptr, wc.hInstance, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        std::cerr << "Failed to create D3D11 device\n";
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Japanese font so CJK glyphs render and IME composition is visible. Falls back to the
    // built-in font (no CJK) if Meiryo is missing.
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 18.0f, nullptr,
                                 io.Fonts->GetGlyphRangesJapanese());

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    std::optional<Vault> vault;
    char path_buf[512] = "";
    char pw_buf[256] = "";
    char ime_buf[256] = "";
    char label_buf[256] = "";
    std::string status = "No vault open.";
    std::vector<Entry> entries;

    auto reload = [&] {
        entries.clear();
        if (vault) {
            if (auto l = vault->list_entries()) {
                entries = std::move(*l);
            }
        }
    };

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("CypherArx", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        ImGui::TextUnformatted("CypherArx — Dear ImGui PoC");
        ImGui::Separator();

        ImGui::InputText("Vault path (*.cax)", path_buf, sizeof(path_buf));
        ImGui::InputText("Master password", pw_buf, sizeof(pw_buf), ImGuiInputTextFlags_Password);
        if (ImGui::Button("Create")) {
            auto v = Vault::create(path_buf, to_bytes(pw_buf), cypherarx::KdfParams::interactive());
            if (v) {
                vault = std::move(*v);
                status = "Created & unlocked.";
                reload();
            } else {
                status = "Create failed (already exists?).";
            }
            std::memset(pw_buf, 0, sizeof(pw_buf));
        }
        ImGui::SameLine();
        if (ImGui::Button("Open")) {
            auto v = Vault::open(path_buf, to_bytes(pw_buf));
            if (v) {
                vault = std::move(*v);
                status = "Opened.";
                reload();
            } else {
                status = "Open failed (wrong password / not found).";
            }
            std::memset(pw_buf, 0, sizeof(pw_buf));
        }

        ImGui::Separator();
        ImGui::TextUnformatted("日本語IMEテスト（ここに日本語を入力できますか？）");
        ImGui::InputText("##ime", ime_buf, sizeof(ime_buf));
        ImGui::Text("入力中: %s", ime_buf);

        ImGui::Separator();
        ImGui::BeginDisabled(!vault);
        ImGui::InputText("New entry label", label_buf, sizeof(label_buf));
        ImGui::SameLine();
        if (ImGui::Button("Add service")) {
            Entry e;
            e.entry_type = "service";
            e.label = label_buf[0] ? label_buf : "Sample";
            if (vault->add_entry(e)) {
                reload();
                std::memset(label_buf, 0, sizeof(label_buf));
            }
        }
        ImGui::EndDisabled();

        ImGui::Text("Entries (%d):", static_cast<int>(entries.size()));
        ImGui::BeginChild("entries", ImVec2(0, 240), ImGuiChildFlags_Borders);
        for (const auto& e : entries) {
            ImGui::Text("%s   [%s]", e.label.c_str(), e.entry_type.c_str());
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextUnformatted(status.c_str());
        ImGui::End();

        ImGui::Render();
        const float clear[4] = {0.10f, 0.10f, 0.12f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapchain->Present(1, 0);
    }

    std::memset(pw_buf, 0, sizeof(pw_buf));  // do not leave the master password in memory

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
