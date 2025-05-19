#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

HWND            g_hGridWnd    = nullptr;
enum GridState  { HIDDEN, SHOW_ALL, WAIT_CLICK } g_state = HIDDEN;
std::wstring    g_typed;
struct Cell { std::wstring lbl; RECT rc; };
std::vector<Cell>     g_cells;
std::vector<Cell*>    g_filtered;
const UINT      HOTKEY_ID   = 1;
const std::wstring POOL     = L"abcdefghijklmnopqrstuvwxyz0123456789";

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void    GenerateCells();
void    FilterCells();
void    LayoutAndDraw(HWND, int, int);
void    MoveToAndPrompt(Cell*);
void    SimClick(DWORD);

// Helper to draw a rounded rectangle with given brush
void DrawRounded(Gdiplus::Graphics& g, const Gdiplus::RectF& r, Gdiplus::Brush* brush) {
    using namespace Gdiplus;
    float radius = 4.0f;
    GraphicsPath path;
    path.AddArc(r.X,               r.Y,               radius, radius, 180, 90);
    path.AddArc(r.X + r.Width - radius, r.Y,               radius, radius, 270, 90);
    path.AddArc(r.X + r.Width - radius, r.Y + r.Height - radius, radius, radius,   0, 90);
    path.AddArc(r.X,               r.Y + r.Height - radius, radius, radius,  90, 90);
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

    const wchar_t CLASS_NAME[] = L"GridClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    g_hGridWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        CLASS_NAME, L"Grid Overlay", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr
    );

    GenerateCells();
    RegisterHotKey(g_hGridWnd, HOTKEY_ID, MOD_ALT, VK_F12);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(g_hGridWnd, HOTKEY_ID);
    DestroyWindow(g_hGridWnd);
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
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Generate cells with double columns, right side with dot inserted after first char
void GenerateCells() {
    g_cells.clear();
    int poolSize = (int)POOL.size();

    // We split columns horizontally: left half is normal two-char labels,
    // right half is first char + dot + second char
    // Total columns = poolSize * 2
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
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);

    Graphics mg(memDC);
    mg.SetSmoothingMode(SmoothingModeAntiAlias);
    mg.Clear(Color(0, 0, 0, 0));

    SolidBrush blueBrush(Color(128, 173, 216, 230));
    Font        font(L"Arial", 11, FontStyleBold);
    SolidBrush  textBrush(Color(255, 0, 0, 0));

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);  // Prevent text wrapping

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
                    LONG((row + 1) * cellH)
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
        RECT rc = c->rc;
        RectF layoutRect((FLOAT)rc.left, (FLOAT)rc.top, (FLOAT)(rc.right - rc.left), (FLOAT)(rc.bottom - rc.top));

        // Measure string with a very wide rectangle to get accurate width without wrapping
        RectF unlimitedRect(0, 0, 1000, layoutRect.Height);
        RectF textBounds;
        mg.MeasureString(c->lbl.c_str(), -1, &font, unlimitedRect, &textBounds);

        // Center the box rect inside the cell, make it wide enough to fit text on one line
        float bx = layoutRect.X + (layoutRect.Width - textBounds.Width) / 2 - 1;
        float by = layoutRect.Y + (layoutRect.Height - textBounds.Height) / 2 - 1;
        RectF boxRect(bx, by, textBounds.Width + 2, textBounds.Height + 2);

        DrawRounded(mg, boxRect, &blueBrush);
        mg.DrawString(c->lbl.c_str(), -1, &font, boxRect, &sf, &textBrush);
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
        SolidBrush promptBg(Color(255, 173, 216, 230));
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
