#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

#include "AnAn/Call_Me.h"
#include "Headers.h"
#include "Global.h"
#include "Logo.h"

// 0:DWORD, 1:FLOAT, 2:DOUBLE, 3:QWORD, 4:WORD, 5:BYTE
static int scanType = 0;
static char searchBuf[64] = "0";
static char replaceBuf[64] = "99999";
static bool freezeMem = false;
static std::vector<uintptr_t> foundAddrs;

bool MatchVal(uintptr_t a) {
    if (scanType == 0) return *(int32_t*)a == std::stoi(searchBuf);
    if (scanType == 1) return fabsf(*(float*)a - std::stof(searchBuf)) < 0.01f;
    if (scanType == 2) return fabs(*(double*)a - std::stod(searchBuf)) < 0.01;
    if (scanType == 3) return *(int64_t*)a == std::stoll(searchBuf);
    if (scanType == 4) return *(int16_t*)a == (int16_t)std::stoi(searchBuf);
    if (scanType == 5) return *(uint8_t*)a == (uint8_t)std::stoi(searchBuf);
    return false;
}

void WriteVal(uintptr_t a) {
    if (scanType == 0) *(int32_t*)a = std::stoi(replaceBuf);
    else if (scanType == 1) *(float*)a = std::stof(replaceBuf);
    else if (scanType == 2) *(double*)a = std::stod(replaceBuf);
    else if (scanType == 3) *(int64_t*)a = std::stoll(replaceBuf);
    else if (scanType == 4) *(int16_t*)a = (int16_t)std::stoi(replaceBuf);
    else if (scanType == 5) *(uint8_t*)a = (uint8_t)std::stoi(replaceBuf);
}

void NewScan() {
    foundAddrs.clear();
    freezeMem = false;
    int step = (scanType == 5) ? 1 : (scanType == 4 ? 2 : 4);
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("rw-p") != std::string::npos && line.find("/") == std::string::npos) {
            uintptr_t s = 0, e = 0;
            if (sscanf(line.c_str(), "%lx-%lx", &s, &e) == 2 && e > s && (e - s) < 150 * 1024 * 1024) {
                for (uintptr_t a = s; a <= e - 8; a += step) {
                    if (MatchVal(a)) {
                        foundAddrs.push_back(a);
                        if (foundAddrs.size() >= 50000) return;
                    }
                }
            }
        }
    }
}

void NextScan() {
    std::vector<uintptr_t> next;
    for (uintptr_t a : foundAddrs) if (MatchVal(a)) next.push_back(a);
    foundAddrs = std::move(next);
}

void ModifyAll() {
    for (uintptr_t a : foundAddrs) WriteVal(a);
}

EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setup) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        DrawImGuiStyle();
        io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Custom), sizeof(Custom), 25.f, NULL, io.Fonts->GetGlyphRangesVietnamese());
        ImGui_ImplOpenGL3_Init(OBFUSCATE("#version 300 es"));
        ImGui::GetStyle().ScaleAllSizes(3.0f);
        setup = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    ImGui::NewFrame();

    if (freezeMem && !foundAddrs.empty()) ModifyAll();

    static bool WantTextInputLast = false;
    if (ImGui::GetIO().WantTextInput && !WantTextInputLast) ShowSoftKeyboardInput();
    WantTextInputLast = ImGui::GetIO().WantTextInput;

    if (ShowMenu) {
        ImGui::SetNextWindowSize(ImVec2(700, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(OBFUSCATE("GG Scanner"), &ShowMenu)) {
            const char* types[] = {"DWORD", "FLOAT", "DOUBLE", "QWORD", "WORD", "BYTE"};
            if (ImGui::Combo(OBFUSCATE("Kieu du lieu"), &scanType, types, 6)) {
                foundAddrs.clear();
                freezeMem = false;
            }

            ImGui::InputText(OBFUSCATE("Gia tri tim"), searchBuf, sizeof(searchBuf));

            if (ImGui::Button(OBFUSCATE("Tim moi"))) NewScan();
            ImGui::SameLine();
            if (ImGui::Button(OBFUSCATE("Loc tiep"))) NextScan();
            ImGui::SameLine();
            if (ImGui::Button(OBFUSCATE("Xoa"))) { foundAddrs.clear(); freezeMem = false; }

            ImGui::Text(OBFUSCATE("Ket qua: %zu"), foundAddrs.size());

            ImGui::InputText(OBFUSCATE("Sua thanh"), replaceBuf, sizeof(replaceBuf));

            if (ImGui::Button(OBFUSCATE("Sua tat ca"))) ModifyAll();
            ImGui::SameLine();
            ImGui::Checkbox(OBFUSCATE("Dong bang"), &freezeMem);
        }
        ImGui::End();
    } else {
        static TextureInfo logoTex = createTexturePNGFromMem(logo, sizeof(logo));
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        ImGui::Begin(OBFUSCATE("##Icon"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        if (ImGui::ImageButton((ImTextureID)logoTex.textureId, ImVec2(70, 70))) ShowMenu = true;
        ImGui::End();
    }

    ImGui::Render();
    ImGui::EndFrame();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return orig_eglSwapBuffers(dpy, surface);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm = vm;
    Tools::Hook(DobbySymbolResolver("/system/lib64/libEGL.so", "eglSwapBuffers"), (void*)_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    return JNI_VERSION_1_6;
}
