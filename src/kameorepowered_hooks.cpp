#include "kameorepowered_init.h"

#include "kameorepowered_dlc_swap.h"
#include "kameorepowered_hooks.h"

#include <algorithm>
#include <cmath>

extern "C" REX_FUNC(__imp__sub_822502A0);

std::atomic<uint32_t> g_kameo_infinite_energy_enabled{1};
std::atomic<uint32_t> g_kameo_infinite_health_enabled{1};

namespace {

uint32_t WriteGuestString(PPCContext &ctx, uint8_t *base, const char *text,
                          uint32_t stack_offset);

float LoadGuestFloat(uint8_t *base, uint32_t addr) {
  (void)base;
  PPCRegister value{};
  value.u32 = REX_LOAD_U32(addr);
  return value.f32;
}

void StoreGuestFloat(uint8_t *base, uint32_t addr, float value) {
  (void)base;
  PPCRegister bits{};
  bits.f32 = value;
  REX_STORE_U32(addr, bits.u32);
}

bool IsFinitePositiveHealthRecord(uint8_t *base, uint32_t health_record) {
  if (health_record == 0 || REX_LOAD_U32(health_record) == 0) {
    return false;
  }

  const float current = LoadGuestFloat(base, health_record + 0x0C);
  const float max = LoadGuestFloat(base, health_record + 0x2C);
  return std::isfinite(current) && std::isfinite(max) && max > 0.0f &&
         max < 1000000.0f;
}

bool IsPlayerHealthRecord(uint8_t *base, uint32_t health_record) {
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

  auto *thread_state = rex::runtime::ThreadState::Get();
  if (!thread_state || !thread_state->context()) {
    return false;
  }

  PPCContext call_ctx = *thread_state->context();
  call_ctx.r3.u64 = owner;
  __imp__sub_82252588(call_ctx, base);
  return call_ctx.r3.u32 != 0;
}

bool CallUnlockCheck(uint32_t id, PPCContext &source_ctx, uint8_t *base) {
  PPCContext call_ctx = source_ctx;
  call_ctx.r3.u64 = id;
  __imp__sub_822CC3C0(call_ctx, base);
  return call_ctx.r3.u32 != 0;
}

int32_t OriginalEnergyMax(PPCContext &source_ctx, uint8_t *base) {
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

int32_t OriginalEnergyCurrent(PPCContext &source_ctx, uint8_t *base) {
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

void RestoreOriginalEnergyCurrent(PPCRegister &r3) {
  auto *thread_state = rex::runtime::ThreadState::Get();
  auto *memory = rex::system::kernel_state()->memory();
  if (!thread_state || !thread_state->context() || !memory) {
    r3.s64 = 0;
    return;
  }

  uint8_t *base = memory->virtual_membase();
  r3.s64 = OriginalEnergyCurrent(*thread_state->context(), base);
}

void RestoreOriginalEnergyMax(PPCRegister &r3) {
  auto *thread_state = rex::runtime::ThreadState::Get();
  auto *memory = rex::system::kernel_state()->memory();
  if (!thread_state || !thread_state->context() || !memory) {
    r3.s64 = 100;
    return;
  }

  uint8_t *base = memory->virtual_membase();
  r3.s64 = OriginalEnergyMax(*thread_state->context(), base);
}

const char *KameoDlcSwapSuffix(int request) {
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

uint32_t WriteGuestString(PPCContext &ctx, uint8_t *base, const char *text,
                          uint32_t stack_offset) {
  const uint32_t guest_string = ctx.r1.u32 - stack_offset;
  for (uint32_t i = 0;; ++i) {
    REX_STORE_U8(guest_string + i, static_cast<uint8_t>(text[i]));
    if (text[i] == '\0') {
      break;
    }
  }
  return guest_string;
}

void ProcessPendingDlcSwap(PPCContext &ctx, uint8_t *base) {
  if (g_kameo_pending_standard_swap.exchange(0, std::memory_order_acq_rel) !=
      0) {
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
  const char *suffix = nullptr;
  if (ConsumeKameoPendingDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    const int request =
        g_kameo_pending_dlc_swap.exchange(-1, std::memory_order_acq_rel);
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

void OverrideDlcSelector(PPCContext &ctx, uint8_t *base) {
  if (g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }
  if (ctx.r3.u32 != 0x007183BF) {
    return;
  }

  char suffix_buffer[64]{};
  const char *suffix = nullptr;
  if (ReadKameoActiveDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    suffix = KameoDlcSwapSuffix(
        g_kameo_active_dlc_swap.load(std::memory_order_acquire));
  }
  if (!suffix) {
    return;
  }
  ctx.r4.u64 = WriteGuestString(ctx, base, suffix, 80);
}

} // namespace

void KameoUnlockDlc(PPCRegister &r3) { r3.s64 = 1; }

void KameoInfiniteEnergy(PPCRegister &r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  r3.s64 = 999;
}

void KameoInfiniteEnergyCurrent(PPCRegister &r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    RestoreOriginalEnergyCurrent(r3);
    return;
  }

  r3.s64 = 999;
}

void KameoInfiniteEnergyMax(PPCRegister &r3) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    RestoreOriginalEnergyMax(r3);
    return;
  }

  r3.s64 = 999;
}

void KameoRefillMeterFloat(PPCRegister &r31, PPCRegister &r27) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  if (r31.u32 == 0 || r27.u32 == 0) {
    return;
  }

  auto *memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t *base = memory->virtual_membase();
  REX_STORE_U32(r31.u32, REX_LOAD_U32(r27.u32));
}

void KameoRefillMeterFloatPlus4(PPCRegister &r31, PPCRegister &r27) {
  if (g_kameo_infinite_energy_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  if (r31.u32 == 0 || r27.u32 == 0) {
    return;
  }

  auto *memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t *base = memory->virtual_membase();
  REX_STORE_U32(r31.u32 + 4, REX_LOAD_U32(r27.u32 + 4));
}

void KameoRefillHealth(PPCRegister &r29) {
  if (r29.u32 == 0) {
    return;
  }

  if (g_kameo_infinite_health_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }

  auto *memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  uint8_t *base = memory->virtual_membase();
  if (!IsPlayerHealthRecord(base, r29.u32)) {
    return;
  }

  REX_STORE_U32(r29.u32 + 0x0C, REX_LOAD_U32(r29.u32 + 0x2C));
}

void KameoProcessPendingDlcSwapMid() {
  auto *thread_state = rex::runtime::ThreadState::Get();
  if (!thread_state || !thread_state->context()) {
    return;
  }

  auto *memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  ProcessPendingDlcSwap(*thread_state->context(), memory->virtual_membase());
}

void KameoOverrideDlcSelectorMid(PPCRegister &r3, PPCRegister &r4,
                                 PPCRegister &r1) {
  if (g_kameo_dlc_swap_enabled.load(std::memory_order_acquire) == 0) {
    return;
  }
  if (r3.u32 != 0x007183BF) {
    return;
  }

  char suffix_buffer[64]{};
  const char *suffix = nullptr;
  if (ReadKameoActiveDlcSuffix(suffix_buffer, sizeof(suffix_buffer))) {
    suffix = suffix_buffer;
  } else {
    suffix = KameoDlcSwapSuffix(
        g_kameo_active_dlc_swap.load(std::memory_order_acquire));
  }
  if (!suffix) {
    return;
  }
  auto *memory = rex::system::kernel_state()->memory();
  if (!memory) {
    return;
  }

  const uint32_t guest_string = r1.u32 - 80;
  uint8_t *base = memory->virtual_membase();
  for (uint32_t i = 0;; ++i) {
    REX_STORE_U8(guest_string + i, static_cast<uint8_t>(suffix[i]));
    if (suffix[i] == '\0') {
      break;
    }
  }
  r4.u64 = guest_string;
}

void KameoForceReloadOnSameRecord(PPCCRRegister &cr6) { (void)cr6; }

bool KameoShouldSkipNextDlcModelLoad() {
  return g_kameo_stop_next_dlc_model_load.exchange(
             0, std::memory_order_acq_rel) != 0;
}
