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

#ifndef RENDERER_H
#define RENDERER_H

#include "base.h"
#include "common.h"
#include "json-helpers.h"
#include "rewriter.h"

class FileRenderer {
 public:
  struct ParsedDirectory;

  enum FileType {
    kFileUnknown,
    kFileBinary,

    kFileParsed,
    kFileGenerated,

    kFilePrintable,
    kFileUtf8,

    kFileMedia,
    kFileHtml,
  };

  struct ParsedFile {
    ParsedFile(ParsedDirectory* parent, const std::string& rname)
        : parent(parent),
          name(rname),
          path(parent->path + "/" + rname),
          hash(hash_value(path)) {}
    bool Rendered() { return type != kFileUnknown; }
    bool Preprocessed() { return preprocessed; }

    std::string SourcePath(const char* lextension = nullptr) const {
      return MakeSourcePath(hash, lextension ? lextension : extension);
    }
    std::string HtmlPath() const { return MakeHtmlPath(hash, extension); }

    ParsedDirectory* parent = nullptr;
    std::string name;
    std::string path;
    uint64_t hash;

    off_t size = 0;
    time_t mtime = 0;

    const char* extension = ".html";

    bool preprocessing = false;
    bool preprocessed = false;

    // This is hacky - but the 3 act as a transparent cache.
    mutable FileType type = kFileUnknown;
    mutable HtmlRewriter rewriter;
    mutable std::string body;
  };

  struct ParsedDirectory {
    ParsedDirectory(ParsedDirectory* parent, const std::string& rname)
        : parent(parent),
          name(rname),
          path(parent ? (parent->parent ? parent->path + "/" + rname
                                        : parent->path + rname)
                      : "/"),
          hash(hash_value(path)) {}

    std::string SourcePath(const char* extension = ".html") const {
      return MakeSourcePath(hash, extension);
    }
    std::string HtmlPath(const char* extension = ".html") const {
      return MakeHtmlPath(hash, extension);
    }

    ParsedDirectory* parent = nullptr;
    std::string name;
    std::string path;
    uint64_t hash;

    std::map<std::string, ParsedDirectory> directories;
    std::map<std::string, ParsedFile> files;
  };

  FileRenderer();

  // Paths manipulation functions.
  std::string GetNormalizedPath(const std::string& path);
  StringRef GetUserPath(const StringRef& path) const;

  ParsedDirectory* GetDirectoryFor(const std::string& dirname);
  ParsedFile* GetFileFor(const std::string& path);

  std::pair<ParsedDirectory*, ParsedFile*> GetDirectoryAndFileFor(
      const std::string& filename);

  void SetWorkingPath(const std::string& cwd);
  void SetStripPath(const std::string& sp);

  // Tree rendering functions.
  void RenderFile(const SourceManager& sm, ParsedFile* file, FileID fid,
                  Preprocessor& pp);
  void ScanTree(const std::string& path);

  bool OutputJFiles();
  void OutputJOther();
  void OutputJsonTree(const char* path, const char* tag);

 private:
  void RawHighlight(FileID parsing_fid, Preprocessor& pp, ParsedFile* file);

  bool OutputJFile(const ParsedDirectory& dir, ParsedFile* file);
  bool OutputJDirectory(ParsedDirectory* dir);

  bool ReadFile(ParsedFile* file);

  void OutputJNavbar(json::Writer<json::OStreamWrapper>* writer,
                     const std::string& name, const std::string& path,
                     const FileRenderer::ParsedDirectory* current,
                     const FileRenderer::ParsedDirectory* parent);

  std::string FormatSource(Preprocessor& pp, FileID fid, ParsedFile* file);

  // Used to create absolute paths from relative paths fed to the renderer.
  ParsedDirectory* relative_root_;
  // Parent directories to strip from output when rendering tree.
  ParsedDirectory* stripping_root_ = nullptr;
  // The absolute root of our in-memory tree.
  ParsedDirectory absolute_root_{nullptr, ""};
};

inline const std::string& GetFilePath(FileRenderer::ParsedFile* file) {
  static const std::string fid_error{"<no-file-entry-corresponding-to-fid>"};
  return file ? file->path : fid_error;
}
inline uint64_t GetFileHash(FileRenderer::ParsedFile* file) {
  return file ? file->hash : 0;
}

#endif /* RENDERER_H */
