#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

inline std::atomic<int> g_kameo_pending_dlc_swap{-1};
inline std::atomic<int> g_kameo_active_dlc_swap{-1};
inline std::atomic<int> g_kameo_dlc_swap_enabled{1};
inline std::atomic<int> g_kameo_pending_standard_swap{0};
inline std::atomic<int> g_kameo_stop_next_dlc_model_load{0};
inline std::atomic<uint32_t> g_kameo_standard_setup_object{0};

inline std::mutex g_kameo_dlc_swap_mutex;
inline std::array<char, 64> g_kameo_pending_dlc_suffix{};
inline std::array<char, 64> g_kameo_active_dlc_suffix{};
inline std::atomic<int> g_kameo_pending_dlc_suffix_ready{0};
inline std::atomic<int> g_kameo_active_dlc_suffix_ready{0};

inline void KameoCopySwapSuffix(std::array<char, 64>& target, std::string_view suffix) {
  target.fill('\0');
  const size_t copy_len = std::min(suffix.size(), target.size() - 1);
  std::copy_n(suffix.data(), copy_len, target.data());
}

inline void ClearKameoDlcSuffix() {
  std::lock_guard<std::mutex> lock(g_kameo_dlc_swap_mutex);
  g_kameo_pending_dlc_suffix.fill('\0');
  g_kameo_active_dlc_suffix.fill('\0');
  g_kameo_pending_dlc_suffix_ready.store(0, std::memory_order_release);
  g_kameo_active_dlc_suffix_ready.store(0, std::memory_order_release);
  g_kameo_pending_dlc_swap.store(-1, std::memory_order_release);
  g_kameo_active_dlc_swap.store(-1, std::memory_order_release);
}

inline void QueueKameoDlcSuffix(std::string_view suffix, bool make_active = true) {
  std::lock_guard<std::mutex> lock(g_kameo_dlc_swap_mutex);
  KameoCopySwapSuffix(g_kameo_pending_dlc_suffix, suffix);
  g_kameo_pending_dlc_suffix_ready.store(1, std::memory_order_release);
  if (make_active) {
    KameoCopySwapSuffix(g_kameo_active_dlc_suffix, suffix);
    g_kameo_active_dlc_suffix_ready.store(1, std::memory_order_release);
  }
  g_kameo_pending_dlc_swap.store(-1, std::memory_order_release);
  if (make_active) {
    g_kameo_active_dlc_swap.store(-1, std::memory_order_release);
  }
}

inline bool ConsumeKameoPendingDlcSuffix(char* out, size_t out_size) {
  if (out_size == 0 ||
      g_kameo_pending_dlc_suffix_ready.exchange(0, std::memory_order_acq_rel) == 0) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_kameo_dlc_swap_mutex);
  const size_t copy_len = std::min(out_size - 1, g_kameo_pending_dlc_suffix.size() - 1);
  std::copy_n(g_kameo_pending_dlc_suffix.data(), copy_len, out);
  out[copy_len] = '\0';
  return out[0] != '\0';
}

inline bool ReadKameoActiveDlcSuffix(char* out, size_t out_size) {
  if (out_size == 0 ||
      g_kameo_active_dlc_suffix_ready.load(std::memory_order_acquire) == 0) {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_kameo_dlc_swap_mutex);
  const size_t copy_len = std::min(out_size - 1, g_kameo_active_dlc_suffix.size() - 1);
  std::copy_n(g_kameo_active_dlc_suffix.data(), copy_len, out);
  out[copy_len] = '\0';
  return out[0] != '\0';
}
