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

#ifndef CACHE_H
#define CACHE_H

#include "base.h"
#include "common.h"
#include "renderer.h"

// There are 2 kind of files:
// - source files, need to be parsed and annotated.
// - binary files, need to be carried in the output tree, with minimal changes.
//
// Source files are "discovered":
// - we have a set of files provided by the user / compilation database.
// - we have a set of files that include statements bring in.
//
// Life of a file:
// - file is "discovered" during AST parsing. This means finding FID, path,
// filename.
//
// - at end of processing, for each file not parsed, a copy of the file is made,
// and
//   RawHighlight is called.
// - RawHighlight() -> uses the preprocessor to add rewrite information.

// Pseudo code:
// - each file will be included / parsed mutliple times.
// - GetFileFor in FileCache remembers the files.
//
// Keeps track of which files have been accessed since last Flush or
// creation, and performs operations on those files, once.

// Caches accesses to a FileRenderer, acts as a bridge between
// clang / llvm and FileRenderer.
class FileCache {
 public:
  FileCache(FileRenderer* renderer) : renderer_(renderer) {}

  // Given the provided parameters, the functions here fetch the corresponding
  // file descriptor.
  FileRenderer::ParsedFile* GetFileFor(const StringRef& path);
  FileRenderer::ParsedFile* GetFileFor(const SourceManager& sm,
                                       SourceLocation location);
  FileRenderer::ParsedFile* GetFileFor(const SourceManager& sm,
                                       const FileID& fid);
  FileRenderer::ParsedFile* GetFileFor(const SourceManager& sm,
                                       SourceLocation begin,
                                       SourceLocation end);

  uint64_t GetFileHashFor(const SourceManager& sm, SourceLocation location);

  const std::string& GetNormalizedPath(const SourceManager& sm,
                                       SourceLocation location);
  StringRef GetUserPath(const StringRef& other) const;

  void PreprocessPending();

 private:
  FileID last_id_;
  const SourceManager* last_sm_ = nullptr;
  StringRef last_path_;

  FileRenderer::ParsedFile* last_path_file_ = nullptr;
  FileRenderer::ParsedFile* last_sm_file_ = nullptr;
  FileRenderer* renderer_;

  static const std::string fid_error_;
};

inline FileRenderer::ParsedFile* FileCache::GetFileFor(const StringRef& path) {
  if (path.data() == last_path_.data()) return last_path_file_;

  if (path.empty()) return nullptr;

  last_path_file_ = renderer_->GetFileFor(path);
  return last_path_file_;
}

inline FileRenderer::ParsedFile* FileCache::GetFileFor(const SourceManager& sm,
                                                       const FileID& fid) {
  if (!fid.isValid()) return nullptr;

  if (last_sm_ == &sm && fid == last_id_) return last_sm_file_;

  last_sm_ = &sm;
  last_id_ = fid;

  // This code comes from getPresumedLoc.
  // getFileEntryForID or getFilename
  bool invalid = false;
  auto& entry = sm.getSLocEntry(fid, &invalid);
  if (invalid || !entry.isFile()) {
    last_sm_file_ = nullptr;
    return nullptr;
  }
  auto* cache = entry.getFile().getContentCache();
  if (!cache) {
    last_sm_file_ = nullptr;
    return nullptr;
  }

  const char* filename;
  if (cache->OrigEntry) {
    filename = cache->OrigEntry->getName();
  } else {
    filename = cache->getBuffer(sm.getDiagnostics(), sm)->getBufferIdentifier();
  }

  // Very unfortunate... is there a better way? TODO
  if (strcmp(filename, "<built-in>") == 0) return nullptr;

  last_sm_file_ = GetFileFor(filename);
  return last_sm_file_;
}

inline FileRenderer::ParsedFile* FileCache::GetFileFor(
    const SourceManager& sm, SourceLocation location) {
  if (!location.isValid()) return nullptr;

  FileID fid;
  unsigned offset;
  std::tie(fid, offset) = sm.getDecomposedExpansionLoc(location);
  return GetFileFor(sm, fid);
}

inline FileRenderer::ParsedFile* FileCache::GetFileFor(const SourceManager& sm,
                                                       SourceLocation begin,
                                                       SourceLocation end) {
  FileID bid = sm.getFileID(begin);
  FileID eid = sm.getFileID(end);

  if (eid != bid) {
    std::cerr << "WARNING: begin and end of locaiton in different files ("
              << sm.getFilename(begin).str() << " vs "
              << sm.getFilename(end).str() << ")" << std::endl;
    return nullptr;
  }
  return GetFileFor(sm, bid);
}

inline uint64_t FileCache::GetFileHashFor(const SourceManager& sm,
                                          SourceLocation location) {
  auto* ptr = GetFileFor(sm, location);
  return ptr ? ptr->hash : 0;
}

inline const std::string& FileCache::GetNormalizedPath(
    const SourceManager& sm, SourceLocation location) {
  auto* file = GetFileFor(sm, location);
  if (!file) {
    return fid_error_;
  }
  return file->path;
}
inline StringRef FileCache::GetUserPath(const StringRef& original) const {
  return renderer_->GetUserPath(original);
}

#endif /* CACHE_H */
