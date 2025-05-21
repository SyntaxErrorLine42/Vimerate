#ifndef UNICODE
#define UNICODE
#endif

#include "resource.h"
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <shellapi.h> // Required for Shell_NotifyIcon
#include <fstream>    // Required for file operations (INI)
#include <sstream>    // Required for string stream manipulation
#include <iomanip>    // Required for std::setw and std::setfill
#include <commdlg.h>  // Required for ChooseColor
#include <shlobj.h>   // Still included for general utility
#include <commctrl.h> // Required for trackbar control (INITCOMMONCONTROLSEX), and also for combobox styles
#include <algorithm>  // Required for std::sort and std::unique

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "ComCtl32.lib") // Link with ComCtl32.lib for trackbar and comboboxes

// --- Constants for System Tray Icon and Menu Items ---
#define WM_APP_NOTIFYICON (WM_APP + 1)
#define IDM_EXIT          1001
#define IDM_SETTINGS      1002
// --- End Constants ---

// --- Constants and IDs for Settings Window Controls ---
const wchar_t SETTINGS_CLASS_NAME[] = L"SettingsClass";
#define IDC_COLOR_LABEL         2001
#define IDC_CHOOSE_COLOR_BUTTON 2003
#define IDC_POOL_SIZE_LABEL     2004
#define IDC_POOL_SIZE_SLIDER    2005
#define IDC_POOL_SIZE_VALUE_LABEL 2006
#define IDC_RESET_BUTTON        2007 // New ID for the Reset Button

// Hotkey Control IDs
#define IDC_HOTKEY_MOD1_COMBO   2008
#define IDC_HOTKEY_MOD2_COMBO   2009
#define IDC_HOTKEY_VKEY_COMBO   2010
#define IDC_HOTKEY_DISPLAY_LABEL 2011 // Label to show the effective hotkey

const wchar_t INI_SECTION[] = L"Settings";
const wchar_t INI_KEY_CELL_COLOR[] = L"CellColor";
const wchar_t INI_KEY_POOL_SIZE[] = L"PoolSize";
// New INI keys for hotkey settings
const wchar_t INI_KEY_HOTKEY_MOD1[] = L"HotkeyMod1";
const wchar_t INI_KEY_HOTKEY_MOD2[] = L"HotkeyMod2";
const wchar_t INI_KEY_HOTKEY_VKEY[] = L"HotkeyVKey";
// --- End Constants ---

HWND            g_hGridWnd = nullptr;
HWND            g_hSettingsWnd = nullptr;

// Global variable for the cell color (Default light blue with alpha)
Gdiplus::Color  g_cellColor(128, 173, 216, 230); // Alpha is 128 (half-transparent)
// Default values for settings
const Gdiplus::Color DEFAULT_CELL_COLOR(128, 173, 216, 230); // Same as initial g_cellColor
const int DEFAULT_POOL_SIZE = 36; // Full POOL.length()

// Global hotkey variables
UINT g_hotkeyMod1 = MOD_WIN;    // Default: Win
UINT g_hotkeyMod2 = MOD_SHIFT;  // Default: Shift
UINT g_hotkeyVKey = 'Z';        // Default: 'Z'

// Default hotkey constants
const UINT DEFAULT_HOTKEY_MOD1 = MOD_WIN;
const UINT DEFAULT_HOTKEY_MOD2 = MOD_SHIFT;
const UINT DEFAULT_HOTKEY_VKEY = 'Z';

enum GridState  { HIDDEN, SHOW_ALL, WAIT_CLICK } g_state = HIDDEN;
std::wstring    g_typed;
struct Cell { std::wstring lbl; RECT rc; };
std::vector<Cell>     g_cells;
std::vector<Cell*>    g_filtered;
const UINT      HOTKEY_ID   = 1;
const std::wstring POOL     = L"abcdefghijklmnopqrstuvwxyz0123456789";

// New global variable for the pool size, initialized to full pool
int              g_poolSize = (int)POOL.length();
const int MIN_POOL_SIZE = 6; // Minimum characters (a-f)

// Global variable for the notification icon data
NOTIFYICONDATAW g_nid = {};

// Global variable for the full INI file path
std::wstring g_iniFilePath;


// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SettingsWndProc(HWND, UINT, WPARAM, LPARAM);
void    GenerateCells();
void    FilterCells();
void    LayoutAndDraw(HWND, int, int);
void    MoveToAndPrompt(Cell*);
void    SimClick(DWORD);
void    UpdatePoolSizeDisplay(HWND hSettingsWnd); // New helper
void    UpdateHotkeyDisplay(HWND hSettingsWnd); // New helper
void    PopulateHotkeyDropdowns(HWND hSettingsWnd); // New helper
bool    RegisterAppHotkey(); // Helper to register the current hotkey (returns bool now)
void    UnregisterAppHotkey(); // Helper to unregister the current hotkey


// Helper functions for color and settings persistence
Gdiplus::Color HexToColor(const std::wstring& hex);
std::wstring ColorToHex(const Gdiplus::Color& color);
void LoadSettings();
void SaveSettings();
void ResetToDefaults(HWND hSettingsWnd); // New function to reset settings


// Helper to draw a rounded rectangle with given brush
void DrawRounded(Gdiplus::Graphics& g, const Gdiplus::RectF& r, Gdiplus::Brush* brush) {
    using namespace Gdiplus;
    float radius = 4.0f;
    GraphicsPath path;
    path.AddArc(r.X,           r.Y,           radius, radius, 180, 90);
    path.AddArc(r.X + r.Width - radius, r.Y,           radius, radius, 270, 90);
    path.AddArc(r.X + r.Width - radius, r.Y + r.Height - radius, radius, radius,   0, 90);
    path.AddArc(r.X,           r.Y + r.Height - radius, radius, radius,   90, 90);
    path.CloseFigure();
    g.FillPath(brush, &path);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Initialize Common Controls for Trackbar and Comboboxes
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES; // Trackbar and Combobox classes
    InitCommonControlsEx(&icc);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR token;
    if (Gdiplus::GdiplusStartup(&token, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        MessageBoxA(nullptr, "Failed to initialize GDI+.", "Error", MB_OK);
        return 1;
    }

    // --- Determine and create path for settings file next to EXE in a 'Settings' subfolder ---
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring sExePath = exePath;
    size_t lastSlash = sExePath.rfind(L'\\');
    std::wstring exeDir;
    if (lastSlash != std::wstring::npos) {
        exeDir = sExePath.substr(0, lastSlash);
    } else {
        exeDir = L".";
    }

    g_iniFilePath = exeDir;
    g_iniFilePath += L"\\Settings";
    CreateDirectoryW(g_iniFilePath.c_str(), nullptr);
    g_iniFilePath += L"\\VimerateSettings.ini";
    // --- End custom settings path determination ---

    // Load settings at startup
    LoadSettings();

    // --- Register Main Grid Window Class ---
    const wchar_t GRID_CLASS_NAME[] = L"GridClass";
    WNDCLASSEXW wcGrid = {};
    wcGrid.cbSize = sizeof(wcGrid);
    wcGrid.style = CS_HREDRAW | CS_VREDRAW;
    wcGrid.lpfnWndProc = WndProc;
    wcGrid.hInstance = hInst;
    wcGrid.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcGrid.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcGrid.lpszClassName = GRID_CLASS_NAME;
    // Set the large icon (for Alt+Tab, desktop shortcut, etc.)
    wcGrid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));

    // Set the small icon (for window title bar, taskbar)
    wcGrid.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    RegisterClassExW(&wcGrid);
    // --- End Register Main Grid Window Class ---

    // --- Register Settings Window Class ---
    WNDCLASSEXW wcSettings = {};
    wcSettings.cbSize = sizeof(wcSettings);
    wcSettings.style = CS_HREDRAW | CS_VREDRAW;
    wcSettings.lpfnWndProc = SettingsWndProc;
    wcSettings.hInstance = hInst;
    wcSettings.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcSettings.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); // Set background to white
    wcSettings.lpszClassName = SETTINGS_CLASS_NAME;
    wcSettings.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));   // Large icon
    wcSettings.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)); // Small icon
    RegisterClassExW(&wcSettings);
    // --- End Register Settings Window Class ---


    g_hGridWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_APPWINDOW,
        GRID_CLASS_NAME, L"Vimerate", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr
    );

    GenerateCells(); // Generate cells initially based on loaded settings
    RegisterAppHotkey(); // Register the hotkey loaded from settings

    // --- Tray Icon Initialization ---
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hGridWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_NOTIFYICON;
    // Use your custom icon for the tray as well
    g_nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));
    wcscpy_s(g_nid.szTip, L"Vimerate");

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    // --- End Tray Icon Initialization ---

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterAppHotkey(); // Unregister hotkey before exit
    DestroyWindow(g_hGridWnd);

    // Save settings before exiting
    SaveSettings();

    // --- Delete Tray Icon before exiting ---
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    // --- End Tray Icon ---

    Gdiplus::GdiplusShutdown(token);
    return 0;
}

// Helper to register the application's hotkey
bool RegisterAppHotkey() { // Now returns bool
    // Combine modifiers: Win + Shift + Ctrl + Alt
    UINT combinedModifiers = 0;
    if (g_hotkeyMod1 == MOD_WIN || g_hotkeyMod2 == MOD_WIN) combinedModifiers |= MOD_WIN;
    if (g_hotkeyMod1 == MOD_CONTROL || g_hotkeyMod2 == MOD_CONTROL) combinedModifiers |= MOD_CONTROL;
    if (g_hotkeyMod1 == MOD_SHIFT || g_hotkeyMod2 == MOD_SHIFT) combinedModifiers |= MOD_SHIFT;
    if (g_hotkeyMod1 == MOD_ALT || g_hotkeyMod2 == MOD_ALT) combinedModifiers |= MOD_ALT;

    // g_hotkeyVKey will always be non-zero from UI selection and initial load (due to DEFAULT_HOTKEY_VKEY)
    if (g_hotkeyVKey != 0) {
        if (!RegisterHotKey(g_hGridWnd, HOTKEY_ID, combinedModifiers, g_hotkeyVKey)) {
            MessageBoxW(g_hGridWnd, L"Failed to register hotkey. It might be in use by another application.", L"Hotkey Warning", MB_OK | MB_ICONWARNING);
            return false; // Indicate failure
        }
    }
    return true; // Indicate success (or that no key was intended to be registered, which is less likely now)
}

// Helper to unregister the application's hotkey
void UnregisterAppHotkey() {
    UnregisterHotKey(g_hGridWnd, HOTKEY_ID);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_HOTKEY:
        if (wParam == HOTKEY_ID) {
            if (g_state == HIDDEN) {
                g_state = SHOW_ALL;
                g_typed.clear();
                FilterCells();
                ShowWindow(hWnd, SW_SHOW);
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            } else {
                g_state = HIDDEN;
                ShowWindow(hWnd, SW_HIDE);
            }
        }
        break;

    case WM_APP_NOTIFYICON:
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"S&ettings");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"E&xit");
                SetMenuDefaultItem(hMenu, IDM_EXIT, FALSE);

                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
                PostMessage(hWnd, WM_NULL, 0, 0);
                DestroyMenu(hMenu);
            }
        }
        break;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_SETTINGS) {
            if (g_hSettingsWnd == nullptr) {
                g_hSettingsWnd = CreateWindowExW(
                    0,
                    SETTINGS_CLASS_NAME,
                    L"Vimerate Settings",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    450, 350, // Adjusted initial size to accommodate new controls
                    hWnd,
                    nullptr,
                    GetModuleHandle(nullptr),
                    nullptr
                );

                if (g_hSettingsWnd) {
                    // Controls are created in WM_CREATE of SettingsWndProc
                }
            } else {
                SetForegroundWindow(g_hSettingsWnd);
                ShowWindow(g_hSettingsWnd, SW_RESTORE);
            }
        }
        else if (LOWORD(wParam) == IDM_EXIT) {
            DestroyWindow(hWnd);
        }
        break;

    case WM_KEYDOWN: {
        if (g_state == HIDDEN)
            break;
        if (wParam == VK_ESCAPE) {
            g_state = HIDDEN;
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        if (g_state == WAIT_CLICK) {
            if (wParam == '1') SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
            else if (wParam == '2') SimClick(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP);
            else if (wParam == '3') {
                SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
                SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
            }
            g_state = HIDDEN;
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        if (wParam == VK_BACK) {
            if (!g_typed.empty()) {
                g_typed.pop_back();
                FilterCells();
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            } else {
                g_state = HIDDEN;
                ShowWindow(hWnd, SW_HIDE);
            }
            break;
        }
        BYTE kbState[256];
        GetKeyboardState(kbState);
        wchar_t buf[2];
        int result = ToUnicode((UINT)wParam, HIWORD(lParam), kbState, buf, 1, 0);
        if (result == 1 && (POOL.find(buf[0]) != std::wstring::npos || buf[0] == L'.')) {
            g_typed += buf[0];
            FilterCells();
            if (g_typed.length() == 2 || g_typed.length() == 3) {
                for (auto c : g_filtered) {
                    if (c->lbl == g_typed) {
                        MoveToAndPrompt(c);
                        break;
                    }
                }
            } else {
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            }
        }
        break;
    }

    case WM_DESTROY:
        if (g_hSettingsWnd) {
            DestroyWindow(g_hSettingsWnd);
            g_hSettingsWnd = nullptr;
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Helper to update the text label for pool size display
void UpdatePoolSizeDisplay(HWND hSettingsWnd) {
    HWND hLabel = GetDlgItem(hSettingsWnd, IDC_POOL_SIZE_VALUE_LABEL);
    if (hLabel) {
        std::wstringstream ss;
        ss << L"Currently using " << g_poolSize << L" characters.";
        SetWindowTextW(hLabel, ss.str().c_str());
    }
}

// Helper to update the text label for hotkey display
void UpdateHotkeyDisplay(HWND hSettingsWnd) {
    HWND hLabel = GetDlgItem(hSettingsWnd, IDC_HOTKEY_DISPLAY_LABEL);
    if (hLabel) {
        std::wstring hotkeyString;
        std::vector<std::wstring> activeModifiers;

        // Collect active modifiers
        if (g_hotkeyMod1 == MOD_WIN || g_hotkeyMod2 == MOD_WIN) activeModifiers.push_back(L"Win");
        if (g_hotkeyMod1 == MOD_CONTROL || g_hotkeyMod2 == MOD_CONTROL) activeModifiers.push_back(L"Ctrl");
        if (g_hotkeyMod1 == MOD_SHIFT || g_hotkeyMod2 == MOD_SHIFT) activeModifiers.push_back(L"Shift");
        if (g_hotkeyMod1 == MOD_ALT || g_hotkeyMod2 == MOD_ALT) activeModifiers.push_back(L"Alt");

        // Remove duplicates and sort for consistent display
        std::sort(activeModifiers.begin(), activeModifiers.end());
        activeModifiers.erase(std::unique(activeModifiers.begin(), activeModifiers.end()), activeModifiers.end());

        // Append modifiers to hotkeyString
        for (size_t i = 0; i < activeModifiers.size(); ++i) {
            hotkeyString += activeModifiers[i];
            if (i < activeModifiers.size() - 1) {
                hotkeyString += L" + ";
            }
        }

        // Append Virtual Key if it's not "None" (i.e., g_hotkeyVKey is not 0)
        // Since we removed "None" from the VKey options, g_hotkeyVKey should generally be non-zero
        // when set via the UI. However, if a previously saved "None" (0) VKey was loaded,
        // it would be handled here by simply not appending anything.
        if (g_hotkeyVKey != 0) {
            // Add " + " only if there were modifiers already
            if (!hotkeyString.empty()) {
                hotkeyString += L" + ";
            }

            UINT scanCode = MapVirtualKeyW(g_hotkeyVKey, 0);
            wchar_t keyName[256];
            // GetKeyNameTextW returns the string representation of the key for the current keyboard layout.
            // (scanCode << 16) is required for the lParam.
            if (GetKeyNameTextW(scanCode << 16, keyName, sizeof(keyName) / sizeof(keyName[0]))) {
                hotkeyString += keyName;
            } else {
                // Fallback for character keys that GetKeyNameTextW might not map cleanly
                if (g_hotkeyVKey >= 'A' && g_hotkeyVKey <= 'Z' || g_hotkeyVKey >= '0' && g_hotkeyVKey <= '9') {
                    hotkeyString += (wchar_t)g_hotkeyVKey;
                } else {
                    // For other VKEYs not explicitly handled by GetKeyNameTextW,
                    // this will show its VKey code, e.g., "VKey_45"
                    std::wstringstream ss;
                    ss << L"VKey_" << g_hotkeyVKey;
                    hotkeyString += ss.str();
                }
            }
        }
        // If g_hotkeyVKey is 0 (e.g., from loading an old setting), nothing is appended for the VKey,
        // which matches the user's desire for a blank display for "None" VKey.

        SetWindowTextW(hLabel, (L"Current Hotkey: " + hotkeyString).c_str());
    }
}

// Helper to populate the hotkey dropdowns
void PopulateHotkeyDropdowns(HWND hSettingsWnd) {
    HWND hMod1Combo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_MOD1_COMBO);
    HWND hMod2Combo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_MOD2_COMBO);
    HWND hVKeyCombo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_VKEY_COMBO);

    // Clear existing items in case of re-population (e.g., after reset)
    SendMessageW(hMod1Combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hMod2Combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hVKeyCombo, CB_RESETCONTENT, 0, 0);

    // Populate Modifiers (with "None" option)
    struct {
        const wchar_t* name;
        UINT value;
    } modifiers[] = {
        {L"None", 0},
        {L"Win", MOD_WIN},
        {L"Ctrl", MOD_CONTROL},
        {L"Shift", MOD_SHIFT},
        {L"Alt", MOD_ALT}
    };

    for (const auto& mod : modifiers) {
        int index = (int)SendMessageW(hMod1Combo, CB_ADDSTRING, 0, (LPARAM)mod.name);
        SendMessageW(hMod1Combo, CB_SETITEMDATA, index, (LPARAM)mod.value);
        index = (int)SendMessageW(hMod2Combo, CB_ADDSTRING, 0, (LPARAM)mod.name);
        SendMessageW(hMod2Combo, CB_SETITEMDATA, index, (LPARAM)mod.value);
    }

    // Select current modifiers
    int selectedIndex = 0;
    for (int i = 0; i < _countof(modifiers); ++i) {
        if (modifiers[i].value == g_hotkeyMod1) {
            selectedIndex = i;
            break;
        }
    }
    SendMessageW(hMod1Combo, CB_SETCURSEL, selectedIndex, 0);

    selectedIndex = 0;
    for (int i = 0; i < _countof(modifiers); ++i) {
        if (modifiers[i].value == g_hotkeyMod2) {
            selectedIndex = i;
            break;
        }
    }
    SendMessageW(hMod2Combo, CB_SETCURSEL, selectedIndex, 0);


    // Populate Virtual Keys (WITHOUT "None" option)
    // Store std::wstring directly to ensure proper memory management
    std::vector<std::pair<std::wstring, UINT>> vkeys_data;
    // Removed: vkeys_data.push_back({L"None", 0});

    for (wchar_t c = 'A'; c <= 'Z'; ++c) vkeys_data.push_back({std::wstring(1, c), c});
    for (wchar_t c = '0'; c <= '9'; ++c) vkeys_data.push_back({std::wstring(1, c), c});
    for (int i = 1; i <= 12; ++i) {
        std::wstringstream ss;
        ss << L"F" << i;
        vkeys_data.push_back({ss.str(), VK_F1 + (i - 1)});
    }
    vkeys_data.push_back({L"Space", VK_SPACE});
    vkeys_data.push_back({L"Enter", VK_RETURN});
    vkeys_data.push_back({L"Backspace", VK_BACK});
    vkeys_data.push_back({L"Delete", VK_DELETE});
    vkeys_data.push_back({L"Insert", VK_INSERT});
    vkeys_data.push_back({L"Home", VK_HOME});
    vkeys_data.push_back({L"End", VK_END});
    vkeys_data.push_back({L"Page Up", VK_PRIOR});
    vkeys_data.push_back({L"Page Down", VK_NEXT});
    vkeys_data.push_back({L"Tab", VK_TAB});
    vkeys_data.push_back({L"Escape", VK_ESCAPE});
    vkeys_data.push_back({L"Num Lock", VK_NUMLOCK});
    vkeys_data.push_back({L"Scroll Lock", VK_SCROLL});
    vkeys_data.push_back({L"Print Screen", VK_SNAPSHOT});
    vkeys_data.push_back({L"Pause", VK_PAUSE});
    vkeys_data.push_back({L"Up Arrow", VK_UP});
    vkeys_data.push_back({L"Down Arrow", VK_DOWN});
    vkeys_data.push_back({L"Left Arrow", VK_LEFT});
    vkeys_data.push_back({L"Right Arrow", VK_RIGHT});
    vkeys_data.push_back({L"Numpad 0", VK_NUMPAD0});
    vkeys_data.push_back({L"Numpad 1", VK_NUMPAD1});
    vkeys_data.push_back({L"Numpad 2", VK_NUMPAD2});
    vkeys_data.push_back({L"Numpad 3", VK_NUMPAD3});
    vkeys_data.push_back({L"Numpad 4", VK_NUMPAD4});
    vkeys_data.push_back({L"Numpad 5", VK_NUMPAD5});
    vkeys_data.push_back({L"Numpad 6", VK_NUMPAD6});
    vkeys_data.push_back({L"Numpad 7", VK_NUMPAD7});
    vkeys_data.push_back({L"Numpad 8", VK_NUMPAD8});
    vkeys_data.push_back({L"Numpad 9", VK_NUMPAD9});
    vkeys_data.push_back({L"Numpad *", VK_MULTIPLY});
    vkeys_data.push_back({L"Numpad +", VK_ADD});
    vkeys_data.push_back({L"Numpad -", VK_SUBTRACT});
    vkeys_data.push_back({L"Numpad .", VK_DECIMAL});
    vkeys_data.push_back({L"Numpad /", VK_DIVIDE});


    for (const auto& vk : vkeys_data) {
        // Pass the C-style string from the wstring object
        int index = (int)SendMessageW(hVKeyCombo, CB_ADDSTRING, 0, (LPARAM)vk.first.c_str());
        SendMessageW(hVKeyCombo, CB_SETITEMDATA, index, (LPARAM)vk.second);
    }

    // Select current virtual key
    selectedIndex = 0;
    // g_hotkeyVKey should always be a valid, non-zero VKey because 'None' is not an option
    // and it defaults to DEFAULT_HOTKEY_VKEY if not found in INI.
    for (size_t i = 0; i < vkeys_data.size(); ++i) {
        if (vkeys_data[i].second == g_hotkeyVKey) {
            selectedIndex = i;
            break;
        }
    }
    SendMessageW(hVKeyCombo, CB_SETCURSEL, selectedIndex, 0);
}


// Function to reset all settings to their default values
void ResetToDefaults(HWND hSettingsWnd) {
    // Reset color
    g_cellColor = DEFAULT_CELL_COLOR;

    // Reset pool size
    g_poolSize = DEFAULT_POOL_SIZE;

    // Store current hotkey to try and revert if default fails to register
    UINT oldMod1 = g_hotkeyMod1;
    UINT oldMod2 = g_hotkeyMod2;
    UINT oldVKey = g_hotkeyVKey;

    // Set to default hotkey
    g_hotkeyMod1 = DEFAULT_HOTKEY_MOD1;
    g_hotkeyMod2 = DEFAULT_HOTKEY_MOD2;
    g_hotkeyVKey = DEFAULT_HOTKEY_VKEY; // DEFAULT_HOTKEY_VKEY is guaranteed to be a valid key

    // Unregister and attempt to register the default hotkey
    UnregisterAppHotkey();
    // Only show warning if the default hotkey is a valid key and fails to register
    // g_hotkeyVKey will always be non-zero here
    if (!RegisterAppHotkey()) {
        // Revert to the hotkey that was active before reset
        g_hotkeyMod1 = oldMod1;
        g_hotkeyMod2 = oldMod2;
        g_hotkeyVKey = oldVKey;
        RegisterAppHotkey(); // Re-register the hotkey that was previously working
        MessageBoxW(hSettingsWnd, L"Failed to register the default hotkey. Reverted to previous hotkey.", L"Hotkey Reset Warning", MB_OK | MB_ICONWARNING);
    }

    // Update settings window controls
    InvalidateRect(hSettingsWnd, nullptr, TRUE); // Redraw color preview
    UpdateWindow(hSettingsWnd);
    SendMessage(GetDlgItem(hSettingsWnd, IDC_POOL_SIZE_SLIDER), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)g_poolSize);
    UpdatePoolSizeDisplay(hSettingsWnd);

    // Update hotkey dropdowns and display (after potential rollback)
    PopulateHotkeyDropdowns(hSettingsWnd); // Re-populate to reset selections
    UpdateHotkeyDisplay(hSettingsWnd);


    // Update main grid
    GenerateCells();
    FilterCells();
    if (g_hGridWnd) {
        LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        InvalidateRect(g_hGridWnd, nullptr, TRUE);
        UpdateWindow(g_hGridWnd);
    }

    // Save the (potentially reset or reverted) settings
    SaveSettings();
}


// Window Procedure for the Settings Window
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
        {
            // Define spacing and control dimensions
            const int padding = 20;
            const int controlHeight = 25;
            const int labelWidth = 150;
            const int previewWidth = 50;
            const int previewHeight = 25;
            const int buttonWidth = 140;
            const int controlSpacing = 10;
            const int sliderWidth = 180;
            const int sliderHeight = 30;
            const int comboWidth = 100;
            const int comboHeight = 200; // Height for dropdown list

            int currentY = padding;

            // Color Settings Section
            // Label for "Current Cell Color:"
            CreateWindowW(
                L"STATIC", L"Current Cell Color:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY + (controlHeight - 20) / 2, labelWidth, 20,
                hWnd, (HMENU)IDC_COLOR_LABEL, GetModuleHandle(nullptr), nullptr
            );

            // "Choose Color..." button
            CreateWindowW(
                L"BUTTON", L"Choose Color...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                padding + labelWidth + controlSpacing + previewWidth + 20, currentY, buttonWidth, controlHeight,
                hWnd, (HMENU)IDC_CHOOSE_COLOR_BUTTON, GetModuleHandle(nullptr), nullptr
            );
            currentY += controlHeight + padding;

            // Character Pool Size Settings Section
            // Label for "Number of Characters:"
            CreateWindowW(
                L"STATIC", L"Number of Characters:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY + (sliderHeight - 20) / 2, labelWidth + 20, 20,
                hWnd, (HMENU)IDC_POOL_SIZE_LABEL, GetModuleHandle(nullptr), nullptr
            );

            // Trackbar (Slider)
            HWND hSlider = CreateWindowExW(
                0,
                TRACKBAR_CLASSW,
                L"",
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                padding + labelWidth + 20 + controlSpacing, currentY,
                sliderWidth, sliderHeight,
                hWnd,
                (HMENU)IDC_POOL_SIZE_SLIDER,
                GetModuleHandle(nullptr), nullptr
            );

            SendMessage(hSlider, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(MIN_POOL_SIZE, (int)POOL.length()));
            SendMessage(hSlider, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)g_poolSize);
            SendMessage(hSlider, TBM_SETPAGESIZE, 0, 1);
            SendMessage(hSlider, TBM_SETTICFREQ, 1, 0);

            currentY += sliderHeight + padding;

            // Static text to display current pool size value
            CreateWindowW(
                L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY, labelWidth + sliderWidth, 20,
                hWnd, (HMENU)IDC_POOL_SIZE_VALUE_LABEL, GetModuleHandle(nullptr), nullptr
            );
            UpdatePoolSizeDisplay(hWnd);

            currentY += 20 + padding; // Space after value label

            // Hotkey Settings Section
            CreateWindowW(
                L"STATIC", L"Hotkey Combination:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY + (controlHeight - 20) / 2, labelWidth, 20,
                hWnd, nullptr, GetModuleHandle(nullptr), nullptr
            );

            // Modifier 1 Dropdown
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, // CBS_DROPDOWNLIST makes it non-editable
                padding + labelWidth + controlSpacing, currentY, comboWidth, comboHeight,
                hWnd, (HMENU)IDC_HOTKEY_MOD1_COMBO, GetModuleHandle(nullptr), nullptr
            );

            // Modifier 2 Dropdown
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                padding + labelWidth + controlSpacing + comboWidth + controlSpacing, currentY, comboWidth, comboHeight,
                hWnd, (HMENU)IDC_HOTKEY_MOD2_COMBO, GetModuleHandle(nullptr), nullptr
            );

            // Virtual Key Dropdown
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                padding + labelWidth + controlSpacing + (comboWidth + controlSpacing) * 2, currentY, comboWidth + 20, comboHeight,
                hWnd, (HMENU)IDC_HOTKEY_VKEY_COMBO, GetModuleHandle(nullptr), nullptr
            );

            PopulateHotkeyDropdowns(hWnd); // Populate dropdowns with options
            currentY += controlHeight + padding;

            // Label to display current hotkey (effective combination)
            CreateWindowW(
                L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY, labelWidth + (comboWidth + controlSpacing) * 3 + 20, 20,
                hWnd, (HMENU)IDC_HOTKEY_DISPLAY_LABEL, GetModuleHandle(nullptr), nullptr
            );
            UpdateHotkeyDisplay(hWnd); // Initial update of the hotkey display

            currentY += 20 + padding; // Space after value label


            // Reset Button (Right-aligned)
            const int resetButtonWidth = 120;
            const int resetButtonHeight = 28;
            int windowWidth;
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            windowWidth = clientRect.right - clientRect.left;

            CreateWindowW(
                L"BUTTON", L"Reset to Defaults",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                windowWidth - padding - resetButtonWidth, currentY, resetButtonWidth, resetButtonHeight,
                hWnd, (HMENU)IDC_RESET_BUTTON, GetModuleHandle(nullptr), nullptr
            );

        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // Redraw the color preview rectangle
            const int padding = 20;
            const int controlHeight = 25;
            const int labelWidth = 150;
            const int previewWidth = 50;
            const int previewHeight = 25;
            const int controlSpacing = 10;

            int previewX = padding + labelWidth + controlSpacing;
            int previewY = padding;
            RECT colorPreviewRect = {previewX, previewY, previewX + previewWidth, previewY + previewHeight};

            HBRUSH hBrush = CreateSolidBrush(RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB()));
            FillRect(hdc, &colorPreviewRect, hBrush);
            DeleteObject(hBrush);

            FrameRect(hdc, &colorPreviewRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

            EndPaint(hWnd, &ps);
        }
        break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CHOOSE_COLOR_BUTTON) {
                static COLORREF customColors[16];
                CHOOSECOLOR cc;
                ZeroMemory(&cc, sizeof(cc));
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hWnd;
                cc.lpCustColors = customColors;
                cc.Flags = CC_RGBINIT | CC_FULLOPEN;

                cc.rgbResult = RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB());

                if (ChooseColor(&cc)) {
                    g_cellColor = Gdiplus::Color(g_cellColor.GetA(), GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));

                    if (g_hGridWnd) {
                        InvalidateRect(g_hGridWnd, nullptr, TRUE);
                        UpdateWindow(g_hGridWnd);
                    }

                    InvalidateRect(hWnd, nullptr, TRUE);
                    UpdateWindow(hWnd);

                    SaveSettings();
                }
            } else if (LOWORD(wParam) == IDC_RESET_BUTTON) {
                ResetToDefaults(hWnd); // Call the new reset function
            } else if (HIWORD(wParam) == CBN_SELCHANGE) { // Handle Combobox selections
                // Get selected values first
                int selMod1Index = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD1_COMBO), CB_GETCURSEL, 0, 0);
                UINT newMod1 = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD1_COMBO), CB_GETITEMDATA, selMod1Index, 0);

                int selMod2Index = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD2_COMBO), CB_GETCURSEL, 0, 0);
                UINT newMod2 = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD2_COMBO), CB_GETITEMDATA, selMod2Index, 0);

                int selVKeyIndex = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_VKEY_COMBO), CB_GETCURSEL, 0, 0);
                UINT newVKey = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_VKEY_COMBO), CB_GETITEMDATA, selVKeyIndex, 0);

                // Check if any part of the hotkey has actually changed
                if (newMod1 != g_hotkeyMod1 || newMod2 != g_hotkeyMod2 || newVKey != g_hotkeyVKey) {
                    // Store current working hotkey for rollback
                    UINT oldMod1 = g_hotkeyMod1;
                    UINT oldMod2 = g_hotkeyMod2;
                    UINT oldVKey = g_hotkeyVKey;

                    // Update global variables to new selected values
                    g_hotkeyMod1 = newMod1;
                    g_hotkeyMod2 = newMod2;
                    g_hotkeyVKey = newVKey; // newVKey will always be non-zero from UI selection

                    UnregisterAppHotkey(); // Always unregister the old one first

                    // Try to register the new hotkey
                    // If registration failed (g_hotkeyVKey will always be non-zero here for user selection)
                    if (!RegisterAppHotkey()) {
                        // Revert to old hotkey
                        g_hotkeyMod1 = oldMod1;
                        g_hotkeyMod2 = oldMod2;
                        g_hotkeyVKey = oldVKey;

                        // Re-register the old hotkey (it should succeed as it was working before)
                        RegisterAppHotkey();

                        // Update UI to reflect the reverted hotkey
                        PopulateHotkeyDropdowns(hWnd); // Re-populate to set correct selection
                        UpdateHotkeyDisplay(hWnd);
                        // Do NOT SaveSettings() as the attempt failed and we reverted
                    } else {
                        // Hotkey registration succeeded
                        UpdateHotkeyDisplay(hWnd);
                        SaveSettings(); // Save the new hotkey
                    }
                }
            }
            break;

        case WM_HSCROLL:
            if ((HWND)lParam == GetDlgItem(hWnd, IDC_POOL_SIZE_SLIDER)) {
                int newPoolSize = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                if (newPoolSize != g_poolSize) {
                    g_poolSize = newPoolSize;

                    UpdatePoolSizeDisplay(hWnd);

                    GenerateCells();
                    FilterCells();
                    if (g_hGridWnd) {
                        LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
                        InvalidateRect(g_hGridWnd, nullptr, TRUE);
                        UpdateWindow(g_hGridWnd);
                    }
                    SaveSettings();
                }
            }
            break;

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            g_hSettingsWnd = nullptr;
            break;

        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}


// Helper functions for color conversion and settings persistence

// Converts a hex string (RRGGBB) to a Gdiplus::Color (alpha is set to 255 for valid input)
Gdiplus::Color HexToColor(const std::wstring& hex) {
    std::wstring cleanHex = hex;
    if (cleanHex.length() > 0 && cleanHex[0] == L'#') {
        cleanHex = cleanHex.substr(1);
    }

    if (cleanHex.length() != 6) {
        return Gdiplus::Color(0, 0, 0, 0); // Return transparent black for invalid input
    }

    try {
        unsigned int rgb = std::stoul(cleanHex, nullptr, 16);
        BYTE r = (rgb >> 16) & 0xFF;
        BYTE g = (rgb >> 8) & 0xFF;
        BYTE b = rgb & 0xFF;
        return Gdiplus::Color(255, r, g, b); // Return with full alpha for valid conversion
    } catch (...) {
        return Gdiplus::Color(0, 0, 0, 0); // Return transparent black on error
    }
}

// Converts a Gdiplus::Color to a hex string (RRGGBB)
std::wstring ColorToHex(const Gdiplus::Color& color) {
    std::wstringstream ss;
    ss << std::hex << std::uppercase << std::setfill(L'0');
    ss << std::setw(2) << (int)color.GetR();
    ss << std::setw(2) << (int)color.GetG();
    ss << std::setw(2) << (int)color.GetB();
    return ss.str();
}

// Loads settings from the INI file
void LoadSettings() {
    wchar_t hexColor[10];
    GetPrivateProfileStringW(
        INI_SECTION,
        INI_KEY_CELL_COLOR,
        ColorToHex(DEFAULT_CELL_COLOR).c_str(), // Use default as fallback
        hexColor,
        sizeof(hexColor) / sizeof(hexColor[0]),
        g_iniFilePath.c_str()
    );
    Gdiplus::Color loadedColor = HexToColor(hexColor);
    if (loadedColor.GetA() != 0) { // Check if conversion was successful (not transparent black default)
        g_cellColor = Gdiplus::Color(g_cellColor.GetA(), loadedColor.GetR(), loadedColor.GetG(), loadedColor.GetB());
    } else {
        g_cellColor = DEFAULT_CELL_COLOR; // Fallback to default if INI value invalid
    }

    // Load Pool Size
    g_poolSize = (int)GetPrivateProfileIntW(
        INI_SECTION,
        INI_KEY_POOL_SIZE,
        DEFAULT_POOL_SIZE, // Use default as fallback
        g_iniFilePath.c_str()
    );
    // Ensure g_poolSize is within valid range
    if (g_poolSize < MIN_POOL_SIZE) g_poolSize = MIN_POOL_SIZE;
    if (g_poolSize > (int)POOL.length()) g_poolSize = (int)POOL.length();

    // Load Hotkey Modifiers and Virtual Key
    g_hotkeyMod1 = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_MOD1, DEFAULT_HOTKEY_MOD1, g_iniFilePath.c_str());
    g_hotkeyMod2 = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_MOD2, DEFAULT_HOTKEY_MOD2, g_iniFilePath.c_str());
    g_hotkeyVKey = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_VKEY, DEFAULT_HOTKEY_VKEY, g_iniFilePath.c_str());
    // The explicit check for g_hotkeyVKey == 0 has been removed as per your request.
}

// Saves current settings to the INI file
void SaveSettings() {
    std::wstring hexColor = ColorToHex(g_cellColor);
    WritePrivateProfileStringW(
        INI_SECTION,
        INI_KEY_CELL_COLOR,
        hexColor.c_str(),
        g_iniFilePath.c_str()
    );

    // Save Pool Size
    std::wstringstream ssPoolSize;
    ssPoolSize << g_poolSize;
    WritePrivateProfileStringW(
        INI_SECTION,
        INI_KEY_POOL_SIZE,
        ssPoolSize.str().c_str(),
        g_iniFilePath.c_str()
    );

    // Save Hotkey Modifiers and Virtual Key
    std::wstringstream ssMod1, ssMod2, ssVKey;
    ssMod1 << g_hotkeyMod1;
    ssMod2 << g_hotkeyMod2;
    ssVKey << g_hotkeyVKey;

    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_MOD1, ssMod1.str().c_str(), g_iniFilePath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_MOD2, ssMod2.str().c_str(), g_iniFilePath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_VKEY, ssVKey.str().c_str(), g_iniFilePath.c_str());
}


// Generate cells with double columns, right side with dot inserted after first char
void GenerateCells() {
    g_cells.clear();
    // Use g_poolSize to determine how many characters from POOL to use
    int currentPoolUsedSize = g_poolSize;

    for (int row = 0; row < currentPoolUsedSize; ++row) {
        wchar_t firstChar = POOL[row];
        for (int col = 0; col < currentPoolUsedSize; ++col) {
            wchar_t secondChar = POOL[col];

            // Left half (normal)
            Cell leftCell;
            leftCell.lbl = std::wstring() + firstChar + secondChar;
            g_cells.push_back(leftCell);

            // Right half (with dot)
            Cell rightCell;
            rightCell.lbl = std::wstring() + firstChar + L'.' + secondChar;
            g_cells.push_back(rightCell);
        }
    }
}

void FilterCells() {
    g_filtered.clear();
    if (g_state == SHOW_ALL || g_typed.empty()) {
        for (auto& c : g_cells)
            g_filtered.push_back(&c);
    } else {
        for (auto& c : g_cells) {
            if (c.lbl.rfind(g_typed, 0) == 0)
                g_filtered.push_back(&c);
        }
    }
}

// Layout with doubled columns horizontally, half normal, half dotted
void LayoutAndDraw(HWND hWnd, int W, int H) {
    using namespace Gdiplus;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize            = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth           = W;
    bmi.bmiHeader.biHeight          = -H;
    bmi.bmiHeader.biPlanes          = 1;
    bmi.bmiHeader.biBitCount        = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);

    Graphics mg(memDC);
    mg.SetSmoothingMode(SmoothingModeAntiAlias);
    mg.Clear(Color(0, 0, 0, 0));

    // Use the global cell color
    SolidBrush cellBrush(g_cellColor);

    Font          font(L"Arial", 11, FontStyleBold);
    SolidBrush      textBrush(Color(255, 0, 0, 0)); // Text color remains black

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);

    // Dynamic grid dimensions based on g_poolSize
    int rows = g_poolSize;
    int cols = g_poolSize * 2; // Still two columns (normal + dotted)

    float cellW = (float)W / cols;
    float cellH = (float)H / rows;

    // Recalculate cell positions based on the current g_poolSize
    // This is crucial because GenerateCells now creates a different number of cells.
    for (auto& c : g_cells) {
        std::wstring lbl = c.lbl;
        int firstCharIdx = (lbl.length() >= 1) ? (int)POOL.find(lbl[0]) : std::wstring::npos;

        if (lbl.length() == 2 && firstCharIdx != std::wstring::npos) { // Normal two-character label
            int secondCharIdx = (lbl.length() >= 2) ? (int)POOL.find(lbl[1]) : std::wstring::npos;
            if (secondCharIdx != std::wstring::npos && firstCharIdx < g_poolSize && secondCharIdx < g_poolSize) {
                 c.rc = {
                     LONG(secondCharIdx * cellW),
                     LONG(firstCharIdx * cellH),
                     LONG((secondCharIdx + 1) * cellW),
                     LONG((firstCharIdx + 1) * cellH)
                 };
            } else { c.rc = { -100, -100, -90, -90 }; } // Mark as invalid
        }
        else if (lbl.length() == 3 && lbl[1] == L'.' && firstCharIdx != std::wstring::npos) { // Dotted label
            int secondCharIdx = (lbl.length() >= 3) ? (int)POOL.find(lbl[2]) : std::wstring::npos;
            if (secondCharIdx != std::wstring::npos && firstCharIdx < g_poolSize && secondCharIdx < g_poolSize) {
                c.rc = {
                    LONG((secondCharIdx + g_poolSize) * cellW), // Shifted by g_poolSize for the right column
                    LONG(firstCharIdx * cellH),
                    LONG(((secondCharIdx + g_poolSize) + 1) * cellW),
                    LONG((firstCharIdx + 1) * cellH)
                };
            } else { c.rc = { -100, -100, -90, -90 }; } // Mark as invalid
        }
        else {
            c.rc = { -100, -100, -90, -90 }; // Mark as invalid
        }
    }


    for (auto c : g_filtered) {
        if (c->rc.left >= 0 && c->rc.top >= 0) {
            RECT rc = c->rc;
            RectF layoutRect((FLOAT)rc.left, (FLOAT)rc.top, (FLOAT)(rc.right - rc.left), (FLOAT)(rc.bottom - rc.top));

            RectF unlimitedRect(0, 0, 1000, layoutRect.Height);
            RectF textBounds;
            mg.MeasureString(c->lbl.c_str(), -1, &font, unlimitedRect, &textBounds);

            float bx = layoutRect.X + (layoutRect.Width - textBounds.Width) / 2 - 1;
            float by = layoutRect.Y + (layoutRect.Height - textBounds.Height) / 2 - 1;
            RectF boxRect(bx, by, textBounds.Width + 2, textBounds.Height + 2);

            // Use the global cell brush
            DrawRounded(mg, boxRect, &cellBrush);

            mg.DrawString(c->lbl.c_str(), -1, &font, boxRect, &sf, &textBrush);
        }
    }

    if (g_state == WAIT_CLICK && g_filtered.size() == 1) {
        Cell* sel = g_filtered[0];
        RECT rc = sel->rc;
        std::wstring promptText = L"1=Left 2=Right 3=Double";

        int promptMargin = 8;
        int promptWidth = 160;
        int promptHeight = 25;

        int px = rc.right + promptMargin;
        int py = rc.top + ((rc.bottom - rc.top) / 2) - (promptHeight / 2);

        if (px + promptWidth > W) {
            px = rc.left - promptWidth - promptMargin;
            if (px < 0) px = 0;
        }
        if (py < 0) py = 0;
        if (py + promptHeight > H) py = H - promptHeight;

        RectF promptRect((REAL)px, (REAL)py, (REAL)promptWidth, (REAL)promptHeight);
        SolidBrush promptBg(Color(255, 173, 216, 230)); // Prompt background remains fixed
        SolidBrush promptTextBrush(Color(255, 0, 0, 0));

        DrawRounded(mg, promptRect, &promptBg);

        Font promptFont(L"Arial", 10, FontStyleRegular);
        StringFormat promptFormat;
        promptFormat.SetAlignment(StringAlignmentNear);
        promptFormat.SetLineAlignment(StringAlignmentCenter);

        RectF promptTextRect = promptRect;
        promptTextRect.X += 6;

        mg.DrawString(promptText.c_str(), -1, &promptFont, promptTextRect, &promptFormat, &promptTextBrush);
    }

    POINT ptPos = { 0, 0 };
    SIZE sizeWnd = { W, H };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hWnd, screenDC, &ptPos, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void MoveToAndPrompt(Cell* c) {
    g_state = WAIT_CLICK;
    int x = (c->rc.left + c->rc.right) / 2;
    int y = (c->rc.top + c->rc.bottom) / 2;
    SetCursorPos(x, y);
    ShowWindow(g_hGridWnd, SW_SHOW);
    FilterCells();
    LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    InvalidateRect(g_hGridWnd, nullptr, TRUE);
    UpdateWindow(g_hGridWnd);
}

void SimClick(DWORD flags) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(input));
}