
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// Audio hooks: per-category volume scaling, Bink FMV audio, language selection.

#include "kameorepowered_hooks_internal.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

std::atomic<int32_t> g_kameo_sfx_volume{100};
std::atomic<int32_t> g_kameo_music_volume{100};
std::atomic<int32_t> g_kameo_ambience_volume{100};
std::atomic<int32_t> g_kameo_speech_volume{100};
std::atomic<int32_t> g_kameo_fmv_volume{100};
std::atomic<int32_t> g_kameo_audio_language{-1};
std::atomic<int32_t> g_kameo_volume_dirty{0};
std::atomic<int32_t> g_kameo_language_dirty{0};
std::atomic<int32_t> g_kameo_original_language{-1};
std::atomic<int32_t> g_kameo_startup_language{1};

// Build-specific audio guest addresses. The title update relocated/recompiled
// this code. The volume + bink-language paths are fully re-derived and active on
// both builds; these resolve their guest calls/globals to the matching image.
// (The string-table language reload below is gated off on TU — see the TODO
// there — and the startup-language override hook is not injected on TU at all.)
#ifdef KAMEO_TU
constexpr uint32_t kLangCodeGlobal = 0x827A9B64;
#define KAMEO_GUEST_SET_CATEGORY_VOLUME __imp__sub_82335D38  // SetCategoryVolume
#define KAMEO_GUEST_SET_BINK_LANG_TRACK __imp__sub_8270A9B0  // SetBinkLanguageTrackVolume
#else
constexpr uint32_t kLangCodeGlobal = 0x827556B4;
#define KAMEO_GUEST_SET_CATEGORY_VOLUME __imp__sub_8230C050
#define KAMEO_GUEST_SET_BINK_LANG_TRACK __imp__sub_826D21B0
#endif

namespace {

// Cache of the most recent SetCategoryVolume(cat_ptr, volume) call per
// category, keyed by category name. Replayed when a volume slider changes.
static std::mutex s_volume_snapshot_mutex;
static std::unordered_map<std::string, std::pair<uint32_t, double>> s_cat_last_params;

static bool KameoIsFmvCategory(const char* cat) {
  return strcmp(cat, "MovieMusic") == 0 ||
         strcmp(cat, "MovieSfx") == 0;
}

static bool KameoIsMusicCategory(const char* cat) {
  return strcmp(cat, "Music") == 0 ||
         strcmp(cat, "LevelMusic") == 0 ||
         strcmp(cat, "NonLevelMusic") == 0;
}

static bool KameoIsSfxCategory(const char* cat) {
  return strcmp(cat, "Sfx") == 0 ||
         strcmp(cat, "GameSfx") == 0 ||
         strcmp(cat, "MusicStingSfx") == 0 ||
         strcmp(cat, "InterfaceSfx") == 0;
}

static bool KameoIsAmbienceCategory(const char* cat) {
  return strcmp(cat, "Ambience") == 0 ||
         strcmp(cat, "LevelAmbience") == 0 ||
         strcmp(cat, "StealthAmbience") == 0;
}

static bool KameoIsSpeechCategory(const char* cat) {
  return strcmp(cat, "Speech") == 0 ||
         strcmp(cat, "InGameSpeech") == 0 ||
         strcmp(cat, "InterfaceSpeech") == 0;
}

}  // namespace

void KameoReloadLanguageStringTable(PPCContext& ctx, uint8_t* base) {
  if (g_kameo_language_dirty.exchange(0, std::memory_order_acq_rel) == 0) {
    return;
  }

#ifdef KAMEO_TU
  // TODO(tu-audio): the patched-image string-table language reload is not safely
  // wireable yet. UnloadStringTable (sub_82553A28) and ReloadStringTable
  // (sub_820C1408) are located, but the update refactored ReloadStringTable to
  // take the string-table name in r3 (vanilla ignored it), and the speech-bank
  // list head (vanilla 0x82B732A4) is SDA-relative and unresolved. Until both are
  // verified, skip the reload on TU so the pump only drives the volume replay.
  (void)ctx;
  (void)base;
  return;
#else

  int32_t lang = g_kameo_audio_language.load(std::memory_order_acquire);
  if (lang < 1 || lang > 9) {
    lang = g_kameo_original_language.load(std::memory_order_acquire);
  }
  if (lang < 1) {
    return;
  }

  // Write language code into the guest global that sub_820BF430 reads.
  REX_STORE_U32(0x827556B4, static_cast<uint32_t>(lang));

  // Unload the current string table if one is loaded.
  // sub_8251E538 expects r3 = the struct pointer (dword_827556B8).
  const uint32_t str_handle = REX_LOAD_U32(0x827556B8);
  if (str_handle != 0 && str_handle != 0xFFFFFFFF) {
    PPCContext unload_ctx = ctx;
    unload_ctx.r3.u64 = str_handle;
    KAMEO_CALL_GUEST(__imp__sub_8251E538, unload_ctx, base);
    REX_STORE_U32(0x827556B8, 0xFFFFFFFF);
  }

  // Reload string table — filename is hardcoded inside sub_820BF6E8.
  PPCContext reload_ctx = ctx;
  KAMEO_CALL_GUEST(__imp__sub_820BF6E8, reload_ctx, base);
  REX_STORE_U32(0x827556B8, reload_ctx.r3.u32);

  // Reload speech wave banks from the new language directory.
  // Speech banks are in a linked list at 0x82B732A4 (next ptr at +4, name at +8).
  // Node layout: +44=XACT bank ptr, +48=file buf, +52=reload flag,
  //              +56=state, +60=voice buf, +64=buf size.
  //
  // Dirty reset: zero all load-state fields and set state=0. The game's
  // sub_8230C0C8 and sub_8228A5B0 unconditionally overwrite node+44/60
  // on the next load, so we don't need to call the destructor ourselves
  // (avoids crashing through the COM vtable which holds a truncated host ptr).
  const uint32_t bank_list_head = REX_LOAD_U32(0x82B732A4);
  for (uint32_t node = bank_list_head; node != 0;
       node = REX_LOAD_U32(node + 4)) {
    const char* name = reinterpret_cast<const char*>(base + node + 8);
    const bool is_speech = (strstr(name, "Speech") != nullptr) ||
                           (strstr(name, "_NPC_")  != nullptr) ||
                           (strstr(name, "_Loc_")  != nullptr);
    if (!is_speech) continue;
    if (REX_LOAD_U32(node + 56) == 0) continue;  // already unloaded

    REX_STORE_U32(node + 44, 0);  // XACT bank ptr  — overwritten by sub_8230C0C8
    REX_STORE_U32(node + 48, 0);  // file buffer    — overwritten by async load
    REX_STORE_U32(node + 52, 1);  // reload flag=1  — sub_822ECD10 calls sub_822ECDF8
    REX_STORE_U32(node + 56, 0);  // state=0        — triggers sub_822ECC30 next tick
    REX_STORE_U32(node + 60, 0);  // voice buf ptr  — overwritten by sub_8228A5B0
    REX_STORE_U32(node + 64, 0);  // voice buf size — overwritten on reload
  }
#endif  // KAMEO_TU
}

void KameoReplayCachedVolumes(PPCContext& ctx, uint8_t* base) {
  if (g_kameo_volume_dirty.exchange(0, std::memory_order_acq_rel) == 0) {
    return;
  }

  std::vector<std::pair<uint32_t, double>> entries;
  {
    std::lock_guard<std::mutex> lock(s_volume_snapshot_mutex);
    entries.reserve(s_cat_last_params.size());
    for (auto& [name, p] : s_cat_last_params) {
      entries.push_back(p);
    }
  }
  for (auto& [cat_ptr, orig_f1] : entries) {
    PPCContext call_ctx = ctx;
    call_ctx.r3.u64 = cat_ptr;
    call_ctx.f1.f64 = orig_f1;
    KAMEO_GUEST_SET_CATEGORY_VOLUME(call_ctx, base);
  }
}

void KameoOverrideVolume(PPCRegister& r3, PPCRegister& f1) {
  if (r3.u32 == 0) {
    return;
  }

  uint8_t* base = GuestBase();
  if (!base) {
    return;
  }

  const char* cat = reinterpret_cast<const char*>(base + r3.u32);

  // Cache this call so the game-thread replay can reissue it when sliders change.
  {
    std::lock_guard<std::mutex> lock(s_volume_snapshot_mutex);
    s_cat_last_params[std::string(cat)] = {r3.u32, f1.f64};
  }

  int32_t scale_pct = 100;
  if (KameoIsFmvCategory(cat)) {
    scale_pct = g_kameo_fmv_volume.load(std::memory_order_acquire);
  } else if (KameoIsMusicCategory(cat)) {
    scale_pct = g_kameo_music_volume.load(std::memory_order_acquire);
  } else if (KameoIsSfxCategory(cat)) {
    scale_pct = g_kameo_sfx_volume.load(std::memory_order_acquire);
  } else if (KameoIsAmbienceCategory(cat)) {
    scale_pct = g_kameo_ambience_volume.load(std::memory_order_acquire);
  } else if (KameoIsSpeechCategory(cat)) {
    scale_pct = g_kameo_speech_volume.load(std::memory_order_acquire);
  }

  if (scale_pct == 100) {
    return;
  }

  const float scaled = static_cast<float>(f1.f64) * (static_cast<float>(scale_pct) / 100.0f);
  f1.f64 = static_cast<double>(scaled);
}

void KameoOverrideBinkVolume(PPCRegister& r5) {
  // r5 = 0 means the game is silencing the track (end of playback); leave it.
  if (r5.u32 == 0) {
    return;
  }
  const int32_t fmv_pct = g_kameo_fmv_volume.load(std::memory_order_acquire);
  if (fmv_pct == 100) {
    return;
  }
  r5.u32 = r5.u32 * static_cast<uint32_t>(fmv_pct) / 100;
}

void KameoSetBinkLanguageTrackVolume(PPCRegister& r27, PPCRegister& r29) {
  uint8_t* base = GuestBase();
  if (!base) {
    return;
  }

  const uint32_t bink_handle = REX_LOAD_U32(r29.u32 + 12);
  if (!bink_handle) {
    return;
  }

  // Mirror the switch in sub_82264140 to find the language audio track index.
  // The game initializes 0x827556B4 from XGetLanguage (system default = English)
  // before the opening FMV plays, so we can't rely on it for startup language.
  // Priority: runtime UI override (g_kameo_audio_language) > --user_language
  // (g_kameo_startup_language) > game's XGetLanguage result (0x827556B4).
  uint32_t lang_code = REX_LOAD_U32(kLangCodeGlobal);
  const int32_t audio_lang = g_kameo_audio_language.load(std::memory_order_acquire);
  const int32_t startup_lang = g_kameo_startup_language.load(std::memory_order_acquire);
  if (audio_lang >= 1) {
    lang_code = static_cast<uint32_t>(audio_lang);
  } else if (startup_lang > 1) {
    lang_code = static_cast<uint32_t>(startup_lang);
  }
  uint32_t lang_track;
  switch (lang_code) {
    case 2:  lang_track = 18; break;  // Japanese
    case 3:  lang_track = 15; break;  // German
    case 4:  lang_track = 13; break;  // French
    case 5:  lang_track = 16; break;  // Spanish
    case 6:  lang_track = 14; break;  // Italian
    case 10: lang_track = 17; break;  // Korean/Mexican
    default: lang_track = 12; break;  // English
  }

  uint32_t vol = r27.u32;  // 0x8000 = full, 0 = silence (FMV ending)
  if (vol != 0) {
    const int32_t fmv_pct = g_kameo_fmv_volume.load(std::memory_order_acquire);
    if (fmv_pct != 100) {
      vol = vol * static_cast<uint32_t>(fmv_pct) / 100;
    }
  }

  PPCContext* ctx_ptr = CurrentGuestContext();
  if (!ctx_ptr) {
    return;
  }
  PPCContext ctx = *ctx_ptr;
  ctx.r3.u64 = bink_handle;
  ctx.r4.u64 = lang_track;
  ctx.r5.u64 = vol;
  KAMEO_GUEST_SET_BINK_LANG_TRACK(ctx, base);
}

void KameoOverrideAudioLanguage(PPCRegister& r3) {
  // Save the original XGetLanguage value once so we can restore it for "system default".
  int32_t expected = -1;
  g_kameo_original_language.compare_exchange_strong(
      expected, static_cast<int32_t>(r3.u32),
      std::memory_order_acq_rel, std::memory_order_relaxed);

  const int32_t lang = g_kameo_audio_language.load(std::memory_order_acquire);
  if (lang >= 1 && lang <= 9) {
    r3.s64 = lang;
  }
}
