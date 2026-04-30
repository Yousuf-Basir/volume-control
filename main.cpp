#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <initguid.h>
#include <gdiplus.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")

#define WM_TRAYICON (WM_USER + 1)

HHOOK keyboardHook;
IAudioEndpointVolume* pEndpointVolume = nullptr;
IAudioEndpointVolume* pMicEndpointVolume = nullptr;
NOTIFYICONDATA nid;
HWND g_hwnd = nullptr;
HWND g_foregroundWindow = nullptr;
HWND g_hToastWnd = nullptr;
UINT_PTR g_toastTimer = 0;
ULONG_PTR g_gdiplusToken;

HICON LoadIconFromPNG(const wchar_t* filePath) {
    HICON hIcon = NULL;
    
    Gdiplus::Image* image = Gdiplus::Image::FromFile(filePath);
    if (image && image->GetLastStatus() == Gdiplus::Ok) {
        int width = image->GetWidth();
        int height = image->GetHeight();
        
        HDC hDC = GetDC(NULL);
        HDC hMemDC = CreateCompatibleDC(hDC);
        HBITMAP hBmp = CreateCompatibleBitmap(hDC, width, height);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
        
        Gdiplus::Graphics graphics(hMemDC);
        graphics.DrawImage(image, 0, 0, width, height);
        
        ICONINFO iconInfo = {0};
        iconInfo.fIcon = TRUE;
        iconInfo.hbmColor = hBmp;
        iconInfo.hbmMask = CreateBitmap(width, height, 1, 1, NULL);
        
        hIcon = CreateIconIndirect(&iconInfo);
        
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(iconInfo.hbmMask);
        DeleteObject(hBmp);
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hDC);
        
        delete image;
    }
    
    return hIcon;
}

bool InitVolumeControl() {
    if (pEndpointVolume) {
        pEndpointVolume->Release();
        pEndpointVolume = nullptr;
    }

    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );

    if (FAILED(hr)) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pEnumerator->Release();
    if (FAILED(hr)) return false;

    hr = pDevice->Activate(
        __uuidof(IAudioEndpointVolume),
        CLSCTX_INPROC_SERVER,
        NULL,
        (void**)&pEndpointVolume
    );
    pDevice->Release();
    return SUCCEEDED(hr);
}

float GetCurrentVolume() {
    if (!pEndpointVolume) {
        InitVolumeControl();
    }
    if (!pEndpointVolume) return 0.0f;

    float volume = 0.0f;
    HRESULT hr = pEndpointVolume->GetMasterVolumeLevelScalar(&volume);
    if (FAILED(hr)) {
        if (InitVolumeControl()) {
            pEndpointVolume->GetMasterVolumeLevelScalar(&volume);
        }
    }
    return volume;
}

void SetVolume(float level) {
    if (!pEndpointVolume) {
        InitVolumeControl();
    }
    if (!pEndpointVolume) return;

    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    
    HRESULT hr = pEndpointVolume->SetMasterVolumeLevelScalar(level, NULL);
    if (FAILED(hr)) {
        if (InitVolumeControl()) {
            pEndpointVolume->SetMasterVolumeLevelScalar(level, NULL);
        }
    }
}

bool InitMicControl() {
    if (pMicEndpointVolume) {
        pMicEndpointVolume->Release();
        pMicEndpointVolume = nullptr;
    }

    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator
    );

    if (FAILED(hr)) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
    pEnumerator->Release();
    if (FAILED(hr)) return false;

    hr = pDevice->Activate(
        __uuidof(IAudioEndpointVolume),
        CLSCTX_INPROC_SERVER,
        NULL,
        (void**)&pMicEndpointVolume
    );
    pDevice->Release();
    return SUCCEEDED(hr);
}

bool IsMicMuted() {
    if (!pMicEndpointVolume) {
        InitMicControl();
    }
    if (!pMicEndpointVolume) return false;

    BOOL muted = FALSE;
    HRESULT hr = pMicEndpointVolume->GetMute(&muted);
    if (FAILED(hr)) {
        if (InitMicControl()) {
            pMicEndpointVolume->GetMute(&muted);
        }
    }
    return muted == TRUE;
}

void SetMicMute(bool mute) {
    if (!pMicEndpointVolume) {
        InitMicControl();
    }
    if (!pMicEndpointVolume) return;

    HRESULT hr = pMicEndpointVolume->SetMute(mute, NULL);
    if (FAILED(hr)) {
        if (InitMicControl()) {
            pMicEndpointVolume->SetMute(mute, NULL);
        }
    }
}

void ShowSystemVolumeIndicator(int command) {
    HWND targetWnd = GetForegroundWindow();
    if (targetWnd) {
        SendMessage(targetWnd, WM_APPCOMMAND, 0, MAKELPARAM(0, command));
    }
}

LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        Gdiplus::RectF rectF(0, 0, (Gdiplus::REAL)rect.right, (Gdiplus::REAL)rect.bottom);

        Gdiplus::GraphicsPath path;
        int r = 20;
        path.AddArc(0, 0, r, r, 180, 90);
        path.AddArc(rect.right - r, 0, r, r, 270, 90);
        path.AddArc(rect.right - r, rect.bottom - r, r, r, 0, 90);
        path.AddArc(0, rect.bottom - r, r, r, 90, 90);
        path.CloseFigure();

        // Use opaque background to prevent overlapping/blending with previous frames
        Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 20, 20, 20));
        graphics.FillPath(&bgBrush, &path);

        Gdiplus::Pen borderPen(Gdiplus::Color(100, 100, 100, 100), 1);
        graphics.DrawPath(&borderPen, &path);

        wchar_t text[128];
        GetWindowTextW(hwnd, text, 128);

        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 14, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));

        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        graphics.DrawString(text, -1, &font, rectF, &format, &textBrush);

        EndPaint(hwnd, &ps);
    } else if (msg == WM_TIMER) {
        ShowWindow(hwnd, SW_HIDE);
        KillTimer(hwnd, wParam);
        g_toastTimer = 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ShowToast(const char* message) {
    if (!g_hToastWnd) {
        WNDCLASS toastWc = {};
        toastWc.lpfnWndProc = ToastWndProc;
        toastWc.hInstance = GetModuleHandle(NULL);
        toastWc.lpszClassName = "ToastWindow";
        RegisterClass(&toastWc);

        g_hToastWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                                     "ToastWindow", "Toast", WS_POPUP, 
                                     0, 0, 250, 45, NULL, NULL, GetModuleHandle(NULL), NULL);
        // Set global transparency for the entire window
        SetLayeredWindowAttributes(g_hToastWnd, 0, 220, LWA_ALPHA);
    }

    wchar_t wmsg[128];
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wmsg, 128);
    SetWindowTextW(g_hToastWnd, wmsg);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_hToastWnd, HWND_TOPMOST, (screenW - 250) / 2, screenH - 120, 250, 45, SWP_SHOWWINDOW | SWP_NOACTIVATE);

    InvalidateRect(g_hToastWnd, NULL, TRUE);
    if (g_toastTimer) KillTimer(g_hToastWnd, g_toastTimer);
    g_toastTimer = SetTimer(g_hToastWnd, 1, 1000, NULL);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboardHook = (KBDLLHOOKSTRUCT*)lParam;
        
        bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && altPressed) {
            float currentVol = GetCurrentVolume();
            
            if (pKeyboardHook->vkCode == VK_F2) {
                SetVolume(currentVol - 0.05f);
                ShowSystemVolumeIndicator(APPCOMMAND_VOLUME_DOWN);
            } else if (pKeyboardHook->vkCode == VK_F3) {
                SetVolume(currentVol + 0.05f);
                ShowSystemVolumeIndicator(APPCOMMAND_VOLUME_UP);
            } else if (pKeyboardHook->vkCode == 'K') {
                bool newState = !IsMicMuted();
                SetMicMute(newState);
                ShowToast(newState ? "Microphone Muted" : "Microphone Unmuted");
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

void SetStartup(bool enable);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            
            HKEY hKey;
            bool isStartup = false;
            RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
            isStartup = RegQueryValueEx(hKey, "VolumeControl", NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
            RegCloseKey(hKey);
            
            AppendMenu(hMenu, MF_STRING | (isStartup ? MF_CHECKED : 0), 1, "Start with Windows");
            
            bool micMuted = IsMicMuted();
            AppendMenu(hMenu, MF_STRING | (micMuted ? MF_CHECKED : 0), 3, "Mute Microphone");
            
            AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            AppendMenu(hMenu, MF_STRING, 2, "Quit");
            
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
            
            if (cmd == 1) {
                SetStartup(!isStartup);
            } else if (cmd == 2) {
                DestroyWindow(hwnd);
            } else if (cmd == 3) {
                bool newState = !IsMicMuted();
                SetMicMute(newState);
                ShowToast(newState ? "Microphone Muted" : "Microphone Unmuted");
            }
            
            DestroyMenu(hMenu);
        }
    } else if (msg == WM_DESTROY) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SetStartup(bool enable) {
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    
    if (enable) {
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        RegSetValueEx(hKey, "VolumeControl", 0, REG_SZ, (const BYTE*)exePath, (DWORD)strlen(exePath) + 1);
    } else {
        RegDeleteValue(hKey, "VolumeControl");
    }
    
    RegCloseKey(hKey);
}

void SetStartup(bool enable);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (strstr(lpCmdLine, "--startup") != nullptr) {
        SetStartup(true);
        MessageBox(NULL, "App will start with Windows.", "Volume Control", MB_OK);
        return 0;
    }
    
    CoInitialize(NULL);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Attempt initial connection, but don't exit if it fails (lazy init will retry later)
    InitVolumeControl();

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "VolumeControlApp";
    RegisterClass(&wc);

    g_hwnd = CreateWindow("VolumeControlApp", "Volume Control", WS_OVERLAPPEDWINDOW, 
                          CW_USEDEFAULT, CW_USEDEFAULT, 200, 100, NULL, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *lastSlash = '\0';
    strcat_s(exePath, "\\icon.png");
    
    wchar_t wexePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, exePath, -1, wexePath, MAX_PATH);
    HICON hCustomIcon = LoadIconFromPNG(wexePath);
    nid.hIcon = hCustomIcon ? hCustomIcon : LoadIcon(NULL, IDI_APPLICATION);
    
    strcpy_s(nid.szTip, "Volume Control\nAlt+F2: Vol Down\nAlt+F3: Vol Up");
    Shell_NotifyIcon(NIM_ADD, &nid);

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    
    if (!keyboardHook) {
        MessageBox(NULL, "Failed to install keyboard hook!", "Error", MB_OK);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);
    
    if (pEndpointVolume) pEndpointVolume->Release();
    if (pMicEndpointVolume) pMicEndpointVolume->Release();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();
    
    return 0;
}
