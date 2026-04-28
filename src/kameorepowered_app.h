
// kameo - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/filesystem/vfs.h>
#include <rex/rex_app.h>
#include <rex/system/flags.h>
#include <rex/system/xcontent.h>

#include "icon_data.h"

class KameoApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<KameoApp>(new KameoApp(ctx, "kameo",
        PPCImageConfig));
  }

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
    (void)drawer;
    if (window()) {
      window()->SetIcon(kameo_icon_data, sizeof(kameo_icon_data));
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
