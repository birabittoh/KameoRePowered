// rekameo - ReXGlue Recompiled Project (Kameo: Elements of Power)
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.

#include "generated/rekameo_init.h"

#include <rex/cvar.h>
#include "kameo_engine/rex_macros.h"
#include "kameo_engine/Log.h"
#include "rekameo_app.h"

REX_DEFINE_APP(rekameo, RekameoApp::Create)

// CVar definitions — must be in exactly one .cpp file.
REXCVAR_DEFINE_BOOL(ShowFpsOverlay, false, "_Kameo", "Show FPS overlay on startup");
REXCVAR_DEFINE_BOOL(SkipIntros, false, "_Kameo", "Skip startup logos and intros");
REXCVAR_DEFINE_BOOL(enable_console, false, "_Kameo", "Enable debug console");

// ---------------------------------------------------------------------------
// Hand-written PPC hooks
//
// As you identify functions in Kameo's default.xex that you want to
// intercept, add their addresses to config/rekameo_hooked.toml with a
// name like `rex_foo_00000000`, then implement the hook here:
//
//   int foo_00000000_Hook(...) { ... }
//   REX_PPC_HOOK(foo_00000000);
//
// See src/kameo_engine/hooks.cpp for recommended placement of larger
// hook implementations.
// ---------------------------------------------------------------------------
