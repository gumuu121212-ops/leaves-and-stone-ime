#pragma once

#include <windows.h>
#include <atomic>
#include <fstream>
#include <string>

namespace leaves_stone {

inline std::string DiagnosticUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                      static_cast<int>(text.size()), nullptr,
                                      0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      &result[0], size, nullptr, nullptr);
  return result;
}

// Bounded metadata only: never record typed text or candidate contents.
inline void WriteTsfDiagnostic(const char* event, const std::string& details) {
  static std::atomic<unsigned> count{0};
  if (count.fetch_add(1) >= 128) return;
  wchar_t temp[MAX_PATH]{};
  const DWORD length = GetTempPathW(MAX_PATH, temp);
  if (!length || length >= MAX_PATH) return;
  const std::wstring file = std::wstring(temp) + L"leaves-stone-tsf.log";
  std::ofstream out(file.c_str(), std::ios::app | std::ios::binary);
  if (out)
    out << GetTickCount64() << " pid=" << GetCurrentProcessId()
        << " tid=" << GetCurrentThreadId() << " " << event << " "
        << details << '\n';
}

}  // namespace leaves_stone
