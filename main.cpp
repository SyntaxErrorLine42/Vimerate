#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>
#include <shellapi.h> // Required for Shell_NotifyIcon
#include <fstream>      // Required for file operations (INI)
#include <sstream>      // Required for string stream manipulation
#include <iomanip>      // Required for std::setw and std::setfill
#include <commdlg.h>    // Required for ChooseColor
#include <shlobj.h>     // Required for SHGetFolderPathW (still included for general utility, though not strictly used for INI path in this version)

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shell32.lib") 

// --- Constants for System Tray Icon and Menu Items ---
#define WM_APP_NOTIFYICON (WM_APP + 1)
#define IDM_EXIT        1001
#define IDM_SETTINGS    1002
// --- End Constants ---

// --- Constants and IDs for Settings Window Controls ---
const wchar_t SETTINGS_CLASS_NAME[] = L"SettingsClass";
#define IDC_COLOR_LABEL         2001
#define IDC_CHOOSE_COLOR_BUTTON 2003
const wchar_t INI_SECTION[] = L"Settings";
const wchar_t INI_KEY_CELL_COLOR[] = L"CellColor";
// --- End Constants ---

HWND            g_hGridWnd  = nullptr;
HWND            g_hSettingsWnd = nullptr;

// Global variable for the cell color (Default light blue with alpha)
Gdiplus::Color  g_cellColor(128, 173, 216, 230); // Alpha is 128 (half-transparent)

enum GridState  { HIDDEN, SHOW_ALL, WAIT_CLICK } g_state = HIDDEN;
std::wstring    g_typed;
struct Cell { std::wstring lbl; RECT rc; };
std::vector<Cell>     g_cells;
std::vector<Cell*>    g_filtered;
const UINT      HOTKEY_ID   = 1;
const std::wstring POOL     = L"abcdefghijklmnopqrstuvwxyz0123456789";

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

// Helper functions for color and settings persistence
Gdiplus::Color HexToColor(const std::wstring& hex);
std::wstring ColorToHex(const Gdiplus::Color& color);
void LoadSettings();
void SaveSettings();


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
        // Fallback: If no slash found, assume current directory
        exeDir = L"."; 
    }

    g_iniFilePath = exeDir;
    g_iniFilePath += L"\\Settings"; // Add the Settings subfolder

    // Create the 'Settings' directory if it doesn't exist.
    // CreateDirectoryW returns non-zero on success, zero on failure.
    // Failure might mean it already exists (which is fine), or permission issues.
    CreateDirectoryW(g_iniFilePath.c_str(), nullptr); 

    g_iniFilePath += L"\\GridOverlaySettings.ini"; // Add the INI filename
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
    RegisterClassExW(&wcSettings);
    // --- End Register Settings Window Class ---


    g_hGridWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_APPWINDOW,
        GRID_CLASS_NAME, L"Grid Overlay", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr
    );

    GenerateCells();
    RegisterHotKey(g_hGridWnd, HOTKEY_ID, MOD_ALT, VK_F12);

    // --- Tray Icon Initialization ---
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hGridWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_NOTIFYICON;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Grid Overlay");

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    // --- End Tray Icon Initialization ---

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(g_hGridWnd, HOTKEY_ID);
    DestroyWindow(g_hGridWnd);

    // Save settings before exiting
    SaveSettings();

    // --- Delete Tray Icon before exiting ---
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    // --- End Tray Icon ---

    Gdiplus::GdiplusShutdown(token);
    return 0;
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
                    L"Grid Overlay Settings",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE, // Ensure it's visible on creation
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    400, 150, // Adjusted initial size for better layout
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
            const int buttonWidth = 140; // Increased width for "Choose Color..."
            const int controlSpacing = 10;

            int currentY = padding;

            // Create static label "Current Cell Color:"
            CreateWindowW(
                L"STATIC", L"Current Cell Color:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                padding, currentY + (controlHeight - 20) / 2, labelWidth, 20, // Vertically center text
                hWnd, (HMENU)IDC_COLOR_LABEL, GetModuleHandle(nullptr), nullptr
            );

            currentY += controlHeight + padding; // Move to the next row

            // Create "Choose Color..." button
            CreateWindowW(
                L"BUTTON", L"Choose Color...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                padding, currentY, buttonWidth, controlHeight,
                hWnd, (HMENU)IDC_CHOOSE_COLOR_BUTTON, GetModuleHandle(nullptr), nullptr
            );
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // Define the preview rect using the same layout constants
            const int padding = 20;
            const int controlHeight = 25;
            const int labelWidth = 150;
            const int previewWidth = 50;
            const int previewHeight = 25;
            const int controlSpacing = 10;

            int currentY_for_preview_row = padding; // This is the Y-coordinate of the row containing the label and preview
            int previewX = padding + labelWidth + controlSpacing;
            int previewY = currentY_for_preview_row; // Top of the preview rect
            RECT colorPreviewRect = {previewX, previewY, previewX + previewWidth, previewY + previewHeight};

            // Draw the color preview rectangle
            HBRUSH hBrush = CreateSolidBrush(RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB()));
            FillRect(hdc, &colorPreviewRect, hBrush);
            DeleteObject(hBrush);

            // Draw a border around it for visual separation
            FrameRect(hdc, &colorPreviewRect, (HBRUSH)GetStockObject(BLACK_BRUSH)); 

            EndPaint(hWnd, &ps);
        }
        break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CHOOSE_COLOR_BUTTON) {
                static COLORREF customColors[16]; // Stores custom colors
                CHOOSECOLOR cc;
                ZeroMemory(&cc, sizeof(cc));
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hWnd;
                cc.lpCustColors = customColors;
                cc.Flags = CC_RGBINIT | CC_FULLOPEN; // CC_RGBINIT uses initial color, CC_FULLOPEN shows custom color controls
                
                // Initialize with current color (Gdiplus::Color to COLORREF)
                cc.rgbResult = RGB(g_cellColor.GetR(), g_cellColor.GetG(), g_cellColor.GetB());

                if (ChooseColor(&cc)) {
                    // Update global color, preserving alpha
                    g_cellColor = Gdiplus::Color(g_cellColor.GetA(), GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));

                    // Redraw the main grid window to show new color
                    if (g_hGridWnd) {
                        InvalidateRect(g_hGridWnd, nullptr, TRUE);
                        UpdateWindow(g_hGridWnd);
                    }

                    // Redraw the settings window to update the color preview
                    InvalidateRect(hWnd, nullptr, TRUE); 
                    UpdateWindow(hWnd); 

                    // Save the new setting immediately
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
        L"ADDAE6", // Default hex for our light blue
        hexColor,
        sizeof(hexColor) / sizeof(hexColor[0]),
        g_iniFilePath.c_str() // Use the determined INI file path
    );

    Gdiplus::Color loadedColor = HexToColor(hexColor);

    if (loadedColor.GetA() != 0) {
        g_cellColor = Gdiplus::Color(g_cellColor.GetA(), loadedColor.GetR(), loadedColor.GetG(), loadedColor.GetB());
    }
}

// Saves current settings to the INI file
void SaveSettings() {
    std::wstring hexColor = ColorToHex(g_cellColor);

    WritePrivateProfileStringW(
        INI_SECTION,
        INI_KEY_CELL_COLOR,
        hexColor.c_str(),
        g_iniFilePath.c_str() // Use the determined INI file path
    );
}


// Generate cells with double columns, right side with dot inserted after first char
void GenerateCells() {
    g_cells.clear();
    int poolSize = (int)POOL.size();

    for (int row = 0; row < poolSize; ++row) {
        wchar_t firstChar = POOL[row];
        for (int col = 0; col < poolSize; ++col) {
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

    Font        font(L"Arial", 11, FontStyleBold);
    SolidBrush    textBrush(Color(255, 0, 0, 0)); // Text color remains black

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);

    int count = (int)g_filtered.size();

    if (count > 1) {
        int rows = (int)POOL.size();
        int cols = (int)POOL.size() * 2;

        float cellW = (float)W / cols;
        float cellH = (float)H / rows;

        for (auto c : g_filtered) {
            std::wstring lbl = c->lbl;
            if (lbl.length() == 2) {
                wchar_t first = lbl[0];
                wchar_t second = lbl[1];
                int row = (int)POOL.find(first);
                int col = (int)POOL.find(second);
                RECT rc = {
                    LONG(col * cellW),
                    LONG(row * cellH),
                    LONG((col + 1) * cellW),
                    LONG((row + 1) * cellH)
                };
                c->rc = rc;
            }
            else if (lbl.length() == 3 && lbl[1] == L'.') {
                wchar_t first = lbl[0];
                wchar_t second = lbl[2];
                int row = (int)POOL.find(first);
                int col = (int)POOL.find(second) + (int)POOL.size();
                RECT rc = {
                    LONG(col * cellW),
                    LONG(row * cellH),
                    LONG((col + 1) * cellW),
                    LONG((row + 1) * cellH) // FIX: Corrected from (col + 1) * cellH
                };
                c->rc = rc;
            }
            else {
                RECT rc = { -100, -100, -90, -90 };
                c->rc = rc;
            }
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