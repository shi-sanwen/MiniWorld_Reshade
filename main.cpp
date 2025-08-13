#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winreg.h>
#include <tlhelp32.h>
#include <urlmon.h>
#include <psapi.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <memory>
#include <iostream>
#include <gdiplus.h>
#include <dwmapi.h>
#include <versionhelpers.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "dwmapi.lib")

// 资源 ID
#define IDC_OUTPUT_TEXT 101
#define IDC_PROGRESS_BAR 102
#define IDC_START_SCAN 103
#define IDC_PAUSE_SCAN 104
#define IDC_STOP_SCAN 105
#define IDC_INSTALL_BUTTON 106
#define IDC_ADD_CUSTOM 107
#define IDC_DOWNLOAD_PRESET 108
#define IDC_YIRIXC_CHECKBOX 109
#define IDC_TIANXINGDASHE_CHECKBOX 110
#define IDC_VERSION_GROUP 111
#define IDC_STATUS_BAR 112
#define IDC_MINIMIZE_BUTTON 113

// 自定义消息
#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_LOG_MESSAGE (WM_USER + 2)

// 现代化颜色方案
#define COLOR_BG RGB(255, 255, 255)                // 背景色
#define COLOR_PRIMARY RGB(79, 70, 229)             // 主色调 - 靛蓝色
#define COLOR_PRIMARY_LIGHT RGB(129, 140, 248)     // 浅主色调
#define COLOR_PRIMARY_DARK RGB(67, 56, 202)        // 深主色调
#define COLOR_SECONDARY RGB(236, 72, 153)          // 次要色调 - 粉色
#define COLOR_TEXT RGB(0, 0, 0)                    // 文本颜色
#define COLOR_TEXT_LIGHT RGB(0, 0, 0)              // 浅文本颜色
#define COLOR_BUTTON_NORMAL RGB(173, 216, 230)     // 按钮正常颜色
#define COLOR_BUTTON_HOVER RGB(173, 216, 230)      // 按钮悬停颜色
#define COLOR_BUTTON_PRESSED RGB(173, 216, 230)    // 按钮按下颜色
#define COLOR_PROGRESS_BAR COLOR_PRIMARY           // 进度条颜色
#define COLOR_BORDER RGB(0, 0, 0)                  // 边框颜色
#define COLOR_TITLE_BAR RGB(192, 192, 192)         // 标题栏颜色
#define COLOR_TITLE_TEXT COLOR_PRIMARY             // 标题文字颜色
#define COLOR_SUCCESS RGB(34, 197, 94)             // 成功颜色
#define COLOR_WARNING RGB(234, 179, 8)             // 警告颜色
#define COLOR_ERROR RGB(239, 68, 68)               // 错误颜色
#define COLOR_INFO RGB(59, 130, 246)               // 信息颜色

// 添加文字动画相关结构和变量
struct AnimatedText {
    std::wstring text;
    std::vector<float> charOpacity;
    std::vector<float> charOffsetY;
    bool isAnimating = false;
    int animationStep = 0;
    int totalSteps = 0;
};

// 全局变量
HWND g_hWnd = NULL;
HWND g_hOutputText = NULL;
HWND g_hProgressBar = NULL;
HWND g_hVersionGroup = NULL;
HWND g_hStatusBar = NULL;
std::map<std::wstring, std::wstring> g_detectedVersions;
bool g_scanning = false;
bool g_scanPaused = false;
std::chrono::steady_clock::time_point g_scanStartTime;
std::vector<std::wstring> g_failedInstallations;
int g_retryCount = 0;
std::wstring g_applicationPath;
HANDLE g_scanThread = NULL;
HANDLE g_installThread = NULL;
bool g_yirixcChecked = false;
bool g_tianxingdasheChecked = false;
HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontTitle = NULL;
HFONT g_hFontLarge = NULL;
HBRUSH g_hBackgroundBrush = NULL;
ULONG_PTR g_gdiplusToken;
std::map<HWND, bool> g_buttonHoverState;
std::map<HWND, bool> g_buttonPressedState;

// 修改全局变量部分，优化动画参数
bool g_animationActive = true;
int g_animationStep = 0;
int g_animationMaxSteps = 120; // 增加动画步数，使动画更平滑
int g_windowStartX = 0;
int g_windowTargetX = 0;
int g_windowY = 0;
int g_windowWidth = 1200;
int g_windowHeight = 700;
int g_windowRadius = 12;  // 窗口圆角半径
UINT_PTR g_animationTimerId = 1;
COLORREF g_currentGradientColors[4] = {
    RGB(79, 70, 229),   // 靛蓝色
    RGB(236, 72, 153),  // 粉色
    RGB(245, 158, 11),  // 琥珀色
    RGB(255, 255, 255)  // 白色
};
float g_colorBlendFactor = 0.0f;
HDC g_memDC = NULL;      // 添加内存DC用于双缓冲
HBITMAP g_memBitmap = NULL; // 添加内存位图用于双缓冲
HBITMAP g_oldBitmap = NULL; // 保存原始位图

std::map<HWND, AnimatedText> g_animatedTexts;
UINT_PTR g_textAnimationTimerId = 2;

// 日志文件路径
std::wofstream g_logFile;

// 函数声明

// UI初始化和设置相关函数
void SetupUI(HWND hWnd);                                    // 设置主界面控件
void CreateCustomFonts();                                   // 创建自定义字体
void CleanupResources();                                    // 清理资源
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius, COLORREF color, bool filled); // 绘制圆角矩形
void SetStatusText(const std::wstring& text);               // 设置状态栏文本
HWND CreateCustomButton(HWND hParent, LPCWSTR text, int x, int y, int width, int height, HMENU hMenu); // 创建自定义按钮

// 日志和输出相关函数
void LogOutput(const std::wstring& message);                // 输出日志信息
void SetupLogging();                                        // 设置日志记录

// 扫描和检测相关函数
void StartScan();                                           // 开始扫描游戏路径
void PauseScan();                                           // 暂停扫描
void StopScan();                                            // 停止扫描
void AddCustomPath();                                       // 添加自定义安装路径
void CheckAllVersions();                                    // 检查所有已安装的游戏版本
void ScanForMicroMiniNew();                                 // 扫描MicroMiniNew游戏文件
bool WindowsIndexSearch();                                  // 使用Windows索引搜索游戏
void FullDiskScan();                                        // 全盘扫描寻找游戏
void AddDetectedVersion(const std::wstring& version, const std::wstring& path); // 添加检测到的游戏版本
void PopulateOptions();                                     // 填充游戏版本选项

// 线程处理函数
DWORD WINAPI ScanThreadProc(LPVOID lpParameter);            // 扫描线程处理函数
DWORD WINAPI InstallThreadProc(LPVOID lpParameter);         // 安装线程处理函数

// 安装和下载相关函数
void DownloadPresets();                                     // 下载预设光影包
void InstallAction();                                       // 执行安装操作
void StartInstallation(const std::wstring& installPath);    // 开始安装过程
void OnInstallationComplete(bool success);                  // 安装完成回调
void DownloadAndExtractPreset(const std::wstring& url, const std::wstring& installPath, const std::wstring& presetName); // 下载并解压预设
bool ExtractZipWith7z(const std::wstring& zipPath, const std::wstring& extractPath); // 使用7z解压ZIP文件
void ScheduleRebootAndInstall();                            // 安排重启并安装

// 系统和权限相关函数
bool IsAdmin();                                             // 检查是否具有管理员权限
std::wstring GetWindowsVersion();                           // 获取Windows版本信息
std::vector<std::wstring> CheckRunningProcesses(const std::wstring& installPath); // 检查运行中的游戏进程
bool TerminateProcessByPath(const std::wstring& processPath); // 根据路径终止进程
std::wstring CheckRegistryAndExtract(const std::wstring& registryKeyPath); // 从注册表检查并提取路径

// 自定义控件窗口过程函数
LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);   // 自定义按钮窗口过程
LRESULT CALLBACK CustomGroupBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData); // 自定义分组框窗口过程
LRESULT CALLBACK CustomProgressProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData); // 自定义进度条窗口过程
LRESULT CALLBACK CustomCheckboxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);  // 自定义复选框窗口过程

// 添加新函数声明

// 动画和视觉效果相关函数
COLORREF BlendColors(COLORREF color1, COLORREF color2, float factor);        // 混合两种颜色
void DrawGradientRect(HDC hdc, RECT rect, COLORREF colorStart, COLORREF colorEnd); // 绘制渐变矩形
double EaseInOutQuad(double t);                                              // 平滑的二次缓动函数
double EaseOutBack(double t);                                                // 弹性缓动函数
void AnimateText(HWND hWnd, const std::wstring& text);                       // 为文字添加动画效果
void UpdateTextAnimation(HWND hWnd);                                         // 更新文字动画
void DrawAnimatedText(HDC hdc, const AnimatedText& anim, RECT rect, COLORREF textColor, HFONT hFont); // 绘制带动画效果的文字
void DrawShadow(HDC hdc, RECT rect, int radius, int strength);               // 绘制阴影效果
void DrawGlassEffect(HDC hdc, RECT rect, int radius);                        // 绘制玻璃效果
void DrawModernUI(HDC hdc, RECT rect);                                       // 绘制现代UI
void DrawRoundedRectPath(Gdiplus::GraphicsPath& path, float x, float y, float width, float height, float radius); // 绘制圆角矩形路径

// 窗口控制相关函数
void AddCustomWindowControls(HWND hWnd);                                     // 添加自定义窗口控制按钮
LRESULT CALLBACK CustomCloseButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);  // 自定义关闭按钮窗口过程
LRESULT CALLBACK CustomMinimizeButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData); // 自定义最小化按钮窗口过程
void EnableBlurBehind(HWND hWnd);                                            // 启用窗口模糊效果



//【K24T3Q4T3W】注册表相关代码 {

// 新增函数声明 - 用于修改和运行注册表文件
bool ModifyRegistryFile(const std::wstring& installPath);
bool RunRegistryFile(const std::wstring& regFilePath);

// 修改注册表文件 - 创建或修改注册表文件，将游戏路径写入其中
bool ModifyRegistryFile(const std::wstring& installPath) {
    // 构建注册表文件路径
    std::wstring regFilePath = installPath + L"\\1.reg";

    LogOutput(L"开始修改注册表文件: " + regFilePath);

    // 1. 检查安装目录是否存在
    if (!PathFileExists(installPath.c_str())) {
        LogOutput(L"安装目录不存在: " + installPath);
        return false;
    }

    // 2. 构建游戏主程序路径
    std::wstring gameExePath = installPath + L"\\2.exe";
    LogOutput(L"游戏主程序路径: " + gameExePath);

    // 3. 转义路径中的反斜杠字符
    std::wstring escapedPath;
    for (wchar_t c : gameExePath) {
        escapedPath += c;
        if (c == L'\\') escapedPath += L"\\"; // 添加额外的反斜杠进行转义
    }

    // 4. 构建注册表文件内容
    std::wstring regContent =
        L"Windows Registry Editor Version 5.00\r\n"
        L"\r\n"
        L"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\minigameapp.exe]\r\n"
        L"\"Debugger\"=\"" + escapedPath + L"\"\r\n";

    // 5. 创建文件句柄用于写入
    HANDLE hFile = CreateFile(regFilePath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogOutput(L"创建文件失败: " + std::to_wstring(GetLastError()));
        return false;
    }

    // 6. 写入UTF-16 BOM (0xFEFF)以标识Unicode编码
    const WCHAR BOM = 0xFEFF;
    DWORD bytesWritten;
    WriteFile(hFile, &BOM, sizeof(BOM), &bytesWritten, NULL);

    // 7. 写入注册表内容到文件
    WriteFile(hFile, regContent.c_str(),
        regContent.size() * sizeof(WCHAR), &bytesWritten, NULL);

    CloseHandle(hFile);

    LogOutput(L"注册表文件修改完成");
    return true;
}

// 运行注册表文件 - 通过regedit.exe导入注册表文件到系统注册表
bool RunRegistryFile(const std::wstring& regFilePath) {
    LogOutput(L"开始运行注册表文件: " + regFilePath);

    // 检查注册表文件是否存在
    if (!PathFileExists(regFilePath.c_str())) {
        LogOutput(L"注册表文件不存在: " + regFilePath);
        return false;
    }

    // 构建命令行参数，使用regedit.exe静默导入注册表文件
    std::wstring command = L"regedit.exe /s \"" + regFilePath + L"\"";

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // 隐藏窗口运行
    ZeroMemory(&pi, sizeof(pi));

    // 创建进程运行regedit
    if (CreateProcess(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // 等待进程完成执行
        WaitForSingleObject(pi.hProcess, INFINITE);

        // 获取进程退出码以判断执行是否成功
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        // 关闭进程和线程句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0) {
            LogOutput(L"注册表文件运行成功");
            return true;
        }
        else {
            LogOutput(L"注册表文件运行失败，退出码: " + std::to_wstring(exitCode));
            return false;
        }
    }
    else {
        LogOutput(L"创建regedit进程失败: " + std::to_wstring(GetLastError()));
        return false;
    }
}

// 【K24T3Q4T3W】注册表相关代码 结束 }






// 辅助函数：绘制圆角矩形路径 - 创建一个带圆角的矩形路径，用于GDI+绘图
void DrawRoundedRectPath(Gdiplus::GraphicsPath& path, float x, float y, float width, float height, float radius) {
    float diameter = radius * 2.0f;

    // 左上角圆角弧
    path.AddArc(x, y, diameter, diameter, 180, 90);
    // 顶边直线
    path.AddLine(x + radius, y, x + width - radius, y);
    // 右上角圆角弧
    path.AddArc(x + width - diameter, y, diameter, diameter, 270, 90);
    // 右边直线
    path.AddLine(x + width, y + radius, x + width, y + height - radius);
    // 右下角圆角弧
    path.AddArc(x + width - diameter, y + height - diameter, diameter, diameter, 0, 90);
    // 底边直线
    path.AddLine(x + width - radius, y + height, x + radius, y + height);
    // 左下角圆角弧
    path.AddArc(x, y + height - diameter, diameter, diameter, 90, 90);
    // 左边直线
    path.AddLine(x, y + height - radius, x, y + radius);

    path.CloseFigure(); // 闭合路径
}

// 文字动画函数实现 - 为指定窗口中的文字添加逐字显现的动画效果
void AnimateText(HWND hWnd, const std::wstring& text) {
    // 初始化动画结构体
    AnimatedText animation;
    animation.text = text;
    animation.charOpacity.resize(text.length(), 0.0f);
    animation.charOffsetY.resize(text.length(), 30.0f); // 设置初始Y轴偏移
    animation.isAnimating = true;
    animation.animationStep = 0;
    animation.totalSteps = 40; // 设置动画总步数，数值越大动画越慢越平滑

    // 将动画信息保存到全局映射中
    g_animatedTexts[hWnd] = animation;

    // 启动动画定时器，约60fps
    SetTimer(hWnd, g_textAnimationTimerId, 16, NULL);
}

// 更新文字动画
void UpdateTextAnimation(HWND hWnd) {
    if (g_animatedTexts.find(hWnd) == g_animatedTexts.end()) {
        return;
    }

    AnimatedText& anim = g_animatedTexts[hWnd];
    if (!anim.isAnimating) {
        return;
    }

    anim.animationStep++;
    bool allDone = true;

    for (size_t i = 0; i < anim.text.length(); i++) {
        // 为每个字符添加延迟，使其依次出现
        int charDelay = i * 2;
        if (anim.animationStep > charDelay) {
            float progress = (float)(anim.animationStep - charDelay) / (anim.totalSteps - charDelay);
            if (progress > 1.0f) progress = 1.0f;

            // 使用弹性缓动函数
            float easedProgress = (float)EaseOutBack(progress);

            anim.charOpacity[i] = progress; // 透明度线性变化
            anim.charOffsetY[i] = 30.0f * (1.0f - easedProgress);

            if (progress < 1.0f) {
                allDone = false;
            }
        }
        else {
            allDone = false;
        }
    }

    if (allDone) {
        anim.isAnimating = false;
        KillTimer(hWnd, g_textAnimationTimerId);
    }

    InvalidateRect(hWnd, NULL, FALSE);
}

// 绘制带动画的文字
void DrawAnimatedText(HDC hdc, const AnimatedText& anim, RECT rect, COLORREF textColor, HFONT hFont) {
    SetTextColor(hdc, textColor);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, hFont);

    // 使用GDI+绘制带动画效果的文字
    Gdiplus::Graphics graphics(hdc);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::Font font(hdc, hFont);
    Gdiplus::SolidBrush brush(Gdiplus::Color(
        255,
        GetRValue(textColor),
        GetGValue(textColor),
        GetBValue(textColor)
    ));

    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    // 首先计算总宽度
    float totalWidth = 0;
    std::vector<float> charWidths;
    for (size_t i = 0; i < anim.text.length(); i++) {
        wchar_t charStr[2] = { anim.text[i], L'\0' };
        Gdiplus::RectF charRect;
        graphics.MeasureString(charStr, 1, &font, Gdiplus::PointF(0, 0), &charRect);
        charWidths.push_back(charRect.Width);
        totalWidth += charRect.Width;
    }

    // 计算起始X坐标，使文本居中
    float x = (float)rect.left + ((rect.right - rect.left) - totalWidth) / 2.0f;
    float y = (float)rect.top + (rect.bottom - rect.top) / 2.0f;

    // 逐字符绘制
    for (size_t i = 0; i < anim.text.length(); i++) {
        if (anim.charOpacity[i] <= 0.0f) {
            x += charWidths[i];
            continue;
        }

        // 创建单个字符的字符串
        wchar_t charStr[2] = { anim.text[i], L'\0' };

        // 设置透明度
        Gdiplus::SolidBrush charBrush(Gdiplus::Color(
            (BYTE)(255 * anim.charOpacity[i]),
            GetRValue(textColor),
            GetGValue(textColor),
            GetBValue(textColor)
        ));

        // 绘制字符，应用Y轴偏移
        graphics.DrawString(
            charStr, 1,
            &font,
            Gdiplus::PointF(x, y - anim.charOffsetY[i]),
            &format,
            &charBrush
        );

        x += charWidths[i];
    }
}

// 添加自定义窗口控制按钮
void AddCustomWindowControls(HWND hWnd) {
    // 创建关闭按钮
    HWND hCloseButton = CreateWindowEx(
        0,
        L"BUTTON",
        L"×",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        g_windowWidth - 45, 10, 35, 35,
        hWnd,
        (HMENU)1001, // 自定义ID
        GetModuleHandle(NULL),
        NULL
    );

    // 设置关闭按钮样式
    SetWindowSubclass(hCloseButton, CustomCloseButtonProc, 0, 0);
    SendMessage(hCloseButton, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    // 创建最小化按钮
    HWND hMinimizeButton = CreateWindowEx(
        0,
        L"BUTTON",
        L"−",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        g_windowWidth - 85, 10, 35, 35,
        hWnd,
        (HMENU)IDC_MINIMIZE_BUTTON,
        GetModuleHandle(NULL),
        NULL
    );

    // 设置最小化按钮样式
    SetWindowSubclass(hMinimizeButton, CustomMinimizeButtonProc, 0, 0);
    SendMessage(hMinimizeButton, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
}

// 自定义关闭按钮窗口过程
LRESULT CALLBACK CustomCloseButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static bool hover = false;
    static bool pressed = false;

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 确定按钮状态和颜色
        COLORREF bgColor = COLOR_TITLE_BAR; // 使用标题栏颜色
        COLORREF textColor = COLOR_TEXT_LIGHT;

        if (pressed) {
            bgColor = COLOR_ERROR; // 按下时为红色
            textColor = RGB(255, 255, 255);
        }
        else if (hover) {
            bgColor = RGB(254, 226, 226); // 悬停时为浅红色
            textColor = COLOR_ERROR;
        }

        // 使用GDI+绘制按钮
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制按钮背景
        Gdiplus::Color gdipBgColor(255, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
        Gdiplus::SolidBrush bgBrush(gdipBgColor);

        // 使用 Gdiplus::RectF 明确指定矩形参数
        Gdiplus::RectF buttonRect((float)rect.left, (float)rect.top, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
        graphics.FillRectangle(&bgBrush, buttonRect);

        // 绘制文本
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        SelectObject(hdc, g_hFontTitle);
        DrawText(hdc, L"×", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!hover) {
            hover = true;

            // 跟踪鼠标离开事件
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            tme.dwHoverTime = HOVER_DEFAULT;
            TrackMouseEvent(&tme);

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_MOUSELEAVE:
        hover = false;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONDOWN:
        pressed = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONUP:
        if (pressed) {
            pressed = false;
            InvalidateRect(hWnd, NULL, TRUE);

            // 发送关闭消息
            SendMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
        }
        break;
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 自定义最小化按钮窗口过程
LRESULT CALLBACK CustomMinimizeButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static bool hover = false;
    static bool pressed = false;

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 确定按钮状态和颜色
        COLORREF bgColor = COLOR_TITLE_BAR; // 使用标题栏颜色
        COLORREF textColor = COLOR_TEXT_LIGHT;

        if (pressed) {
            bgColor = COLOR_BUTTON_PRESSED;
            textColor = COLOR_TEXT;
        }
        else if (hover) {
            bgColor = COLOR_BUTTON_HOVER;
            textColor = COLOR_TEXT;
        }

        // 使用GDI+绘制按钮
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制按钮背景
        Gdiplus::Color gdipBgColor(255, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
        Gdiplus::SolidBrush bgBrush(gdipBgColor);

        // 使用 Gdiplus::RectF 明确指定矩形参数
        Gdiplus::RectF buttonRect((float)rect.left, (float)rect.top, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
        graphics.FillRectangle(&bgBrush, buttonRect);

        // 绘制文本
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        SelectObject(hdc, g_hFontTitle);
        DrawText(hdc, L"−", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!hover) {
            hover = true;

            // 跟踪鼠标离开事件
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            tme.dwHoverTime = HOVER_DEFAULT;
            TrackMouseEvent(&tme);

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_MOUSELEAVE:
        hover = false;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONDOWN:
        pressed = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONUP:
        if (pressed) {
            pressed = false;
            InvalidateRect(hWnd, NULL, TRUE);

            // 最小化窗口
            ShowWindow(GetParent(hWnd), SW_MINIMIZE);
        }
        break;
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 绘制阴影效果
void DrawShadow(HDC hdc, RECT rect, int radius, int strength) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    for (int i = 0; i < strength; i++) {
        int alpha = 30 - i * 2;
        if (alpha < 0) alpha = 0;

        Gdiplus::Color shadowColor(alpha, 0, 0, 0);
        Gdiplus::Pen shadowPen(shadowColor, 1.0f);

        Gdiplus::GraphicsPath path;
        DrawRoundedRectPath(path,
            (float)(rect.left - i),
            (float)(rect.top - i),
            (float)(rect.right - rect.left + i * 2),
            (float)(rect.bottom - rect.top + i * 2),
            (float)radius);

        graphics.DrawPath(&shadowPen, &path);
    }
}

// 绘制玻璃效果
void DrawGlassEffect(HDC hdc, RECT rect, int radius) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 创建渐变画刷
    Gdiplus::LinearGradientBrush brush(
        Gdiplus::Point(rect.left, rect.top),
        Gdiplus::Point(rect.right, rect.bottom),
        Gdiplus::Color(40, 255, 255, 255),
        Gdiplus::Color(10, 255, 255, 255)
    );

    // 绘制圆角矩形
    Gdiplus::GraphicsPath path;
    DrawRoundedRectPath(path,
        (float)rect.left,
        (float)rect.top,
        (float)(rect.right - rect.left),
        (float)(rect.bottom - rect.top),
        (float)radius);

    graphics.FillPath(&brush, &path);
}

// 绘制现代UI
void DrawModernUI(HDC hdc, RECT rect) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 绘制背景
    Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 255, 255, 255));
    graphics.FillRectangle(&bgBrush,
        (Gdiplus::REAL)rect.left,
        (Gdiplus::REAL)rect.top,
        (Gdiplus::REAL)(rect.right - rect.left),
        (Gdiplus::REAL)(rect.bottom - rect.top));

    // 绘制顶部装饰条
    Gdiplus::LinearGradientBrush topBrush(
        Gdiplus::Point(rect.left, rect.top),
        Gdiplus::Point(rect.right, rect.top),
        Gdiplus::Color(255, 79, 70, 229),  // 靛蓝色
        Gdiplus::Color(255, 236, 72, 153)  // 粉色
    );

    graphics.FillRectangle(&topBrush,
        (Gdiplus::REAL)rect.left,
        (Gdiplus::REAL)rect.top,
        (Gdiplus::REAL)(rect.right - rect.left),
        (Gdiplus::REAL)4);
}

// 平滑的二次缓动函数，使动画更加自然
double EaseInOutQuad(double t) {
    return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
}

// 弹性缓动函数，使动画有弹性效果
double EaseOutBack(double t) {
    const double c1 = 1.70158;
    const double c3 = c1 + 1;
    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

// 混合两种颜色
COLORREF BlendColors(COLORREF color1, COLORREF color2, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    BYTE r1 = GetRValue(color1);
    BYTE g1 = GetGValue(color1);
    BYTE b1 = GetBValue(color1);

    BYTE r2 = GetRValue(color2);
    BYTE g2 = GetGValue(color2);
    BYTE b2 = GetBValue(color2);

    BYTE r = (BYTE)(r1 + (r2 - r1) * factor);
    BYTE g = (BYTE)(g1 + (g2 - g1) * factor);
    BYTE b = (BYTE)(b1 + (b2 - b1) * factor);

    return RGB(r, g, b);
}

// 绘制渐变矩形
void DrawGradientRect(HDC hdc, RECT rect, COLORREF colorStart, COLORREF colorEnd) {
    // 创建渐变画刷
    TRIVERTEX vertex[2];
    vertex[0].x = rect.left;
    vertex[0].y = rect.top;
    vertex[0].Red = GetRValue(colorStart) << 8;
    vertex[0].Green = GetGValue(colorStart) << 8;
    vertex[0].Blue = GetBValue(colorStart) << 8;
    vertex[0].Alpha = 0x0000;

    vertex[1].x = rect.right;
    vertex[1].y = rect.bottom;
    vertex[1].Red = GetRValue(colorEnd) << 8;
    vertex[1].Green = GetGValue(colorEnd) << 8;
    vertex[1].Blue = GetBValue(colorEnd) << 8;
    vertex[1].Alpha = 0x0000;

    GRADIENT_RECT gRect = { 0, 1 };

    // 绘制渐变
    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

// 绘制圆角矩形
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius, COLORREF color, bool filled) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    Gdiplus::Color gdipColor(GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(gdipColor, 1);
    Gdiplus::SolidBrush brush(gdipColor);

    Gdiplus::GraphicsPath path;
    DrawRoundedRectPath(path,
        (float)rect.left,
        (float)rect.top,
        (float)(rect.right - rect.left),
        (float)(rect.bottom - rect.top),
        (float)radius);

    if (filled) {
        graphics.FillPath(&brush, &path);
    }
    else {
        graphics.DrawPath(&pen, &path);
    }
}

// 创建自定义字体
void CreateCustomFonts() {
    g_hFontNormal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    g_hFontBold = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    g_hFontTitle = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    g_hFontLarge = CreateFont(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    g_hBackgroundBrush = CreateSolidBrush(COLOR_BG);
}

// 清理资源
void CleanupResources() {
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontLarge) DeleteObject(g_hFontLarge);
    if (g_hBackgroundBrush) DeleteObject(g_hBackgroundBrush);
}

// 设置状态栏文本
void SetStatusText(const std::wstring& text) {
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)text.c_str());
}

// 自定义按钮窗口过程 - 处理自定义按钮的所有消息，实现美观的视觉效果
LRESULT CALLBACK CustomButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static bool hover = false;    // 鼠标悬停状态
    static bool pressed = false;  // 按钮按下状态

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 确定按钮状态和颜色
        COLORREF bgColor = COLOR_BUTTON_NORMAL;
        COLORREF textColor = COLOR_PRIMARY;
        COLORREF borderColor = COLOR_BORDER;

        // 根据按钮状态设置颜色
        if (pressed) {
            bgColor = COLOR_BUTTON_PRESSED;
            textColor = COLOR_PRIMARY_DARK;
            borderColor = COLOR_PRIMARY;
        }
        else if (hover) {
            bgColor = COLOR_BUTTON_HOVER;
            textColor = COLOR_PRIMARY;
            borderColor = COLOR_PRIMARY_LIGHT;
        }

        // 使用GDI+绘制按钮
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制按钮背景
        Gdiplus::Color gdipBgColor(255, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor));
        Gdiplus::SolidBrush bgBrush(gdipBgColor);

        // 使用 Gdiplus::RectF 明确指定矩形参数
        Gdiplus::RectF buttonRect((float)rect.left, (float)rect.top, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
        graphics.FillRectangle(&bgBrush, buttonRect);

        // 绘制按钮边框
        Gdiplus::Color gdipBorderColor(255, GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor));
        Gdiplus::Pen borderPen(gdipBorderColor, 1.0f);
        graphics.DrawRectangle(&borderPen, buttonRect);

        // 获取按钮文本
        wchar_t text[256] = { 0 };
        GetWindowText(hWnd, text, 256);

        // 绘制文本
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        SelectObject(hdc, g_hFontNormal);
        DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!hover) {
            hover = true;
            g_buttonHoverState[hWnd] = true;

            // 跟踪鼠标离开事件
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            tme.dwHoverTime = HOVER_DEFAULT;
            TrackMouseEvent(&tme);

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_MOUSELEAVE:
        hover = false;
        g_buttonHoverState[hWnd] = false;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONDOWN:
        pressed = true;
        g_buttonPressedState[hWnd] = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_LBUTTONUP:
        if (pressed) {
            pressed = false;
            g_buttonPressedState[hWnd] = false;
            InvalidateRect(hWnd, NULL, TRUE);

            // 发送点击消息给父窗口
            RECT rect;
            GetClientRect(hWnd, &rect);
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            if (PtInRect(&rect, pt)) {
                DWORD id = GetDlgCtrlID(hWnd);
                SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)hWnd);
            }
        }
        break;
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 自定义复选   窗口过程
LRESULT CALLBACK CustomCheckboxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    static bool hover = false;

    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 填充背景
        HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        // 获取复选框文本
        wchar_t text[256] = { 0 };
        GetWindowText(hWnd, text, 256);

        // 绘制复选框
        int checkboxSize = 18;
        int checkboxLeft = rect.left + 2;
        int checkboxTop = rect.top + (rect.bottom - rect.top - checkboxSize) / 2;

        RECT checkboxRect = { checkboxLeft, checkboxTop, checkboxLeft + checkboxSize, checkboxTop + checkboxSize };

        // 使用GDI+绘制更美观的复选框
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 确定复选框状态和颜色
        bool checked = (SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
        COLORREF boxColor = checked ? COLOR_PRIMARY : COLOR_BUTTON_NORMAL;
        COLORREF borderColor = checked ? COLOR_PRIMARY : COLOR_BORDER;

        if (hover) {
            if (!checked) {
                boxColor = COLOR_BUTTON_HOVER;
                borderColor = COLOR_PRIMARY_LIGHT;
            }
        }

        // 绘制复选框背景
        Gdiplus::Color gdipBoxColor(255, GetRValue(boxColor), GetGValue(boxColor), GetBValue(boxColor));
        Gdiplus::SolidBrush boxBrush(gdipBoxColor);

        Gdiplus::GraphicsPath boxPath;
        DrawRoundedRectPath(boxPath,
            (float)checkboxRect.left,
            (float)checkboxRect.top,
            (float)(checkboxRect.right - checkboxRect.left),
            (float)(checkboxRect.bottom - checkboxRect.top),
            4.0f);

        graphics.FillPath(&boxBrush, &boxPath);

        // 绘制复选框边框
        Gdiplus::Color gdipBorderColor(255, GetRValue(borderColor), GetGValue(borderColor), GetBValue(borderColor));
        Gdiplus::Pen borderPen(gdipBorderColor, 1.0f);
        graphics.DrawPath(&borderPen, &boxPath);

        // 如果选中，绘制勾选标记
        if (checked) {
            Gdiplus::Color checkColor(255, 255, 255, 255);
            Gdiplus::Pen checkPen(checkColor, 2.0f);

            graphics.DrawLine(&checkPen,
                (float)(checkboxRect.left + 4),
                (float)(checkboxRect.top + 9),
                (float)(checkboxRect.left + 7),
                (float)(checkboxRect.top + 13)
            );

            graphics.DrawLine(&checkPen,
                (float)(checkboxRect.left + 7),
                (float)(checkboxRect.top + 13),
                (float)(checkboxRect.left + 14),
                (float)(checkboxRect.top + 5)
            );
        }

        // 绘制文本
        RECT textRect = rect;
        textRect.left = checkboxRect.right + 8;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT);
        SelectObject(hdc, g_hFontNormal);
        DrawText(hdc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (!hover) {
            hover = true;

            // 跟踪鼠标离开事件
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            tme.dwHoverTime = HOVER_DEFAULT;
            TrackMouseEvent(&tme);

            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_MOUSELEAVE:
        hover = false;
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 自定义分组框窗口过程
LRESULT CALLBACK CustomGroupBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 填充背景
        HBRUSH hBrush = CreateSolidBrush(COLOR_BG);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        // 使用GDI+绘制更美观的分组框
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制分组框边框
        Gdiplus::Color borderColor(255, GetRValue(COLOR_BORDER), GetGValue(COLOR_BORDER), GetBValue(COLOR_BORDER));
        Gdiplus::Pen borderPen(borderColor, 1.0f);

        Gdiplus::GraphicsPath boxPath;
        DrawRoundedRectPath(boxPath,
            (float)rect.left + 1,
            (float)rect.top + 10,
            (float)(rect.right - rect.left - 2),
            (float)(rect.bottom - rect.top - 11),
            8.0f);

        graphics.DrawPath(&borderPen, &boxPath);

        // 获取分组框文本
        wchar_t text[256] = { 0 };
        GetWindowText(hWnd, text, 256);

        // 如果有文本，绘制文本
        if (text[0] != L'\0') {
            // 测量文本宽度
            SIZE textSize;
            SelectObject(hdc, g_hFontBold);
            GetTextExtentPoint32(hdc, text, wcslen(text), &textSize);

            // 绘制文本背景
            RECT textRect = { rect.left + 10, rect.top, rect.left + 20 + textSize.cx, rect.top + textSize.cy };
            hBrush = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &textRect, hBrush);
            DeleteObject(hBrush);

            // 绘制文本
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, COLOR_PRIMARY);
            TextOut(hdc, rect.left + 15, rect.top, text, wcslen(text));
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 自定义进度条窗口过程
LRESULT CALLBACK CustomProgressProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // 使用GDI+绘制更美观的进度条
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制进度条背景
        Gdiplus::Color bgColor(255, 243, 244, 246);
        Gdiplus::SolidBrush bgBrush(bgColor);

        Gdiplus::GraphicsPath bgPath;
        DrawRoundedRectPath(bgPath,
            (float)rect.left,
            (float)rect.top,
            (float)(rect.right - rect.left),
            (float)(rect.bottom - rect.top),
            8.0f);

        graphics.FillPath(&bgBrush, &bgPath);

        // 获取当前进度
        int pos = (int)SendMessage(hWnd, PBM_GETPOS, 0, 0);
        int range = (int)SendMessage(hWnd, PBM_GETRANGE, FALSE, 0);

        if (pos > 0 && range > 0) {
            // 计算进度条填充宽度
            int width = (rect.right - rect.left) * pos / range;

            // 创建渐变画刷
            Gdiplus::LinearGradientBrush progressBrush(
                Gdiplus::Point(rect.left, rect.top),
                Gdiplus::Point(rect.left + width, rect.top),
                Gdiplus::Color(255, GetRValue(COLOR_PRIMARY), GetGValue(COLOR_PRIMARY), GetBValue(COLOR_PRIMARY)),
                Gdiplus::Color(255, GetRValue(COLOR_PRIMARY_LIGHT), GetGValue(COLOR_PRIMARY_LIGHT), GetBValue(COLOR_PRIMARY_LIGHT))
            );

            // 绘制进度条填充
            if (width > 16) {  // 确保有足够的宽度绘制圆角
                Gdiplus::GraphicsPath progressPath;
                DrawRoundedRectPath(progressPath,
                    (float)rect.left,
                    (float)rect.top,
                    (float)width,
                    (float)(rect.bottom - rect.top),
                    8.0f);

                graphics.FillPath(&progressBrush, &progressPath);

                // 绘制进度文本
                wchar_t progressText[32];
                swprintf_s(progressText, 32, L"%d%%", (pos * 100) / range);

                Gdiplus::FontFamily fontFamily(L"微软雅黑");
                Gdiplus::Font font(&fontFamily, 12, Gdiplus::FontStyleBold);
                Gdiplus::StringFormat format;
                format.SetAlignment(Gdiplus::StringAlignmentCenter);
                format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

                Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
                Gdiplus::RectF textRect(
                    (float)rect.left,
                    (float)rect.top,
                    (float)(rect.right - rect.left),
                    (float)(rect.bottom - rect.top)
                );

                graphics.DrawString(progressText, -1, &font, textRect, &format, &textBrush);
            }
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefSubclassProc(hWnd, message, wParam, lParam);
}

// 创建自定义按钮 - 创建一个具有自定义外观和行为的按钮控件
HWND CreateCustomButton(HWND hParent, LPCWSTR text, int x, int y, int width, int height, HMENU hMenu) {
    // 创建标准按钮控件
    HWND hButton = CreateWindow(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, width, height,
        hParent,
        hMenu,
        GetModuleHandle(NULL),
        NULL
    );

    // 设置子类化过程，使用自定义的按钮处理函数
    SetWindowSubclass(hButton, CustomButtonProc, 0, 0);

    // 设置按钮字体
    SendMessage(hButton, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    return hButton;
}

// 窗口过程 - 处理所有窗口消息
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBrushBackground = CreateSolidBrush(COLOR_BG);
    static HBRUSH hBrushEdit = CreateSolidBrush(RGB(255, 255, 255));
    static HBRUSH hBrushButton = CreateSolidBrush(COLOR_BUTTON_NORMAL);
    static HBRUSH hBrushGroupBox = CreateSolidBrush(COLOR_BG);

    switch (message) {
    case WM_CREATE:
    {
        // 初始化GDI+图形库 - 用于绘制高质量的图形界面元素
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

        // 创建自定义字体 - 为界面提供一致美观的字体样式
        CreateCustomFonts();

        // 设置UI界面控件 - 创建并初始化所有用户界面元素
        SetupUI(hWnd);

        // 添加自定义窗口控制按钮 - 创建无边框窗口的关闭和最小化按钮
        AddCustomWindowControls(hWnd);

        // 检查游戏版本 - 自动检测系统中已安装的迷你世界游戏版本
        CheckAllVersions();

        // 为标题添加动画效果 - 使程序标题具有动态显现的视觉效果
        AnimateText(hWnd, L"迷你世界光影包安装器 V2.7.53");

        // 启用DWM扩展窗口框架 - 实现现代化的窗口边框和视觉效果
        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hWnd, &margins);

        // 设置窗口圆角 - 为窗口添加圆角外观
        DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(hWnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    }
    break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START_SCAN:
            StartScan();
            break;
        case IDC_PAUSE_SCAN:
            PauseScan();
            break;
        case IDC_STOP_SCAN:
            StopScan();
            break;
        case IDC_ADD_CUSTOM:
            AddCustomPath();
            break;
        case IDC_DOWNLOAD_PRESET:
            DownloadPresets();
            break;
        case IDC_INSTALL_BUTTON:
            InstallAction();
            break;
        case IDC_YIRIXC_CHECKBOX:
            g_yirixcChecked = !g_yirixcChecked;
            CheckDlgButton(hWnd, IDC_YIRIXC_CHECKBOX, g_yirixcChecked ? BST_CHECKED : BST_UNCHECKED);
            InvalidateRect(GetDlgItem(hWnd, IDC_YIRIXC_CHECKBOX), NULL, TRUE);
            break;
        case IDC_TIANXINGDASHE_CHECKBOX:
            g_tianxingdasheChecked = !g_tianxingdasheChecked;
            CheckDlgButton(hWnd, IDC_TIANXINGDASHE_CHECKBOX, g_tianxingdasheChecked ? BST_CHECKED : BST_UNCHECKED);
            InvalidateRect(GetDlgItem(hWnd, IDC_TIANXINGDASHE_CHECKBOX), NULL, TRUE);
            break;
        }
        break;

    case WM_UPDATE_PROGRESS:
        SendMessage(g_hProgressBar, PBM_SETPOS, wParam, 0);
        break;

    case WM_LOG_MESSAGE:
        if (lParam) {
            std::wstring* pMessage = reinterpret_cast<std::wstring*>(lParam);
            LogOutput(*pMessage);
            delete pMessage;
        }
        break;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, RGB(255, 255, 255));
        return (LRESULT)hBrushEdit;

    case WM_CTLCOLORSTATIC:
        // 为静态控件设置颜色
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, COLOR_BG);
        return (LRESULT)hBrushBackground;

    case WM_CTLCOLORBTN:
        // 为按钮设置颜色
        SetTextColor((HDC)wParam, COLOR_TEXT);
        SetBkColor((HDC)wParam, COLOR_BUTTON_NORMAL);
        return (LRESULT)hBrushButton;

        // 修改WM_TIMER消息处理，添加文字动画更新
    case WM_TIMER:
        if (wParam == g_animationTimerId && g_animationActive) {
            g_animationStep++;

            if (g_animationStep <= g_animationMaxSteps) {
                // 使用缓动函数计算窗口位置，使动画更平滑
                double progress = (double)g_animationStep / g_animationMaxSteps;
                double easedProgress = EaseInOutQuad(progress);

                // 计算窗口位置
                int newX = g_windowStartX + (int)((g_windowTargetX - g_windowStartX) * easedProgress);

                // 计算透明度
                BYTE alpha = (BYTE)(255 * easedProgress);

                // 更新颜色混合因子
                g_colorBlendFactor = (float)easedProgress;

                // 设置窗口位置
                SetWindowPos(hWnd, NULL, newX, g_windowY, g_windowWidth, g_windowHeight, SWP_NOZORDER);

                // 设置窗口透明度
                SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

                // 强制重绘窗口，但减少不必要的重绘
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else {
                // 动画结束
                g_animationActive = false;
                KillTimer(hWnd, g_animationTimerId);

                // 确保窗口完全不透明
                SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

                // 清理双缓冲资源
                if (g_memDC) {
                    SelectObject(g_memDC, g_oldBitmap);
                    DeleteObject(g_memBitmap);
                    DeleteDC(g_memDC);
                    g_memDC = NULL;
                    g_memBitmap = NULL;
                    g_oldBitmap = NULL;
                }
            }
        }
        else if (wParam == g_textAnimationTimerId) {
            // 文字动画更新
            UpdateTextAnimation(hWnd);
        }
        break;

        // 修改WM_PAINT消息处理，添加文字动画绘制
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 获取客户区大小
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        // 创建或重用兼容DC和位图用于双缓冲
        if (!g_memDC) {
            g_memDC = CreateCompatibleDC(hdc);
            g_memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            g_oldBitmap = (HBITMAP)SelectObject(g_memDC, g_memBitmap);
        }

        // 绘制现代UI背景
        DrawModernUI(g_memDC, clientRect);

        // 绘制标题栏
        RECT titleRect = clientRect;
        titleRect.bottom = 55;

        // 使用GDI+绘制标题栏
        Gdiplus::Graphics graphics(g_memDC);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // 绘制标题栏背景
        Gdiplus::SolidBrush titleBrush(Gdiplus::Color(1, 0, 0, 0));
        graphics.FillRectangle(&titleBrush,
            (Gdiplus::REAL)0,
            (Gdiplus::REAL)0,
            (Gdiplus::REAL)clientRect.right,
            (Gdiplus::REAL)titleRect.bottom);

        // 绘制标题栏底部阴影
        Gdiplus::LinearGradientBrush shadowBrush(
            Gdiplus::Point(0, titleRect.bottom - 1),
            Gdiplus::Point(0, titleRect.bottom + 5),
            Gdiplus::Color(30, 0, 0, 0),
            Gdiplus::Color(0, 0, 0, 0)
        );
        graphics.FillRectangle(&shadowBrush,
            (Gdiplus::REAL)0,
            (Gdiplus::REAL)(titleRect.bottom - 1),
            (Gdiplus::REAL)clientRect.right,
            (Gdiplus::REAL)6);

        // 使用动画文字绘制标题
        if (g_animatedTexts.find(hWnd) != g_animatedTexts.end()) {
            DrawAnimatedText(g_memDC, g_animatedTexts[hWnd], titleRect, COLOR_PRIMARY, g_hFontTitle);
        }
        else {
            // 如果没有动画，使用普通方式绘制
            SetTextColor(g_memDC, COLOR_PRIMARY);
            SetBkMode(g_memDC, TRANSPARENT);
            SelectObject(g_memDC, g_hFontTitle);
            DrawText(g_memDC, L"迷你世界光影包安装器 V2.7.53", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // 将内存DC的内容复制到屏幕
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, g_memDC, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }
    break;

    // 添加窗口拖动支持
    case WM_NCHITTEST:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);

        // 如果鼠标在标题栏区域，返回HTCAPTION使窗口可拖动
        if (pt.y < 55) {
            return HTCAPTION;
        }

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_CLOSE:
        if (g_scanThread != NULL) {
            g_scanning = false;
            WaitForSingleObject(g_scanThread, 1000);
            CloseHandle(g_scanThread);
        }
        if (g_installThread != NULL) {
            WaitForSingleObject(g_installThread, 1000);
            CloseHandle(g_installThread);
        }
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        CleanupResources();

        // 清理双缓冲资源
        if (g_memDC) {
            SelectObject(g_memDC, g_oldBitmap);
            DeleteObject(g_memBitmap);
            DeleteDC(g_memDC);
        }

        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        DeleteObject(hBrushBackground);
        DeleteObject(hBrushEdit);
        DeleteObject(hBrushButton);
        DeleteObject(hBrushGroupBox);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 设置UI
void SetupUI(HWND hWnd) {
    // 创建状态栏
    g_hStatusBar = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hWnd,
        (HMENU)IDC_STATUS_BAR,
        GetModuleHandle(NULL),
        NULL
    );

    // 设置状态栏面板
    int statwidths[] = { -1 };
    SendMessage(g_hStatusBar, SB_SETPARTS, 1, (LPARAM)statwidths);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"准备就绪");

    // 创建输出文本框
    g_hOutputText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        420, 70,
        760, 480,
        hWnd,
        (HMENU)IDC_OUTPUT_TEXT,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hOutputText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // 创建进度条
    g_hProgressBar = CreateWindowEx(
        0,
        PROGRESS_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE,
        420, 560,
        760, 30,
        hWnd,
        (HMENU)IDC_PROGRESS_BAR,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SetWindowSubclass(g_hProgressBar, CustomProgressProc, 0, 0);

    // 创建版权声明标签
    HWND hCopyrightLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"版权声明",
        WS_CHILD | WS_VISIBLE,
        20, 70,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hCopyrightLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // 创建版权声明文本框
    HWND hCopyrightText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"版权声明： \r\n \r\n本作品（包括但不限于文字、图片、音频、视频、设计元素等）的版权归作者\"创梦星际\"共所有。未经作者及工作室明确书面授权，任何个人、组织或机构不得以任何形式复制、分发、传播、展示、修改、改编或以其他任何方式使用本作品的全部或部分内容。作者及工作室保留对本作品的全部权利，包括但不限于著作权、商标权、专利权等。任何未经授权的使用行为均构成侵权，作者及工作室将依法追究侵权者的法律责任，并要求侵权者承担相应的赔偿责任。如需获取本作品的授权使用，请与\"迷影、大喵工作室、Qichee、创梦星际\"工作室联系，工作室将根据具体情况决定是否授权以及授权的条件和范围。作者及工作室保留随时修改、更新或终止本版权声明的权利，而无需事先通知任何第三方。作者及工作室对本版权声明拥有最终解释权。\r\n \r\n联系方式：\r\n工作室名称：创梦星际\r\n地址：https://www.scmgzs.top/\r\n联系邮箱：ssw2196634956@outlook.com",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        20, 95,
        380, 100,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hCopyrightText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // 创建更新日志标签
    HWND hUpdateLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"更新日志",
        WS_CHILD | WS_VISIBLE,
        20, 205,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hUpdateLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // 创建更新日志文本框
    HWND hUpdateText = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"## 更新日志\r\n \r\n### 版本 2.7.50（2025年6月）\r\n- **修复**:\r\n -修复迷你世界1.46.1.0更新后渲染效果失效问题 \r\n- **优化**:\r\n -美化安装器UI \r\n  \r\n### 版本 2.7.42 (2025年5月) \r\n- **修复**:\r\n -修复迷你世界1.46.0.0更新后渲染效果失效问题 \r\n -使用C++重构代码 \r\n \r\n### 版本 2.6.32（2025年4月）\r\n- **修复**:\r\n -修复迷你世界1.45.90更新后渲染效果失效问题 \r\n \r\n### 版本 2.6.31（2025年1月）\r\n- **优化**:\r\n - 优化代码架构，更好的兼容win7系统运行。\r\n - 优化UI界面，使用pyqt5开发。\r\n- **修复**：\r\n - 修复过ACE，使用天梦零惜大佬制作。\r\n - 修复扫盘寻找问题。\r\n### 版本 2.6.24 (2023年10月)\r\n- **新增功能**:\r\n - 添加了对新游戏版本的支持。\r\n  - 增加了用户反馈功能，用户可以直接在应用内提交反馈。\r\n \r\n- **优化**:\r\n  - 优化了安装过程中的文件解压速度。\r\n  - 改进了UI界面，使其更加友好和直观。\r\n \r\n- **修复**:\r\n  - 修复了在某些情况下快捷方式创建失败的问题。\r\n  - 修复了安装过程中可能出现的崩溃问题。",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        20, 230,
        380, 100,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hUpdateText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    // 创建检测到的游戏版本标签
    HWND hVersionLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"检测到的游戏版本",
        WS_CHILD | WS_VISIBLE,
        20, 340,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hVersionLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // 创建版本组框
    g_hVersionGroup = CreateWindowEx(
        0,
        L"BUTTON",
        L"游戏版本",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 365,
        380, 120,
        hWnd,
        (HMENU)IDC_VERSION_GROUP,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(g_hVersionGroup, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SetWindowSubclass(g_hVersionGroup, CustomGroupBoxProc, 0, 0);

    // 创建添加自定义安装按钮
    HWND hAddCustomButton = CreateCustomButton(
        hWnd,
        L"添加自定义安装",
        20, 495,
        380, 30,
        (HMENU)IDC_ADD_CUSTOM
    );

    // 创建预设下载选项标签
    HWND hPresetLabel = CreateWindowEx(
        0,
        L"STATIC",
        L"预设下载选项",
        WS_CHILD | WS_VISIBLE,
        20, 535,
        380, 20,
        hWnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hPresetLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    // 创建预设复选框
    HWND hYirixcCheckbox = CreateWindowEx(
        0,
        L"BUTTON",
        L"预设制作：YiRixc",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 560,
        180, 20,
        hWnd,
        (HMENU)IDC_YIRIXC_CHECKBOX,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hYirixcCheckbox, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SetWindowSubclass(hYirixcCheckbox, CustomCheckboxProc, 0, 0);

    HWND hTianxingdasheCheckbox = CreateWindowEx(
        0,
        L"BUTTON",
        L"预设制作：天星大蛇",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        210, 560,
        190, 20,
        hWnd,
        (HMENU)IDC_TIANXINGDASHE_CHECKBOX,
        GetModuleHandle(NULL),
        NULL
    );
    SendMessage(hTianxingdasheCheckbox, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SetWindowSubclass(hTianxingdasheCheckbox, CustomCheckboxProc, 0, 0);

    // 创建下载预设按钮
    HWND hDownloadPresetButton = CreateCustomButton(
        hWnd,
        L"下载预设",
        20, 590,
        380, 30,
        (HMENU)IDC_DOWNLOAD_PRESET
    );

    // 创建扫描控制按钮
    HWND hStartScanButton = CreateCustomButton(
        hWnd,
        L"开始扫描",
        20, 630,
        120, 30,
        (HMENU)IDC_START_SCAN
    );

    HWND hPauseScanButton = CreateCustomButton(
        hWnd,
        L"暂停扫描",
        150, 630,
        120, 30,
        (HMENU)IDC_PAUSE_SCAN
    );
    EnableWindow(hPauseScanButton, FALSE);

    HWND hStopScanButton = CreateCustomButton(
        hWnd,
        L"停止扫描",
        280, 630,
        120, 30,
        (HMENU)IDC_STOP_SCAN
    );
    EnableWindow(hStopScanButton, FALSE);

    // 创建安装按钮
    HWND hInstallButton = CreateCustomButton(
        hWnd,
        L"安装",
        420, 600,
        760, 40,
        (HMENU)IDC_INSTALL_BUTTON
    );
}

// 输出日志
void LogOutput(const std::wstring& message) {
    // 添加到输出文本框
    int length = GetWindowTextLength(g_hOutputText);
    SendMessage(g_hOutputText, EM_SETSEL, (WPARAM)length, (LPARAM)length);
    SendMessageW(g_hOutputText, EM_REPLACESEL, FALSE, (LPARAM)message.c_str());
    SendMessageW(g_hOutputText, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

    // 滚动到底部
    SendMessage(g_hOutputText, EM_SCROLLCARET, 0, 0);

    // 写入日志文件
    if (g_logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::wstring timeStr = std::to_wstring(now_time_t);
        g_logFile << L"[" << timeStr << L"] " << message << std::endl;
    }

    // 更新状态栏
    SetStatusText(message);
}

// 设置日志
void SetupLogging() {
    // 获取桌面路径
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath))) {
        std::wstring logPath = std::wstring(desktopPath) + L"\\installer_log.txt";
        g_logFile.open(logPath, std::ios::out | std::ios::app);
        if (g_logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::wstring timeStr = std::to_wstring(now_time_t);
            g_logFile << L"[" << timeStr << L"] 日志开始记录" << std::endl;
        }
    }
}

// 检查是否为管理员
bool IsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;

    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        if (!CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(AdministratorsGroup);
    }

    return isAdmin != FALSE;
}

// 获取Windows版本
std::wstring GetWindowsVersion() {
    // 使用VersionHelpers.h中的函数代替GetVersionEx
    if (IsWindows10OrGreater()) {
        return L"10.0";
    }
    else if (IsWindows8Point1OrGreater()) {
        return L"6.3";
    }
    else if (IsWindows8OrGreater()) {
        return L"6.2";
    }
    else if (IsWindows7OrGreater()) {
        return L"6.1";
    }
    else if (IsWindowsVistaOrGreater()) {
        return L"6.0";
    }
    else if (IsWindowsXPOrGreater()) {
        return L"5.1";
    }
    else {
        return L"0.0";
    }
}

// 检查所有版本 - 检测系统中已安装的迷你世界游戏版本
void CheckAllVersions() {
    g_detectedVersions.clear();
    LogOutput(L"检查是否以管理员身份运行...");
    LogOutput(L"开始检测游戏版本...");

    // 获取用户文件夹路径
    wchar_t usersFolderPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, usersFolderPath))) {
        std::wstring usersFolder = usersFolderPath;
        size_t lastSlash = usersFolder.find_last_of(L"\\");
        if (lastSlash != std::wstring::npos) {
            usersFolder = usersFolder.substr(0, lastSlash);
        }

        // 检查注册表中的官方版本
        std::wstring officialPath = CheckRegistryAndExtract(L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\官方版迷你世界");
        if (!officialPath.empty()) {
            g_detectedVersions[L"官方版迷你世界"] = officialPath; // 保存路径到全局变量
            LogOutput(L"检测到: 官方版迷你世界，路径: " + officialPath); // 输出路径到日志
        }

        // 检查用户文件夹中的其他版本
        WIN32_FIND_DATA findData;
        HANDLE hFind = FindFirstFile((usersFolder + L"\\*").c_str(), &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                        std::wstring userFolder = usersFolder + L"\\" + findData.cFileName;

                        // 定义不同版本的路径
                        std::map<std::wstring, std::wstring> paths = {
                            {L"4399版迷你世界", userFolder + L"\\AppData\\Roaming\\miniworldgame4399\\"},
                            {L"7k7k版迷你世界", userFolder + L"\\AppData\\Roaming\\miniworldgame7k7k\\"},
                            {L"迷你世界国际版", userFolder + L"\\AppData\\Roaming\\miniworldOverseasgame\\"}
                        };

                        // 检查各个版本是否存在
                        for (const auto& pair : paths) {
                            if (PathFileExists(pair.second.c_str())) {
                                std::wstring version = pair.first + L" (用户: " + findData.cFileName + L")";
                                g_detectedVersions[version] = pair.second;
                                LogOutput(L"检测到: " + version);
                            }
                        }
                    }
                }
            } while (FindNextFile(hFind, &findData));
            FindClose(hFind);
        }
    }

    // 如果没有检测到任何版本，则询问用户是否进行全盘扫描
    if (g_detectedVersions.empty()) {
        int result = MessageBox(g_hWnd, L"未检测到安装路径，是否进行全盘扫描寻找迷你世界？", L"未检测到安装路径", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            StartScan();
        }
    }

    LogOutput(L"检测到的版本数量: " + std::to_wstring(g_detectedVersions.size()));
    PopulateOptions(); // 填充界面中的版本选项
}

// 从注册表检查并提取路径
std::wstring CheckRegistryAndExtract(const std::wstring& registryKeyPath) {
    HKEY hKey;
    DWORD accessFlag = KEY_READ;

    std::wstring windowsVersion = GetWindowsVersion();
    double version = _wtof(windowsVersion.c_str());
    if (version <= 6.1) {
        accessFlag = KEY_READ;
    }
    else {
        accessFlag = KEY_READ | KEY_WOW64_32KEY;
    }

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, registryKeyPath.c_str(), 0, accessFlag, &hKey) == ERROR_SUCCESS) {
        wchar_t publisher[MAX_PATH];
        DWORD bufferSize = sizeof(publisher);
        DWORD type;

        if (RegQueryValueEx(hKey, L"Publisher", 0, &type, (LPBYTE)publisher, &bufferSize) == ERROR_SUCCESS) {
            std::wstring publisherStr = publisher;
            std::wstring folderPath = publisherStr;

            // 替换"迷你玩科技有限公司"为空
            size_t pos = folderPath.find(L"迷你玩科技有限公司");
            if (pos != std::wstring::npos) {
                folderPath.replace(pos, wcslen(L"迷你玩科技有限公司"), L"");
            }

            // 去除前后空格
            while (!folderPath.empty() && (folderPath[0] == L' ' || folderPath[0] == L'\t' ||
                folderPath[0] == L'\n' || folderPath[0] == L'\r')) {
                folderPath.erase(0, 1);
            }

            while (!folderPath.empty() && (folderPath[folderPath.length() - 1] == L' ' ||
                folderPath[folderPath.length() - 1] == L'\t' || folderPath[folderPath.length() - 1] == L'\n' ||
                folderPath[folderPath.length() - 1] == L'\r')) {
                folderPath.erase(folderPath.length() - 1, 1);
            }

            RegCloseKey(hKey);
            if (!folderPath.empty()) {
                return folderPath;
            }
            else {
                LogOutput(L"发布者路径无效。");
            }
        }
        RegCloseKey(hKey);
    }

    return L"";
}

// 填充选项
void PopulateOptions() {
    // 清除现有的单选按钮
    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
        DestroyWindow(hChild);
        hChild = hNext;
    }

    // 添加新的单选按钮
    int y = 25;
    int buttonId = 1000; // 起始ID
    HWND firstButton = NULL;

    for (const auto& pair : g_detectedVersions) {
        HWND hRadio = CreateWindowEx(
            0,
            L"BUTTON",
            pair.first.c_str(),
            WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            15, y,
            350, 20,
            g_hVersionGroup,
            (HMENU)(INT_PTR)buttonId,
            GetModuleHandle(NULL),
            NULL
        );

        SendMessage(hRadio, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        if (!firstButton) {
            firstButton = hRadio;
        }

        y += 25;
        buttonId++;
    }

    // 选中第一个按钮
    if (firstButton) {
        SendMessage(firstButton, BM_SETCHECK, BST_CHECKED, 0);
    }
}

// 添加自定义路径
void AddCustomPath() {
    wchar_t path[MAX_PATH] = { 0 };
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = L"选择安装目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDList(pidl, path)) {
            std::wstring pathStr = path;
            std::wstring version = L"自定义路径: " + pathStr;
            g_detectedVersions[version] = pathStr;
            PopulateOptions();
            LogOutput(L"添加自定义路径: " + pathStr);
        }
        CoTaskMemFree(pidl);
    }
}

// 开始扫描
void StartScan() {
    if (g_scanning) {
        return;
    }

    g_scanning = true;
    g_scanPaused = false;
    EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), FALSE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), TRUE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), TRUE);
    g_scanStartTime = std::chrono::steady_clock::now();

    // 创建扫描线程
    g_scanThread = CreateThread(NULL, 0, ScanThreadProc, NULL, 0, NULL);
    if (g_scanThread == NULL) {
        LogOutput(L"扫描线程创建失败");
        g_scanning = false;
        EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), TRUE);
        EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), FALSE);
        EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), FALSE);
    }
    else {
        LogOutput(L"开始扫描");
    }
}

// 扫描线程过程
DWORD WINAPI ScanThreadProc(LPVOID lpParameter) {
    ScanForMicroMiniNew();
    return 0;
}

// 安装线程过程
DWORD WINAPI InstallThreadProc(LPVOID lpParameter) {
    // 安装过程的实现
    std::wstring* installPath = (std::wstring*)lpParameter;
    bool success = ExtractZipWith7z(g_applicationPath + L"\\reshade\\MiNi-reshade.7z", *installPath);

    // 发送安装完成消息
    PostMessage(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_INSTALL_BUTTON, 0), 0);

    // 调用安装完成回调
    OnInstallationComplete(success);

    delete installPath;
    return 0;
}

// 安装完成回调
void OnInstallationComplete(bool success) {
    if (success) {
        std::wstring* pMessage = new std::wstring(L"安装完成");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        MessageBox(g_hWnd, L"安装完成", L"完成", MB_ICONINFORMATION);
    }
    else {
        std::wstring* pMessage = new std::wstring(L"安装失败");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        int result = MessageBox(g_hWnd, L"安装失败。是否要重启系统并尝试重新安装？", L"安装失败", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ScheduleRebootAndInstall();
        }
    }
}

// 扫描MicroMiniNew
void ScanForMicroMiniNew() {
    // 首先尝试使用Windows索引搜索
    if (WindowsIndexSearch()) {
        g_scanning = false;
        PostMessage(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_STOP_SCAN, 0), 0);

        if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
            auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            int hours = static_cast<int>(seconds / 3600);
            int minutes = static_cast<int>((seconds % 3600) / 60);
            int secs = static_cast<int>(seconds % 60);

            wchar_t timeStr[50];
            swprintf_s(timeStr, 50, L"%02d:%02d:%02d", hours, minutes, secs);

            std::wstring* pMessage = new std::wstring(L"扫描完成，总用时: " + std::wstring(timeStr));
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        }
        else {
            std::wstring* pMessage = new std::wstring(L"扫描完成");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        }

        return;
    }

    // 如果Windows索引搜索失败或没有找到结果，进行全盘扫描
    FullDiskScan();

    if (g_detectedVersions.empty()) {
        std::wstring* pMessage = new std::wstring(L"未找到MicroMiniNew.exe");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }

    g_scanning = false;
    PostMessage(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_STOP_SCAN, 0), 0);

    if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
        auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds % 3600) / 60);
        int secs = static_cast<int>(seconds % 60);

        wchar_t timeStr[50];
        swprintf_s(timeStr, 50, L"%02d:%02d:%02d", hours, minutes, secs);

        std::wstring* pMessage = new std::wstring(L"扫描完成，总用时: " + std::wstring(timeStr));
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }
    else {
        std::wstring* pMessage = new std::wstring(L"扫描完成");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    }
}

// Windows索引搜索
bool WindowsIndexSearch() {
    // 这部分需要使用COM接口，比较复杂，这里简化处理
    // 在实际应用中，可以使用OLE DB或ADO来查询Windows索引
    std::wstring* pMessage = new std::wstring(L"Windows索引搜索功能在C++版本中简化处理");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
    return false;
}

// 全盘扫描 - 在所有固定磁盘上搜索游戏启动器文件
void FullDiskScan() {
    std::wstring* pMessage = new std::wstring(L"开始全盘扫描...");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    // 定义需要排除的文件夹，避免在系统文件夹中浪费时间
    std::vector<std::wstring> excludedFolders = {
        L"$Recycle.Bin", L"System Volume Information", L"Windows",
        L"Program Files", L"Program Files (x86)", L"ProgramData",
        L"temp", L"tmp"
    };

    pMessage = new std::wstring(L"排除的文件夹: $Recycle.Bin, System Volume Information, Windows, Program Files, Program Files (x86), ProgramData, temp, tmp，以及路径中包含 'Temp' 的文件夹");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    // 遍历所有可能的驱动器字母（A-Z）
    for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
        std::wstring drivePath = std::wstring(1, drive) + L":\\";
        // 只扫描固定磁盘（不包括网络驱动器、光驱等）
        if (GetDriveType(drivePath.c_str()) == DRIVE_FIXED) {
            pMessage = new std::wstring(L"正在扫描驱动器 " + std::wstring(1, drive) + L"...");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

            std::wstring searchPath = drivePath + L"*";
            WIN32_FIND_DATA findData;
            HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

            if (hFind != INVALID_HANDLE_VALUE) {
                // 使用栈结构存储待扫描的目录
                std::vector<std::wstring> directories;
                directories.push_back(drivePath);

                // 循环扫描所有目录直到完成或用户中止
                while (!directories.empty() && g_scanning) {
                    // 如果扫描被暂停，则等待
                    while (g_scanPaused) {
                        Sleep(100);
                    }

                    // 如果扫描被停止，则退出
                    if (!g_scanning) {
                        break;
                    }

                    // 取出栈顶目录进行扫描
                    std::wstring currentDir = directories.back();
                    directories.pop_back();

                    searchPath = currentDir + L"*";
                    hFind = FindFirstFile(searchPath.c_str(), &findData);

                    if (hFind != INVALID_HANDLE_VALUE) {
                        do {
                            // 忽略当前目录和父目录标记
                            if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                                std::wstring fullPath = currentDir + findData.cFileName;

                                // 如果是目录
                                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                    // 检查是否为排除的文件夹
                                    bool excluded = false;
                                    for (const auto& folder : excludedFolders) {
                                        if (_wcsicmp(findData.cFileName, folder.c_str()) == 0 ||
                                            wcsstr(findData.cFileName, L"$") == findData.cFileName) {
                                            excluded = true;
                                            break;
                                        }
                                    }

                                    // 如果不是排除的文件夹，则添加到待扫描目录列表
                                    if (!excluded) {
                                        directories.push_back(fullPath + L"\\");
                                    }
                                }
                                // 如果是启动器.exe文件，则认为找到了游戏安装目录
                                else if (wcscmp(findData.cFileName, L"启动器.exe") == 0) {
                                    std::wstring path = currentDir;
                                    std::wstring version = L"未知版本 (路径: " + path + L")";
                                    AddDetectedVersion(version, path);
                                }
                            }
                        } while (FindNextFile(hFind, &findData) && g_scanning);

                        FindClose(hFind);
                    }
                }
            }
        }
    }
}

// 添加检测到的版本
void AddDetectedVersion(const std::wstring& version, const std::wstring& path) {
    g_detectedVersions[version] = path;
    std::wstring* pMessage = new std::wstring(L"检测到: " + version);
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
}

// 暂停扫描
void PauseScan() {
    if (!g_scanning) {
        return;
    }

    g_scanPaused = !g_scanPaused;
    if (g_scanPaused) {
        SetWindowText(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), L"继续扫描");
        LogOutput(L"扫描已暂停");
    }
    else {
        SetWindowText(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), L"暂停扫描");
        LogOutput(L"扫描已继续");
    }
}

// 停止扫描
void StopScan() {
    if (!g_scanning) {
        return;
    }

    g_scanning = false;
    EnableWindow(GetDlgItem(g_hWnd, IDC_START_SCAN), TRUE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_PAUSE_SCAN), FALSE);
    EnableWindow(GetDlgItem(g_hWnd, IDC_STOP_SCAN), FALSE);

    if (g_scanStartTime != std::chrono::steady_clock::time_point()) {
        auto elapsed = std::chrono::steady_clock::now() - g_scanStartTime;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds % 3600) / 60);
        int secs = static_cast<int>(seconds % 60);

        wchar_t timeStr[50];
        swprintf_s(timeStr, 50, L"%02d:%02d:%02d", hours, minutes, secs);
        LogOutput(L"用户终止扫描，总用时: " + std::wstring(timeStr));
    }
    else {
        LogOutput(L"扫描未开始就被终止");
    }

    PopulateOptions();
}

// 下载预设
void DownloadPresets() {
    // 获取选中的版本
    std::wstring selectedVersion;
    std::wstring installPath;

    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        if (SendMessage(hChild, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            wchar_t text[256];
            GetWindowText(hChild, text, 256);
            selectedVersion = text;
            break;
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    if (selectedVersion.empty()) {
        MessageBox(g_hWnd, L"请先选择一个游戏版本", L"警告", MB_ICONWARNING);
        LogOutput(L"尝试下载预设时未选择游戏版本");
        return;
    }

    installPath = g_detectedVersions[selectedVersion];

    if (IsDlgButtonChecked(g_hWnd, IDC_YIRIXC_CHECKBOX) == BST_CHECKED) {
        DownloadAndExtractPreset(L"https://json.scmgzs.top/preset/YiRixc/YiRixc.zip", installPath, L"YiRixc");
    }

    if (IsDlgButtonChecked(g_hWnd, IDC_TIANXINGDASHE_CHECKBOX) == BST_CHECKED) {
        DownloadAndExtractPreset(L"https://json.scmgzs.top/preset/StarSerpent/StarSerpent.zip", installPath, L"StarSerpent");
    }
}

// 下载并解压预设
void DownloadAndExtractPreset(const std::wstring& url, const std::wstring& installPath, const std::wstring& presetName) {
    std::wstring presetPath = installPath + L"\\presets";
    CreateDirectory(presetPath.c_str(), NULL);
    std::wstring zipPath = presetPath + L"\\" + presetName + L".zip";

    LogOutput(L"开始下载 " + presetName + L" 预设...");

    // 使用URLDownloadToFile下载文件
    HRESULT hr = URLDownloadToFile(NULL, url.c_str(), zipPath.c_str(), 0, NULL);
    if (SUCCEEDED(hr)) {
        LogOutput(presetName + L" 预设下载完成");

        if (ExtractZipWith7z(zipPath, presetPath)) {
            DeleteFile(zipPath.c_str());
            LogOutput(presetName + L" 预设下载并解压完成");
        }
        else {
            LogOutput(presetName + L" 预设解压失败");
        }
    }
    else {
        LogOutput(L"下载 " + presetName + L" 预设时出错: " + std::to_wstring(hr));
    }
}

// 使用7z解压ZIP文件 - 调用7za.exe工具解压光影包文件
bool ExtractZipWith7z(const std::wstring& zipPath, const std::wstring& extractPath) {
    LogOutput(L"开始解压缩 " + zipPath + L" 到 " + extractPath + L"...");

    // 检查压缩文件是否存在
    if (!PathFileExists(zipPath.c_str())) {
        LogOutput(L"7Z文件不存在: " + zipPath);
        return false;
    }

    // 创建解压目录（如果不存在）
    if (!PathFileExists(extractPath.c_str())) {
        CreateDirectory(extractPath.c_str(), NULL);
    }

    // 构建7z工具路径
    std::wstring sevenZipPath = g_applicationPath + L"\\7z\\7za.exe";
    if (!PathFileExists(sevenZipPath.c_str())) {
        LogOutput(L"7z.exe not found at " + sevenZipPath);
        return false;
    }

    // 最多重试3次解压操作
    int maxAttempts = 3;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        try {
            // 构建7z命令行参数
            std::wstring command = L"\"" + sevenZipPath + L"\" x \"" + zipPath + L"\" -o\"" + extractPath + L"\" -y";

            // 创建进程信息结构
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE; // 隐藏窗口
            ZeroMemory(&pi, sizeof(pi));

            // 创建7z解压进程
            if (CreateProcess(NULL, (LPWSTR)command.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                // 等待进程执行完成
                WaitForSingleObject(pi.hProcess, INFINITE);

                // 获取进程退出码判断是否成功
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);

                // 关闭进程句柄
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode != 0) {
                    LogOutput(L"7z extraction failed with return code " + std::to_wstring(exitCode));
                    if (attempt < maxAttempts - 1) {
                        LogOutput(L"等待3秒后重试... (尝试 " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                        Sleep(3000);
                    }
                    else {
                        return false;
                    }
                }
                else {
                    LogOutput(L"解压缩完成！");
                    return true;
                }
            }
            else {
                LogOutput(L"创建7z进程失败: " + std::to_wstring(GetLastError()));
                if (attempt < maxAttempts - 1) {
                    LogOutput(L"等待3秒后重试... (尝试 " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                    Sleep(3000);
                }
                else {
                    return false;
                }
            }
        }
        catch (...) {
            LogOutput(L"解压缩过程中出错");
            if (attempt < maxAttempts - 1) {
                LogOutput(L"等待3秒后重试... (尝试 " + std::to_wstring(attempt + 2) + L"/" + std::to_wstring(maxAttempts) + L")");
                Sleep(3000);
            }
            else {
                return false;
            }
        }
    }

    return false;
}

// 安装操作 - 修改版本，添加注册表文件处理
void InstallAction() {
    // 获取选中的版本
    std::wstring selectedVersion;
    std::wstring installPath;

    HWND hChild = GetWindow(g_hVersionGroup, GW_CHILD);
    while (hChild) {
        if (SendMessage(hChild, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            wchar_t text[256];
            GetWindowText(hChild, text, 256);
            selectedVersion = text;
            break;
        }
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    if (selectedVersion.empty()) {
        MessageBox(g_hWnd, L"请先选择一个游戏版本", L"警告", MB_ICONWARNING);
        LogOutput(L"尝试安装时未选择游戏版本");
        return;
    }

    installPath = g_detectedVersions[selectedVersion];

    // 检查运行中的进程
    std::vector<std::wstring> runningProcesses = CheckRunningProcesses(installPath);
    if (!runningProcesses.empty()) {
        std::wstring processList;
        for (const auto& process : runningProcesses) {
            if (!processList.empty()) {
                processList += L", ";
            }
            processList += process;
        }

        int result = MessageBox(g_hWnd, (L"以下进程正在运行：" + processList + L"\n是否结束这些进程并继续安装？").c_str(),
            L"警告", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            for (const auto& process : runningProcesses) {
                TerminateProcessByPath(installPath);
            }
            LogOutput(L"等待3秒以确保进程完全终止...");
            Sleep(3000);
        }
        else {
            return;
        }
    }

    StartInstallation(installPath);
}

// 检查运行中的进程
std::vector<std::wstring> CheckRunningProcesses(const std::wstring& installPath) {
    std::vector<std::wstring> runningProcesses;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return runningProcesses;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                wchar_t processPath[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, processPath, MAX_PATH)) {
                    std::wstring processPathStr = processPath;
                    if (processPathStr.find(installPath) == 0) {
                        runningProcesses.push_back(pe32.szExeFile);
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return runningProcesses;
}

// 根据路径终止进程
bool TerminateProcessByPath(const std::wstring& processPath) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool terminated = false;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                wchar_t path[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, path, MAX_PATH)) {
                    std::wstring pathStr = path;
                    if (pathStr.find(processPath) == 0) {
                        if (TerminateProcess(hProcess, 0)) {
                            terminated = true;
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return terminated;
}



// 开始安装 - 执行光影包的安装过程，包括解压文件和处理注册表
void StartInstallation(const std::wstring& installPath) {
    // 构建光影包ZIP文件路径
    std::wstring zipPath = g_applicationPath + L"\\reshade\\MiNi-reshade.7z";
    LogOutput(L"设置 zip 文件路径为: " + zipPath);

    std::wstring* pMessage = new std::wstring(L"开始安装...");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    // 解压光影包文件
    bool success = ExtractZipWith7z(zipPath, installPath);

    if (success) {
        pMessage = new std::wstring(L"文件解压完成，开始处理注册表文件...");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

        // 修改注册表文件（将游戏路径写入注册表文件）
        if (ModifyRegistryFile(installPath)) {
            pMessage = new std::wstring(L"注册表文件修改完成，开始运行注册表文件...");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

            // 运行注册表文件（导入到系统注册表）
            std::wstring regFilePath = installPath + L"\\1.reg";
            if (RunRegistryFile(regFilePath)) {
                pMessage = new std::wstring(L"安装完成！注册表已成功导入。");
                PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
                MessageBox(g_hWnd, L"安装完成！注册表已成功导入。", L"完成", MB_ICONINFORMATION);
            }
            else {
                pMessage = new std::wstring(L"注册表文件运行失败，但文件安装成功。");
                PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
                MessageBox(g_hWnd, L"文件安装成功，但注册表导入失败。请手动运行安装目录下的1.reg文件。", L"部分完成", MB_ICONWARNING);
            }
        }
        else {
            pMessage = new std::wstring(L"注册表文件修改失败，但文件安装成功。");
            PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
            MessageBox(g_hWnd, L"文件安装成功，但注册表文件修改失败。", L"部分完成", MB_ICONWARNING);
        }
    }
    else {
        pMessage = new std::wstring(L"安装失败");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        int result = MessageBox(g_hWnd, L"安装失败。是否要重启系统并尝试重新安装？", L"安装失败", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ScheduleRebootAndInstall();
        }
    }
}

//替代代码：
/*
void StartInstallation(const std::wstring& installPath) {
    std::wstring zipPath = g_applicationPath + L"\\reshade\\MiNi-reshade.7z";
    LogOutput(L"设置 zip 文件路径为: " + zipPath);

    std::wstring* pMessage = new std::wstring(L"开始安装...");
    PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);

    bool success = ExtractZipWith7z(zipPath, installPath);

    if (success) {
        pMessage = new std::wstring(L"文件解压完成，安装成功！");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        MessageBox(g_hWnd, L"安装完成！", L"完成", MB_ICONINFORMATION);
    }
    else {
        pMessage = new std::wstring(L"安装失败");
        PostMessage(g_hWnd, WM_LOG_MESSAGE, 0, (LPARAM)pMessage);
        int result = MessageBox(g_hWnd, L"安装失败。是否要重启系统并尝试重新安装？", L"安装失败", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            ScheduleRebootAndInstall();
        }
    }
}
*/


// 安排重启并安装
void ScheduleRebootAndInstall() {
    HKEY key;
    if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", &key) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        RegSetValueEx(key, L"MiniWorldInstaller", 0, REG_SZ, (BYTE*)path, (wcslen(path) + 1) * sizeof(wchar_t));
        RegCloseKey(key);

        system("shutdown /r /t 1");
    }
}

// 主函数 - 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化通用控件 - 为使用状态栏等通用控件做准备
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // 设置日志 - 初始化日志记录功能
    SetupLogging();

    // 获取应用程序路径 - 用于定位资源文件
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    g_applicationPath = std::wstring(path);
    size_t lastSlash = g_applicationPath.find_last_of(L"\\");
    if (lastSlash != std::wstring::npos) {
        g_applicationPath = g_applicationPath.substr(0, lastSlash);
    }

    // 检查管理员权限 - 确保程序以管理员身份运行以进行系统级操作
    if (!IsAdmin()) {
        LogOutput(L"不是管理员，尝试以管理员身份重新启动...");

        // 以管理员身份重新启动程序
        SHELLEXECUTEINFO sei = { sizeof(sei) };
        sei.lpVerb = L"runas";  // 使用"runas"动词请求管理员权限
        sei.lpFile = path;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteEx(&sei)) {
            return 0;  // 成功启动管理员版本，退出当前实例
        }
        else {
            MessageBox(NULL, L"无法以管理员身份启动程序。", L"错误", MB_ICONERROR);
            return 1;  // 启动失败，返回错误码
        }
    }

    // 注册窗口类
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"MiniWorldShaderInstallerClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"窗口注册失败！", L"错误", MB_ICONERROR);
        return 1;
    }

    // 修改窗口创建样式，移除标准窗口控制按钮
    DWORD exStyle = WS_EX_LAYERED | WS_EX_APPWINDOW;
    g_hWnd = CreateWindowEx(
        exStyle,
        L"MiniWorldShaderInstallerClass",
        L"迷你世界光影包安装器V2.7.53",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, // 使用WS_POPUP代替WS_OVERLAPPEDWINDOW
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_windowWidth, g_windowHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hWnd) {
        MessageBox(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
        return 1;
    }

    // 设置窗口初始位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    g_windowStartX = (screenWidth - g_windowWidth) / 2 - 200; // 从左侧开始
    g_windowTargetX = (screenWidth - g_windowWidth) / 2;      // 移动到中间
    g_windowY = (screenHeight - g_windowHeight) / 2;

    // 设置窗口位置
    SetWindowPos(g_hWnd, NULL, g_windowStartX, g_windowY, g_windowWidth, g_windowHeight, SWP_NOZORDER);

    // 设置窗口透明度
    SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);

    // 启动窗口动画
    SetTimer(g_hWnd, g_animationTimerId, 16, NULL); // 约60fps

    // 显示窗口
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 关闭日志文件
    if (g_logFile.is_open()) {
        g_logFile.close();
    }

    return (int)msg.wParam;
}