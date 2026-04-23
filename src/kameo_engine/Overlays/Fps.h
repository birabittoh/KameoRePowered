#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <rex/ui/imgui_dialog.h>
#include "imgui.h"

// Lightweight FPS overlay for rekameo. Each FPSCounter represents a
// single timeline (e.g. CPU tick, GPU draw) and averages its last N
// frame intervals. FPSManager owns counters and draws the overlay.

class FPSCounter {
public:
    std::string name;
    int AverageCount = 100;
    std::vector<float> frameTimes;
    float averageFps = 0.0f;
    float averageMs  = 0.0f;
    std::chrono::steady_clock::time_point lastTick =
        std::chrono::steady_clock::now();

    void Tick();
};

class FPSManager {
public:
    std::vector<std::unique_ptr<FPSCounter>> counters;
    bool showFPS = false;

    FPSCounter* GetCreateCounter(const std::string& name) {
        for (auto& counter : counters) {
            if (counter->name == name) {
                return counter.get();
            }
        }
        auto newCounter = std::make_unique<FPSCounter>();
        newCounter->name = name;
        FPSCounter* ptr = newCounter.get();
        counters.push_back(std::move(newCounter));
        return ptr;
    }
};

class FpsOverlayDialog : public rex::ui::ImGuiDialog {
public:
    explicit FpsOverlayDialog(rex::ui::ImGuiDrawer* drawer)
        : rex::ui::ImGuiDialog(drawer) {}

    FPSManager* fpsManager = nullptr;

    void OnDraw(ImGuiIO& io) override {
        if (!fpsManager || !fpsManager->showFPS) return;
        ImGui::Begin("FPS Overlay", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        for (auto& counter : fpsManager->counters) {
            ImGui::Text("%s: %.1f FPS (%.2f ms)",
                counter->name.c_str(),
                counter->averageFps,
                counter->averageMs);
        }
        ImGui::End();
    }
};
