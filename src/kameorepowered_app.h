
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <memory>
#include <string>

#include <rex/platform/fpscr.h>
#include <rex/rex_app.h>
#include <rex/system/flags.h>

#include "kameorepowered_dialogs.h"
#include "kameorepowered_dlc_models.h"
#include "kameorepowered_fp_guard.h"

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
    veh_handle_ = InstallGuestFpExceptionHandlerWin();
#endif
  }

  void OnShutdown() override {
    kameo_model_dialog_.reset();
    kameo_audio_dialog_.reset();
    RemoveGuestFpExceptionHandler(veh_handle_);
    veh_handle_ = nullptr;
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
    g_kameo_startup_language.store(static_cast<int32_t>(lang),
                                   std::memory_order_relaxed);
    // Prime g_kameo_audio_language so KameoOverrideAudioLanguage overrides
    // XGetLanguage before the opening FMV plays. Without this, the game stores
    // the system language (English) at 0x827556B4 and the Bink track selection
    // uses English even when --user_language is non-English.
    if (lang > 1) {
      g_kameo_audio_language.store(static_cast<int32_t>(lang),
                                   std::memory_order_relaxed);
    }
    // No symlink needed for English (lang=1): the game hardcodes D:\english,
    // which already resolves to the English folder. Registering english->English
    // creates a cycle on case-insensitive filesystems.
    if (lang > 1 && lang < std::size(kLangFolders) && kLangFolders[lang]) {
      auto* vfs = runtime()->file_system();
      std::string target = std::string("\\Device\\Harddisk0\\Partition1\\") + kLangFolders[lang];
      vfs->RegisterSymbolicLink("\\Device\\Harddisk0\\Partition1\\english", target);
    }
#ifndef _WIN32
    // Install after SDK setup so we override any SDK-installed SIGFPE handler.
    veh_handle_ = InstallGuestFpExceptionHandlerPosix();
#endif
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    kameo_model_dialog_ = std::make_unique<KameoModelDialog>(drawer);
    kameo_audio_dialog_ = std::make_unique<KameoAudioDialog>(drawer);
  }
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  void* veh_handle_ = nullptr;
  std::unique_ptr<KameoModelDialog> kameo_model_dialog_;
  std::unique_ptr<KameoAudioDialog> kameo_audio_dialog_;
};
