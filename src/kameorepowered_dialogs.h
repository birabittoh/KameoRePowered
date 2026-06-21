
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// ImGui dialogs: audio mixer (F9) and model-swap (F11).

#pragma once

#include <filesystem>
#include <set>
#include <string>

#include <imgui.h>
#include <rex/rex_app.h>

#include "kameorepowered_dlc_models.h"
#include "kameorepowered_dlc_swap.h"
#include "kameorepowered_fp_guard.h"
#include "kameorepowered_hooks.h"

// ---------------------------------------------------------------------------
// Audio mixer dialog (F9)
// ---------------------------------------------------------------------------

class KameoAudioDialog : public rex::ui::ImGuiDialog {
 public:
  explicit KameoAudioDialog(rex::ui::ImGuiDrawer* drawer)
      : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& /*io*/) override {
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
      visible_ = !visible_;
    }

    if (!visible_) {
      return;
    }

    ImGui::SetNextWindowPos(ImVec2(300.0f, 120.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Kameo Audio (F9)", &visible_)) {
      ImGui::End();
      return;
    }

    ImGui::TextUnformatted("Volume (0 - 100)");
    ImGui::Separator();

    int music = g_kameo_music_volume.load(std::memory_order_relaxed);
    if (ImGui::SliderInt("Music", &music, 0, 100)) {
      g_kameo_music_volume.store(music, std::memory_order_release);
      g_kameo_volume_dirty.store(1, std::memory_order_release);
    }

    int sfx = g_kameo_sfx_volume.load(std::memory_order_relaxed);
    if (ImGui::SliderInt("SFX", &sfx, 0, 100)) {
      g_kameo_sfx_volume.store(sfx, std::memory_order_release);
      g_kameo_volume_dirty.store(1, std::memory_order_release);
    }

    int ambience = g_kameo_ambience_volume.load(std::memory_order_relaxed);
    if (ImGui::SliderInt("Ambience", &ambience, 0, 100)) {
      g_kameo_ambience_volume.store(ambience, std::memory_order_release);
      g_kameo_volume_dirty.store(1, std::memory_order_release);
    }

    int speech = g_kameo_speech_volume.load(std::memory_order_relaxed);
    if (ImGui::SliderInt("Speech", &speech, 0, 100)) {
      g_kameo_speech_volume.store(speech, std::memory_order_release);
      g_kameo_volume_dirty.store(1, std::memory_order_release);
    }

    int fmv = g_kameo_fmv_volume.load(std::memory_order_relaxed);
    if (ImGui::SliderInt("FMV", &fmv, 0, 100)) {
      g_kameo_fmv_volume.store(fmv, std::memory_order_release);
      g_kameo_volume_dirty.store(1, std::memory_order_release);
    }

    ImGui::End();
  }

 private:
  bool visible_ = false;
};

// ---------------------------------------------------------------------------
// Model-swap dialog (F11)
// ---------------------------------------------------------------------------

class KameoModelDialog : public rex::ui::ImGuiDialog {
 public:
  explicit KameoModelDialog(rex::ui::ImGuiDrawer* drawer)
      : rex::ui::ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& /*io*/) override {
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
      visible_ = !visible_;
    }

    if (!visible_) {
      return;
    }

    ImGui::SetNextWindowPos(ImVec2(20.0f, 120.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Kameo Model Swap (F11)", &visible_)) {
      ImGui::End();
      return;
    }

    bool enabled = g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) != 0;
    if (ImGui::Checkbox("Enable Hotswap", &enabled)) {
      g_kameo_dlc_swap_enabled.store(enabled ? 1 : 0, std::memory_order_release);
      if (!enabled) {
        ClearKameoDlcSuffix();
        last_result_ = "Kameo hotswap disabled";
      } else {
        last_result_ = "Kameo hotswap enabled";
      }
    }

    ImGui::Separator();
    bool infinite_energy =
        g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) != 0;
    if (ImGui::Checkbox("Infinite Energy", &infinite_energy)) {
      g_kameo_infinite_energy_enabled.store(infinite_energy ? 1 : 0,
                                            std::memory_order_release);
    }
    ImGui::SameLine();
    bool infinite_health =
        g_kameo_infinite_health_enabled.load(std::memory_order_acquire) != 0;
    if (ImGui::Checkbox("Infinite Health", &infinite_health)) {
      g_kameo_infinite_health_enabled.store(infinite_health ? 1 : 0,
                                            std::memory_order_release);
    }

    ImGui::Text("FP faults: %llu",
      static_cast<unsigned long long>(
        g_kameo_fp_exception_count.load(std::memory_order_relaxed)));
    const uint32_t fp_last_code =
      g_kameo_fp_last_code.load(std::memory_order_acquire);
    const uint32_t fp_last_mxcsr =
      g_kameo_fp_last_mxcsr.load(std::memory_order_acquire);
    const uint64_t fp_last_rip =
      g_kameo_fp_last_rip.load(std::memory_order_acquire);
    if (fp_last_code != 0) {
      ImGui::Text("Last FP code: %08X MXCSR: %08X RIP: %016llX",
        fp_last_code, fp_last_mxcsr,
        static_cast<unsigned long long>(fp_last_rip));
    }

#ifdef KAMEO_TU
    ImGui::TextDisabled("DLC/model swap unavailable (TU build)");
#else
    const auto dlc_root = KameoActiveDlcPath();
    if (dlc_root.empty() || !std::filesystem::exists(dlc_root)) {
      ImGui::TextUnformatted("DLC folder missing");
    } else {
      int index = 0;
      std::set<std::string> shown_suffixes;
      for (const auto& pkg_entry : std::filesystem::directory_iterator(dlc_root)) {
        if (!pkg_entry.is_directory()) {
          continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(pkg_entry.path())) {
          if (!entry.is_regular_file()) {
            continue;
          }

          const auto filename = entry.path().filename().string();
          if (!IsKameoDlcModelName(filename)) {
            continue;
          }

          const auto suffix = KameoDlcModelSuffix(filename);
          if (suffix == "00" || suffix == "01" || suffix == "02" ||
              suffix == "std" || suffix == "prototype" ||
              suffix == "missing01" || suffix == "missing02" ||
              suffix == "alt01" || suffix == "alt02") {
            continue;
          }

          if (!shown_suffixes.insert(suffix).second) {
            continue;
          }

          if (!enabled) {
            ImGui::BeginDisabled();
          }

          if (ImGui::Button(suffix.c_str(), ImVec2(86.0f, 0.0f))) {
            g_kameo_stop_next_dlc_model_load.store(0, std::memory_order_release);
            QueueKameoDlcSuffix(suffix, IsNativeKameoDlcSuffix(suffix));
            last_result_ = "Queued DLC Kameo ";
            last_result_ += suffix;
          }

          if (!enabled) {
            ImGui::EndDisabled();
          }

          ++index;
          if ((index % 4) != 0) {
            ImGui::SameLine();
          }
        }
      }

      if (index > 0) {
        ImGui::Separator();
      }
      if (!enabled) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Standard")) {
        ClearKameoDlcSuffix();
        g_kameo_stop_next_dlc_model_load.store(1, std::memory_order_release);
        QueueKameoDlcSuffix("67", false);
        last_result_ = "Queued standard transition";
      }
      ImGui::SameLine();
      if (ImGui::Button("Prototype")) {
        g_kameo_stop_next_dlc_model_load.store(0, std::memory_order_release);
        QueueKameoDlcSuffix("std");
        last_result_ = "Queued prototype Kameo";
      }
      if (!enabled) {
        ImGui::EndDisabled();
      }
    }
#endif

    if (!last_result_.empty()) {
      ImGui::Separator();
      ImGui::TextUnformatted(last_result_.c_str());
    }

    ImGui::End();
  }

 private:
  std::string last_result_;
  bool visible_ = false;
};
