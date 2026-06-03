
// kameorepowered - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
// DLC model-name/path discovery helpers and KameoDLCList sync.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

// Returns true if `filename` is a Kameo DLC model file for title 0x007183BF,
// e.g. "007183BF_xmas1.mdl".
inline bool IsKameoDlcModelName(const std::string& filename) {
  return filename.rfind("007183BF_", 0) == 0 &&
         filename.size() > 13 &&
         filename.substr(filename.size() - 4) == ".mdl";
}

// Extracts the suffix from a DLC model filename, e.g. "007183BF_xmas1.mdl" → "xmas1".
inline std::string KameoDlcModelSuffix(const std::string& filename) {
  return filename.substr(9, filename.size() - 13);
}

// Returns true for suffixes that ship with the original game DLC
// (numeric two-digit IDs, plus named built-in variants).
inline bool IsNativeKameoDlcSuffix(const std::string& suffix) {
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

// Returns the path to the active Kameo DLC save folder, or an empty path if
// the user profile / HOME environment variable is unavailable.
inline std::filesystem::path KameoActiveDlcPath() {
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

// Scans the DLC folder for custom model files not yet listed in KameoDLCList.txt
// and appends any missing entries. No-ops if the folder or list don't exist.
inline void SyncKameoDlcListForCustomModels() {
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
