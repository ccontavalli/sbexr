// Copyright (c) 2017 Carlo Contavalli (ccontavalli@gmail.com).
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY Carlo Contavalli ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Carlo Contavalli OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Carlo Contavalli.

#include "common.h"

std::string MakeOutputPath(uint64_t hash, const char* extension) {
  const auto& hex = ToHex(hash);
  return JoinPath(
             {{&hex.buffer[hex.size - 2], 2}, {hex.buffer, hex.size - 2}}) +
         extension;
}
std::string MakeHtmlPath(uint64_t hash, const char* extension) {
  return JoinPath({{"..", 2}, MakeOutputPath(hash, extension)});
}
std::string MakeSourcePath(uint64_t hash, const char* extension) {
  return JoinPath({"output", "sources", MakeOutputPath(hash, extension)});
}
std::string MakeMetaPath(const std::string& filename) {
  return JoinPath({"output", "sources", "meta", filename});
}

// Creates all the directories up to the last /.
// This is convenient to ensure the directory for a file exists.
// Example: MakeDirs("/etc/defaults/test", 0777) will ensure that
// "/etc", "/etc/defaults" exist, so the test file can be created.
bool MakeDirs(const std::string& path, int mode) {
  std::string copy(path);

  for (std::size_t index = 1;
       (index = copy.find('/', index)) != std::string::npos;) {
    copy[index] = '\0';
    if (!mkdir(copy.c_str(), mode) && errno != EEXIST) return false;

    copy[index] = '/';
    index = index + 1;
  }
  return true;
}
// Creates all the directories specified.
// Example: MakeAllDirs("/etc/defaults/test", 0777) will ensure that
// "/etc", "/etc/defaults", "/etc/defaults/test" all exist.
bool MakeAllDirs(const std::string& path, int mode) {
  if (!MakeDirs(path, mode)) {
    return false;
  }

  if (!mkdir(path.c_str(), mode) && errno != EEXIST) return false;
  return true;
}

std::string GetCwd() {
  std::string result;
  result.resize(64);

  while (getcwd(&result[0], result.size()) == NULL) {
    if (errno != ERANGE) {
      result.clear();
      return result;
    }

    auto newsize = std::min(result.size() * 2, result.max_size());
    if (newsize == result.size()) {
      result.clear();
      return result;
    }

    result.resize(newsize);
  }

  auto end = result.find('\0');
  if (end != std::string::npos) result.resize(end);
  return result;
}

std::string GetSuffixedValue(int64_t uv, std::array<const char*, 5> suffixes) {
  static constexpr const int kKb = 1024;
  static constexpr const int kMb = kKb * 1024;
  static constexpr const int kGb = kMb * 1024;
  static constexpr const int64_t kTb = kGb * 1024ULL;

  std::string retval;
  retval.resize(32);

  float value = uv;
  int suffix;
  if (value > kTb) {
    value /= kTb;
    suffix = 0;
  } else if (value > kGb) {
    value /= kGb;
    suffix = 1;
  } else if (value > kMb) {
    value /= kMb;
    suffix = 2;
  } else if (value > kKb) {
    value /= kKb;
    suffix = 3;
  } else {
    return std::to_string(uv) + suffixes[4];
  }

  int n =
      snprintf(&retval[0], retval.size(), "%3.2f%s", value, suffixes[suffix]);
  retval.resize(n);

  return retval;
}
