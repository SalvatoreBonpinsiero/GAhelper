#include <windows.h>
#include <d3d9.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "kiero.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__stdcall* EndSceneFn)(LPDIRECT3DDEVICE9);
typedef HRESULT(__stdcall* ResetFn)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
typedef void(__cdecl* CMDPROC)(const char*);

static EndSceneFn oEndScene = nullptr;
static ResetFn oReset = nullptr;
static WNDPROC oWndProc = nullptr;
static bool g_D3DInitialized = false;

struct ToastNotification {
    std::string text;
    float timer;
};

struct ChatInspectorEntry {
    std::string sender;
    std::string message;
    bool isOOCViolation;
};

static bool g_MenuOpen = false;
static int g_SelectedTab = 1;

static bool g_AutoReport = true;
static int g_ReportDelayMs = 1;
static bool g_ShowNotifications = true;

static char g_SpecId[32] = "";
static char g_SearchFilter[64] = "";

static char g_BanId[32] = "";
static int g_BanDays = 30;
static int g_BanReason = 1;
static int g_BanSubReason = 1;
static char g_BanAdmin[64] = "";

static char g_JailId[32] = "";
static int g_JailMin = 60;
static int g_JailReason = 1;
static int g_JailSubReason = 1;
static char g_JailAdmin[64] = "";

static char g_MuteId[32] = "";
static int g_MuteMin = 30;
static int g_MuteReason = 1;
static int g_MuteSubReason = 1;
static char g_MuteAdmin[64] = "";

static std::vector<ToastNotification> g_Toasts;
static std::vector<ChatInspectorEntry> g_InspectorLogs;

uintptr_t GetSampBase() {
    static uintptr_t sampBase = 0;
    if (!sampBase) {
        sampBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("samp.dll"));
    }
    return sampBase;
}

void SendSampChat(const std::string& text) {
    uintptr_t base = GetSampBase();
    if (!base) return;

    typedef void(__thiscall* SendChatFn)(void*, const char*);
    uintptr_t chatInput = *reinterpret_cast<uintptr_t*>(base + 0x21A0E8);
    if (!chatInput) return;

    SendChatFn sendChat = reinterpret_cast<SendChatFn>(base + 0x65C60);
    sendChat(reinterpret_cast<void*>(chatInput), text.c_str());
}

void RegisterSampCommand(const char* szCmdName, CMDPROC pFunc) {
    uintptr_t base = GetSampBase();
    if (!base) return;

    typedef void(__thiscall* AddCmdFn)(void*, const char*, CMDPROC);
    uintptr_t chatInput = *reinterpret_cast<uintptr_t*>(base + 0x21A0E8);
    if (!chatInput) return;

    AddCmdFn addCommand = reinterpret_cast<AddCmdFn>(base + 0x65AD0);
    addCommand(reinterpret_cast<void*>(chatInput), szCmdName, pFunc);
}

void AddNotification(const std::string& text) {
    if (!g_ShowNotifications) return;
    g_Toasts.push_back({ text, 3.5f });
}

void FastAcceptReport(bool isAuto) {
    CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        bool autoCatch = *reinterpret_cast<bool*>(param);
        delete reinterpret_cast<bool*>(param);

        SendSampChat("/reports");
        Sleep(450);

        for (int i = 0; i < 3; ++i) {
            keybd_event(VK_RETURN, 0, 0, 0);
            Sleep(10);
            keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            if (i < 2) Sleep(static_cast<DWORD>(g_ReportDelayMs));
        }

        if (autoCatch) {
            g_AutoReport = false;
            AddNotification("Репорт взят! Авто-ловля отключена (Вкл: K)");
        } else {
            AddNotification("Ручная ловля: /reports -> 3x Enter");
        }
        return 0;
    }, new bool(isAuto), 0, nullptr);
}

void FreezeSceneAction() {
    SendSampChat("/b [HelperBotAdmin] Ситуация заморожена до выяснения обстоятельств. Не двигайтесь и не выходите из игры.");
    AddNotification("Ситуация заморожена [HelperBotAdmin]");
}

void __cdecl CmdFreezeScene(const char*) {
    FreezeSceneAction();
}

void __cdecl CmdOpenForum(const char*) {
    ShellExecuteA(nullptr, "open", "https://forum.gambit-rp.ru", nullptr, nullptr, SW_SHOWNORMAL);
    AddNotification("Открываем форум Gambit RP...");
}

void ApplyPureGrayTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.48f, 0.48f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.48f, 0.48f, 0.48f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding      = 3.0f;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == 'Z' && !g_MenuOpen) {
            FastAcceptReport(false);
        }
        if (wParam == 'K') {
            g_AutoReport = !g_AutoReport;
            AddNotification(g_AutoReport ? "Авто-ловля ВКЛЮЧЕНА" : "Авто-ловля ВЫКЛЮЧЕНА");
        }
        if (wParam == VK_F11) {
            g_MenuOpen = !g_MenuOpen;
        }
    }

    if (g_MenuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice) {
    if (!g_D3DInitialized) {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);

        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(params.hFocusWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImFontConfig fontCfg;
        fontCfg.OversampleH = 1;
        fontCfg.OversampleV = 1;
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 15.0f, &fontCfg, io.Fonts->GetGlyphRangesCyrillic());

        ApplyPureGrayTheme();

        ImGui_ImplWin32_Init(params.hFocusWindow);
        ImGui_ImplDX9_Init(pDevice);

        RegisterSampCommand("sfreeze", CmdFreezeScene);
        RegisterSampCommand("pause", CmdFreezeScene);
        RegisterSampCommand("forum", CmdOpenForum);

        g_InspectorLogs.push_back({ "Leo_Galante", "Здорово, парни. Всё готово?", false });
        g_InspectorLogs.push_back({ "Don_Salieri", "Да, заходи в дс быстрее))", true });

        g_D3DInitialized = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    float dt = ImGui::GetIO().DeltaTime;
    for (size_t i = 0; i < g_Toasts.size();) {
        g_Toasts[i].timer -= dt;
        if (g_Toasts[i].timer <= 0.0f) {
            g_Toasts.erase(g_Toasts.begin() + i);
        } else {
            ++i;
        }
    }

    if (!g_Toasts.empty()) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float yPos = vp->WorkSize.y - 80.0f;
        for (const auto& toast : g_Toasts) {
            ImGui::SetNextWindowPos(ImVec2(vp->WorkSize.x - 330.0f, yPos), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(310.0f, 50.0f));
            ImGui::Begin(("##Toast" + toast.text).c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
            ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.70f, 1.0f), "[HelperBotAdmin]");
            ImGui::TextUnformatted(toast.text.c_str());
            ImGui::End();
            yPos -= 58.0f;
        }
    }

    if (g_MenuOpen) {
        ImGui::SetNextWindowSize(ImVec2(780.0f, 470.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("GAhelper | Pure Gray Edition", &g_MenuOpen, ImGuiWindowFlags_NoCollapse);

        ImGui::BeginChild("Sidebar", ImVec2(170.0f, 0.0f), true);
        if (ImGui::Selectable("Слежка", g_SelectedTab == 1)) g_SelectedTab = 1;
        if (ImGui::Selectable("Игроки", g_SelectedTab == 2)) g_SelectedTab = 2;
        if (ImGui::Selectable("Инспектор (RP/MG)", g_SelectedTab == 7)) g_SelectedTab = 7;
        if (ImGui::Selectable("Бан", g_SelectedTab == 3)) g_SelectedTab = 3;
        if (ImGui::Selectable("Деморган", g_SelectedTab == 4)) g_SelectedTab = 4;
        if (ImGui::Selectable("Мут", g_SelectedTab == 5)) g_SelectedTab = 5;
        ImGui::Separator();
        if (ImGui::Selectable("Настройки", g_SelectedTab == 6)) g_SelectedTab = 6;
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("WorkArea", ImVec2(0.0f, 0.0f), true);

        if (g_SelectedTab == 1) {
            ImGui::Text("Режим наблюдения (/re) и контроль ситуации");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputText("ID Игрока", g_SpecId, IM_ARRAYSIZE(g_SpecId));
            if (ImGui::Button("Начать слежку", ImVec2(150.0f, 32.0f))) {
                SendSampChat("/re " + std::string(g_SpecId));
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Быстрая заморозка ситуации (/sfreeze /pause):");
            if (ImGui::Button("Заморозить ситуацию [HelperBotAdmin]", ImVec2(320.0f, 36.0f))) {
                FreezeSceneAction();
            }

        } else if (g_SelectedTab == 7) {
            ImGui::Text("Чат-инспектор и детектор ООС нарушений");
            ImGui::Separator();
            ImGui::TextDisabled("Фиксирует метагейминг, скобки в IC и внешние каналы связи.");
            ImGui::Spacing();

            ImGui::BeginChild("LogViewer", ImVec2(0.0f, 0.0f), true);
            for (const auto& log : g_InspectorLogs) {
                if (log.isOOCViolation) {
                    ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.0f), "[OOC В IC!] ");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "%s: %s", log.sender.c_str(), log.message.c_str());
                } else {
                    ImGui::Text("%s: %s", log.sender.c_str(), log.message.c_str());
                }
            }
            ImGui::EndChild();

        } else if (g_SelectedTab == 2) {
            ImGui::Text("Список игроков (Анти-краш режим)");
            ImGui::Separator();
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputText("Поиск", g_SearchFilter, IM_ARRAYSIZE(g_SearchFilter));
            ImGui::BeginChild("PlayerRows", ImVec2(0.0f, 0.0f), true);
            for (int i = 0; i < 30; ++i) {
                ImGui::Text("[%d] Player_%d", i, i);
                ImGui::SameLine(180.0f);
                if (ImGui::Button(("Бан##" + std::to_string(i)).c_str())) {
                    strcpy_s(g_BanId, std::to_string(i).c_str());
                    g_SelectedTab = 3;
                }
                ImGui::SameLine();
                if (ImGui::Button(("Джаил##" + std::to_string(i)).c_str())) {
                    strcpy_s(g_JailId, std::to_string(i).c_str());
                    g_SelectedTab = 4;
                }
                ImGui::SameLine();
                if (ImGui::Button(("Мут##" + std::to_string(i)).c_str())) {
                    strcpy_s(g_MuteId, std::to_string(i).c_str());
                    g_SelectedTab = 5;
                }
            }
            ImGui::EndChild();

        } else if (g_SelectedTab == 3) {
            ImGui::Text("Блокировка аккаунта");
            ImGui::Separator();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputText("ID", g_BanId, IM_ARRAYSIZE(g_BanId));
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Дней", &g_BanDays, 1, 999);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Пункт", &g_BanReason, 1, 44);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Подпункт", &g_BanSubReason, 1, 10);
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputText("Тег / ID админа", g_BanAdmin, IM_ARRAYSIZE(g_BanAdmin));
            if (ImGui::Button("Выдать блокировку", ImVec2(180.0f, 34.0f))) {
                std::string cmd = "/ban " + std::string(g_BanId) + " " + std::to_string(g_BanDays) + " Пункт " +
                                  std::to_string(g_BanReason) + "." + std::to_string(g_BanSubReason) + " // " + std::string(g_BanAdmin);
                SendSampChat(cmd);
            }

        } else if (g_SelectedTab == 4) {
            ImGui::Text("Посадка в деморган");
            ImGui::Separator();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputText("ID", g_JailId, IM_ARRAYSIZE(g_JailId));
            ImGui::SetNextItemWidth(240.0f);
            ImGui::SliderInt("Минут", &g_JailMin, 1, 3000);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Пункт", &g_JailReason, 1, 44);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Подпункт", &g_JailSubReason, 1, 10);
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputText("Тег / ID админа", g_JailAdmin, IM_ARRAYSIZE(g_JailAdmin));
            if (ImGui::Button("Посадить в деморган", ImVec2(180.0f, 34.0f))) {
                std::string cmd = "/jail " + std::string(g_JailId) + " " + std::to_string(g_JailMin) + " Пункт " +
                                  std::to_string(g_JailReason) + "." + std::to_string(g_JailSubReason) + " // " + std::string(g_JailAdmin);
                SendSampChat(cmd);
            }

        } else if (g_SelectedTab == 5) {
            ImGui::Text("Блокировка чата");
            ImGui::Separator();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputText("ID", g_MuteId, IM_ARRAYSIZE(g_MuteId));
            ImGui::SetNextItemWidth(240.0f);
            ImGui::SliderInt("Минут", &g_MuteMin, 1, 600);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Пункт", &g_MuteReason, 1, 44);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Подпункт", &g_MuteSubReason, 1, 10);
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputText("Тег / ID админа", g_MuteAdmin, IM_ARRAYSIZE(g_MuteAdmin));
            if (ImGui::Button("Выдать мут", ImVec2(180.0f, 34.0f))) {
                std::string cmd = "/mute " + std::string(g_MuteId) + " " + std::to_string(g_MuteMin) + " Пункт " +
                                  std::to_string(g_MuteReason) + "." + std::to_string(g_MuteSubReason) + " // " + std::string(g_MuteAdmin);
                SendSampChat(cmd);
            }

        } else if (g_SelectedTab == 6) {
            ImGui::Text("Системные параметры");
            ImGui::Separator();
            ImGui::Checkbox("Авто-ловля репортов (Клавиша: K)", &g_AutoReport);
            ImGui::Checkbox("Всплывающие уведомления", &g_ShowNotifications);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::SliderInt("Задержка Enter (мс)", &g_ReportDelayMs, 1, 50);
            ImGui::Spacing();
            if (ImGui::Button("Открыть форум Gambit RP (/forum)", ImVec2(250.0f, 32.0f))) {
                CmdOpenForum(nullptr);
            }
        }

        ImGui::EndChild();
        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return oEndScene(pDevice);
}

HRESULT __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = oReset(pDevice, pPresentationParameters);
    ImGui_ImplDX9_CreateDeviceObjects();
    return hr;
}

DWORD WINAPI MainInitThread(LPVOID) {
    while (GetModuleHandleA("samp.dll") == nullptr || GetModuleHandleA("d3d9.dll") == nullptr) {
        Sleep(100);
    }

    if (kiero::init(kiero::RenderType::D3D9) == kiero::Status::Success) {
        kiero::bind(42, reinterpret_cast<void**>(&oEndScene), reinterpret_cast<void*>(hkEndScene));
        kiero::bind(16, reinterpret_cast<void**>(&oReset), reinterpret_cast<void*>(hkReset));
    }
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainInitThread), nullptr, 0, nullptr);
    } else if (dwReason == DLL_PROCESS_DETACH) {
        kiero::shutdown();
    }
    return TRUE;
}
