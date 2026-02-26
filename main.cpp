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
NOTIFYICONDATA nid;
HWND g_hwnd = nullptr;
HWND g_foregroundWindow = nullptr;

HICON LoadIconFromPNG(const wchar_t* filePath) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    gdiplusStartupInput.GdiplusVersion = 1;
    gdiplusStartupInput.DebugEventCallback = NULL;
    gdiplusStartupInput.SuppressBackgroundThread = FALSE;
    gdiplusStartupInput.SuppressExternalCodecs = FALSE;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

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
    
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return hIcon;
}

bool InitVolumeControl() {
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
    if (!pEndpointVolume) return 0.0f;
    float volume = 0.0f;
    pEndpointVolume->GetMasterVolumeLevelScalar(&volume);
    return volume;
}

void SetVolume(float level) {
    if (!pEndpointVolume) return;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    pEndpointVolume->SetMasterVolumeLevelScalar(level, NULL);
}

void ShowSystemVolumeIndicator(int command) {
    HWND targetWnd = GetForegroundWindow();
    if (targetWnd) {
        SendMessage(targetWnd, WM_APPCOMMAND, 0, MAKELPARAM(0, command));
    }
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
    
    if (!InitVolumeControl()) {
        MessageBox(NULL, "Failed to initialize volume control!", "Error", MB_OK);
        return 1;
    }

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
    CoUninitialize();
    
    return 0;
}
