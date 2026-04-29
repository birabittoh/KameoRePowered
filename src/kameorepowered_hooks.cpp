#include "kameorepowered_init.h"

#include "kameorepowered_dlc_swap.h"
#include "kameorepowered_hooks.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" REX_FUNC(__imp__sub_822502A0);
extern "C" REX_FUNC(__imp__sub_8230C050);
extern "C" REX_FUNC(__imp__sub_8251E538);
extern "C" REX_FUNC(__imp__sub_820BF6E8);
extern "C" REX_FUNC(__imp__sub_8228A760);
extern "C" REX_FUNC(__imp__sub_8230C320);
extern "C" REX_FUNC(__imp__sub_826D21B0);

std::atomic<uint32_t> g_kameo_infinite_energy_enabled{1};
std::atomic<uint32_t> g_kameo_infinite_health_enabled{1};

std::atomic<int32_t> g_kameo_sfx_volume{100};
std::atomic<int32_t> g_kameo_music_volume{100};
std::atomic<int32_t> g_kameo_ambience_volume{100};
std::atomic<int32_t> g_kameo_speech_volume{100};
std::atomic<int32_t> g_kameo_fmv_volume{100};
std::atomic<int32_t> g_kameo_audio_language{-1};
std::atomic<int32_t> g_kameo_volume_dirty{0};
std::atomic<int32_t> g_kameo_language_dirty{0};
std::atomic<int32_t> g_kameo_original_language{-1};

namespace {

uint32_t WriteGuestString(PPCContext& ctx, uint8_t* base, const char* text, uint32_t stack_offset);

float LoadGuestFloat(uint8_t* base, uint32_t addr) {
  (void)base;
  PPCRegister value{};
  value.u32 = REX_LOAD_U32(addr);
  return value.f32;
}

void StoreGuestFloat(uint8_t* base, uint32_t addr, float value) {
  (void)base;
  PPCRegister bits{};
  bits.f32 = value;
  REX_STORE_U32(addr, bits.u32);
}

bool IsFinitePositiveHealthRecord(uint8_t* base, uint32_t health_record) {
  if (health_record == 0 || REX_LOAD_U32(health_record) == 0) {
    return false;
  }

  const float current = LoadGuestFloat(base, health_record + 0x0C);
  const float max = LoadGuestFloat(base, health_record + 0x2C);
  return std::isfinite(current) && std::isfinite(max) && max > 0.0f && max < 1000000.0f;
}

bool IsPlayerHealthRecord(uint8_t* base, uint32_t health_record) {
  if (health_record == 0) {
    return false;
  }

  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t player_root = REX_LOAD_U32(0x8280D1A0 + i * 4);
    if (player_root == 0) {
      continue;
    }

    const uint32_t actor = REX_LOAD_U32(player_root + 0x10);
    if (actor != 0 && REX_LOAD_U32(actor + 0xA24) == health_record) {
      return true;
    }
  }

  const uint32_t owner = REX_LOAD_U32(health_record);
  if (owner == 0) {
    return false;
  }

  auto* thread_state = rex::runtime::ThreadState::Get();
  if (!thread_state || !thread_state->context()) {
    return false;
  }

  PPCContext call_ctx = *thread_state->context();
  call_ctx.r3.u64 = owner;
  __imp__sub_82252588(call_ctx, base);
  return call_ctx.r3.u32 != 0;
}

bool CallUnlockCheck(uint32_t id, PPCContext& source_ctx, uint8_t* base) {
  PPCContext call_ctx = source_ctx;
  call_ctx.r3.u64 = id;
  __imp__sub_822CC3C0(call_ctx, base);
  return call_ctx.r3.u32 != 0;
}

int32_t OriginalEnergyMax(PPCContext& source_ctx, uint8_t* base) {
  if (CallUnlockCheck(1159, source_ctx, base)) {
    return 999;
  }
  if (CallUnlockCheck(1158, source_ctx, base)) {
    return 700;
  }
  if (CallUnlockCheck(1157, source_ctx, base)) {
    return 400;
  }
  if (CallUnlockCheck(1156, source_ctx, base)) {
    return 200;
  }
  return 100;
}

int32_t OriginalEnergyCurrent(PPCContext& source_ctx, uint8_t* base) {
  const uint32_t player_root = REX_LOAD_U32(0x8280D1A0);
  if (player_root == 0) {
    return 0;
  }

  const uint32_t actor = REX_LOAD_U32(player_root + 0x10);
  if (actor == 0) {
    return 0;
  }

  const uint32_t energy_record = REX_LOAD_U32(actor + 0xA24);
  if (energy_record == 0) {
    return 0;
  }

  PPCContext call_ctx = source_ctx;
  call_ctx.r3.u64 = energy_record;
  __imp__sub_8217CEC0(call_ctx, base);
  if (call_ctx.r3.u32 == 0) {
    return 0;
  }

  return static_cast<int32_t>(REX_LOAD_U32(call_ctx.r3.u32 + 0x18));
}

void RestoreOriginalEnergyCurrent(PPCRegister& r3) {
  auto* thread_state = rex::runtime::ThreadState::Get();
  auto* memory = rex::system::kernel_state()->memory();
  if (!thread_state || !thread_state->context() || !memory) {
    r3.s64 = 0;
    return;
  }

  uint8_t* base = memory->virtual_membase();
  r3.s64 = OriginalEnergyCurrent(*thread_state->context(), base);
}

void RestoreOriginalEnergyMax(PPCRegister& r3) {
  auto* thread_state = rex::runtime::ThreadState::Get();
  auto* memory = rex::system::kernel_state()->memory();
  if (!thread_state || !thread_state->context() || !memory) {
    r3.s64 = 100;
    return;
  }

  uint8_t* base = memory->virtual_membase();
  r3.s64 = OriginalEnergyMax(*thread_state->context(), base);
}

const char* KameoDlcSwapSuffix(int request) {
  if (request == 1000) {
    return "xmas1";
  }
  if (request == 1001) {
    return "std";
  }
  if (request == 1002) {
    return "prototype";
  }
  if (request == 1003) {
    return "alt01";
  }
  if (request == 1004) {
    return "alt02";
  }

  static thread_local char suffix[3]{};
  if (request < 0 || request > 99) {
    return nullptr;
  }

  suffix[0] = static_cast<char>('0' + (request / 10));
  suffix[1] = static_cast<char>('0' + (request % 10));
  suffix[2] = '\0';
  return suffix;
}

uint32_t WriteGuestString(PPCContext& ctx, uint8_t* base, const char* text, uint32_t stack_offset) {
  const uint32_t guest_string = ctx.r1.u32 - stack_offset;
  for (uint32_t i = 0;; ++i) {
    REX_STORE_U8(guest_string + i, static_cast<uint8_t>(text[i]));
    if (text[i] == '\0') {
      break;
    }
  }
  return guest_string;
}

void ProcessPendingDlcSwap(PPCContext& ctx, uint8_t* base) {
  if (g_kameo_pending_standard_swap.exchange(0, std::memory_order_acq_rel) != 0) {
    ClearKameoDlcSuffix();
    g_kameo_stop_next_dlc_model_load.store(1, std::memory_order_release);
    QueueKameoDlcSuffix("67", false);
    return;
  }

  if (g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) == 0) {
    g_kameo_pending_dlc_suffix_ready.store(0, std::memory_order_release);
    g_kameo_pending_dlc_swap.store(-1, std::memory_order_release);
    return;
  }

  char suffix_buffer[64]{};
  const char* suffix = nullptr;
  if (ConsumeKameoPendingDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    const int request = g_kameo_pending_dlc_swap.exchange(-1, std::memory_order_acq_rel);
    suffix = KameoDlcSwapSuffix(request);
  }
  if (!suffix) {
    return;
  }

  PPCContext call_ctx = ctx;
  call_ctx.r3.u64 = 0x007183BF;
  call_ctx.r4.u64 = WriteGuestString(call_ctx, base, suffix, 64);
  __imp__sub_822502A0(call_ctx, base);
}

void OverrideDlcSelector(PPCContext& ctx, uint8_t* base) {
  if (g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }
  if (ctx.r3.u32 != 0x007183BF) {
    return;
  }

  char suffix_buffer[64]{};
  const char* suffix = nullptr;
  if (ReadKameoActiveDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    suffix = KameoDlcSwapSuffix(g_kameo_active_dlc_swap.load(std::memory_order_acquire));
  }
  if (!suffix) {
    return;
  }
  ctx.r4.u64 = WriteGuestString(ctx, base, suffix, 80);
}

}  // namespace

void KameoUnlockDlc(PPCRegister& r3) {
  r3.s64 = 1;
}

void KameoInfiniteEnergy(PPCRegister& r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  r3.s64 = 999;
}

void KameoInfiniteEnergyCurrent(PPCRegister& r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    RestoreOriginalEnergyCurrent(r3);
    return;
  }

  r3.s64 = 999;
}

void KameoInfiniteEnergyMax(PPCRegister& r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    RestoreOriginalEnergyMax(r3);
    return;
  }

  r3.s64 = 999;
}

void KameoRefillMeterFloat(PPCRegister& r31, PPCRegister& r27) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  if (r31.u32 == 0 || r27.u32 == 0) {
    return;
  }

  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t* base = memory->virtual_membase();
  REX_STORE_U32(r31.u32, REX_LOAD_U32(r27.u32));
}

void KameoRefillMeterFloatPlus4(PPCRegister& r31, PPCRegister& r27) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  if (r31.u32 == 0 || r27.u32 == 0) {
    return;
  }

  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t* base = memory->virtual_membase();
  REX_STORE_U32(r31.u32 + 4, REX_LOAD_U32(r27.u32 + 4));
}

void KameoRefillHealth(PPCRegister& r29) {
  if (r29.u32 == 0) {
    return;
  }

  if (g_kameo_infinite_health_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t* base = memory->virtual_membase();
  if (!IsPlayerHealthRecord(base, r29.u32)) {
    return;
  }

  REX_STORE_U32(r29.u32 + 0x0C, REX_LOAD_U32(r29.u32 + 0x2C));
}

static std::mutex s_volume_snapshot_mutex;
static std::unordered_map<std::string, std::pair<uint32_t, double>> s_cat_last_params;

void KameoProcessPendingDlcSwapMid() {
  auto* thread_state = rex::runtime::ThreadState::Get();
  if (!thread_state || !thread_state->context()) {
    return;
  }

  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t* base = memory->virtual_membase();

  // Reload string table when the user changes language in the UI.
  if (g_kameo_language_dirty.exchange(0, std::memory_order_acq_rel) != 0) {
    int32_t lang = g_kameo_audio_language.load(std::memory_order_acquire);
    if (lang < 1 || lang > 9) {
      lang = g_kameo_original_language.load(std::memory_order_acquire);
    }
    if (lang >= 1) {
      // Write language code into the guest global that sub_820BF430 reads.
      REX_STORE_U32(0x827556B4, static_cast<uint32_t>(lang));

      // Unload the current string table if one is loaded.
      // sub_8251E538 expects r3 = the struct pointer (dword_827556B8).
      const uint32_t str_handle = REX_LOAD_U32(0x827556B8);
      if (str_handle != 0 && str_handle != 0xFFFFFFFF) {
        PPCContext unload_ctx = *thread_state->context();
        unload_ctx.r3.u64 = str_handle;
        __imp__sub_8251E538(unload_ctx, base);
        REX_STORE_U32(0x827556B8, 0xFFFFFFFF);
      }

      // Reload string table — filename is hardcoded inside sub_820BF6E8.
      PPCContext reload_ctx = *thread_state->context();
      __imp__sub_820BF6E8(reload_ctx, base);
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
    }
  }

  // Replay cached volume calls when the user moves a slider in the UI.
  if (g_kameo_volume_dirty.exchange(0, std::memory_order_acq_rel) != 0) {
    std::vector<std::pair<uint32_t, double>> entries;
    {
      std::lock_guard<std::mutex> lock(s_volume_snapshot_mutex);
      entries.reserve(s_cat_last_params.size());
      for (auto& [name, p] : s_cat_last_params) {
        entries.push_back(p);
      }
    }
    for (auto& [cat_ptr, orig_f1] : entries) {
      PPCContext ctx = *thread_state->context();
      ctx.r3.u64 = cat_ptr;
      ctx.f1.f64 = orig_f1;
      __imp__sub_8230C050(ctx, base);
    }
  }

  ProcessPendingDlcSwap(*thread_state->context(), base);
}

void KameoOverrideDlcSelectorMid(PPCRegister& r3, PPCRegister& r4, PPCRegister& r1) {
  if (g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }
  if (r3.u32 != 0x007183BF) {
    return;
  }

  char suffix_buffer[64]{};
  const char* suffix = nullptr;
  if (ReadKameoActiveDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    suffix = KameoDlcSwapSuffix(g_kameo_active_dlc_swap.load(std::memory_order_acquire));
  }
  if (!suffix) {
    return;
  }
  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  const uint32_t guest_string = r1.u32 - 80;
  uint8_t* base = memory->virtual_membase();
  for (uint32_t i = 0;; ++i) {
    REX_STORE_U8(guest_string + i, static_cast<uint8_t>(suffix[i]));
    if (suffix[i] == '\0') {
      break;
    }
  }
  r4.u64 = guest_string;
}

void KameoForceReloadOnSameRecord(PPCCRRegister& cr6) {
  (void)cr6;
}

bool KameoShouldSkipNextDlcModelLoad() {
  return g_kameo_stop_next_dlc_model_load.exchange(0, std::memory_order_acq_rel) != 0;
}

namespace {

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

void KameoOverrideVolume(PPCRegister& r3, PPCRegister& f1) {
  if (r3.u32 == 0) {
    return;
  }

  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  const char* cat = reinterpret_cast<const char*>(memory->virtual_membase() + r3.u32);

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
  auto* memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }
  uint8_t* base = memory->virtual_membase();

  const uint32_t bink_handle = REX_LOAD_U32(r29.u32 + 12);
  if (!bink_handle) {
    return;
  }

  // Mirror the switch in sub_82264140 to find the language audio track index.
  const uint32_t lang_code = REX_LOAD_U32(0x827556B4);
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

  auto* thread_state = rex::runtime::ThreadState::Get();
  if (!thread_state || !thread_state->context()) {
    return;
  }
  PPCContext ctx = *thread_state->context();
  ctx.r3.u64 = bink_handle;
  ctx.r4.u64 = lang_track;
  ctx.r5.u64 = vol;
  __imp__sub_826D21B0(ctx, base);
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
