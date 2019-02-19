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

#ifndef COMMON_H
#define COMMON_H

#include "base.h"

// Category for all relevant flags.
extern cl::OptionCategory gl_category;

extern cl::opt<std::string> gl_tag;
extern cl::opt<bool> gl_verbose;

// Returns a path like xx/yyyy.html.
// Used to compute other paths.
std::string MakeOutputPath(uint64_t hash, const char* extension = ".html");

// Returns a path like ../../xx/yyyy.html.
// Used to fill href= fields or generally links in html / css / js files.
std::string MakeHtmlPath(uint64_t hash, const char* extension = ".html");

// Returns a path like output/sources/xx/yyyy.html.
// Used by the generator to write out the source files.
std::string MakeSourcePath(uint64_t hash, const char* extension = ".html");

// Returns a path like output/sources/meta/@path.html
// Used to generate meta index files and similar.
std::string MakeMetaPath(const std::string& path);

// Some declarations / objects don't have a valid end range.
// Generally, it is implicitly declared methods, like implicit
// constructors or copy operators.
// This function make an invalid range valid.
SourceRange NormalizeSourceRange(const SourceRange& range);
// Returns true if the specified range is valid.
bool IsValidRange(const clang::SourceRange& range);

// Returns the original text found in the specified source range.
std::string GetSourceRange(const SourceManager& sm, const SourceRange& range);

// Create all directories in path.
// Last element of the path name assumed to be a file.
bool MakeDirs(const std::string& path, int mode);
// Create all directories in path, including last element.
bool MakeAllDirs(const std::string& path, int mode);
// Returns the current working directory.
std::string GetCwd();

template <typename T>
struct HexConverted;
template <typename T>
static inline HexConverted<T> ToHex(T value);

extern std::string GetSuffixedValue(int64_t uv,
                                    std::array<const char*, 5> suffixes);

static inline std::string GetHumanValue(int64_t uv) {
  return GetSuffixedValue(uv, {" Tb", " Gb", " Mb", " Kb", " bytes"});
}

static inline std::string GetSuffixedValueBytes(int64_t uv) {
  return GetSuffixedValue(uv, {"Tb", "Gb", "Mb", "Kb", "b"});
}

static inline std::string GetSuffixedValueIS(int64_t uv) {
  return GetSuffixedValue(uv, {"T", "G", "M", "K", ""});
}

// IMPLEMENTATION

template <typename T>
struct HexConverted {
  static constexpr const int size = sizeof(T) * 2;
  char buffer[size + 1];

  operator StringRef() const { return StringRef(buffer, size); }
  operator std::string() const { return std::string(buffer, size); }
};

template <typename T>
static inline HexConverted<T> ToHex(T value) {
  HexConverted<T> retval;
  static constexpr const char conversion[] = {'0', '1', '2', '3', '4', '5',
                                              '6', '7', '8', '9', 'a', 'b',
                                              'c', 'd', 'e', 'f'};
  for (size_t i = 0, shift = sizeof(T) * 8; i < sizeof(T); ++i, shift -= 8) {
    retval.buffer[i * 2] =
        conversion[static_cast<unsigned>((value >> (shift - 4)) & 0xf)];
    retval.buffer[i * 2 + 1] =
        conversion[static_cast<unsigned>((value >> (shift - 8)) & 0xf)];
  }
  retval.buffer[retval.size] = '\0';
  return retval;
}

struct ConstCharCmp {
  bool operator()(char const* a, char const* b) {
    return std::strcmp(a, b) < 0;
  }
};

namespace std {
template <typename UnknownT>
inline string to_string(const UnknownT& unknown) {
  std::ostringstream ss;
  ss << unknown;
  return ss.str();
}

template <>
inline string to_string<StringRef>(const StringRef& str) {
  return str.str();
}
}  // namespace std

template <typename ArrayT, typename SeparatorT>
static inline std::string Join(const ArrayT& array, const SeparatorT& sep) {
  std::string result;
  bool first = true;
  for (const auto& element : array) {
    if (!first)
      result += sep;
    else
      first = false;

    result.append(element);
  }
  return result;
}

static inline std::string JoinPath(
    const std::initializer_list<StringRef>& paths) {
  std::string result;
  if (!paths.size()) return result;

  // Compute how much space to reserve.
  int size = 0;
  for (const auto& path : paths) {
    size += path.size() + 1;
  }

  // Append all elements of the list, followed by a "/".
  result.reserve(size);
  for (const auto& path : paths) {
    result.append(path.data(), path.size());
    result.append("/", 1);
  }

  // Remove last trailing "/".
  result.resize(result.size() - 1);
  return result;
}

class ScopedWorkingDirectory {
 public:
  ScopedWorkingDirectory(int error) : error_(error) {}
  ScopedWorkingDirectory(std::string&& oldcwd) : oldcwd_(oldcwd) {}

  bool HasError() { return error_ != 0; }

  ~ScopedWorkingDirectory() {
    if (error_ != 0) return;
    chdir(oldcwd_.c_str());
  }

 private:
  std::string oldcwd_;
  int error_ = 0;
};

inline ScopedWorkingDirectory ChangeDirectoryForScope(const std::string& dir) {
  auto buffer = GetCwd();
  if (buffer.empty() || chdir(dir.c_str()) != 0) {
    return ScopedWorkingDirectory(errno);
  }
  return ScopedWorkingDirectory(std::move(buffer));
}

inline std::string GetRealPath(const std::string& path) {
  char* real = realpath(path.c_str(), NULL);
  if (!real) return path;
  std::string retval(real);
  free(real);

  return retval;
}

#endif /* COMMON_H */
