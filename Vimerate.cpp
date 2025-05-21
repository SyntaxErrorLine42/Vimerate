#ifndef UNICODE
#define UNICODE
#endif

// Ensure Unicode character support for Windows
#include "resource.h"    // App resource definitions (icons, IDs)
#include <windows.h>     // Core Windows API functions
#include <gdiplus.h>     // GDI+ graphics library
#include <vector>        // Dynamic array container (std::vector)
#include <string>        // String manipulation (std::wstring for Unicode)
#include <cmath>         // Mathematical functions
#include <shellapi.h>    // For Shell_NotifyIcon (system tray)
#include <fstream>       // File operations (for INI settings)
#include <sstream>       // String stream manipulation
#include <iomanip>       // For formatted output (setw, setfill)
#include <commdlg.h>     // Common dialogs (ChooseColor)
#include <shlobj.h>      // Shell utility functions
#include <commctrl.h>    // Common controls (trackbar, combobox)
#include <algorithm>     // Standard algorithms (sort, unique)

// Link necessary libraries for the project
#pragma comment(lib, "gdiplus.lib")   // Link GDI+ library
#pragma comment(lib, "Shell32.lib")   // Link Shell32 library
#pragma comment(lib, "ComCtl32.lib")  // Link Common Controls library

// --- Constants for System Tray Icon and Menu Items ---
#define WM_APP_NOTIFYICON (WM_APP + 1) // Custom message for tray icon events
#define IDM_EXIT          1001         // ID for 'Exit' menu item
#define IDM_SETTINGS      1002         // ID for 'Settings' menu item
// --- End Constants ---

// --- Constants and IDs for Settings Window Controls ---
const wchar_t SETTINGS_CLASS_NAME[] = L"SettingsClass"; // Window class name for settings

#define IDC_COLOR_LABEL         2001 // ID for color display label
#define IDC_CHOOSE_COLOR_BUTTON 2003 // ID for color picker button
#define IDC_POOL_SIZE_LABEL     2004 // ID for pool size text label
#define IDC_POOL_SIZE_SLIDER    2005 // ID for pool size slider control
#define IDC_POOL_SIZE_VALUE_LABEL 2006 // ID for pool size value display
#define IDC_RESET_BUTTON        2007 // ID for the Reset Button

// Hotkey Control IDs
#define IDC_HOTKEY_MOD1_COMBO   2008 // ID for first modifier combo box
#define IDC_HOTKEY_MOD2_COMBO   2009 // ID for second modifier combo box
#define IDC_HOTKEY_VKEY_COMBO   2010 // ID for virtual key combo box
#define IDC_HOTKEY_DISPLAY_LABEL 2011 // ID for hotkey display label

const wchar_t INI_SECTION[] = L"Settings";             // Section name in INI file
const wchar_t INI_KEY_CELL_COLOR[] = L"CellColor";     // INI key for cell color
const wchar_t INI_KEY_POOL_SIZE[] = L"PoolSize";       // INI key for pool size
const wchar_t INI_KEY_HOTKEY_MOD1[] = L"HotkeyMod1";   // INI key for first hotkey modifier
const wchar_t INI_KEY_HOTKEY_MOD2[] = L"HotkeyMod2";   // INI key for second hotkey modifier
const wchar_t INI_KEY_HOTKEY_VKEY[] = L"HotkeyVKey";   // INI key for hotkey virtual key
// --- End Constants ---

// Global window handles
HWND            g_hGridWnd = nullptr;     // Handle to main overlay window
HWND            g_hSettingsWnd = nullptr; // Handle to settings window

// Global application settings variables
Gdiplus::Color  g_cellColor(128, 173, 216, 230); // Current cell color (semi-transparent light blue)
const Gdiplus::Color DEFAULT_CELL_COLOR(128, 173, 216, 230); // Default cell color
const int DEFAULT_POOL_SIZE = 36;             // Default number of characters in pool

// Global hotkey variables (current active hotkey)
UINT g_hotkeyMod1 = MOD_WIN;    // First hotkey modifier (default: Win)
UINT g_hotkeyMod2 = MOD_SHIFT;  // Second hotkey modifier (default: Shift)
UINT g_hotkeyVKey = 'Z';        // Hotkey virtual key (default: 'Z')

// Default hotkey constants for reset
const UINT DEFAULT_HOTKEY_MOD1 = MOD_WIN;   // Default first modifier
const UINT DEFAULT_HOTKEY_MOD2 = MOD_SHIFT; // Default second modifier
const UINT DEFAULT_HOTKEY_VKEY = 'Z';       // Default virtual key

// Grid state enumeration
enum GridState { HIDDEN, SHOW_ALL, WAIT_CLICK } g_state = HIDDEN; // Current grid display state
std::wstring    g_typed;                // User's typed input string
struct Cell { std::wstring lbl; RECT rc; }; // Structure for a single grid cell
std::vector<Cell>     g_cells;        // All possible grid cells
std::vector<Cell*>    g_filtered;     // Cells matching user's input
const UINT      HOTKEY_ID   = 1;      // Unique ID for the registered hotkey
const std::wstring POOL     = L"abcdefghijklmnopqrstuvwxyz0123456789"; // Character pool

// Current pool size, initialized to full pool length
int             g_poolSize = (int)POOL.length();
const int MIN_POOL_SIZE = 6; // Minimum characters allowed in pool

// System tray notification icon data
NOTIFYICONDATAW g_nid = {};

// Full path to the settings INI file
std::wstring g_iniFilePath;

// --- Forward Declarations ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);          // Main window message handler
LRESULT CALLBACK SettingsWndProc(HWND, UINT, WPARAM, LPARAM);  // Settings window message handler
void    GenerateCells();                                       // Create all grid cells
void    FilterCells();                                         // Filter cells based on input
void    LayoutAndDraw(HWND, int, int);                         // Position and draw cells
void    MoveToAndPrompt(Cell*);                                // Move mouse and show click prompt
void    SimClick(DWORD);                                       // Simulate mouse click
void    UpdatePoolSizeDisplay(HWND hSettingsWnd);              // Update pool size label
void    UpdateHotkeyDisplay(HWND hSettingsWnd);                // Update hotkey display label
void    PopulateHotkeyDropdowns(HWND hSettingsWnd);            // Fill hotkey combo boxes
bool    RegisterAppHotkey();                                   // Register global hotkey
void    UnregisterAppHotkey();                                 // Unregister global hotkey

// Helper functions for settings and drawing
Gdiplus::Color HexToColor(const std::wstring& hex);            // Convert hex string to color
std::wstring ColorToHex(const Gdiplus::Color& color);          // Convert color to hex string
void LoadSettings();                                           // Load settings from INI
void SaveSettings();                                           // Save settings to INI
void ResetToDefaults(HWND hSettingsWnd);                       // Reset all settings to defaults

// Helper to draw a rounded rectangle
void DrawRounded(Gdiplus::Graphics& g, const Gdiplus::RectF& r, Gdiplus::Brush* brush) {
    using namespace Gdiplus; // Use GDI+ namespace
    float radius = 4.0f;     // Corner radius
    GraphicsPath path;       // Path to define shape
    // Add arcs for rounded corners
    path.AddArc(r.X,                  r.Y,                  radius, radius, 180, 90);
    path.AddArc(r.X + r.Width - radius, r.Y,                  radius, radius, 270, 90);
    path.AddArc(r.X + r.Width - radius, r.Y + r.Height - radius, radius, radius,   0, 90);
    path.AddArc(r.X,                  r.Y + r.Height - radius, radius, radius,  90, 90);
    path.CloseFigure();              // Close the path to form a shape
    g.FillPath(brush, &path);        // Fill the path with the specified brush
}

// --- Main Entry Point of the Application ---
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Initialize Common Controls for UI elements
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc); // Size of structure
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES; // Load trackbar and standard classes
    InitCommonControlsEx(&icc); // Perform initialization

    // Initialize GDI+ for graphics rendering
    Gdiplus::GdiplusStartupInput gdiplusInput; // GDI+ startup input
    ULONG_PTR token;                           // GDI+ startup token
    // Start GDI+, show error if fails
    if (Gdiplus::GdiplusStartup(&token, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        MessageBoxA(nullptr, "Failed to initialize GDI+.", "Error", MB_OK);
        return 1; // Exit with error
    }

    // --- Determine and create path for settings file ---
    wchar_t exePath[MAX_PATH];     // Buffer for executable path
    GetModuleFileNameW(nullptr, exePath, MAX_PATH); // Get EXE path

    std::wstring sExePath = exePath; // Convert to wstring
    size_t lastSlash = sExePath.rfind(L'\\'); // Find last backslash
    std::wstring exeDir; // Directory of EXE
    if (lastSlash != std::wstring::npos) {
        exeDir = sExePath.substr(0, lastSlash); // Extract directory
    } else {
        exeDir = L"."; // Assume current directory
    }

    g_iniFilePath = exeDir;         // Start INI path with EXE dir
    g_iniFilePath += L"\\Settings"; // Add 'Settings' subfolder
    CreateDirectoryW(g_iniFilePath.c_str(), nullptr); // Create the directory
    g_iniFilePath += L"\\VimerateSettings.ini"; // Add INI file name
    // --- End custom settings path determination ---

    LoadSettings(); // Load settings at application startup

    // --- Register Main Grid Window Class ---
    const wchar_t GRID_CLASS_NAME[] = L"GridClass"; // Name for grid window class
    WNDCLASSEXW wcGrid = {}; // Window class structure
    wcGrid.cbSize = sizeof(wcGrid); // Size of structure
    wcGrid.style = CS_HREDRAW | CS_VREDRAW; // Redraw on resize
    wcGrid.lpfnWndProc = WndProc; // Assign main window procedure
    wcGrid.hInstance = hInst; // Application instance
    wcGrid.hCursor = LoadCursor(nullptr, IDC_ARROW); // Default cursor
    wcGrid.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent background
    wcGrid.lpszClassName = GRID_CLASS_NAME; // Class name
    wcGrid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));   // Large app icon
    wcGrid.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)); // Small app icon
    RegisterClassExW(&wcGrid); // Register the window class
    // --- End Register Main Grid Window Class ---

    // --- Register Settings Window Class ---
    WNDCLASSEXW wcSettings = {}; // Settings window class structure
    wcSettings.cbSize = sizeof(wcSettings); // Size of structure
    wcSettings.style = CS_HREDRAW | CS_VREDRAW; // Redraw on resize
    wcSettings.lpfnWndProc = SettingsWndProc; // Assign settings window procedure
    wcSettings.hInstance = hInst; // Application instance
    wcSettings.hCursor = LoadCursor(nullptr, IDC_ARROW); // Default cursor
    wcSettings.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); // White background
    wcSettings.lpszClassName = SETTINGS_CLASS_NAME; // Class name
    wcSettings.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON));   // Large settings icon
    wcSettings.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)); // Small settings icon
    RegisterClassExW(&wcSettings); // Register the window class
    // --- End Register Settings Window Class ---

    // Create the main grid window (full screen, transparent overlay)
    g_hGridWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_APPWINDOW, // Extended styles
        GRID_CLASS_NAME, L"Vimerate", WS_POPUP, // Class, title, style
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), // Position and size
        nullptr, nullptr, hInst, nullptr // Parent, menu, instance, param
    );

    GenerateCells();     // Generate initial grid cells
    RegisterAppHotkey(); // Register application's global hotkey

    // --- Tray Icon Initialization ---
    g_nid.cbSize = sizeof(NOTIFYICONDATAW); // Size of structure
    g_nid.hWnd = g_hGridWnd;              // Window to receive messages
    g_nid.uID = 1;                        // Unique icon ID
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // Flags for icon, message, tooltip
    g_nid.uCallbackMessage = WM_APP_NOTIFYICON;      // Custom callback message
    g_nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APPICON)); // Icon handle
    wcscpy_s(g_nid.szTip, L"Vimerate");               // Tooltip text

    Shell_NotifyIconW(NIM_ADD, &g_nid); // Add icon to system tray
    // --- End Tray Icon Initialization ---

    // --- Main Message Loop ---
    MSG msg; // Message structure
    while (GetMessageW(&msg, nullptr, 0, 0)) { // Loop while messages exist
        TranslateMessage(&msg); // Translate keyboard messages
        DispatchMessageW(&msg); // Send message to window procedure
    }

    UnregisterAppHotkey(); // Unregister hotkey before exiting
    DestroyWindow(g_hGridWnd); // Destroy main window

    SaveSettings(); // Save current settings before exit

    // --- Delete Tray Icon before exiting ---
    Shell_NotifyIconW(NIM_DELETE, &g_nid); // Remove tray icon
    // --- End Tray Icon ---

    Gdiplus::GdiplusShutdown(token); // Shutdown GDI+
    return 0; // Indicate successful exit
}

// Helper to register the application's hotkey
bool RegisterAppHotkey() {
    UINT combinedModifiers = 0; // Combined hotkey modifiers
    // Combine modifiers (Win, Ctrl, Shift, Alt)
    if (g_hotkeyMod1 == MOD_WIN || g_hotkeyMod2 == MOD_WIN) combinedModifiers |= MOD_WIN;
    if (g_hotkeyMod1 == MOD_CONTROL || g_hotkeyMod2 == MOD_CONTROL) combinedModifiers |= MOD_CONTROL;
    if (g_hotkeyMod1 == MOD_SHIFT || g_hotkeyMod2 == MOD_SHIFT) combinedModifiers |= MOD_SHIFT;
    if (g_hotkeyMod1 == MOD_ALT || g_hotkeyMod2 == MOD_ALT) combinedModifiers |= MOD_ALT;

    if (g_hotkeyVKey != 0) { // If a virtual key is defined
        // Register the hotkey with Windows
        if (!RegisterHotKey(g_hGridWnd, HOTKEY_ID, combinedModifiers, g_hotkeyVKey)) {
            // Show warning if hotkey registration fails
            MessageBoxW(g_hGridWnd, L"Failed to register hotkey. It might be in use by another application.", L"Hotkey Warning", MB_OK | MB_ICONWARNING);
            return false; // Indicate failure
        }
    }
    return true; // Indicate success
}

// Helper to unregister the application's hotkey
void UnregisterAppHotkey() {
    UnregisterHotKey(g_hGridWnd, HOTKEY_ID); // Unregister hotkey by ID
}

// --- Main Window Procedure (WndProc) ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_HOTKEY: // Hotkey pressed message
        if (wParam == HOTKEY_ID) { // Check if it's our hotkey
            if (g_state == HIDDEN) { // If grid is hidden, show it
                g_state = SHOW_ALL; // Set state to show all cells
                g_typed.clear();    // Clear typed input
                FilterCells();      // Filter cells (shows all)
                ShowWindow(hWnd, SW_SHOW); // Show the window
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // Redraw
                SetForegroundWindow(hWnd);
                SetFocus(hWnd); // Fixing a glitch on some desktops
            } else { // If grid is visible, hide it
                g_state = HIDDEN;      // Set state to hidden
                ShowWindow(hWnd, SW_HIDE); // Hide the window
            }
        }
        break;

    case WM_APP_NOTIFYICON: // Tray icon message
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP: // Right-click on tray icon
        {
            POINT pt; // Mouse cursor position
            GetCursorPos(&pt); // Get cursor coordinates

            HMENU hMenu = CreatePopupMenu(); // Create pop-up menu
            if (hMenu) {
                // Add menu items
                AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"S&ettings");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"E&xit");
                SetMenuDefaultItem(hMenu, IDM_EXIT, FALSE); // Set exit as default

                SetForegroundWindow(hWnd); // Bring window to foreground for menu
                // Show pop-up menu at cursor position
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
                PostMessage(hWnd, WM_NULL, 0, 0); // Dummy message for menu dismissal
                DestroyMenu(hMenu); // Destroy menu
            }
        }
        break;
        }
        break;

    case WM_COMMAND: // Command message (menu item click)
        if (LOWORD(wParam) == IDM_SETTINGS) { // If 'Settings' clicked
            if (g_hSettingsWnd == nullptr) { // If settings window not open, create it
                g_hSettingsWnd = CreateWindowExW(
                    0, SETTINGS_CLASS_NAME, L"Vimerate Settings", // No extended style, class, title
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE, // Standard window style, visible
                    CW_USEDEFAULT, CW_USEDEFAULT, 450, 350, // Default pos, size
                    hWnd, nullptr, GetModuleHandle(nullptr), nullptr // Parent, menu, instance, param
                );
                // Controls will be created in SettingsWndProc's WM_CREATE
            } else { // If settings window exists, bring to foreground
                SetForegroundWindow(g_hSettingsWnd);
                ShowWindow(g_hSettingsWnd, SW_RESTORE); // Restore if minimized
            }
        }
        else if (LOWORD(wParam) == IDM_EXIT) { // If 'Exit' clicked
            DestroyWindow(hWnd); // Close main window
        }
        break;

    case WM_KEYDOWN: { // Key pressed message
        if (g_state == HIDDEN) // Ignore if grid is hidden
            break;
        if (wParam == VK_ESCAPE) { // If Escape key
            g_state = HIDDEN;      // Hide grid
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        if (g_state == WAIT_CLICK) { // If waiting for click
            if (wParam == '1') SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP); // Left click
            else if (wParam == '2') SimClick(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP); // Right click
            else if (wParam == '3') { // Double left click
                SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
                SimClick(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP);
            }
            g_state = HIDDEN;      // Hide grid after click
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        if (wParam == VK_BACK) { // If Backspace key
            if (!g_typed.empty()) { // If input exists, remove last char
                g_typed.pop_back();
                FilterCells();      // Re-filter cells
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // Redraw
            } else { // If no input, hide grid
                g_state = HIDDEN;
                ShowWindow(hWnd, SW_HIDE);
            }
            break;
        }
        BYTE kbState[256];      // Keyboard state array
        GetKeyboardState(kbState); // Get current keyboard state
        wchar_t buf[2];         // Buffer for converted char
        // Convert virtual key to Unicode char
        int result = ToUnicode((UINT)wParam, HIWORD(lParam), kbState, buf, 1, 0);
        // If char is valid and in pool or is '.'
        if (result == 1 && (POOL.find(buf[0]) != std::wstring::npos || buf[0] == L'.')) {
            g_typed += buf[0]; // Append char to typed string
            FilterCells();     // Filter cells
            if (g_typed.length() == 2 || g_typed.length() == 3) { // If 2 or 3 chars typed
                for (auto c : g_filtered) { // Find exact match
                    if (c->lbl == g_typed) {
                        MoveToAndPrompt(c); // Move mouse and prompt
                        break;
                    }
                }
            } else { // Otherwise, just redraw grid
                LayoutAndDraw(hWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            }
        }
        break;
    }

    case WM_DESTROY: // Window destroy message
        if (g_hSettingsWnd) { // If settings window is open
            DestroyWindow(g_hSettingsWnd); // Close settings window
            g_hSettingsWnd = nullptr;
        }
        PostQuitMessage(0); // Post quit message to exit app
        break;

    default: // Default message handling
        return DefWindowProcW(hWnd, message, wParam, lParam); // Pass to default handler
    }
    return 0; // Message handled
}

// Helper to update the text label for pool size display
void UpdatePoolSizeDisplay(HWND hSettingsWnd) {
    HWND hLabel = GetDlgItem(hSettingsWnd, IDC_POOL_SIZE_VALUE_LABEL); // Get label handle
    if (hLabel) { // If label exists
        std::wstringstream ss; // String stream for building text
        ss << L"Currently using " << g_poolSize << L" characters."; // Build display string
        SetWindowTextW(hLabel, ss.str().c_str()); // Set label text
    }
}

// Helper to update the text label for hotkey display
void UpdateHotkeyDisplay(HWND hSettingsWnd) {
    HWND hLabel = GetDlgItem(hSettingsWnd, IDC_HOTKEY_DISPLAY_LABEL); // Get label handle
    if (hLabel) { // If label exists
        std::wstring hotkeyString; // String to build hotkey text
        std::vector<std::wstring> activeModifiers; // Vector for modifier names

        // Collect active modifiers (Win, Ctrl, Shift, Alt)
        if (g_hotkeyMod1 == MOD_WIN || g_hotkeyMod2 == MOD_WIN) activeModifiers.push_back(L"Win");
        if (g_hotkeyMod1 == MOD_CONTROL || g_hotkeyMod2 == MOD_CONTROL) activeModifiers.push_back(L"Ctrl");
        if (g_hotkeyMod1 == MOD_SHIFT || g_hotkeyMod2 == MOD_SHIFT) activeModifiers.push_back(L"Shift");
        if (g_hotkeyMod1 == MOD_ALT || g_hotkeyMod2 == MOD_ALT) activeModifiers.push_back(L"Alt");

        // Sort and remove duplicate modifiers for consistent display
        std::sort(activeModifiers.begin(), activeModifiers.end());
        activeModifiers.erase(std::unique(activeModifiers.begin(), activeModifiers.end()), activeModifiers.end());

        // Append modifiers to hotkey string
        for (size_t i = 0; i < activeModifiers.size(); ++i) {
            hotkeyString += activeModifiers[i];
            if (i < activeModifiers.size() - 1) { hotkeyString += L" + "; } // Add '+' separator
        }

        if (g_hotkeyVKey != 0) { // If a virtual key is set
            if (!hotkeyString.empty()) { hotkeyString += L" + "; } // Add '+' if modifiers exist

            UINT scanCode = MapVirtualKeyW(g_hotkeyVKey, 0); // Convert VKey to scan code
            wchar_t keyName[256]; // Buffer for key name
            // Get human-readable key name (e.g., "Z", "F1")
            if (GetKeyNameTextW(scanCode << 16, keyName, sizeof(keyName) / sizeof(keyName[0]))) {
                hotkeyString += keyName; // Append key name
            } else { // Fallback for specific character keys
                if (g_hotkeyVKey >= 'A' && g_hotkeyVKey <= 'Z' || g_hotkeyVKey >= '0' && g_hotkeyVKey <= '9') {
                    hotkeyString += (wchar_t)g_hotkeyVKey; // Append character directly
                } else { // Fallback for other VKey codes
                    std::wstringstream ss;
                    ss << L"VKey_" << g_hotkeyVKey;
                    hotkeyString += ss.str();
                }
            }
        }
        // Set the final hotkey display text
        SetWindowTextW(hLabel, (L"Current Hotkey: " + hotkeyString).c_str());
    }
}

// Helper to populate the hotkey dropdowns in Settings Window
void PopulateHotkeyDropdowns(HWND hSettingsWnd) {
    // Get handles to modifier and VKey combo boxes
    HWND hMod1Combo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_MOD1_COMBO);
    HWND hMod2Combo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_MOD2_COMBO);
    HWND hVKeyCombo = GetDlgItem(hSettingsWnd, IDC_HOTKEY_VKEY_COMBO);

    // Clear existing items in combo boxes
    SendMessageW(hMod1Combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hMod2Combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(hVKeyCombo, CB_RESETCONTENT, 0, 0);

    // Populate Modifiers (with "None" option)
    struct { const wchar_t* name; UINT value; } modifiers[] = {
        {L"None", 0}, {L"Win", MOD_WIN}, {L"Ctrl", MOD_CONTROL},
        {L"Shift", MOD_SHIFT}, {L"Alt", MOD_ALT}
    };

    // Add modifiers to both combo boxes
    for (const auto& mod : modifiers) {
        int index = (int)SendMessageW(hMod1Combo, CB_ADDSTRING, 0, (LPARAM)mod.name);
        SendMessageW(hMod1Combo, CB_SETITEMDATA, index, (LPARAM)mod.value);
        index = (int)SendMessageW(hMod2Combo, CB_ADDSTRING, 0, (LPARAM)mod.name);
        SendMessageW(hMod2Combo, CB_SETITEMDATA, index, (LPARAM)mod.value);
    }

    // Select current g_hotkeyMod1 in first combo box
    int selectedIndex = 0;
    for (int i = 0; i < _countof(modifiers); ++i) {
        if (modifiers[i].value == g_hotkeyMod1) { selectedIndex = i; break; }
    }
    SendMessageW(hMod1Combo, CB_SETCURSEL, selectedIndex, 0);

    // Select current g_hotkeyMod2 in second combo box
    selectedIndex = 0;
    for (int i = 0; i < _countof(modifiers); ++i) {
        if (modifiers[i].value == g_hotkeyMod2) { selectedIndex = i; break; }
    }
    SendMessageW(hMod2Combo, CB_SETCURSEL, selectedIndex, 0);

    // Populate Virtual Keys (WITHOUT "None" option)
    std::vector<std::pair<std::wstring, UINT>> vkeys_data;
    // Add A-Z and 0-9
    for (wchar_t c = 'A'; c <= 'Z'; ++c) vkeys_data.push_back({std::wstring(1, c), c});
    for (wchar_t c = '0'; c <= '9'; ++c) vkeys_data.push_back({std::wstring(1, c), c});
    // Add F1-F12
    for (int i = 1; i <= 12; ++i) {
        std::wstringstream ss; ss << L"F" << i;
        vkeys_data.push_back({ss.str(), VK_F1 + (i - 1)});
    }
    // Add various special keys
    vkeys_data.push_back({L"Space", VK_SPACE}); vkeys_data.push_back({L"Enter", VK_RETURN});
    vkeys_data.push_back({L"Backspace", VK_BACK}); vkeys_data.push_back({L"Delete", VK_DELETE});
    vkeys_data.push_back({L"Insert", VK_INSERT}); vkeys_data.push_back({L"Home", VK_HOME});
    vkeys_data.push_back({L"End", VK_END}); vkeys_data.push_back({L"Page Up", VK_PRIOR});
    vkeys_data.push_back({L"Page Down", VK_NEXT}); vkeys_data.push_back({L"Tab", VK_TAB});
    vkeys_data.push_back({L"Escape", VK_ESCAPE}); vkeys_data.push_back({L"Num Lock", VK_NUMLOCK});
    vkeys_data.push_back({L"Scroll Lock", VK_SCROLL}); vkeys_data.push_back({L"Print Screen", VK_SNAPSHOT});
    vkeys_data.push_back({L"Pause", VK_PAUSE}); vkeys_data.push_back({L"Up Arrow", VK_UP});
    vkeys_data.push_back({L"Down Arrow", VK_DOWN}); vkeys_data.push_back({L"Left Arrow", VK_LEFT});
    vkeys_data.push_back({L"Right Arrow", VK_RIGHT}); vkeys_data.push_back({L"Numpad 0", VK_NUMPAD0});
    vkeys_data.push_back({L"Numpad 1", VK_NUMPAD1}); vkeys_data.push_back({L"Numpad 2", VK_NUMPAD2});
    vkeys_data.push_back({L"Numpad 3", VK_NUMPAD3}); vkeys_data.push_back({L"Numpad 4", VK_NUMPAD4});
    vkeys_data.push_back({L"Numpad 5", VK_NUMPAD5}); vkeys_data.push_back({L"Numpad 6", VK_NUMPAD6});
    vkeys_data.push_back({L"Numpad 7", VK_NUMPAD7}); vkeys_data.push_back({L"Numpad 8", VK_NUMPAD8});
    vkeys_data.push_back({L"Numpad 9", VK_NUMPAD9}); vkeys_data.push_back({L"Numpad *", VK_MULTIPLY});
    vkeys_data.push_back({L"Numpad +", VK_ADD}); vkeys_data.push_back({L"Numpad -", VK_SUBTRACT});
    vkeys_data.push_back({L"Numpad .", VK_DECIMAL}); vkeys_data.push_back({L"Numpad /", VK_DIVIDE});

    // Add all virtual keys to the combo box
    for (const auto& vk : vkeys_data) {
        int index = (int)SendMessageW(hVKeyCombo, CB_ADDSTRING, 0, (LPARAM)vk.first.c_str());
        SendMessageW(hVKeyCombo, CB_SETITEMDATA, index, (LPARAM)vk.second);
    }

    // Select current g_hotkeyVKey in the combo box
    selectedIndex = 0;
    for (size_t i = 0; i < vkeys_data.size(); ++i) {
        if (vkeys_data[i].second == g_hotkeyVKey) { selectedIndex = i; break; }
    }
    SendMessageW(hVKeyCombo, CB_SETCURSEL, selectedIndex, 0);
}

// Function to reset all settings to their default values
void ResetToDefaults(HWND hSettingsWnd) {
    g_cellColor = DEFAULT_CELL_COLOR; // Reset cell color
    g_poolSize = DEFAULT_POOL_SIZE;     // Reset pool size

    // Store current hotkey for potential rollback
    UINT oldMod1 = g_hotkeyMod1; UINT oldMod2 = g_hotkeyMod2; UINT oldVKey = g_hotkeyVKey;

    // Set hotkey to default values
    g_hotkeyMod1 = DEFAULT_HOTKEY_MOD1;
    g_hotkeyMod2 = DEFAULT_HOTKEY_MOD2;
    g_hotkeyVKey = DEFAULT_HOTKEY_VKEY;

    UnregisterAppHotkey(); // Unregister current hotkey
    if (!RegisterAppHotkey()) { // Try to register default hotkey
        // If default fails, revert to previous hotkey
        g_hotkeyMod1 = oldMod1; g_hotkeyMod2 = oldMod2; g_hotkeyVKey = oldVKey;
        RegisterAppHotkey(); // Re-register the working hotkey
        MessageBoxW(hSettingsWnd, L"Failed to register the default hotkey. Reverted to previous hotkey.", L"Hotkey Reset Warning", MB_OK | MB_ICONWARNING);
    }

    // Update settings window controls
    InvalidateRect(hSettingsWnd, nullptr, TRUE); // Redraw color preview
    UpdateWindow(hSettingsWnd); // Force immediate redraw
    SendMessage(GetDlgItem(hSettingsWnd, IDC_POOL_SIZE_SLIDER), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)g_poolSize); // Set slider position
    UpdatePoolSizeDisplay(hSettingsWnd); // Update pool size label

    PopulateHotkeyDropdowns(hSettingsWnd); // Repopulate and select hotkey dropdowns
    UpdateHotkeyDisplay(hSettingsWnd); // Update hotkey display label

    // Update main grid
    GenerateCells(); // Regenerate cells based on new settings
    FilterCells(); // Re-filter cells
    if (g_hGridWnd) { // If main window exists
        LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // Redraw grid
        InvalidateRect(g_hGridWnd, nullptr, TRUE);
        UpdateWindow(g_hGridWnd);
    }

    SaveSettings(); // Save updated (or reverted) settings
}


// Window Procedure for the Settings Window
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
        {
            // Define control layout constants
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
            const int comboHeight = 200; // Dropdown list height

            int currentY = padding; // Y-position for current control

            // --- Color Settings Section ---
            // "Current Cell Color:" label
            CreateWindowW(
                L"STATIC", L"Current Cell Color:",
                WS_CHILD | WS_VISIBLE | SS_LEFT, // Child, visible, left-aligned
                padding, currentY + (controlHeight - 20) / 2, labelWidth, 20, // Position and size
                hWnd, (HMENU)IDC_COLOR_LABEL, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            // "Choose Color..." button
            CreateWindowW(
                L"BUTTON", L"Choose Color...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, // Child, visible, push button
                padding + labelWidth + controlSpacing + previewWidth + 20, currentY, buttonWidth, controlHeight, // Position and size
                hWnd, (HMENU)IDC_CHOOSE_COLOR_BUTTON, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );
            currentY += controlHeight + padding; // Advance Y position

            // --- Character Pool Size Settings Section ---
            // "Number of Characters:" label
            CreateWindowW(
                L"STATIC", L"Number of Characters:",
                WS_CHILD | WS_VISIBLE | SS_LEFT, // Child, visible, left-aligned
                padding, currentY + (sliderHeight - 20) / 2, labelWidth + 20, 20, // Position and size
                hWnd, (HMENU)IDC_POOL_SIZE_LABEL, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            // Trackbar (Slider) control
            HWND hSlider = CreateWindowExW(
                0, TRACKBAR_CLASSW, L"", // No extended style, trackbar class, no text
                WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS, // Child, visible, auto ticks, no tick marks
                padding + labelWidth + 20 + controlSpacing, currentY, // Position
                sliderWidth, sliderHeight, // Size
                hWnd, (HMENU)IDC_POOL_SIZE_SLIDER, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            // Set slider range (min to max pool size)
            SendMessage(hSlider, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(MIN_POOL_SIZE, (int)POOL.length()));
            SendMessage(hSlider, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)g_poolSize); // Set current position
            SendMessage(hSlider, TBM_SETPAGESIZE, 0, 1); // Page increment
            SendMessage(hSlider, TBM_SETTICFREQ, 1, 0); // Tick frequency

            currentY += sliderHeight + padding; // Advance Y position

            // Static text for current pool size value
            CreateWindowW(
                L"STATIC", L"", // Static control, no initial text
                WS_CHILD | WS_VISIBLE | SS_LEFT, // Child, visible, left-aligned
                padding, currentY, labelWidth + sliderWidth, 20, // Position and size
                hWnd, (HMENU)IDC_POOL_SIZE_VALUE_LABEL, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );
            UpdatePoolSizeDisplay(hWnd); // Set initial display text

            currentY += 20 + padding; // Advance Y position

            // --- Hotkey Settings Section ---
            // "Hotkey Combination:" label
            CreateWindowW(
                L"STATIC", L"Hotkey Combination:",
                WS_CHILD | WS_VISIBLE | SS_LEFT, // Child, visible, left-aligned
                padding, currentY + (controlHeight - 20) / 2, labelWidth, 20, // Position and size
                hWnd, nullptr, GetModuleHandle(nullptr), nullptr // Parent, no ID
            );

            // Modifier 1 Dropdown (ComboBox)
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, // Child, visible, scroll, dropdown list style
                padding + labelWidth + controlSpacing, currentY, comboWidth, comboHeight, // Position and size
                hWnd, (HMENU)IDC_HOTKEY_MOD1_COMBO, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            // Modifier 2 Dropdown (ComboBox)
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, // Child, visible, scroll, dropdown list style
                padding + labelWidth + controlSpacing + comboWidth + controlSpacing, currentY, comboWidth, comboHeight, // Position and size
                hWnd, (HMENU)IDC_HOTKEY_MOD2_COMBO, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            // Virtual Key Dropdown (ComboBox)
            CreateWindowW(
                L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, // Child, visible, scroll, dropdown list style
                padding + labelWidth + controlSpacing + (comboWidth + controlSpacing) * 2, currentY, comboWidth + 20, comboHeight, // Position and size
                hWnd, (HMENU)IDC_HOTKEY_VKEY_COMBO, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );

            PopulateHotkeyDropdowns(hWnd); // Fill hotkey combo boxes
            currentY += controlHeight + padding; // Advance Y position

            // Label to display current hotkey combination
            CreateWindowW(
                L"STATIC", L"", // Static control, no initial text
                WS_CHILD | WS_VISIBLE | SS_LEFT, // Child, visible, left-aligned
                padding, currentY, labelWidth + (comboWidth + controlSpacing) * 3 + 20, 20, // Position and size
                hWnd, (HMENU)IDC_HOTKEY_DISPLAY_LABEL, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );
            UpdateHotkeyDisplay(hWnd); // Initial hotkey text update

            currentY += 20 + padding; // Advance Y position

            // --- Reset Button ---
            const int resetButtonWidth = 120; // Button width
            const int resetButtonHeight = 28; // Button height
            int windowWidth; // Settings window width
            RECT clientRect; // Client rectangle
            GetClientRect(hWnd, &clientRect); // Get client area dimensions
            windowWidth = clientRect.right - clientRect.left; // Calculate width

            CreateWindowW(
                L"BUTTON", L"Reset to Defaults",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, // Child, visible, push button
                windowWidth - padding - resetButtonWidth, currentY, resetButtonWidth, resetButtonHeight, // Position and size
                hWnd, (HMENU)IDC_RESET_BUTTON, GetModuleHandle(nullptr), nullptr // Parent, ID, instance
            );
        }
        break;

        case WM_PAINT: // Paint message for window redraw
        {
            PAINTSTRUCT ps; // Paint structure
            HDC hdc = BeginPaint(hWnd, &ps); // Start painting

            // --- Redraw Color Preview Rectangle ---
            const int padding = 20; // Layout padding
            const int controlHeight = 25; // Control height
            const int labelWidth = 150; // Label width
            const int previewWidth = 50; // Preview width
            const int previewHeight = 25; // Preview height
            const int controlSpacing = 10; // Spacing between controls

            int previewX = padding + labelWidth + controlSpacing; // Preview X position
            int previewY = padding; // Preview Y position
            // Rectangle for color preview
            RECT colorPreviewRect = {previewX, previewY, previewX + previewWidth, previewY + previewHeight};

            // Create solid brush with current cell color
            HBRUSH hBrush = CreateSolidBrush(RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB()));
            FillRect(hdc, &colorPreviewRect, hBrush); // Fill rectangle
            DeleteObject(hBrush); // Delete brush to prevent leaks

            FrameRect(hdc, &colorPreviewRect, (HBRUSH)GetStockObject(BLACK_BRUSH)); // Draw black frame
            EndPaint(hWnd, &ps); // End painting
        }
        break;

        case WM_COMMAND: // Control notification messages
            if (LOWORD(wParam) == IDC_CHOOSE_COLOR_BUTTON) { // Color button clicked
                static COLORREF customColors[16]; // Custom color array
                CHOOSECOLOR cc; // CHOOSECOLOR structure
                ZeroMemory(&cc, sizeof(cc)); // Initialize to zero
                cc.lStructSize = sizeof(cc); // Structure size
                cc.hwndOwner = hWnd; // Parent window
                cc.lpCustColors = customColors; // Custom color array pointer
                cc.Flags = CC_RGBINIT | CC_FULLOPEN; // Init with RGB, full dialog

                // Set initial color for dialog
                cc.rgbResult = RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB());

                if (ChooseColor(&cc)) { // If user selected color
                    // Update global cell color (preserve alpha)
                    g_cellColor = Gdiplus::Color(g_cellColor.GetA(), GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));

                    if (g_hGridWnd) { // If main grid window exists
                        InvalidateRect(g_hGridWnd, nullptr, TRUE); // Redraw grid
                        UpdateWindow(g_hGridWnd); // Force redraw
                    }

                    InvalidateRect(hWnd, nullptr, TRUE); // Redraw settings window
                    UpdateWindow(hWnd); // Force redraw
                    SaveSettings(); // Save new settings
                }
            } else if (LOWORD(wParam) == IDC_RESET_BUTTON) { // Reset button clicked
                ResetToDefaults(hWnd); // Call reset function
            } else if (HIWORD(wParam) == CBN_SELCHANGE) { // Combobox selection changed
                // Get selected modifier 1 value
                int selMod1Index = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD1_COMBO), CB_GETCURSEL, 0, 0);
                UINT newMod1 = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD1_COMBO), CB_GETITEMDATA, selMod1Index, 0);

                // Get selected modifier 2 value
                int selMod2Index = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD2_COMBO), CB_GETCURSEL, 0, 0);
                UINT newMod2 = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_MOD2_COMBO), CB_GETITEMDATA, selMod2Index, 0);

                // Get selected virtual key value
                int selVKeyIndex = (int)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_VKEY_COMBO), CB_GETCURSEL, 0, 0);
                UINT newVKey = (UINT)SendMessage(GetDlgItem(hWnd, IDC_HOTKEY_VKEY_COMBO), CB_GETITEMDATA, selVKeyIndex, 0);

                // Check if hotkey parts actually changed
                if (newMod1 != g_hotkeyMod1 || newMod2 != g_hotkeyMod2 || newVKey != g_hotkeyVKey) {
                    // Store current hotkey for rollback
                    UINT oldMod1 = g_hotkeyMod1;
                    UINT oldMod2 = g_hotkeyMod2;
                    UINT oldVKey = g_hotkeyVKey;

                    // Update global hotkey variables
                    g_hotkeyMod1 = newMod1;
                    g_hotkeyMod2 = newMod2;
                    g_hotkeyVKey = newVKey;

                    UnregisterAppHotkey(); // Unregister existing hotkey

                    // Try to register new hotkey
                    if (!RegisterAppHotkey()) {
                        // If new hotkey fails, revert to old
                        g_hotkeyMod1 = oldMod1;
                        g_hotkeyMod2 = oldMod2;
                        g_hotkeyVKey = oldVKey;

                        RegisterAppHotkey(); // Re-register working hotkey
                        // Update UI to show reverted hotkey
                        PopulateHotkeyDropdowns(hWnd); // Re-populate selections
                        UpdateHotkeyDisplay(hWnd); // Update display text
                        // Don't save: hotkey failed to register
                    } else {
                        // Hotkey registration succeeded
                        UpdateHotkeyDisplay(hWnd); // Update display text
                        SaveSettings(); // Save new hotkey
                    }
                }
            }
            break;

        case WM_HSCROLL: // Scroll bar (slider) message
            if ((HWND)lParam == GetDlgItem(hWnd, IDC_POOL_SIZE_SLIDER)) { // If it's our slider
                int newPoolSize = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); // Get slider position
                if (newPoolSize != g_poolSize) { // If pool size changed
                    g_poolSize = newPoolSize; // Update global pool size

                    UpdatePoolSizeDisplay(hWnd); // Update display label

                    GenerateCells(); // Re-generate grid cells
                    FilterCells(); // Re-filter cells
                    if (g_hGridWnd) { // If main grid window exists
                        LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // Redraw grid
                        InvalidateRect(g_hGridWnd, nullptr, TRUE); // Invalidate area
                        UpdateWindow(g_hGridWnd); // Force redraw
                    }
                    SaveSettings(); // Save new settings
                }
            }
            break;

        case WM_CLOSE: // Window close message
            ShowWindow(hWnd, SW_HIDE); // Hide window instead of destroying
            return 0; // Handled message, prevent default destroy

        case WM_DESTROY: // Window destroy message
            g_hSettingsWnd = nullptr; // Clear global handle
            break;

        default: // Default message handling
            return DefWindowProcW(hWnd, message, wParam, lParam); // Pass to default
    }
    return 0; // Message handled
}

// --- Helper functions for color conversion and settings persistence ---

// Converts hex string (RRGGBB) to Gdiplus::Color
Gdiplus::Color HexToColor(const std::wstring& hex) {
    std::wstring cleanHex = hex; // Copy string
    if (cleanHex.length() > 0 && cleanHex[0] == L'#') { // Remove '#' if present
        cleanHex = cleanHex.substr(1);
    }

    if (cleanHex.length() != 6) { // Check for valid length
        return Gdiplus::Color(0, 0, 0, 0); // Invalid: transparent black
    }

    try {
        unsigned int rgb = std::stoul(cleanHex, nullptr, 16); // Convert hex to int
        BYTE r = (rgb >> 16) & 0xFF; // Extract red
        BYTE g = (rgb >> 8) & 0xFF; // Extract green
        BYTE b = rgb & 0xFF; // Extract blue
        return Gdiplus::Color(255, r, g, b); // Valid: full alpha color
    } catch (...) { // Catch any conversion errors
        return Gdiplus::Color(0, 0, 0, 0); // Error: transparent black
    }
}

// Converts Gdiplus::Color to hex string (RRGGBB)
std::wstring ColorToHex(const Gdiplus::Color& color) {
    std::wstringstream ss; // String stream for hex
    ss << std::hex << std::uppercase << std::setfill(L'0'); // Hex format, uppercase, fill with '0'
    ss << std::setw(2) << (int)color.GetR(); // Red component
    ss << std::setw(2) << (int)color.GetG(); // Green component
    ss << std::setw(2) << (int)color.GetB(); // Blue component
    return ss.str(); // Return hex string
}

// Loads settings from the INI file
void LoadSettings() {
    wchar_t hexColor[10]; // Buffer for hex color string
    GetPrivateProfileStringW(
        INI_SECTION, INI_KEY_CELL_COLOR, // Section and key
        ColorToHex(DEFAULT_CELL_COLOR).c_str(), // Default as fallback
        hexColor, sizeof(hexColor) / sizeof(hexColor[0]), // Buffer and size
        g_iniFilePath.c_str() // INI file path
    );
    Gdiplus::Color loadedColor = HexToColor(hexColor); // Convert loaded hex
    if (loadedColor.GetA() != 0) { // Check if valid color loaded
        g_cellColor = Gdiplus::Color(g_cellColor.GetA(), loadedColor.GetR(), loadedColor.GetG(), loadedColor.GetB()); // Apply new color
    } else {
        g_cellColor = DEFAULT_CELL_COLOR; // Fallback to default
    }

    // Load Pool Size
    g_poolSize = (int)GetPrivateProfileIntW(
        INI_SECTION, INI_KEY_POOL_SIZE, // Section and key
        DEFAULT_POOL_SIZE, // Default as fallback
        g_iniFilePath.c_str() // INI file path
    );
    // Clamp pool size to valid range
    if (g_poolSize < MIN_POOL_SIZE) g_poolSize = MIN_POOL_SIZE;
    if (g_poolSize > (int)POOL.length()) g_poolSize = (int)POOL.length();

    // Load Hotkey Modifiers and Virtual Key
    g_hotkeyMod1 = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_MOD1, DEFAULT_HOTKEY_MOD1, g_iniFilePath.c_str());
    g_hotkeyMod2 = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_MOD2, DEFAULT_HOTKEY_MOD2, g_iniFilePath.c_str());
    g_hotkeyVKey = (UINT)GetPrivateProfileIntW(INI_SECTION, INI_KEY_HOTKEY_VKEY, DEFAULT_HOTKEY_VKEY, g_iniFilePath.c_str());
}

// Saves current settings to the INI file
void SaveSettings() {
    std::wstring hexColor = ColorToHex(g_cellColor); // Convert color to hex
    WritePrivateProfileStringW(
        INI_SECTION, INI_KEY_CELL_COLOR, // Section and key
        hexColor.c_str(), // Value to write
        g_iniFilePath.c_str() // INI file path
    );

    // Save Pool Size
    std::wstringstream ssPoolSize; // String stream for pool size
    ssPoolSize << g_poolSize; // Convert int to string
    WritePrivateProfileStringW(
        INI_SECTION, INI_KEY_POOL_SIZE, // Section and key
        ssPoolSize.str().c_str(), // Value to write
        g_iniFilePath.c_str() // INI file path
    );

    // Save Hotkey Modifiers and Virtual Key
    std::wstringstream ssMod1, ssMod2, ssVKey; // String streams for hotkey parts
    ssMod1 << g_hotkeyMod1; // Convert mod1 to string
    ssMod2 << g_hotkeyMod2; // Convert mod2 to string
    ssVKey << g_hotkeyVKey; // Convert vkey to string

    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_MOD1, ssMod1.str().c_str(), g_iniFilePath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_MOD2, ssMod2.str().c_str(), g_iniFilePath.c_str());
    WritePrivateProfileStringW(INI_SECTION, INI_KEY_HOTKEY_VKEY, ssVKey.str().c_str(), g_iniFilePath.c_str());
}

// Generate cells with double columns (normal and dotted)
void GenerateCells() {
    g_cells.clear(); // Clear existing cells
    int currentPoolUsedSize = g_poolSize; // Use current pool size

    for (int row = 0; row < currentPoolUsedSize; ++row) { // Iterate for first char
        wchar_t firstChar = POOL[row]; // Get first char
        for (int col = 0; col < currentPoolUsedSize; ++col) { // Iterate for second char
            wchar_t secondChar = POOL[col]; // Get second char

            // Create normal two-character cell
            Cell leftCell;
            leftCell.lbl = std::wstring() + firstChar + secondChar;
            g_cells.push_back(leftCell);

            // Create dotted three-character cell
            Cell rightCell;
            rightCell.lbl = std::wstring() + firstChar + L'.' + secondChar;
            g_cells.push_back(rightCell);
        }
    }
}

// Filter cells based on user's typed input
void FilterCells() {
    g_filtered.clear(); // Clear filtered list
    if (g_state == SHOW_ALL || g_typed.empty()) { // If showing all or no input
        for (auto& c : g_cells)
            g_filtered.push_back(&c); // Add all cells
    } else { // Filter based on input
        for (auto& c : g_cells) {
            if (c.lbl.rfind(g_typed, 0) == 0) // If label starts with typed string
                g_filtered.push_back(&c); // Add to filtered list
        }
    }
}

// Layout and draw grid cells on overlay window
void LayoutAndDraw(HWND hWnd, int W, int H) {
    using namespace Gdiplus; // Use GDI+ namespace

    HDC screenDC = GetDC(nullptr); // Get screen device context
    HDC memDC = CreateCompatibleDC(screenDC); // Create memory device context

    BITMAPINFO bmi = {}; // Bitmap info structure
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); // Structure size
    bmi.bmiHeader.biWidth = W; // Bitmap width
    bmi.bmiHeader.biHeight = -H; // Negative height for top-down DIB
    bmi.bmiHeader.biPlanes = 1; // Number of planes
    bmi.bmiHeader.biBitCount = 32; // 32 bits per pixel
    bmi.bmiHeader.biCompression = BI_RGB; // RGB compression

    void* bits = nullptr; // Pointer to bitmap bits
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0); // Create DIB section
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp); // Select bitmap into memory DC

    Graphics mg(memDC); // GDI+ graphics object
    mg.SetSmoothingMode(SmoothingModeAntiAlias); // Enable anti-aliasing
    mg.Clear(Color(0, 0, 0, 0)); // Clear with transparent black

    SolidBrush cellBrush(g_cellColor); // Brush for cell background (global color)
    Font font(L"Arial", 11, FontStyleBold); // Font for cell labels
    SolidBrush textBrush(Color(255, 0, 0, 0)); // Brush for text (black)

    StringFormat sf; // String format for text alignment
    sf.SetAlignment(StringAlignmentCenter); // Center horizontally
    sf.SetLineAlignment(StringAlignmentCenter); // Center vertically
    sf.SetFormatFlags(StringFormatFlagsNoWrap); // No text wrapping

    int rows = g_poolSize; // Number of rows in grid
    int cols = g_poolSize * 2; // Double columns (normal + dotted)

    float cellW = (float)W / cols; // Cell width
    float cellH = (float)H / rows; // Cell height

    // Recalculate cell positions based on g_poolSize
    for (auto& c : g_cells) { // Iterate through all cells
        std::wstring lbl = c.lbl; // Cell label
        int firstCharIdx = (lbl.length() >= 1) ? (int)POOL.find(lbl[0]) : std::wstring::npos; // Index of first char

        if (lbl.length() == 2 && firstCharIdx != std::wstring::npos) { // Normal two-char label
            int secondCharIdx = (lbl.length() >= 2) ? (int)POOL.find(lbl[1]) : std::wstring::npos; // Index of second char
            if (secondCharIdx != std::wstring::npos && firstCharIdx < g_poolSize && secondCharIdx < g_poolSize) {
                 c.rc = {
                     LONG(secondCharIdx * cellW), // X position
                     LONG(firstCharIdx * cellH), // Y position
                     LONG((secondCharIdx + 1) * cellW), // Right edge
                     LONG((firstCharIdx + 1) * cellH) // Bottom edge
                 };
            } else { c.rc = { -100, -100, -90, -90 }; } // Mark invalid
        }
        else if (lbl.length() == 3 && lbl[1] == L'.' && firstCharIdx != std::wstring::npos) { // Dotted label
            int secondCharIdx = (lbl.length() >= 3) ? (int)POOL.find(lbl[2]) : std::wstring::npos; // Index of third char (after dot)
            if (secondCharIdx != std::wstring::npos && firstCharIdx < g_poolSize && secondCharIdx < g_poolSize) {
                c.rc = {
                    LONG((secondCharIdx + g_poolSize) * cellW), // Shifted for right column
                    LONG(firstCharIdx * cellH), // Y position
                    LONG(((secondCharIdx + g_poolSize) + 1) * cellW), // Right edge
                    LONG((firstCharIdx + 1) * cellH) // Bottom edge
                };
            } else { c.rc = { -100, -100, -90, -90 }; } // Mark invalid
        }
        else {
            c.rc = { -100, -100, -90, -90 }; // Mark invalid
        }
    }

    // Draw filtered cells
    for (auto c : g_filtered) {
        if (c->rc.left >= 0 && c->rc.top >= 0) { // If cell is valid
            RECT rc = c->rc; // Cell rectangle
            RectF layoutRect((FLOAT)rc.left, (FLOAT)rc.top, (FLOAT)(rc.right - rc.left), (FLOAT)(rc.bottom - rc.top)); // GDI+ rectangle

            RectF unlimitedRect(0, 0, 1000, layoutRect.Height); // Large rect for measuring text
            RectF textBounds; // Bounds of text
            mg.MeasureString(c->lbl.c_str(), -1, &font, unlimitedRect, &textBounds); // Measure text size

            float bx = layoutRect.X + (layoutRect.Width - textBounds.Width) / 2 - 1; // Box X position
            float by = layoutRect.Y + (layoutRect.Height - textBounds.Height) / 2 - 1; // Box Y position
            RectF boxRect(bx, by, textBounds.Width + 2, textBounds.Height + 2); // Box around text

            DrawRounded(mg, boxRect, &cellBrush); // Draw rounded rectangle
            mg.DrawString(c->lbl.c_str(), -1, &font, boxRect, &sf, &textBrush); // Draw text
        }
    }

    if (g_state == WAIT_CLICK && g_filtered.size() == 1) { // If waiting for click and one cell
        Cell* sel = g_filtered[0]; // Selected cell
        RECT rc = sel->rc; // Selected cell rectangle
        std::wstring promptText = L"1=Left 2=Right 3=Double"; // Prompt text

        int promptMargin = 8; // Margin for prompt box
        int promptWidth = 160; // Prompt box width
        int promptHeight = 25; // Prompt box height

        int px = rc.right + promptMargin; // Prompt X position (right of cell)
        int py = rc.top + ((rc.bottom - rc.top) / 2) - (promptHeight / 2); // Prompt Y position (centered)

        if (px + promptWidth > W) { // If prompt goes off screen right
            px = rc.left - promptWidth - promptMargin; // Move to left of cell
            if (px < 0) px = 0; // Clamp to left edge
        }
        if (py < 0) py = 0; // Clamp to top edge
        if (py + promptHeight > H) py = H - promptHeight; // Clamp to bottom edge

        RectF promptRect((REAL)px, (REAL)py, (REAL)promptWidth, (REAL)promptHeight); // Prompt rectangle
        SolidBrush promptBg(Color(255, 173, 216, 230)); // Prompt background color
        SolidBrush promptTextBrush(Color(255, 0, 0, 0)); // Prompt text color

        DrawRounded(mg, promptRect, &promptBg); // Draw rounded prompt background

        Font promptFont(L"Arial", 10, FontStyleRegular); // Font for prompt text
        StringFormat promptFormat; // String format for prompt text
        promptFormat.SetAlignment(StringAlignmentNear); // Align near (left)
        promptFormat.SetLineAlignment(StringAlignmentCenter); // Center vertically

        RectF promptTextRect = promptRect; // Text rectangle
        promptTextRect.X += 6; // Indent text slightly

        mg.DrawString(promptText.c_str(), -1, &promptFont, promptTextRect, &promptFormat, &promptTextBrush); // Draw prompt text
    }

    POINT ptPos = { 0, 0 }; // Window position
    SIZE sizeWnd = { W, H }; // Window size
    POINT ptSrc = { 0, 0 }; // Source point for blitting
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA }; // Blending function for transparency
    // Update layered window with blended bitmap
    UpdateLayeredWindow(hWnd, screenDC, &ptPos, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp); // Restore old bitmap
    DeleteObject(hBmp); // Delete created bitmap
    DeleteDC(memDC); // Delete memory DC
    ReleaseDC(nullptr, screenDC); // Release screen DC
}

// Move mouse to cell and prompt for click
void MoveToAndPrompt(Cell* c) {
    g_state = WAIT_CLICK; // Set state to wait for click
    int x = (c->rc.left + c->rc.right) / 2; // Calculate center X
    int y = (c->rc.top + c->rc.bottom) / 2; // Calculate center Y
    SetCursorPos(x, y); // Set mouse cursor position
    ShowWindow(g_hGridWnd, SW_SHOW); // Show grid window
    FilterCells(); // Filter cells (shows only selected)
    LayoutAndDraw(g_hGridWnd, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)); // Redraw grid
    InvalidateRect(g_hGridWnd, nullptr, TRUE); // Invalidate window
    UpdateWindow(g_hGridWnd); // Force window update
}

// Simulate a mouse click
void SimClick(DWORD flags) {
    INPUT input = {}; // Input event structure
    input.type = INPUT_MOUSE; // Event type: mouse
    input.mi.dwFlags = flags; // Mouse event flags (e.g., left down/up)
    SendInput(1, &input, sizeof(input)); // Send input event
}