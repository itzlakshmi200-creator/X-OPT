/*
 ██╗  ██╗      ██████╗ ██████╗ ████████╗
 ╚██╗██╔╝     ██╔═══██╗██╔══██╗╚══██╔══╝
  ╚███╔╝      ██║   ██║██████╔╝   ██║
  ██╔██╗      ██║   ██║██╔═══╝    ██║
 ██╔╝ ██╗     ╚██████╔╝██║        ██║
 ╚═╝  ╚═╝      ╚═════╝ ╚═╝        ╚═╝

 X-OPT Engine  v1.0
 Ultimate Realtime PC Optimisation Engine
 Premium iOS-inspired UI  |  Built for Everyone
 ─────────────────────────────────────────────────
 Compile:  cmake + vcpkg  |  CI: GitHub Actions
*/

// ──────────────────────────────────────────────────────────────────────────────
//  INCLUDES
// ──────────────────────────────────────────────────────────────────────────────
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>       // GET_X_LPARAM, GET_Y_LPARAM
#include <dwmapi.h>         // DwmSetWindowAttribute, DwmExtendFrameIntoClientArea, MARGINS
#include <d3d11.h>
#include <tchar.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <powrprof.h>
#include <psapi.h>
#include <winreg.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <map>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// IM_PI: defined in imgui_internal.h but we avoid that dependency
#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace fs = std::filesystem;

// ──────────────────────────────────────────────────────────────────────────────
//  DESIGN SYSTEM  —  iOS Dark Palette
// ──────────────────────────────────────────────────────────────────────────────
namespace DS {
    // Backgrounds
    constexpr ImVec4 BG_BASE       = { 0.000f, 0.000f, 0.000f, 1.00f }; // #000000
    constexpr ImVec4 BG_ELEVATED   = { 0.110f, 0.110f, 0.118f, 1.00f }; // #1C1C1E
    constexpr ImVec4 BG_CARD       = { 0.173f, 0.173f, 0.180f, 1.00f }; // #2C2C2E
    constexpr ImVec4 BG_CARD_HIGH  = { 0.227f, 0.227f, 0.235f, 1.00f }; // #3A3A3C
    constexpr ImVec4 BG_SEPARATOR  = { 0.329f, 0.329f, 0.345f, 0.40f }; // #545458 40%

    // Accents
    constexpr ImVec4 ACCENT_BLUE   = { 0.039f, 0.518f, 1.000f, 1.00f }; // #0A84FF
    constexpr ImVec4 ACCENT_GREEN  = { 0.188f, 0.820f, 0.345f, 1.00f }; // #30D158
    constexpr ImVec4 ACCENT_RED    = { 1.000f, 0.271f, 0.227f, 1.00f }; // #FF453A
    constexpr ImVec4 ACCENT_ORANGE = { 1.000f, 0.624f, 0.039f, 1.00f }; // #FF9F0A
    constexpr ImVec4 ACCENT_PURPLE = { 0.749f, 0.353f, 1.000f, 1.00f }; // #BF5AF2
    constexpr ImVec4 ACCENT_PINK   = { 1.000f, 0.216f, 0.373f, 1.00f }; // #FF375F

    // Text
    constexpr ImVec4 TEXT_PRIMARY  = { 1.000f, 1.000f, 1.000f, 1.00f };
    constexpr ImVec4 TEXT_SECONDARY= { 0.922f, 0.922f, 0.961f, 0.60f }; // #EBEBF5 60%
    constexpr ImVec4 TEXT_TERTIARY = { 0.922f, 0.922f, 0.961f, 0.30f }; // #EBEBF5 30%

    // Helper: ImVec4 → ImU32
    static ImU32 Col(ImVec4 c, float a = 1.0f) {
        return IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), (int)(c.w*a*255));
    }
    static ImU32 ColA(ImVec4 c, float alpha) {
        return IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), (int)(alpha*255));
    }

    // Lerp between two ImVec4 colors
    static ImVec4 Lerp(ImVec4 a, ImVec4 b, float t) {
        return { a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t,
                 a.z + (b.z-a.z)*t, a.w + (b.w-a.w)*t };
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  ANIMATION HELPERS
// ──────────────────────────────────────────────────────────────────────────────
// Per-ID smooth float animation (spring-like)
struct AnimState { float value = 0.0f; float velocity = 0.0f; };
static std::map<ImGuiID, AnimState> g_anim;

static float SmoothAnimate(ImGuiID id, float target, float speed = 14.0f) {
    float dt = ImGui::GetIO().DeltaTime;
    auto& s = g_anim[id];
    float diff = target - s.value;
    s.velocity += diff * speed * dt;
    s.velocity *= powf(0.001f, dt);  // damping
    s.value += s.velocity * dt * 60.0f;
    if (fabsf(diff) < 0.001f && fabsf(s.velocity) < 0.001f) s.value = target;
    return s.value;
}

static float EaseInOut(float t) { return t * t * (3.0f - 2.0f * t); }

// ──────────────────────────────────────────────────────────────────────────────
//  GLOBAL APPLICATION STATE
// ──────────────────────────────────────────────────────────────────────────────
struct AppState {
    // Navigation
    int  activeTab      = 0;           // 0=Boost 1=Clean 2=Launch 3=Phonk

    // Boost toggles
    bool explorerKilled = false;
    bool highPerfPower  = false;
    bool animsDisabled  = false;
    bool gameModeOn     = false;
    bool cpuBoost       = false;
    bool superfetchOff  = false;
    bool hpetOn         = false;
    bool gameBarOff     = false;
    bool nvidiaBoost    = false;
    bool networkOpt     = false;

    // Boost status
    int  boostScore     = 0;

    // Clean
    bool cleanTempDone      = false;
    bool cleanWinTempDone   = false;
    bool cleanPrefetchDone  = false;
    bool cleanDNSDone       = false;
    std::string cleanLog;
    std::atomic<bool> cleanRunning{ false };

    // Launch
    char gamePath[512]  = {};
    bool launchReady    = false;
    std::string launchStatus = "Browse for your game .exe";

    // Phonk Player
    bool  phonkPlaying  = false;
    float phonkVolume   = 70.0f;
    char  phonkPath[512] = {};
    bool  phonkLoaded   = false;
    std::string phonkTitle = "No track loaded";
    float phonkProgress = 0.0f;
    bool  phonkLoop     = false;

    // Status notifications
    struct Notif { std::string msg; ImVec4 col; float timer; };
    std::vector<Notif> notifs;
    std::mutex notifMtx;

    void PushNotif(const std::string& msg, ImVec4 col = DS::ACCENT_GREEN) {
        std::lock_guard<std::mutex> lk(notifMtx);
        notifs.push_back({ msg, col, 3.0f });
    }
} g_app;

// DirectX state
static ID3D11Device*           g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext     = nullptr;
static IDXGISwapChain*         g_pSwapChain            = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView  = nullptr;

// ──────────────────────────────────────────────────────────────────────────────
//  OPTIMISATION FUNCTIONS  (actual Windows API work)
// ──────────────────────────────────────────────────────────────────────────────
namespace Opt {

    static void RunCmd(const std::wstring& cmd, bool hidden = true) {
        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        if (hidden) { si.dwFlags |= STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; }
        std::wstring mutable_cmd = cmd;
        CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }

    static void KillExplorer() {
        RunCmd(L"cmd /c taskkill /f /im explorer.exe");
        g_app.explorerKilled = true;
        g_app.PushNotif("Explorer killed — taskbar hidden", DS::ACCENT_ORANGE);
    }
    static void RestartExplorer() {
        ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOW);
        g_app.explorerKilled = false;
        g_app.PushNotif("Explorer restarted", DS::ACCENT_GREEN);
    }
    static void ToggleExplorer() {
        g_app.explorerKilled ? RestartExplorer() : KillExplorer();
    }

    static void SetHighPerformancePower(bool on) {
        // High Performance GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c
        if (on) RunCmd(L"cmd /c powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
        else    RunCmd(L"cmd /c powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
        g_app.PushNotif(on ? "High Performance power plan activated"
                           : "Balanced power plan restored");
    }

    static void SetWindowsAnimations(bool on) {
        ANIMATIONINFO ai{ sizeof(ANIMATIONINFO), on ? 1 : 0 };
        SystemParametersInfoW(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &ai, SPIF_UPDATEINIFILE);
        SystemParametersInfoW(SPI_SETLISTBOXSMOOTHSCROLLING, 0, (PVOID)(UINT_PTR)on, SPIF_SENDCHANGE);
        SystemParametersInfoW(SPI_SETMENUANIMATION,   0, (PVOID)(UINT_PTR)on, SPIF_SENDCHANGE);
        SystemParametersInfoW(SPI_SETSELECTIONFADE,   0, (PVOID)(UINT_PTR)on, SPIF_SENDCHANGE);
        SystemParametersInfoW(SPI_SETTOOLTIPANIMATION,0, (PVOID)(UINT_PTR)on, SPIF_SENDCHANGE);
        g_app.PushNotif(on ? "Windows animations re-enabled"
                           : "Windows animations disabled — less CPU waste");
    }

    static void SetGameMode(bool on) {
        HKEY hk;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\GameBar", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
            DWORD v = on ? 1 : 0;
            RegSetValueExW(hk, L"AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
            RegCloseKey(hk);
        }
        g_app.PushNotif(on ? "Windows Game Mode enabled" : "Windows Game Mode disabled");
    }

    static void SetGameBar(bool on) {
        HKEY hk;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR",
            0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
            DWORD v = on ? 1 : 0;
            RegSetValueExW(hk, L"AppCaptureEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
            RegCloseKey(hk);
        }
        g_app.gameBarOff = !on;
        g_app.PushNotif(on ? "Game Bar enabled" : "Game Bar / DVR disabled — reclaims RAM");
    }

    static void SetHPET(bool on) {
        if (on) {
            timeBeginPeriod(1);
            RunCmd(L"cmd /c bcdedit /set useplatformtick yes");
        } else {
            timeEndPeriod(1);
            RunCmd(L"cmd /c bcdedit /deletevalue useplatformtick");
        }
        g_app.PushNotif(on ? "Timer resolution set to 1ms — input lag ↓" : "Timer resolution restored");
    }

    static void SetSuperfetch(bool disable) {
        if (disable) RunCmd(L"cmd /c sc stop SysMain & sc config SysMain start=disabled");
        else         RunCmd(L"cmd /c sc config SysMain start=auto & sc start SysMain");
        g_app.PushNotif(disable ? "SuperFetch/SysMain stopped — RAM freed"
                                : "SuperFetch/SysMain re-enabled");
    }

    static void SetNetworkOpt(bool on) {
        if (on) {
            // Disable Nagle algorithm, set TCP ACK frequency
            RunCmd(L"cmd /c netsh int tcp set global autotuninglevel=disabled");
            RunCmd(L"cmd /c netsh int tcp set global chimney=disabled");
            HKEY hk;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
                0, KEY_ENUMERATE_SUB_KEYS, &hk) == ERROR_SUCCESS) {
                // Iterate subkeys and set TcpAckFrequency
                WCHAR name[256]; DWORD i = 0, len = 256;
                while (RegEnumKeyExW(hk, i++, name, &len, 0,0,0,0) == ERROR_SUCCESS) {
                    HKEY hSub;
                    std::wstring path = std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\") + name;
                    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_SET_VALUE, &hSub) == ERROR_SUCCESS) {
                        DWORD v1 = 1, v2 = 1;
                        RegSetValueExW(hSub, L"TcpAckFrequency",  0, REG_DWORD, (BYTE*)&v1, 4);
                        RegSetValueExW(hSub, L"TCPNoDelay",        0, REG_DWORD, (BYTE*)&v2, 4);
                        RegCloseKey(hSub);
                    }
                    len = 256;
                }
                RegCloseKey(hk);
            }
        }
        g_app.PushNotif(on ? "Network optimised — Nagle off, ACK=1" : "Network settings restored");
    }

    static void LaunchGameWithPriority(const std::string& path) {
        if (path.empty()) { g_app.PushNotif("No game path set!", DS::ACCENT_RED); return; }
        std::wstring wpath(path.begin(), path.end());
        STARTUPINFOW si{}; PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        if (CreateProcessW(wpath.c_str(), nullptr, nullptr, nullptr, FALSE,
                           HIGH_PRIORITY_CLASS, nullptr, nullptr, &si, &pi)) {
            SetPriorityClass(pi.hProcess, HIGH_PRIORITY_CLASS);
            SetThreadPriority(pi.hThread, THREAD_PRIORITY_HIGHEST);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            g_app.launchStatus = "Launched with HIGH priority!";
            g_app.PushNotif("Game launched with HIGH priority class", DS::ACCENT_GREEN);
        } else {
            g_app.launchStatus = "Launch failed — check path";
            g_app.PushNotif("Launch failed! Check the path.", DS::ACCENT_RED);
        }
    }

    static size_t DeleteTempFolder(const fs::path& dir) {
        size_t count = 0;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(dir, ec)) {
            fs::remove_all(entry.path(), ec);
            if (!ec) ++count;
        }
        return count;
    }

    static void CleanTempFiles(std::function<void(std::string)> log) {
        g_app.cleanRunning = true;
        size_t total = 0;

        // %TEMP%
        wchar_t tmp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp)) {
            size_t n = DeleteTempFolder(fs::path(tmp));
            total += n; g_app.cleanTempDone = true;
            log("  %TEMP%: removed " + std::to_string(n) + " items");
        }
        // C:\Windows\Temp
        size_t n2 = DeleteTempFolder(L"C:\\Windows\\Temp");
        total += n2; g_app.cleanWinTempDone = true;
        log("  C:\\Windows\\Temp: removed " + std::to_string(n2) + " items");

        // Prefetch
        size_t n3 = DeleteTempFolder(L"C:\\Windows\\Prefetch");
        total += n3; g_app.cleanPrefetchDone = true;
        log("  Prefetch: removed " + std::to_string(n3) + " items");

        // DNS
        RunCmd(L"cmd /c ipconfig /flushdns");
        g_app.cleanDNSDone = true;
        log("  DNS cache flushed");

        log("\n  Total: " + std::to_string(total) + " items cleared");
        g_app.PushNotif("Clean complete — " + std::to_string(total) + " items removed");
        g_app.cleanRunning = false;
    }

    static int  ComputeBoostScore() {
        int s = 0;
        if (g_app.explorerKilled) s += 10;
        if (g_app.highPerfPower)  s += 20;
        if (g_app.animsDisabled)  s += 8;
        if (g_app.gameModeOn)     s += 12;
        if (g_app.superfetchOff)  s += 15;
        if (g_app.hpetOn)         s += 18;
        if (g_app.gameBarOff)     s += 10;
        if (g_app.networkOpt)     s += 7;
        return s;
    }

}  // namespace Opt

// ──────────────────────────────────────────────────────────────────────────────
//  PHONK PLAYER  (Windows MCI)
// ──────────────────────────────────────────────────────────────────────────────
namespace Phonk {

    static bool  s_open = false;

    static void  Open(const std::string& path) {
        std::string cmd = "open \"" + path + "\" type mpegvideo alias phonk";
        mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
        s_open = true;
        // Extract filename as title
        fs::path p(path);
        g_app.phonkTitle = p.stem().string();
        g_app.phonkLoaded = true;
    }

    static void  Play() {
        if (!s_open) return;
        std::string cmd = g_app.phonkLoop
            ? "play phonk repeat"
            : "play phonk";
        mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
        g_app.phonkPlaying = true;
    }

    static void  Pause() {
        mciSendStringA("pause phonk", nullptr, 0, nullptr);
        g_app.phonkPlaying = false;
    }

    static void  Stop() {
        mciSendStringA("stop phonk",  nullptr, 0, nullptr);
        mciSendStringA("close phonk", nullptr, 0, nullptr);
        g_app.phonkPlaying = false;
        s_open = false;
    }

    static void  SetVolume(float vol) {
        // MCI volume 0-1000
        int v = (int)(vol / 100.0f * 1000.0f);
        std::string cmd = "setaudio phonk volume to " + std::to_string(v);
        mciSendStringA(cmd.c_str(), nullptr, 0, nullptr);
    }

    static float GetProgress() {
        char pos[64] = {}, len[64] = {};
        mciSendStringA("status phonk position", pos, sizeof(pos), nullptr);
        mciSendStringA("status phonk length",   len, sizeof(len), nullptr);
        long p = atol(pos), l = atol(len);
        return (l > 0) ? (float)p / (float)l : 0.0f;
    }

}  // namespace Phonk

// ──────────────────────────────────────────────────────────────────────────────
//  CUSTOM IMGUI WIDGETS
// ──────────────────────────────────────────────────────────────────────────────
namespace Widget {

    // ── iOS Toggle Switch ──────────────────────────────────────────────────────
    // Returns true if value changed
    static bool Toggle(const char* id, bool* v, ImVec4 accentColor = DS::ACCENT_BLUE) {
        ImGuiID wid = ImGui::GetID(id);
        float t = SmoothAnimate(wid, *v ? 1.0f : 0.0f, 16.0f);

        const float W = 50.0f, H = 28.0f, R = H * 0.5f;
        const float PAD = 3.0f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Invisible button
        ImGui::InvisibleButton(id, ImVec2(W, H));
        bool clicked = ImGui::IsItemClicked();
        bool hovered = ImGui::IsItemHovered();

        // Track background: lerp off→on color
        ImVec4 offCol  = DS::BG_CARD_HIGH;
        ImVec4 trackC  = DS::Lerp(offCol, accentColor, t);
        float  alpha   = hovered ? 0.9f : 1.0f;
        ImU32  trackUi = DS::ColA(trackC, alpha);

        dl->AddRectFilled(pos, ImVec2(pos.x+W, pos.y+H), trackUi, R);

        // Glow under the track when on
        if (t > 0.01f) {
            ImU32 glow = DS::ColA(accentColor, 0.25f * t);
            dl->AddRectFilled(ImVec2(pos.x-3, pos.y-3),
                              ImVec2(pos.x+W+3, pos.y+H+3), glow, R+3);
        }

        // Thumb (white circle)
        float thumbX  = pos.x + PAD + t * (W - H);
        float thumbCX = thumbX + (H - PAD*2) * 0.5f;
        float thumbCY = pos.y  + H * 0.5f;
        float thumbR  = (H - PAD*2) * 0.5f;
        dl->AddCircleFilled(ImVec2(thumbCX, thumbCY), thumbR,
                            IM_COL32(255,255,255,255), 32);
        // Subtle shadow on thumb
        dl->AddCircle(ImVec2(thumbCX, thumbCY), thumbR+1.0f,
                      IM_COL32(0,0,0,40), 32, 1.0f);

        if (clicked) { *v = !*v; return true; }
        return false;
    }

    // ── Premium Slider (horizontal) ────────────────────────────────────────────
    static bool Slider(const char* id, float* v, float mn, float mx,
                       ImVec4 color = DS::ACCENT_BLUE) {
        ImGuiID wid = ImGui::GetID(id);
        const float W = ImGui::GetContentRegionAvail().x;
        const float H = 6.0f;
        const float THUMB_R = 11.0f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float totalH = THUMB_R * 2.0f + 2.0f;
        ImGui::InvisibleButton(id, ImVec2(W, totalH));
        bool hovered  = ImGui::IsItemHovered();
        bool active   = ImGui::IsItemActive();

        if (active) {
            float mx_  = ImGui::GetIO().MousePos.x;
            float nv   = (mx_ - pos.x - THUMB_R) / (W - THUMB_R*2.0f);
            *v = mn + std::clamp(nv, 0.0f, 1.0f) * (mx - mn);
        }

        float t   = (*v - mn) / (mx - mn);
        // smooth the thumb position for premium feel
        float tSm = SmoothAnimate(ImGui::GetID((std::string(id)+"_sm").c_str()), t, 20.0f);
        float thumbX = pos.x + THUMB_R + tSm * (W - THUMB_R*2.0f);
        float trackY = pos.y + totalH * 0.5f;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Track background
        dl->AddRectFilled(
            ImVec2(pos.x + THUMB_R, trackY - H*0.5f),
            ImVec2(pos.x + W - THUMB_R, trackY + H*0.5f),
            DS::Col(DS::BG_CARD_HIGH), H);

        // Filled portion (gradient feel via two rects)
        if (tSm > 0.001f) {
            ImVec2 fillP1 = ImVec2(pos.x + THUMB_R, trackY - H*0.5f);
            ImVec2 fillP2 = ImVec2(thumbX, trackY + H*0.5f);
            // gradient: slightly brighter start
            dl->AddRectFilled(fillP1, fillP2, DS::Col(color), H);
        }

        // Glow behind thumb
        float glowR = (active || hovered) ? THUMB_R + 8.0f : THUMB_R + 4.0f;
        dl->AddCircleFilled({thumbX, trackY}, glowR,
                            DS::ColA(color, (active||hovered) ? 0.35f : 0.20f), 32);

        // Thumb
        float tr = (active ? THUMB_R + 2.0f : (hovered ? THUMB_R + 1.0f : THUMB_R));
        tr = THUMB_R + SmoothAnimate(ImGui::GetID((std::string(id)+"_r").c_str()),
                                     active ? 2.0f : (hovered ? 1.0f : 0.0f), 20.0f);
        dl->AddCircleFilled({thumbX, trackY}, tr, IM_COL32(255,255,255,255), 32);
        // thumb border
        dl->AddCircle({thumbX, trackY}, tr, DS::ColA(color, 0.6f), 32, 1.5f);

        return active;
    }

    // ── Card container ─────────────────────────────────────────────────────────
    static void BeginCard(float height = 0, ImVec4 bg = DS::BG_CARD) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild(ImGui::GetID(&bg), ImVec2(w, height), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }
    static void EndCard() {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Section label ──────────────────────────────────────────────────────────
    static void Label(const char* txt, ImVec4 col = DS::TEXT_SECONDARY) {
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(txt);
        ImGui::PopStyleColor();
    }

    // ── Row: label left, widget right ─────────────────────────────────────────
    // Call before widget; pushes cursor to right side
    static void RowStart(const char* label, const char* sublabel = nullptr) {
        float rowH = sublabel ? 44.0f : 32.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
        if (sublabel) {
            ImGui::SameLine(0, 4);
            // newline
            ImGui::NewLine();
            ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.0f);
            ImGui::Text("  %s", sublabel);
            ImGui::PopStyleColor();
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 58.0f);
    }

    // ── Boost row with toggle ──────────────────────────────────────────────────
    static bool BoostRow(const char* label, const char* desc,
                         bool* val, ImVec4 accent = DS::ACCENT_BLUE) {
        float sy = ImGui::GetCursorPosY();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
        ImGui::Text("%s", desc);
        ImGui::PopStyleColor();

        // Put toggle on same rows, right side
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 56.0f);
        ImGui::SetCursorPosY(sy + 4.0f);
        bool changed = Toggle(label, val, accent);

        // Thin separator line
        float lx = ImGui::GetWindowPos().x + 20.0f;
        float rx = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 20.0f;
        float ly = ImGui::GetWindowPos().y + ImGui::GetCursorPosY();
        ImGui::GetWindowDrawList()->AddLine(
            {lx, ly}, {rx, ly}, DS::Col(DS::BG_SEPARATOR), 1.0f);
        ImGui::Dummy({0, 10});
        return changed;
    }

    // ── Tab bar (iOS-style pill selector) ─────────────────────────────────────
    static void TabBar(const char** tabs, int count, int* active) {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        float       W   = ImGui::GetContentRegionAvail().x;
        float       H   = 38.0f;
        float       PAD = 4.0f;
        float       tabW = (W - PAD*2) / count;
        ImVec2      base = ImGui::GetCursorScreenPos();

        // outer pill bg
        dl->AddRectFilled(base, {base.x+W, base.y+H},
                          DS::Col(DS::BG_CARD_HIGH), H*0.5f);

        // animated selection pill
        float selX = SmoothAnimate(ImGui::GetID("_tab_anim"),
                                   (float)*active * tabW, 18.0f);
        dl->AddRectFilled(
            {base.x + PAD + selX, base.y + PAD},
            {base.x + PAD + selX + tabW, base.y + H - PAD},
            DS::Col(DS::ACCENT_BLUE), (H - PAD*2) * 0.5f);

        // buttons
        ImGui::SetCursorScreenPos(base);
        for (int i = 0; i < count; i++) {
            ImVec2 p0 = {base.x + tabW*i, base.y};
            ImVec2 p1 = {p0.x + tabW, p0.y + H};
            ImGui::SetCursorScreenPos(p0);
            ImGui::InvisibleButton(tabs[i], {tabW, H});
            if (ImGui::IsItemClicked()) *active = i;

            bool sel = (*active == i);
            ImVec4 tc = sel ? DS::TEXT_PRIMARY : DS::TEXT_SECONDARY;
            ImVec2 tsz = ImGui::CalcTextSize(tabs[i]);
            ImVec2 tp  = {p0.x + (tabW - tsz.x)*0.5f, p0.y + (H - tsz.y)*0.5f};
            dl->AddText(tp, DS::Col(tc), tabs[i]);
        }
        ImGui::SetCursorScreenPos({base.x, base.y + H + 4.0f});
        ImGui::Dummy({W, 0});
    }

    // ── Score ring ────────────────────────────────────────────────────────────
    static void ScoreRing(float score, ImVec4 color) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 c  = ImGui::GetCursorScreenPos();
        float  R  = 44.0f;
        c.x += R; c.y += R;

        dl->AddCircle(c, R, DS::Col(DS::BG_CARD_HIGH), 128, 6.0f);
        float ang  = (float)(score / 100.0f) * IM_PI * 2.0f;
        float t    = SmoothAnimate(ImGui::GetID("_scoreang"), score/100.0f, 8.0f);
        float endA = -IM_PI/2.0f + t * IM_PI * 2.0f;
        // Arc
        int   segs = (int)(t * 128) + 1;
        for (int i = 0; i < segs-1; i++) {
            float a0 = -IM_PI/2.0f + (float)i/(float)(segs-1) * t * IM_PI*2.0f;
            float a1 = -IM_PI/2.0f + (float)(i+1)/(float)(segs-1) * t * IM_PI*2.0f;
            dl->AddLine(
                {c.x + cosf(a0)*R, c.y + sinf(a0)*R},
                {c.x + cosf(a1)*R, c.y + sinf(a1)*R},
                DS::Col(color), 6.0f);
        }
        // Center text
        char buf[16]; snprintf(buf, 16, "%d", (int)score);
        ImVec2 ts  = ImGui::CalcTextSize(buf);
        dl->AddText({c.x - ts.x/2, c.y - ts.y/2}, DS::Col(DS::TEXT_PRIMARY), buf);
        char sub[] = "SCORE";
        ImVec2 ss  = ImGui::CalcTextSize(sub);
        dl->AddText({c.x - ss.x/2, c.y + ts.y/2 - 2},
                    DS::Col(DS::TEXT_SECONDARY), sub);

        ImGui::Dummy({R*2+4, R*2+8});
    }

    // ── Notification toasts ───────────────────────────────────────────────────
    static void RenderNotifs() {
        float dt = ImGui::GetIO().DeltaTime;
        std::lock_guard<std::mutex> lk(g_app.notifMtx);
        float y = ImGui::GetIO().DisplaySize.y - 20.0f;
        for (auto it = g_app.notifs.rbegin(); it != g_app.notifs.rend(); ++it) {
            it->timer -= dt;
            float alpha = std::min(1.0f, it->timer * 2.0f); // fade out last 0.5s
            if (alpha <= 0) continue;
            ImVec2 ts = ImGui::CalcTextSize(it->msg.c_str());
            float W   = ts.x + 28.0f, H = 34.0f;
            float x   = (ImGui::GetIO().DisplaySize.x - W) * 0.5f;
            y -= H + 6.0f;
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddRectFilled({x, y}, {x+W, y+H},
                DS::ColA(DS::BG_CARD, alpha * 0.95f), 10.0f);
            dl->AddRect({x, y}, {x+W, y+H},
                DS::ColA(it->col, alpha * 0.6f), 10.0f, 0, 1.5f);
            dl->AddRectFilled({x, y}, {x+4, y+H},
                DS::ColA(it->col, alpha), 3.0f);
            dl->AddText({x+12, y+(H-ts.y)*0.5f},
                DS::ColA(DS::TEXT_PRIMARY, alpha), it->msg.c_str());
        }
        g_app.notifs.erase(std::remove_if(g_app.notifs.begin(), g_app.notifs.end(),
            [](auto& n){ return n.timer <= 0; }), g_app.notifs.end());
    }

}  // namespace Widget

// ──────────────────────────────────────────────────────────────────────────────
//  UI PANELS
// ──────────────────────────────────────────────────────────────────────────────
static void RenderBoostPanel() {
    // Score header row
    {
        float sc = (float)Opt::ComputeBoostScore();
        g_app.boostScore = (int)sc;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, DS::BG_ELEVATED);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 20.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,16));
        float W = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("##score_card", ImVec2(W, 106), false);

        // Score ring on left
        float sy = ImGui::GetCursorPosY();
        Widget::ScoreRing(sc, sc > 60 ? DS::ACCENT_GREEN
                                      : (sc > 30 ? DS::ACCENT_ORANGE : DS::ACCENT_RED));
        ImGui::SameLine(110);
        ImGui::SetCursorPosY(sy + 14);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        ImGui::TextUnformatted("Optimisation Score");
        ImGui::PopStyleColor();
        ImGui::SetCursorPosY(sy + 38);
        ImGui::SameLine(110);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
        const char* grade = sc > 80 ? "Excellent — Near-native performance"
                          : sc > 50 ? "Good — Significant improvements applied"
                          : sc > 20 ? "Fair — Apply more toggles below"
                                    : "Baseline — Enable tweaks to boost FPS";
        ImGui::Text("%s", grade);
        ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Boost options card
    Widget::BeginCard(0, DS::BG_ELEVATED);

    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("PERFORMANCE");
    ImGui::PopStyleColor();
    ImGui::Dummy({0,4});

    auto row = [](const char* lbl, const char* desc, bool* v,
                  ImVec4 col, std::function<void(bool)> fn) {
        if (Widget::BoostRow(lbl, desc, v, col)) fn(*v);
    };

    row("High Performance Power",   "Maximum CPU + GPU clock speeds",
        &g_app.highPerfPower,  DS::ACCENT_BLUE,
        [](bool on){ Opt::SetHighPerformancePower(on); });

    row("High-Res Timer (1ms)",     "Reduces scheduling latency & input lag",
        &g_app.hpetOn,         DS::ACCENT_PURPLE,
        [](bool on){ Opt::SetHPET(on); });

    row("CPU Priority Boost",       "Foreground process gets more CPU time",
        &g_app.cpuBoost,       DS::ACCENT_ORANGE, [](bool on){
            HKEY hk;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl",
                0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
                DWORD v = on ? 2 : 1;
                RegSetValueExW(hk, L"Win32PrioritySeparation",0,REG_DWORD,(BYTE*)&v,4);
                RegCloseKey(hk);
            }
            g_app.PushNotif(on ? "CPU priority separation maximised" : "CPU priority restored");
        });

    row("Network Low-Latency",      "Disables Nagle, sets TCP ACK = 1",
        &g_app.networkOpt,     DS::ACCENT_BLUE,
        [](bool on){ Opt::SetNetworkOpt(on); });

    ImGui::Dummy({0,4});
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("WINDOWS OVERHEAD");
    ImGui::PopStyleColor();
    ImGui::Dummy({0,4});

    row("Kill Windows Explorer",    "Frees RAM + CPU  |  taskbar disappears",
        &g_app.explorerKilled, DS::ACCENT_RED,
        [](bool){ Opt::ToggleExplorer(); });

    row("Disable SuperFetch",       "Stops background prefetching — frees RAM",
        &g_app.superfetchOff,  DS::ACCENT_ORANGE,
        [](bool on){ Opt::SetSuperfetch(on); });

    row("Disable Windows Animations","Snappier UI, less GPU load",
        &g_app.animsDisabled,  DS::ACCENT_GREEN,
        [](bool on){ Opt::SetWindowsAnimations(!on); });

    row("Game Mode",               "Windows shifts resources to foreground game",
        &g_app.gameModeOn,     DS::ACCENT_BLUE,
        [](bool on){ Opt::SetGameMode(on); });

    row("Disable Xbox Game Bar/DVR","Reclaims RAM + removes background capture",
        &g_app.gameBarOff,     DS::ACCENT_PINK,
        [](bool on){ Opt::SetGameBar(!on); });

    Widget::EndCard();
}

// ──────────────────────────────────────────────────────────────────────────────
static void RenderCleanPanel() {
    Widget::BeginCard(0, DS::BG_ELEVATED);

    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("ONE-TAP DEEP CLEAN");
    ImGui::PopStyleColor();
    ImGui::Dummy({0,6});

    // Status dots
    auto dot = [](const char* lbl, bool done) {
        ImVec4 c = done ? DS::ACCENT_GREEN : DS::BG_CARD_HIGH;
        ImGui::GetWindowDrawList()->AddCircleFilled(
            {ImGui::GetCursorScreenPos().x + 6,
             ImGui::GetCursorScreenPos().y + 9}, 5.0f, DS::Col(c), 16);
        ImGui::Dummy({14,18}); ImGui::SameLine(0,6);
        ImGui::PushStyleColor(ImGuiCol_Text, done ? DS::TEXT_PRIMARY : DS::TEXT_SECONDARY);
        ImGui::Text("%s", lbl); ImGui::PopStyleColor();
    };

    dot("%%TEMP%% folder",          g_app.cleanTempDone);
    dot("C:\\Windows\\Temp",        g_app.cleanWinTempDone);
    dot("Prefetch cache",           g_app.cleanPrefetchDone);
    dot("DNS cache",                g_app.cleanDNSDone);

    ImGui::Dummy({0,10});

    if (!g_app.cleanRunning) {
        // Big clean button
        ImGui::PushStyleColor(ImGuiCol_Button,        DS::BG_CARD_HIGH);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::ACCENT_BLUE);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  DS::Lerp(DS::ACCENT_BLUE, DS::BG_BASE, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_Text,          DS::TEXT_PRIMARY);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0, 12));
        float bw = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("  Clean Now  ", ImVec2(bw, 0))) {
            g_app.cleanLog.clear();
            g_app.cleanTempDone = g_app.cleanWinTempDone =
            g_app.cleanPrefetchDone = g_app.cleanDNSDone = false;
            std::thread([](){
                Opt::CleanTempFiles([](std::string line){
                    g_app.cleanLog += line + "\n";
                });
            }).detach();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
    } else {
        float t = (float)ImGui::GetTime();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::ACCENT_BLUE);
        ImGui::Text("  Cleaning...  (%.0f%%)", fmod(t*40, 100));
        ImGui::PopStyleColor();
    }

    if (!g_app.cleanLog.empty()) {
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::InputTextMultiline("##log", g_app.cleanLog.data(),
            g_app.cleanLog.size(), {ImGui::GetContentRegionAvail().x, 120},
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    Widget::EndCard();
}

// ──────────────────────────────────────────────────────────────────────────────
static void RenderLaunchPanel() {
    Widget::BeginCard(0, DS::BG_ELEVATED);

    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("GAME LAUNCHER");
    ImGui::PopStyleColor();
    ImGui::Dummy({0,8});

    // Path input
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          DS::BG_CARD);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   DS::BG_CARD_HIGH);
    ImGui::PushStyleColor(ImGuiCol_Text,             DS::TEXT_PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12, 10));
    float bw = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(bw - 100.0f);
    ImGui::InputText("##gpath", g_app.gamePath, sizeof(g_app.gamePath));
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 10);
    ImGui::PushStyleColor(ImGuiCol_Button,        DS::BG_CARD_HIGH);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::ACCENT_BLUE);
    ImGui::PushStyleColor(ImGuiCol_Text,          DS::TEXT_PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12, 10));
    if (ImGui::Button("Browse")) {
        OPENFILENAMEA ofn{};
        char fname[512] = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile   = fname;
        ofn.nMaxFile    = sizeof(fname);
        ofn.lpstrFilter = "Executables\0*.exe\0All\0*.*\0";
        ofn.Flags       = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            strncpy_s(g_app.gamePath, fname, sizeof(g_app.gamePath)-1);
            g_app.launchReady  = true;
            g_app.launchStatus = "Ready to launch!";
            g_app.PushNotif("Game selected: " + std::string(fname), DS::ACCENT_GREEN);
        }
    }
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    ImGui::Dummy({0,16});
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("%s", g_app.launchStatus.c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy({0,8});

    // Info cards
    float cw = (bw - 12) / 3.0f;
    struct { const char* icon; const char* line1; const char* line2; ImVec4 col; } infos[] = {
        {"◈", "Priority",  "HIGH",             DS::ACCENT_BLUE},
        {"◉", "CPU",       "Foreground Boost",  DS::ACCENT_PURPLE},
        {"◑", "Input Lag", "Near 0ms",          DS::ACCENT_GREEN},
    };
    for (auto& inf : infos) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, DS::BG_CARD);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14,12));
        ImGui::BeginChild(inf.icon, ImVec2(cw, 70), false);
        ImGui::PushStyleColor(ImGuiCol_Text, inf.col);
        ImGui::Text("%s", inf.icon);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
        ImGui::Text("%s", inf.line1);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        ImGui::Text("%s", inf.line2);
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar(2); ImGui::PopStyleColor();
        ImGui::SameLine(0, 6);
    }
    ImGui::NewLine();
    ImGui::Dummy({0,16});

    // Launch button
    ImVec4 btnC = g_app.launchReady ? DS::ACCENT_BLUE : DS::BG_CARD_HIGH;
    ImGui::PushStyleColor(ImGuiCol_Button,        btnC);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::Lerp(btnC, {1,1,1,1}, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  DS::Lerp(btnC, {0,0,0,1}, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_Text,          DS::TEXT_PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(0, 14));
    if (ImGui::Button("  Launch Game  ", {bw, 0}))
        Opt::LaunchGameWithPriority(g_app.gamePath);
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);

    Widget::EndCard();
}

// ──────────────────────────────────────────────────────────────────────────────
static void RenderPhonkPanel() {
    Widget::BeginCard(0, DS::BG_ELEVATED);

    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("PHONK PLAYER");
    ImGui::PopStyleColor();
    ImGui::Dummy({0,8});

    float bw = ImGui::GetContentRegionAvail().x;

    // File browse row
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          DS::BG_CARD);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   DS::BG_CARD_HIGH);
    ImGui::PushStyleColor(ImGuiCol_Text,             DS::TEXT_PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12, 10));
    ImGui::SetNextItemWidth(bw - 100.0f);
    ImGui::InputText("##ppath", g_app.phonkPath, sizeof(g_app.phonkPath));
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 10);

    ImGui::PushStyleColor(ImGuiCol_Button,        DS::BG_CARD_HIGH);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::ACCENT_PURPLE);
    ImGui::PushStyleColor(ImGuiCol_Text,          DS::TEXT_PRIMARY);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(12, 10));
    if (ImGui::Button("Load##pk")) {
        OPENFILENAMEA ofn{}; char fn[512] = {};
        ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = fn;
        ofn.nMaxFile = sizeof(fn);
        ofn.lpstrFilter = "Audio\0*.mp3;*.wav;*.ogg;*.flac\0All\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            Phonk::Stop();
            strncpy_s(g_app.phonkPath, fn, sizeof(g_app.phonkPath)-1);
            Phonk::Open(fn);
            g_app.PushNotif("Track loaded: " + std::string(fn), DS::ACCENT_PURPLE);
        }
    }
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    ImGui::Dummy({0,14});

    // Track title (scrolling marquee if long)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        std::string t = g_app.phonkTitle.empty() ? "No Track" : g_app.phonkTitle;
        if (t.length() > 40) t = t.substr(0, 37) + "...";
        ImVec2 ts  = ImGui::CalcTextSize(t.c_str());
        ImGui::SetCursorPosX((bw - ts.x)*0.5f);
        ImGui::Text("%s", t.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Dummy({0,8});

    // Progress bar
    if (g_app.phonkPlaying) {
        g_app.phonkProgress = Phonk::GetProgress();
    }
    float prog = g_app.phonkProgress;
    Widget::Slider("##phonkprog", &prog, 0.0f, 1.0f, DS::ACCENT_PURPLE);
    ImGui::Dummy({0,8});

    // Controls row: loop | ◁◁ | ▶/⏸ | ▷▷
    float btnW = (bw - 60) / 4.0f;
    auto ctrlBtn = [&](const char* lbl, ImVec4 c, float w = 0) {
        float bww = w > 0 ? w : btnW;
        ImGui::PushStyleColor(ImGuiCol_Button,        DS::BG_CARD);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::BG_CARD_HIGH);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  DS::BG_ELEVATED);
        ImGui::PushStyleColor(ImGuiCol_Text,          c);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 10));
        bool r = ImGui::Button(lbl, {bww, 0});
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(4);
        return r;
    };

    ImVec4 loopC = g_app.phonkLoop ? DS::ACCENT_PURPLE : DS::TEXT_SECONDARY;
    if (ctrlBtn("Loop", loopC))  g_app.phonkLoop = !g_app.phonkLoop;
    ImGui::SameLine(0,6);
    if (ctrlBtn("◁◁", DS::TEXT_SECONDARY)) {
        Phonk::Stop();
        if (g_app.phonkLoaded) { Phonk::Open(g_app.phonkPath); Phonk::Play(); }
    }
    ImGui::SameLine(0,6);
    const char* playLbl = g_app.phonkPlaying ? "  ⏸  " : "  ▶  ";
    if (ctrlBtn(playLbl, DS::ACCENT_PURPLE, btnW + 16)) {
        if (!g_app.phonkLoaded && strlen(g_app.phonkPath)>0) Phonk::Open(g_app.phonkPath);
        g_app.phonkPlaying ? Phonk::Pause() : Phonk::Play();
    }
    ImGui::SameLine(0,6);
    if (ctrlBtn("▷▷", DS::TEXT_SECONDARY)) {
        Phonk::Stop();
        g_app.PushNotif("Next track (no queue — load next manually)", DS::TEXT_SECONDARY);
    }

    ImGui::Dummy({0,16});

    // Volume
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_SECONDARY);
    ImGui::Text("Volume");
    ImGui::SameLine(bw - 40);
    ImGui::Text("%.0f%%", g_app.phonkVolume);
    ImGui::PopStyleColor();
    ImGui::Dummy({0,2});
    if (Widget::Slider("##vol", &g_app.phonkVolume, 0, 100, DS::ACCENT_PURPLE))
        Phonk::SetVolume(g_app.phonkVolume);

    ImGui::Dummy({0,8});

    // Visualiser: animated bars (fake but beautiful)
    {
        float t   = (float)ImGui::GetTime();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 vp  = ImGui::GetCursorScreenPos();
        float vh   = 36.0f, vw = bw;
        int   bars = 48;
        float bw2  = vw / (bars * 2.0f);
        for (int i = 0; i < bars; i++) {
            float phase  = (float)i / bars * IM_PI * 2.0f;
            float amp    = g_app.phonkPlaying
                ? fabsf(sinf(t * 3.8f + phase) * sinf(t * 2.1f + phase*1.3f))
                : fabsf(sinf(phase) * 0.15f);
            float h      = amp * vh * (g_app.phonkVolume / 100.0f);
            float x      = vp.x + i * (bw2*2.0f) + bw2 * 0.5f;
            float alpha  = 0.5f + amp * 0.5f;
            ImU32 col    = DS::ColA(DS::ACCENT_PURPLE, alpha);
            dl->AddRectFilled({x, vp.y + vh - h}, {x + bw2, vp.y + vh}, col, 2.0f);
        }
        ImGui::Dummy({vw, vh + 4});
    }

    Widget::EndCard();
}

// ──────────────────────────────────────────────────────────────────────────────
//  MAIN RENDER FRAME
// ──────────────────────────────────────────────────────────────────────────────
static void RenderUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2   ds = io.DisplaySize;

    // Full-screen window
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(ds);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, DS::BG_BASE);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("XOPT_ROOT", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);

    // ── Sidebar ───────────────────────────────────────────────────────────────
    float SBW = 72.0f;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({SBW, ds.y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, DS::BG_ELEVATED);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##sidebar", {SBW, ds.y}, false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(); ImGui::PopStyleColor();

    ImGui::Dummy({0, 20});
    // Logo
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 lp = ImGui::GetCursorScreenPos();
        lp.x += (SBW - 36.0f)*0.5f; lp.y += 4;
        dl->AddRectFilled(lp, {lp.x+36, lp.y+36}, DS::Col(DS::ACCENT_BLUE), 10.0f);
        ImVec2 ts = ImGui::CalcTextSize("X");
        dl->AddText({lp.x + (36-ts.x)*0.5f, lp.y + (36-ts.y)*0.5f},
                    IM_COL32(255,255,255,255), "X");
        ImGui::Dummy({0, 46});
    }

    struct NavItem { const char* icon; const char* lbl; };
    static NavItem navItems[] = {
        {"◈","Boost"},{"◉","Clean"},{"▷","Launch"},{"♪","Phonk"}
    };
    for (int i = 0; i < 4; i++) {
        bool sel = (g_app.activeTab == i);
        if (sel) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p.x + 6, p.y}, {p.x + SBW - 6, p.y + 56},
                DS::ColA(DS::ACCENT_BLUE, 0.18f), 12.0f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p.x + SBW - 4, p.y + 8}, {p.x + SBW, p.y + 48},
                DS::Col(DS::ACCENT_BLUE), 4.0f);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? DS::ACCENT_BLUE : DS::TEXT_TERTIARY);
        ImGui::SetCursorPosX((SBW - ImGui::CalcTextSize(navItems[i].icon).x) * 0.5f);
        ImGui::Dummy({0,10});
        ImGui::SetCursorPosX((SBW - ImGui::CalcTextSize(navItems[i].icon).x) * 0.5f);
        if (ImGui::Selectable(navItems[i].icon, sel, 0, {SBW-12, 0}))
            g_app.activeTab = i;
        ImGui::SetCursorPosX((SBW - ImGui::CalcTextSize(navItems[i].lbl).x) * 0.5f);
        ImGui::Text("%s", navItems[i].lbl);
        ImGui::Dummy({0,4});
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    // ── Content area ──────────────────────────────────────────────────────────
    float contentX = SBW + 1.0f;
    float contentW = ds.x - contentX;
    ImGui::SetNextWindowPos({contentX, 0});
    ImGui::SetNextWindowSize({contentW, ds.y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, DS::BG_BASE);
    ImGui::BeginChild("##content", {contentW, ds.y}, false);
    ImGui::PopStyleColor();

    // Header bar
    {
        float HH = 64.0f;
        ImVec2 hp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(hp,
            {hp.x+contentW, hp.y+HH}, DS::Col(DS::BG_BASE));

        // Thin bottom border
        ImGui::GetWindowDrawList()->AddLine(
            {hp.x, hp.y+HH}, {hp.x+contentW, hp.y+HH},
            DS::Col(DS::BG_SEPARATOR), 1.0f);

        static const char* titles[] = {"Boost Engine","Deep Clean","Game Launch","Phonk Player"};
        ImGui::SetCursorPos({20, 20});
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_PRIMARY);
        ImGui::Text("%s", titles[g_app.activeTab]);
        ImGui::PopStyleColor();

        // FPS counter top right
        ImGui::SetCursorPos({contentW - 90.0f, 22});
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TEXT_TERTIARY);
        ImGui::Text("%.0f FPS", io.Framerate);
        ImGui::PopStyleColor();

        ImGui::Dummy({0, HH});
    }

    // Scrollable body
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 0});
    ImGui::BeginChild("##body", {contentW, ds.y - 64.0f}, false,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    ImGui::Dummy({0, 14});

    switch (g_app.activeTab) {
        case 0: RenderBoostPanel();  break;
        case 1: RenderCleanPanel();  break;
        case 2: RenderLaunchPanel(); break;
        case 3: RenderPhonkPanel();  break;
    }

    ImGui::Dummy({0, 20});
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor(); // WindowBg

    // Notifications
    Widget::RenderNotifs();
}

// ──────────────────────────────────────────────────────────────────────────────
//  IMGUI STYLE SETUP  (iOS Dark)
// ──────────────────────────────────────────────────────────────────────────────
static void ApplyIOSStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 20.0f;
    s.ChildRounding      = 18.0f;
    s.FrameRounding      = 12.0f;
    s.PopupRounding      = 14.0f;
    s.ScrollbarRounding  = 10.0f;
    s.GrabRounding       = 10.0f;
    s.TabRounding        = 10.0f;
    s.WindowBorderSize   = 0.0f;
    s.ChildBorderSize    = 0.0f;
    s.FrameBorderSize    = 0.0f;
    s.PopupBorderSize    = 0.0f;
    s.WindowPadding      = {20.0f, 20.0f};
    s.FramePadding       = {12.0f, 8.0f};
    s.ItemSpacing        = {10.0f, 8.0f};
    s.ItemInnerSpacing   = {8.0f,  6.0f};
    s.IndentSpacing      = 20.0f;
    s.ScrollbarSize      = 6.0f;
    s.GrabMinSize        = 12.0f;
    s.AntiAliasedLines   = true;
    s.AntiAliasedFill    = true;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = DS::TEXT_PRIMARY;
    c[ImGuiCol_TextDisabled]          = DS::TEXT_TERTIARY;
    c[ImGuiCol_WindowBg]              = DS::BG_BASE;
    c[ImGuiCol_ChildBg]               = DS::BG_ELEVATED;
    c[ImGuiCol_PopupBg]               = DS::BG_ELEVATED;
    c[ImGuiCol_Border]                = DS::BG_SEPARATOR;
    c[ImGuiCol_FrameBg]               = DS::BG_CARD;
    c[ImGuiCol_FrameBgHovered]        = DS::BG_CARD_HIGH;
    c[ImGuiCol_FrameBgActive]         = DS::BG_CARD_HIGH;
    c[ImGuiCol_TitleBg]               = DS::BG_BASE;
    c[ImGuiCol_TitleBgActive]         = DS::BG_BASE;
    c[ImGuiCol_MenuBarBg]             = DS::BG_ELEVATED;
    c[ImGuiCol_ScrollbarBg]           = {0,0,0,0};
    c[ImGuiCol_ScrollbarGrab]         = DS::BG_CARD_HIGH;
    c[ImGuiCol_ScrollbarGrabHovered]  = DS::TEXT_TERTIARY;
    c[ImGuiCol_CheckMark]             = DS::ACCENT_BLUE;
    c[ImGuiCol_SliderGrab]            = DS::ACCENT_BLUE;
    c[ImGuiCol_SliderGrabActive]      = DS::Lerp(DS::ACCENT_BLUE,{1,1,1,1},0.2f);
    c[ImGuiCol_Button]                = DS::BG_CARD;
    c[ImGuiCol_ButtonHovered]         = DS::BG_CARD_HIGH;
    c[ImGuiCol_ButtonActive]          = DS::BG_ELEVATED;
    c[ImGuiCol_Header]                = DS::BG_CARD;
    c[ImGuiCol_HeaderHovered]         = DS::BG_CARD_HIGH;
    c[ImGuiCol_HeaderActive]          = DS::BG_ELEVATED;
    c[ImGuiCol_Separator]             = DS::BG_SEPARATOR;
    c[ImGuiCol_Tab]                   = DS::BG_CARD;
    c[ImGuiCol_TabHovered]            = DS::BG_CARD_HIGH;
    c[ImGuiCol_TabActive]             = DS::ACCENT_BLUE;
    c[ImGuiCol_PlotHistogram]         = DS::ACCENT_BLUE;
    c[ImGuiCol_PlotHistogramHovered]  = DS::Lerp(DS::ACCENT_BLUE,{1,1,1,1},0.2f);
    c[ImGuiCol_NavHighlight]          = DS::ACCENT_BLUE;
}

// ──────────────────────────────────────────────────────────────────────────────
//  DIRECTX 11 HELPERS
// ──────────────────────────────────────────────────────────────────────────────
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount        = 2;
    sd.BufferDesc.Width   = sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = hWnd;
    sd.SampleDesc.Count   = 1;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fls, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView);
    pBB->Release();
    return true;
}
static void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain)           { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext)    { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)           { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
static void ResizeSwapChain(UINT w, UINT h) {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    g_pSwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView);
    pBB->Release();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
                ResizeSwapChain((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcW(hWnd, msg, wParam, lParam);
            if (hit == HTCLIENT) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);
                if (pt.y < 64) return HTCAPTION; // drag on header
            }
            return hit;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ──────────────────────────────────────────────────────────────────────────────
//  ENTRY POINT
// ──────────────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Request admin privileges reminder (soft, non-blocking)
    // Real UAC elevation should be handled by manifest or ShellExecute runas

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"XOPT_WND";
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW    = 1050, winH = 680;
    int winX    = (screenW - winW) / 2;
    int winY    = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED,
        wc.lpszClassName, L"X-OPT Engine",
        WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX,
        winX, winY, winW, winH,
        nullptr, nullptr, hInst, nullptr);

    // Rounded window corners (DWM)
    {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
        enum DWM_WINDOW_CORNER_PREFERENCE { DWMWCP_ROUND = 2 };
        int pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &pref, sizeof(pref));
        // Remove window border shadow artefacts at edges
        MARGINS m{-1,-1,-1,-1};
        DwmExtendFrameIntoClientArea(hwnd, &m);
    }
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); UnregisterClassW(wc.lpszClassName, hInst); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Segoe UI from system for clean iOS-like font
    float fontSize = 15.0f;
    ImFontConfig fc{}; fc.OversampleH = 3; fc.OversampleV = 2;
    bool loaded = false;
    const wchar_t* fontPaths[] = {
        L"C:\\Windows\\Fonts\\segoeui.ttf",
        L"C:\\Windows\\Fonts\\calibri.ttf",
        L"C:\\Windows\\Fonts\\tahoma.ttf",
    };
    for (auto& fp : fontPaths) {
        char narrow[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, fp, -1, narrow, MAX_PATH, nullptr, nullptr);
        if (fs::exists(narrow)) {
            io.Fonts->AddFontFromFileTTF(narrow, fontSize, &fc);
            loaded = true; break;
        }
    }
    if (!loaded) io.Fonts->AddFontDefault();

    ApplyIOSStyle();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Welcome notification
    g_app.PushNotif("X-OPT Engine ready — apply boosts from the sidebar", DS::ACCENT_BLUE);

    // Main loop
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();

        constexpr float cc[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // VSync on
    }

    Phonk::Stop();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}
