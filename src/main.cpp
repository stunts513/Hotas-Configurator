#define DIRECTINPUT_VERSION 0x0800
#include <unordered_map>
#include <string>
// User-friendly labels for mapping keys
const std::unordered_map<std::string, std::string> mapping_labels = {
    {"Flight_Gun", "Fire Gun"},
    {"Flight_Missile", "Fire Missile"},
    {"Flight_Weapon", "Switch Weapon"},
    {"Flight_Target", "Target Select"},
    {"Flight_Radar", "Radar Toggle"},
    {"Flight_Flare", "Deploy Flare"},
    {"Flight_View", "Change View"},
    {"Flight_AccelerationDeceleration", "Acceleration/Deceleration"},
    {"Flight_AutoPilot", "Toggle Autopilot"},
    {"Flight_Pause", "Pause Game"},
    {"Flight_HatSwitchUp", "Hat Switch Up"},
    {"Flight_HatSwitchDown", "Hat Switch Down"},
    {"Flight_HatSwitchLeft", "Hat Switch Left"},
    {"Flight_HatSwitchRight", "Hat Switch Right"},
    {"Flight_Pitch", "Pitch (Up/Down)"},
    {"Flight_Roll", "Roll (Left/Right)"},
    {"Flight_Yaw", "Yaw (Twist)"},
    {"Flight_Throttle", "Throttle"},
    {"Flight_CameraPitch", "Camera Pitch"},
    {"Flight_CameraYaw", "Camera Yaw"},
    {"UI_B", "UI Button B"},
    {"UI_A", "UI Button A"},
    {"UI_X", "UI Button X"},
    {"UI_Y", "UI Button Y"},
    {"UI_LeftStickPress", "Left Stick Press"},
    {"UI_RightStickPress", "Right Stick Press"},
    {"UI_DPadUp", "D-Pad Up"},
    {"UI_DPadDown", "D-Pad Down"},
    {"UI_DPadLeft", "D-Pad Left"},
    {"UI_DPadRight", "D-Pad Right"},
    {"UI_LeftStickUp", "Left Stick Up"},
    {"UI_LeftStickDown", "Left Stick Down"},
    {"UI_LeftStickLeft", "Left Stick Left"},
    {"UI_LeftStickRight", "Left Stick Right"},
    {"UI_Menu", "Menu Button"},
    {"UI_LB", "Left Bumper"},
    {"UI_RB", "Right Bumper"},
    {"UI_LT", "Left Trigger"},
    {"UI_RT", "Right Trigger"},
    {"UI_RightStickUp", "Right Stick Up"},
    {"UI_RightStickDown", "Right Stick Down"},
    {"UI_RightStickLeft", "Right Stick Left"},
    {"UI_RightStickRight", "Right Stick Right"}};
#include <windows.h>
#include <dinput.h>
#include <d3d11.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "mapping_keys.h"
#include <map>
#include <fstream>
#include <sstream>

// DirectInput and Direct3D globals
LPDIRECTINPUT8 dinput = nullptr;

struct HotasDevice {
    LPDIRECTINPUTDEVICE8 device = nullptr;
    GUID guid;
    std::string name;
};

std::vector<HotasDevice> hotasDevices;
int activeDeviceIndex = 0;

// bindings per device
std::vector<std::map<std::string, std::string>> action_bindings;

ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
IDXGISwapChain *g_pSwapChain = nullptr;
ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;
HWND g_hWnd = nullptr;

// Mapping state
int binding_index = -1;

// Function prototypes
std::string DetectHotasInput(const DIJOYSTATE &js, const DIJOYSTATE &prev_js);
void SaveBindings(const std::string &filename);
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext);
void InitDirectInput(HWND hwnd);
bool PollHotas(DIJOYSTATE2 &js);
void Cleanup();

// Forward declaration for ImGui Win32 backend handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_SIZE:
        if (g_pSwapChain != nullptr && wParam != SIZE_MINIMIZED)
        {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            if (g_mainRenderTargetView)
            {
                g_mainRenderTargetView->Release();
                g_mainRenderTargetView = nullptr;
            }
            g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D *pBackBuffer = nullptr;
            g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance, VOID *pContext)
{
    HotasDevice hd{};
    if (SUCCEEDED(dinput->CreateDevice(pdidInstance->guidInstance, &hd.device, nullptr)))
    {
        hd.guid = pdidInstance->guidInstance;
        hd.name = pdidInstance->tszProductName;
        hotasDevices.push_back(hd);
    }
    return DIENUM_CONTINUE;
}

void InitDirectInput(HWND hwnd)
{
    HRESULT hr;
    hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID **)&dinput, nullptr);
    if (FAILED(hr))
        return;
    hr = dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, nullptr, DIEDFL_ATTACHEDONLY);
    if (hotasDevices.empty())
        return;

    action_bindings.resize(hotasDevices.size());

    for (auto &hd : hotasDevices)
    {
        hd.device->SetDataFormat(&c_dfDIJoystick2);
        hd.device->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        hd.device->Acquire();
    }
}

 bool PollHotas(DIJOYSTATE2 &js)
 {
     if (hotasDevices.empty())
         return false;
     auto &dev = hotasDevices[activeDeviceIndex].device;
     if (!dev)
         return false;
     HRESULT hr = dev->Poll();
     if (FAILED(hr))
     {
         hr = dev->Acquire();
         if (FAILED(hr))
             return false;
         hr = dev->Poll();
         if (FAILED(hr))
             return false;
     }
     hr = dev->GetDeviceState(sizeof(DIJOYSTATE2), &js);
     if (FAILED(hr))
         return false;
     return true;
 }

void Cleanup()
{
    for (auto &hd : hotasDevices)
    {
        if (hd.device)
        {
            hd.device->Unacquire();
            hd.device->Release();
        }
    }
    hotasDevices.clear();

    if (dinput)
    {
        dinput->Release();
        dinput = nullptr;
    }
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

// Detect which HOTAS input changed (button, axis, POV)
std::string DetectHotasInput(const DIJOYSTATE2 &js, const DIJOYSTATE2 &prev_js)
{
    // Buttons (support up to 128)
    for (int i = 0; i < 128; ++i)
    {
        if ((js.rgbButtons[i] & 0x80) && !(prev_js.rgbButtons[i] & 0x80))
        {
            return "Button" + std::to_string(i + 1);
        }
    }
    // Axes
    struct Axis
    {
        const char *name;
        LONG DIJOYSTATE2::*member;
    };
    Axis axes[] = {
        {"X", &DIJOYSTATE2::lX}, {"Y", &DIJOYSTATE2::lY}, {"Z", &DIJOYSTATE2::lZ},
        {"Rx", &DIJOYSTATE2::lRx}, {"Ry", &DIJOYSTATE2::lRy}, {"Rz", &DIJOYSTATE2::lRz}};
    for (int i = 0; i < 6; ++i)
    {
        if (abs(js.*(axes[i].member) - prev_js.*(axes[i].member)) > 1000) // Reduced threshold for axis detection
        {
            return axes[i].name;
        }
    }
    // Sliders
    for (int i = 0; i < 2; ++i)
    {
        if (abs(js.rglSlider[i] - prev_js.rglSlider[i]) > 10000)
        {
            return "Slider" + std::to_string(i + 1);
        }
    }
    // POV hats
    for (int i = 0; i < 4; ++i)
    {
        if (prev_js.rgdwPOV[i] != js.rgdwPOV[i] && js.rgdwPOV[i] != -1)
        {
            int pov = js.rgdwPOV[i];
            if (pov >= 0 && pov <= 4500)
                return "POV_U" + std::to_string(i + 1);
            if (pov > 4500 && pov <= 13500)
                return "POV_R" + std::to_string(i + 1);
            if (pov > 13500 && pov <= 22500)
                return "POV_D" + std::to_string(i + 1);
            if (pov > 22500 && pov <= 31500)
                return "POV_L" + std::to_string(i + 1);
        }
    }
    return "";
}

// Save bindings to ini file
void SaveBindings(const std::string &filename)
{
    std::ofstream out(filename);
    out << "[JoystickSetting]\nEnableJoystick=True\nEnableDeviceJoystick=True\nEnableDeviceFlight=True\nEnableDevice1stPerson=True\n\n";
    out << "[JoystickMappingFormat]\n";
    out << "HelpComment=\" ObjectFormat\"\nHelpComment=\"     X, Y, Z, Rx, Ry, Rz, Slider[1-2], POV_[U, D, L, R][1-4], Button[1-32]\"\n";
    out << "HelpComment=\" ValueFormat\"\nHelpComment=\"     Object, Object:[P, N, R, C]...[P, N, R, C]\"\n";
    out << "HelpComment=\"         P:PositiveValue / N:NegativeValue / R:ReverseSign / C:ConvertRange(+1.0~-1.0 -> 0.0~1.0)\"\n";
    out << "HelpComment=\" MappingFormat\"\nHelpComment=\"     Value, Value1 [&, !, +, >, <] Value2\"\n";
    out << "HelpComment=\"         &:IfValue2!=0 / !:IfValue2==0 / +:AddValue / >:SelectLarger / <:SelectSmaller\"\n";
    out << "\n";

    for (size_t d = 0; d < hotasDevices.size(); ++d)
    {
        out << "[Joystick-" << d << "]\n";
        out << "ProductName=" << hotasDevices[d].name << "\n";

        for (const auto &key : mapping_keys)
        {
            out << key << "=" << action_bindings[d][key] << "\n";
        }

        out << "\n";
    }
}

int main()
{
    // Win32 window setup
    WNDCLASSEXA wc = {sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleA(nullptr), nullptr, nullptr, nullptr, nullptr, "HOTAS Configurator", nullptr};
    RegisterClassExA(&wc);
    g_hWnd = CreateWindowA(wc.lpszClassName, "HOTAS Configurator", WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_MAXIMIZEBOX, 100, 100, 800, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Direct3D11 device and swap chain setup
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
    {
        Cleanup();
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Create render target view
    ID3D11Texture2D *pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // DirectInput HOTAS setup
    InitDirectInput(g_hWnd);

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Poll HOTAS
        DIJOYSTATE2 js = {};
        bool gotInput = PollHotas(js);

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static DIJOYSTATE2 prev_js = {};
        // Get window/client size
        RECT rect;
        GetClientRect(g_hWnd, &rect);
        ImVec2 win_pos(0, 0);
        ImVec2 win_size((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
        ImGui::SetNextWindowPos(win_pos);
        // Adjust the main window size to fit all content dynamically
        float total_height = ImGui::GetFrameHeight() * mapping_keys.size() + ImGui::GetTextLineHeightWithSpacing() * 10 + 100.0f; // Generous padding for all elements
        ImGui::SetNextWindowSizeConstraints(ImVec2(win_size.x, total_height), ImVec2(win_size.x, total_height));

        ImGui::Begin("Ace Combat 7 Joystick Rebinding Tool", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        if (hotasDevices.empty())
        {
            ImGui::Text("No HOTAS/game controller found.");
        }
        else if (!gotInput)
        {
            ImGui::Text("Failed to poll Joystick device.");
        }
        else
        {
            ImGui::Text("Assign Joystick inputs to actions:");
            ImGui::Separator();

            if (!hotasDevices.empty())
            {
                ImGui::Combo(
                    "Active Device",
                    &activeDeviceIndex,
                    [](void* data, int idx, const char** out_text) {
                        auto* devices = (std::vector<HotasDevice>*)data;
                        *out_text = (*devices)[idx].name.c_str();
                        return true;
                    },
                    &hotasDevices,
                    (int)hotasDevices.size()
                );
            }

            // Scrollable list container for mappings
            float button_height = ImGui::GetFrameHeight() + 32.0f;
            // Adjust the height of the scrollable list to ensure the button does not overlap
            float adjusted_height = win_size.y - button_height - 48.0f; // Ensure buttons remain visible
            ImGui::BeginChild("##mapping_list", ImVec2(0, adjusted_height), true, ImGuiWindowFlags_HorizontalScrollbar);
            float col1 = win_size.x * 0.35f;
            float col2 = win_size.x * 0.35f;
            float col3 = win_size.x * 0.25f;
            ImGui::Columns(3, nullptr, false);
            ImGui::SetColumnWidth(0, col1);
            ImGui::SetColumnWidth(1, col2);
            ImGui::SetColumnWidth(2, col3);
            for (size_t i = 0; i < mapping_keys.size(); ++i)
            {
                const std::string &key = mapping_keys[i];
                auto label_it = mapping_labels.find(key);
                const char *friendly_label = (label_it != mapping_labels.end()) ? label_it->second.c_str() : key.c_str();
                ImGui::Text("%s:", friendly_label);
                ImGui::NextColumn();
                ImGui::Text("%s",
                    action_bindings[activeDeviceIndex][key].empty()
                        ? "(unbound)"
                        : action_bindings[activeDeviceIndex][key].c_str());
                ImGui::NextColumn();
                if (binding_index == (int)i)
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Press a button/axis/POV...");
                    std::string detected = DetectHotasInput(js, prev_js);
                    if (!detected.empty())
                    {
                        action_bindings[activeDeviceIndex][key] = detected;
                        binding_index = -1;
                    }
                }
                else if (ImGui::Button(("Bind##" + key).c_str(), ImVec2(-FLT_MIN, 0)))
                {
                    binding_index = (int)i;
                }
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::EndChild();
        }
        ImGui::BeginGroup();
        ImGui::Separator();
        // Place the two buttons in two columns on the same row
        ImGui::Columns(2, "##buttons_row", false);
        if (!hotasDevices.empty() && gotInput)
        {
            if (ImGui::Button("Save to input.ini", ImVec2(-FLT_MIN, 0)))
            {
                SaveBindings("input.ini");
            }
            ImGui::NextColumn();
            if (ImGui::Button("Open input.ini", ImVec2(-FLT_MIN, 0)))
            {
                OPENFILENAMEW ofn;
                wchar_t file_name[MAX_PATH] = L"";
                wchar_t filter[] = L"INI Files\0*.ini\0All Files\0*.*\0";
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = g_hWnd;
                ofn.lpstrFilter = filter;
                ofn.lpstrFile = file_name;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn))
                {
                    std::wifstream in(file_name);
                    if (in.is_open())
                    {
                        std::wstring line;
                        while (std::getline(in, line))
                        {
                            size_t delimiter_pos = line.find(L'=');
                            if (delimiter_pos != std::wstring::npos)
                            {
                                std::wstring key = line.substr(0, delimiter_pos);
                                std::wstring value = line.substr(delimiter_pos + 1);
                                // Use WideCharToMultiByte for explicit conversion from wide string to narrow string
                                auto WideStringToString = [](const std::wstring &wstr) -> std::string
                                {
                                    if (wstr.empty())
                                        return std::string();
                                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
                                    std::string str(size_needed, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
                                    return str;
                                };
                                action_bindings[activeDeviceIndex][WideStringToString(key)] = WideStringToString(value);
                            }
                        }
                        in.close();
                    }
                }
            }
        }
        ImGui::Columns(1);
        ImGui::EndGroup();

        // end HOTAS logic block
        ImGui::End();
        prev_js = js;
        // Rendering
        const float clear_color[4] = {45.0f / 255.0f, 45.0f / 255.0f, 48.0f / 255.0f, 1.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Cleanup();
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return 0;
}