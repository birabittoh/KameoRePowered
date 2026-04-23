// rekameo - ReXGlue Recompiled Project (Kameo: Elements of Power)
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Customize the app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <memory>
#include <rex/rex_app.h>
#include <rex/ppc/function.h>
#include <rex/cvar.h>

#include "generated/rekameo_init.h"
#include "kameo_engine/Log.h"
#include "kameo_engine/Overlays/Fps.h"

#if defined(_WIN32)
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

// Global FPS manager instance used by hooks that want to time specific
// guest functions (wire these up once you have midasm hooks on key
// tick / draw addresses).
inline FPSManager fpsManager;

// Declared here, defined once in main.cpp via REXCVAR_DEFINE_BOOL.
REXCVAR_DECLARE(bool, ShowFpsOverlay);
REXCVAR_DECLARE(bool, SkipIntros);
REXCVAR_DECLARE(bool, enable_console);

class RekameoApp : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;

    static std::unique_ptr<rex::ui::WindowedApp> Create(
        rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<RekameoApp>(
            new RekameoApp(ctx, "rekameo", PPCImageConfig));
    }

    void OnPostSetup() override {
        // Raise Windows timer resolution so sleep-based frame limiting
        // behaves. Safe no-op on non-Windows.
#if defined(_WIN32)
        timeBeginPeriod(1);
#endif
        REKAMEO_INFO("rekameo started");
    }

    void OnShutdown() override {
        REKAMEO_INFO("rekameo shutting down");
#if defined(_WIN32)
        timeEndPeriod(1);
#endif
    }

    void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
        auto* fpsDialog = new FpsOverlayDialog(drawer);
        fpsDialog->fpsManager = &fpsManager;
        fpsManager.showFPS = REXCVAR_GET(ShowFpsOverlay);
        drawer->AddDialog(fpsDialog);
    }
};
