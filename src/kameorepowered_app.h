
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/system/flags.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <csignal>
#include <ucontext.h>
#endif
#include <cstdlib>
#include <exception>
#include <fstream>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <xmmintrin.h>

#include <imgui.h>

#include "kameorepowered_dlc_swap.h"
#include "kameorepowered_hooks.h"

#include <rex/platform/fpscr.h>

namespace {

static std::atomic<uint64_t> g_kameo_fp_exception_count{0};
static std::atomic<uint32_t> g_kameo_fp_last_code{0};
static std::atomic<uint32_t> g_kameo_fp_last_mxcsr{0};
static std::atomic<uint64_t> g_kameo_fp_last_rip{0};

// The Xbox 360 Xenon CPU runs with all FP exceptions masked. The rexglue
// runtime sets FZ+DAZ in MXCSR to match PPC FP semantics but leaves the
// exception mask bits clear, so any inexact/overflow/underflow result from
// recompiled guest code raises an SEH fault on the host. This VEH catches
// those faults on every thread, masks all SSE FP exceptions in the live
// MXCSR, and resumes execution from the faulting instruction.
#ifdef _WIN32
static LONG WINAPI GuestFpExceptionHandler(EXCEPTION_POINTERS* ep) {
    switch (ep->ExceptionRecord->ExceptionCode) {
        case 0xC000008C:  // STATUS_FLOAT_DIVIDE_BY_ZERO
        case 0xC000008D:  // STATUS_FLOAT_OVERFLOW
        case 0xC000008E:  // STATUS_FLOAT_UNDERFLOW
        case 0xC000008F:  // STATUS_FLOAT_INEXACT_RESULT
        case 0xC0000090:  // STATUS_FLOAT_INVALID_OPERATION
        case 0xC0000091:  // STATUS_FLOAT_STACK_CHECK
            g_kameo_fp_exception_count.fetch_add(1, std::memory_order_relaxed);
            g_kameo_fp_last_code.store(ep->ExceptionRecord->ExceptionCode,
                                       std::memory_order_release);
            g_kameo_fp_last_mxcsr.store(ep->ContextRecord->MxCsr,
                                        std::memory_order_release);
            g_kameo_fp_last_rip.store(
                reinterpret_cast<uint64_t>(ep->ExceptionRecord->ExceptionAddress),
                std::memory_order_release);
            ep->ContextRecord->MxCsr |= _MM_MASK_MASK;
            return EXCEPTION_CONTINUE_EXECUTION;
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
}
#else
static void GuestFpExceptionHandler(int /*sig*/, siginfo_t* si, void* ctx) {
    switch (si->si_code) {
        case FPE_FLTDIV:
        case FPE_FLTOVF:
        case FPE_FLTUND:
        case FPE_FLTRES:
        case FPE_FLTINV:
        case FPE_FLTSUB: {
            auto* uc = static_cast<ucontext_t*>(ctx);
            const uint32_t mxcsr = uc->uc_mcontext.fpregs->mxcsr;
            g_kameo_fp_exception_count.fetch_add(1, std::memory_order_relaxed);
            g_kameo_fp_last_code.store(static_cast<uint32_t>(si->si_code),
                                       std::memory_order_release);
            g_kameo_fp_last_mxcsr.store(mxcsr, std::memory_order_release);
            g_kameo_fp_last_rip.store(
                static_cast<uint64_t>(uc->uc_mcontext.gregs[REG_RIP]),
                std::memory_order_release);
            uc->uc_mcontext.fpregs->mxcsr |= _MM_MASK_MASK;
            break;
        }
        default:
            break;
    }
}
#endif

}  // namespace

namespace {

static bool IsKameoDlcModelName(const std::string& filename) {
  return filename.rfind("007183BF_", 0) == 0 &&
         filename.size() > 13 &&
         filename.substr(filename.size() - 4) == ".mdl";
}

static std::string KameoDlcModelSuffix(const std::string& filename) {
  return filename.substr(9, filename.size() - 13);
}

static bool IsNativeKameoDlcSuffix(const std::string& suffix) {
  if (suffix == "xmas1" || suffix == "std" || suffix == "prototype" ||
      suffix == "missing01" || suffix == "missing02" ||
      suffix == "alt01" || suffix == "alt02") {
    return true;
  }

  if (suffix.size() != 2) {
    return false;
  }

  return suffix[0] >= '0' && suffix[0] <= '9' &&
         suffix[1] >= '0' && suffix[1] <= '9';
}

static std::filesystem::path KameoActiveDlcPath() {
#ifdef _WIN32
  char* user_profile = nullptr;
  size_t user_profile_len = 0;
  if (_dupenv_s(&user_profile, &user_profile_len, "USERPROFILE") != 0 ||
      !user_profile || user_profile_len == 0) {
    if (user_profile) {
      std::free(user_profile);
    }
    return {};
  }
  std::filesystem::path base(user_profile);
  std::free(user_profile);
#else
  const char* home = std::getenv("HOME");
  if (!home || !home[0]) {
    return {};
  }
  std::filesystem::path base(home);
#endif
  return base / "Documents" / "kameo" /
      "0000000000000000" / "4D5307D2" / "00000002" /
      "3315297F7EFF0B5B4589A164C1EA9AE17FC81EC04D";
}

static void SyncKameoDlcListForCustomModels() {
  const auto dlc_path = KameoActiveDlcPath();
  if (dlc_path.empty() || !std::filesystem::exists(dlc_path)) {
    return;
  }

  const auto list_path = dlc_path / "KameoDLCList.txt";
  if (!std::filesystem::exists(list_path)) {
    return;
  }

  std::ifstream in(list_path);
  if (!in) {
    return;
  }

  std::set<std::string> listed;
  std::string line;
  bool list_ended_with_newline = true;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      listed.insert(line);
    }
    list_ended_with_newline = false;
  }
  if (in.eof()) {
    in.clear();
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size > 0) {
      in.seekg(-1, std::ios::end);
      char last = '\0';
      in.get(last);
      list_ended_with_newline = last == '\n';
    }
  }

  std::ofstream out(list_path, std::ios::app);
  if (!out) {
    return;
  }

  bool wrote_any = false;
  for (const auto& entry : std::filesystem::directory_iterator(dlc_path)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const std::string filename = entry.path().filename().string();
    if (!IsKameoDlcModelName(filename) || listed.count(filename) != 0) {
      continue;
    }

    if (!list_ended_with_newline || wrote_any) {
      out << '\n';
    }
    out << filename;
    listed.insert(filename);
    wrote_any = true;
  }
}

}  // namespace

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

class KameoModelDialog : public rex::ui::ImGuiDialog {
 public:
  KameoModelDialog(rex::ui::ImGuiDrawer* drawer, std::filesystem::path game_data_root)
      : rex::ui::ImGuiDialog(drawer), game_data_root_(std::move(game_data_root)) {}

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

    const auto dlc_path = DlcPath();
    if (dlc_path.empty() || !std::filesystem::exists(dlc_path)) {
      ImGui::TextUnformatted("DLC folder missing");
    } else {
      int index = 0;
      std::set<std::string> shown_suffixes;
      for (const auto& entry : std::filesystem::directory_iterator(dlc_path)) {
        if (!entry.is_regular_file()) {
          continue;
        }

        const auto filename = entry.path().filename().string();
        if (!IsKameoModel(filename)) {
          continue;
        }

        const auto suffix = ModelSuffix(filename);
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

    if (!last_result_.empty()) {
      ImGui::Separator();
      ImGui::TextUnformatted(last_result_.c_str());
    }

    ImGui::End();
  }

 private:
  static bool IsKameoModel(const std::string& filename) {
    return IsKameoDlcModelName(filename);
  }

  static std::string ModelSuffix(const std::string& filename) {
    return KameoDlcModelSuffix(filename);
  }

  static std::filesystem::path DlcPath() {
    return KameoActiveDlcPath();
  }

  std::filesystem::path game_data_root_;
  std::string last_result_;
  bool visible_ = false;
};

class KameorepoweredApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<KameorepoweredApp>(new KameorepoweredApp(ctx, "kameorepowered",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    SyncKameoDlcListForCustomModels();
#ifdef _WIN32
    veh_handle_ = AddVectoredExceptionHandler(1, GuestFpExceptionHandler);
#else
    struct sigaction sa{};
    sa.sa_sigaction = GuestFpExceptionHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    auto* old_sa = new struct sigaction{};
    sigaction(SIGFPE, &sa, old_sa);
    veh_handle_ = old_sa;
#endif
  }

  void OnShutdown() override {
    kameo_model_dialog_.reset();
    kameo_audio_dialog_.reset();
    if (veh_handle_) {
#ifdef _WIN32
      RemoveVectoredExceptionHandler(veh_handle_);
#else
      auto* old_sa = static_cast<struct sigaction*>(veh_handle_);
      sigaction(SIGFPE, old_sa, nullptr);
      delete old_sa;
#endif
      veh_handle_ = nullptr;
    }
  }

  // void OnLoadXexImage(std::string& xex_image) override {}
  void OnPostSetup() override {
    // Game hardcodes "D:\english" regardless of language setting.
    // Redirect to the correct language folder via VFS symlink.
    static const char* kLangFolders[] = {
      nullptr,     // 0 invalid
      "English",   // 1
      "Japanese",  // 2
      "German",    // 3
      "French",    // 4
      "Spanish",   // 5
      "Italian",   // 6
      "Korean",    // 7
      "TChinese",  // 8
      "Portuguese",// 9
      "SChinese",  // 10
      "Polish",    // 11
      "Russian",   // 12
    };
    uint32_t lang = REXCVAR_GET(user_language);
    if (lang == 0 || lang >= std::size(kLangFolders) || !kLangFolders[lang]) {
      lang = 1;  // Default to English if argument is missing or invalid.
    }
    // No symlink needed for English (lang=1): the game hardcodes D:\english,
    // which already resolves to the English folder. Registering english->English
    // creates a cycle on case-insensitive filesystems.
    if (lang > 1 && lang < std::size(kLangFolders) && kLangFolders[lang]) {
      auto* vfs = runtime()->file_system();
      std::string target = std::string("\\Device\\Harddisk0\\Partition1\\") + kLangFolders[lang];
      vfs->RegisterSymbolicLink("\\Device\\Harddisk0\\Partition1\\english", target);
    }
  }
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    kameo_model_dialog_ =
        std::make_unique<KameoModelDialog>(drawer, game_data_root());
    kameo_audio_dialog_ = std::make_unique<KameoAudioDialog>(drawer);
  }
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  void* veh_handle_ = nullptr;
  std::unique_ptr<KameoModelDialog> kameo_model_dialog_;
  std::unique_ptr<KameoAudioDialog> kameo_audio_dialog_;
};
