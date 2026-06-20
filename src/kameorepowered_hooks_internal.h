
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Shared internals for the hooks translation units: extern guest-function
// declarations, common guest-memory helpers, and cross-feature declarations.

#pragma once

#include "kameorepowered_init.h"

#include "kameorepowered_dlc_swap.h"
#include "kameorepowered_hooks.h"

// Guest functions called from host-side hooks that are not necessarily
// declared in the generated kameorepowered_init.h.
extern "C" REX_FUNC(__imp__sub_822502A0);  // FindDlcModel
extern "C" REX_FUNC(__imp__sub_8230C050);  // SetCategoryVolume
extern "C" REX_FUNC(__imp__sub_8251E538);  // UnloadStringTable
extern "C" REX_FUNC(__imp__sub_820BF6E8);  // ReloadStringTable
extern "C" REX_FUNC(__imp__sub_826D21B0);  // SetBinkLanguageTrackVolume

// Calls an original guest function from a hook.
//
// In a title-update build (-DKAMEO_TU) the functions these hooks call were
// relocated by the update and are absent from the patched image. The custom
// hooks are also stripped from the TU codegen config, so these call sites are
// never reached at runtime in a TU build — they are compiled out here only so
// the project links. The argument expression is still evaluated so locals stay
// "used". TODO: re-derive the patched-image addresses and drop this shim to
// restore the cheat/DLC/audio features on title-update builds.
#ifdef KAMEO_TU
#define KAMEO_CALL_GUEST(fn, ...) ((void)(__VA_ARGS__))
#else
#define KAMEO_CALL_GUEST(fn, ...) fn(__VA_ARGS__)
#endif

// ---------------------------------------------------------------------------
// Guest-memory helpers
// ---------------------------------------------------------------------------

// Returns the host pointer to the start of the guest virtual address space,
// or nullptr if the memory subsystem is not yet initialised.
inline uint8_t* GuestBase() {
  auto* memory = rex::system::kernel_state()->memory();
  return memory ? memory->virtual_membase() : nullptr;
}

// Returns a pointer to the PPC register context of the current guest thread,
// or nullptr if the thread state or context is not available.
inline PPCContext* CurrentGuestContext() {
  auto* ts = rex::runtime::ThreadState::Get();
  if (!ts) return nullptr;
  return ts->context();
}

// Writes a NUL-terminated string into the guest virtual address space at
// `guest_addr`. The caller must ensure `guest_addr` points to sufficient space.
// `base` must be the result of GuestBase() — REX_STORE_U8 requires it in scope.
inline void WriteGuestStringAt(uint8_t* base, uint32_t guest_addr, const char* text) {
  (void)base;  // kept in scope for the REX_STORE_U8 macro expansion
  for (uint32_t i = 0;; ++i) {
    REX_STORE_U8(guest_addr + i, static_cast<uint8_t>(text[i]));
    if (text[i] == '\0') break;
  }
}

// ---------------------------------------------------------------------------
// Cross-feature audio helpers (defined in kameorepowered_hooks_audio.cpp)
// ---------------------------------------------------------------------------

// Reloads the string table and speech wave banks when the user changes the
// audio language in the UI. No-ops if g_kameo_language_dirty is not set.
void KameoReloadLanguageStringTable(PPCContext& ctx, uint8_t* base);

// Replays cached SetCategoryVolume calls when the user moves a volume slider.
// No-ops if g_kameo_volume_dirty is not set.
void KameoReplayCachedVolumes(PPCContext& ctx, uint8_t* base);
