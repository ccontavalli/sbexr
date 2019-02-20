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

#include "renderer.h"
#include "json-helpers.h"
#include "wrapping.h"

// DEPRECATE once we remove the template expansion logic.
cl::opt<std::string> gl_project_name(
    "p", cl::desc("Project name, to use in titles of html pages."),
    cl::value_desc("name"), cl::init([]() {
      auto dir = GetCwd();
      auto found = dir.find_last_of('/');
      if (found != std::string::npos) return dir.substr(found + 1);
      return dir;
    }()),
    cl::cat(gl_category));
cl::opt<std::string> gl_scan_filter_regex(
    "x",
    cl::desc(
        "Regex describing which files to EXCLUDE from the directory scan."),
    cl::value_desc("regex"), cl::cat(gl_category));
cl::opt<std::string> gl_tag(
    "t", cl::desc("Tag to use when querying the symbols / tree database."),
    cl::value_desc("tag"), cl::init("output"), cl::cat(gl_category));

std::pair<std::string, std::string> SplitPath(const std::string& name) {
  auto slash = name.rfind('/');
  if (slash == std::string::npos) return std::make_pair(std::string(), name);
  return std::make_pair(name.substr(0, slash), name.substr(slash + 1));
}

StringRef FileRenderer::GetUserPath(const StringRef& path) const {
  if (stripping_root_ && path.startswith(stripping_root_->path)) {
    auto result =
        path.substr(std::min(stripping_root_->path.size() + 1, path.size()));
    if (result.empty()) {
      return "/";
    }
    return result;
  }
  return path;
}

#define STRANDLEN(str) str, sizeof(str)
FileRenderer::FileType GetFileTypeByExtension(const std::string& name,
                                              const char** extension) {
  static constexpr const struct {
    const char* extension;
    size_t length;
    FileRenderer::FileType type;
  } kFileTypes[] = {{STRANDLEN(".htm"), FileRenderer::kFileHtml},
                    {STRANDLEN(".html"), FileRenderer::kFileHtml},
                    {STRANDLEN(".svg"), FileRenderer::kFileHtml},

                    {STRANDLEN(".pdf"), FileRenderer::kFileMedia},

                    {STRANDLEN(".jpeg"), FileRenderer::kFileMedia},
                    {STRANDLEN(".jpg"), FileRenderer::kFileMedia},
                    {STRANDLEN(".png"), FileRenderer::kFileMedia},
                    {STRANDLEN(".gif"), FileRenderer::kFileMedia},
                    {STRANDLEN(".gifv"), FileRenderer::kFileMedia},
                    {STRANDLEN(".bmp"), FileRenderer::kFileMedia},

                    {STRANDLEN(".webm"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mkv"), FileRenderer::kFileMedia},
                    {STRANDLEN(".flv"), FileRenderer::kFileMedia},
                    {STRANDLEN(".vob"), FileRenderer::kFileMedia},
                    {STRANDLEN(".ogv"), FileRenderer::kFileMedia},
                    {STRANDLEN(".ogg"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mp3"), FileRenderer::kFileMedia},
                    {STRANDLEN(".avi"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mov"), FileRenderer::kFileMedia},
                    {STRANDLEN(".wmv"), FileRenderer::kFileMedia},
                    {STRANDLEN(".rm"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mp4"), FileRenderer::kFileMedia},
                    {STRANDLEN(".m4v"), FileRenderer::kFileMedia},
                    {STRANDLEN(".m4p"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mpg"), FileRenderer::kFileMedia},
                    {STRANDLEN(".mpeg"), FileRenderer::kFileMedia},
                    {STRANDLEN(".3gp"), FileRenderer::kFileMedia}};
  char buffer[] = "      ";
  const auto length = name.length();
  if (length < 5) return FileRenderer::kFileUnknown;

  // Find the '.' for the extension.
  size_t i;
  for (i = length - 1; name[i] != '.'; --i)
    if (i <= length - 5) return FileRenderer::kFileUnknown;

  for (int j = 0; i <= length; ++j, ++i) buffer[j] = tolower(name[i]);

  for (const auto& entry : kFileTypes) {
    if (!memcmp(buffer, entry.extension, entry.length)) {
      *extension = entry.extension;
      return entry.type;
    }
  }
  return FileRenderer::kFileUnknown;
}

FileRenderer::FileType GetFileType(const std::string& name,
                                   const std::string& content,
                                   const char** extension) {
  auto type = GetFileTypeByExtension(name, extension);
  if (type != FileRenderer::kFileUnknown) return type;

// This code was adapted from:
// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

  static constexpr const uint8_t utf8d[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 00..1f
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 20..3f
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 40..5f
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 60..7f
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   9,   9,   9,   9,   9,   9,
      9,   9,   9,   9,   9,   9,   9,   9,   9,   9,  // 80..9f
      7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
      7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
      7,   7,   7,   7,   7,   7,   7,   7,   7,   7,  // a0..bf
      8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
      2,   2,   2,   2,   2,   2,   2,   2,   2,   2,  // c0..df
      0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
      0x3, 0x3, 0x4, 0x3, 0x3,  // e0..ef
      0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
      0x8, 0x8, 0x8, 0x8, 0x8,  // f0..ff
      0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4,
      0x6, 0x1, 0x1, 0x1, 0x1,  // s0..s0
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   0,   1,   1,   1,   1,
      1,   0,   1,   0,   1,   1,   1,   1,   1,   1,  // s1..s2
      1,   2,   1,   1,   1,   1,   1,   2,   1,   2,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   2,   1,   1,   1,   1,   1,   1,   1,   1,  // s3..s4
      1,   2,   1,   1,   1,   1,   1,   1,   1,   2,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
      1,   3,   1,   3,   1,   1,   1,   1,   1,   1,  // s5..s6
      1,   3,   1,   1,   1,   1,   1,   3,   1,   3,   1,
      1,   1,   1,   1,   1,   1,   3,   1,   1,   1,   1,
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  // s7..s8
  };

  size_t ascii = 0;
  uint32_t state = UTF8_ACCEPT;
  for (size_t i = 0; i < content.size(); i++) {
    if (isascii(content[i])) {
      ++ascii;
      if (!isprint(content[i]) && !isspace(content[i]))
        return FileRenderer::kFileBinary;
      continue;
    }

    uint32_t type = utf8d[(uint8_t)content[i]];
    state = utf8d[256 + state * 16 + type];

    if (state == UTF8_REJECT) return FileRenderer::kFileBinary;
  }
  if (ascii == content.size()) return FileRenderer::kFilePrintable;
  return FileRenderer::kFileUtf8;
}

bool FileRenderer::ReadFile(ParsedFile* file) {
  static constexpr const size_t kLookSize = 16;
  int rsize = std::min<size_t>(kLookSize, file->size);

  std::string storage;
  storage.resize(rsize);
  // Read at most kLookSize bytes in memory.
  std::ifstream ifile(file->path.c_str());
  ifile.read(&storage[0], rsize);
  if (!ifile) {
    std::cerr << "WARNING: failed to read " << file->path << std::endl;
    return false;
  }

  // Helper to read whatever is left.
  auto ReadRemaining = [&storage, rsize, file, &ifile]() -> bool {
    storage.resize(file->size);
    ifile.read(&storage[rsize], file->size - rsize);
    if (!ifile) {
      std::cerr << "WARNING: error reading " << file->path << std::endl;
      return false;
    }
    return true;
  };

  const char* extension = nullptr;
  file->type = GetFileType(file->name, storage, &extension);
  // std::cerr << "FILE " << file->name <<  " " << file->type << " " <<
  // (extension ? extension : "none") <<  " " << kFilePrintable << " " <<
  // kFileUtf8 << std::endl;

  switch (file->type) {
    case kFileUnknown:
      file->type = kFileBinary;
    case kFileBinary:
      file->body = "&lt;unparsable blob&gt;";
      break;

    case kFilePrintable:
    case kFileUtf8:
      if (!ReadRemaining()) return false;
      file->body = html::EscapeText(storage);
      break;

    case kFileMedia:
      if (!ReadRemaining()) return false;
      file->body = std::move(storage);
      file->extension = extension;
      break;

    case kFileHtml:
      if (!ReadRemaining()) return false;
      file->body = std::move(storage);
      break;

    case kFileParsed:
    case kFileGenerated:
      break;
  }
  return true;
}

void FileRenderer::ScanTree(const std::string& start) {
  std::regex exclude(gl_scan_filter_regex);

  std::queue<ParsedDirectory*> to_scan;
  to_scan.push(GetDirectoryFor(start));

  while (!to_scan.empty()) {
    auto* drecord = to_scan.front();
    to_scan.pop();

    std::cerr << "SCANNING " << drecord->path << std::endl;
    auto* fd = opendir(drecord->path.c_str());
    if (!fd) {
      std::cerr << "ERROR: could not open dir " << drecord->path << std::endl;
      continue;
    }
    while (auto* entry = readdir(fd)) {
      if (entry->d_type == DT_DIR) {
        if (*entry->d_name == '.') continue;

        const auto& insert = drecord->directories.emplace(std::make_pair(
            entry->d_name, ParsedDirectory(drecord, entry->d_name)));
        auto* dir = &insert.first->second;
        if (!gl_scan_filter_regex.empty() &&
            std::regex_search(dir->path, exclude))
          continue;
        to_scan.push(dir);
        continue;
      }

      if (entry->d_type == DT_REG) {
        const auto& insert = drecord->files.emplace(
            std::make_pair(entry->d_name, ParsedFile(drecord, entry->d_name)));
        auto* file = &insert.first->second;
        if (file->Rendered()) continue;
        if (!gl_scan_filter_regex.empty() &&
            std::regex_search(file->path, exclude))
          continue;

        struct stat stats;
        int err = stat(file->path.c_str(), &stats);
        if (err != 0) {
          std::cerr << "WARNING: could not stat() " << file->path << std::endl;
          continue;
        }

        file->size = stats.st_size;
        file->mtime = stats.st_mtime;

        ReadFile(file);
        continue;
      }

      if (entry->d_type == DT_LNK) {
      }
    }
    closedir(fd);
  }
}

// Note that empty directories are possible, for example, a path like:
//   /usr/include/linux/../foo
// will result in the creation of an empty linux directory.
FileRenderer::ParsedDirectory* FileRenderer::GetDirectoryFor(
    const std::string& path) {
  auto* node = path[0] == '/' ? &absolute_root_ : relative_root_;
  size_t position = 0;
  // std::cerr << "PATH " << path << std::endl;
  do {
    auto slash = path.find('/', position);
    std::string dir;
    if (slash == std::string::npos) slash = path.size();

    // Handles paths like "/foo" or "///foo", which are the same.
    if (slash == position) {
      position = slash + 1;
      continue;
    }
    dir = path.substr(position, slash - position);
    //    std::cerr << dir << " ";

    if (dir == "..") {
      if (node->parent) node = node->parent;
    } else if (dir != ".") {
      auto result = node->directories.emplace(dir, ParsedDirectory(node, dir));
      node = &result.first->second;
    }
    position = slash + 1;
  } while (position < path.size());
  // std::cerr << std::endl;
  return node;
}

FileRenderer::FileRenderer() { SetWorkingPath(GetRealPath(GetCwd())); }

void FileRenderer::SetWorkingPath(const std::string& cwd) {
  relative_root_ = GetDirectoryFor(cwd);
}
void FileRenderer::SetStripPath(const std::string& strip) {
  if (strip.empty()) {
    stripping_root_ = nullptr;
    return;
  }
  stripping_root_ = GetDirectoryFor(strip);
}

std::pair<FileRenderer::ParsedDirectory*, FileRenderer::ParsedFile*>
FileRenderer::GetDirectoryAndFileFor(const std::string& path) {
  std::string dirname;
  std::string filename;

  std::tie(dirname, filename) = SplitPath(path);
  ParsedDirectory* node = GetDirectoryFor(dirname);
  ParsedFile* file = nullptr;

  if (!filename.empty()) {
    auto result =
        node->files.emplace(filename, std::move(ParsedFile(node, filename)));
    file = &result.first->second;
  }

  // Never reached.
  return std::make_pair(node, file);
}

FileRenderer::ParsedFile* FileRenderer::GetFileFor(const std::string& path) {
  return GetDirectoryAndFileFor(path).second;
}

void FileRenderer::RenderFile(const SourceManager& sm, ParsedFile* file,
                              FileID fid, Preprocessor& pp) {
  if (file->Rendered()) return;

  auto* entry = sm.getFileEntryForID(fid);
  file->type = kFileParsed;
  file->size = entry->getSize();
  file->mtime = entry->getModificationTime();
  file->body = FormatSource(pp, fid, file);
}

bool FileRenderer::OutputJFiles() {
  std::deque<ParsedDirectory*> to_output({&absolute_root_});
  while (!to_output.empty()) {
    auto* node = to_output.front();
    if (!OutputJDirectory(node)) {
      llvm::errs() << "ERROR: Could not output directory '" << node->name
                   << "' aka " << node->path << "\n";
    }

    to_output.pop_front();
    for (auto& element : node->files) {
      if (!OutputJFile(*node, &element.second)) {
        llvm::errs() << "ERROR: Could not output file '" << element.second.name
                     << "'\n";
      }
    }
    for (auto& element : node->directories)
      to_output.emplace_back(&element.second);
  }
  return true;
}

std::string FileRenderer::GetNormalizedPath(const std::string& filename) {
  if (filename.empty()) return "<invalid-file>";
  ParsedDirectory* directory;
  ParsedFile* file;
  std::tie(directory, file) = GetDirectoryAndFileFor(filename);
  return file->path;
}

void FileRenderer::OutputJOther() {
  const auto& globals = MakeMetaPath("globals.json");
  if (!MakeDirs(globals, 0777)) {
    std::cerr << "ERROR: FAILED TO MAKE META PATH '" << globals << "'"
              << std::endl;
    return;
  }

  std::ofstream myfile;
  myfile.open(globals);
  json::OStreamWrapper osw(myfile);
  json::PrettyWriter<json::OStreamWrapper> writer(osw);
  auto jdata = MakeJsonObject(&writer);
  OutputJNavbar(&writer, "", "", nullptr, nullptr);
}

void FileRenderer::OutputJsonTree(const char* path, const char* tag) {
  std::string basename = tag ? std::string("index.") + tag : "index";
  const auto& filepath = JoinPath({path, basename + ".files.json"});

  std::ofstream myfile;
  myfile.open(filepath);
  json::OStreamWrapper osw(myfile);
  json::PrettyWriter<json::OStreamWrapper> writer(osw);

  auto jdata = MakeJsonObject(&writer);
  auto files = MakeJsonArray(&writer, "data");

  // TODO: root is not output.
  std::deque<const ParsedDirectory*> to_output({&absolute_root_});
  while (!to_output.empty()) {
    auto* node = to_output.front();
    to_output.pop_front();

    {
      const ParsedDirectory* parent = node->parent;
      const ParsedDirectory& current = *node;

      auto dir = MakeJsonObject(&writer);
      WriteJsonKeyValue(&writer, "dir", GetUserPath(current.path));
      WriteJsonKeyValue(&writer, "href", current.HtmlPath());

      if (parent) WriteJsonKeyValue(&writer, "parent", parent->HtmlPath());
    }

    for (auto& element : node->files) {
      const ParsedDirectory& parent = *node;
      const ParsedFile& file = element.second;

      auto dfile = MakeJsonObject(&writer);
      WriteJsonKeyValue(&writer, "file", GetUserPath(file.path));
      WriteJsonKeyValue(&writer, "parent", parent.HtmlPath());
      WriteJsonKeyValue(&writer, "href", file.HtmlPath());
    }

    for (auto& element : node->directories) {
      const ParsedDirectory& current = element.second;
      to_output.emplace_back(&current);
    }
  }
}

void FileRenderer::RawHighlight(FileID parsing_fid, Preprocessor& pp,
                                ParsedFile* file) {
  const SourceManager& sm = pp.getSourceManager();
  const llvm::MemoryBuffer* buffer = sm.getBuffer(parsing_fid);
  // Lexer lexer(parsing_fid, FromFile, pp);
  // const char *BufferStart = pp.getBuffer().data();
  //  pp.SetCommentRetentionState(true, true);

  //  auto NextPP = [&pp](Token& tok) {
  //    pp.Lex(tok);
  //  };

  Lexer L(parsing_fid, buffer, sm, pp.getLangOpts());
  L.SetCommentRetentionState(true);

  auto NextRaw = [&L](Token& tok) { L.LexFromRawLexer(tok); };
  auto Next = NextRaw;

  // Inform the preprocessor that we want to retain comments as tokens, so we
  // can highlight them.

  // Lex all the tokens in raw mode, to avoid entering #includes or expanding
  // macros.
  Token token;
  Next(token);
  // L.LexFromRawLexer(Tok);

  while (token.isNot(tok::eof)) {
    // Find location of token, which could come from a different file.
    // (if includes are processed by the lexer)
    FileID token_fid;
    unsigned offset;
    std::tie(token_fid, offset) = sm.getDecomposedLoc(token.getLocation());
    // unsigned offset = sm.getFileOffset(token.getLocation()); // This only
    // works without includes - absolute offset.

    auto token_length = token.getLength();
    auto token_kind = token.getKind();
    //
    //    std::cerr << "TOKEN DUMP " <<  token.getName() << " " << token_kind <<
    //    " " << pp.getSpelling(token) << " "
    //      << (token.isLiteral() ? "literal " : "")
    //      << (token.isAnyIdentifier() ? "identifier " : "")
    //      << (token.isAnnotation() ? "annotation " : "")
    //      << (token.is(tok::raw_identifier) ? "raw " : "")
    //      << (token.is(tok::raw_identifier) ? token.getRawIdentifier().str() +
    //      " " : "")
    //      << token.getLocation().printToString(sm) << " " <<
    //      token_fid.getHashValue() << " " << offset << std::endl;
    // if (token.is(tok::raw_identifier)) {
    //  // Note that LookUpIdentifierInfo(token) will change the token in many
    //  different ways.
    //  // getRawIdentifier() won't work anymore, token kind will be skewed, ...
    //  auto* info = pp.LookUpIdentifierInfo(token);
    //  if (info) {
    //    std::cerr << "TOKEN INFO "
    //      << "macro " << info->hasMacroDefinition() << " " <<
    //      info->hadMacroDefinition()
    //      << " keyword " << info->isKeyword(pp.getLangOpts()) << std::endl;
    //  }
    //}
    if (token_fid != parsing_fid) {
      // pp.Lex(token);
      Next(token);
      continue;
    }

    switch (token_kind) {
      default:
        break;

      case tok::raw_identifier: {
        //      std::cerr << "TOKEN RAW " << std::endl;
        auto name = token.getRawIdentifier().str();
        auto* info = pp.LookUpIdentifierInfo(token);
        if (info && info->isKeyword(pp.getLangOpts())) {
          //       std::cerr << "TOKEN KEYWORD " << name << std::endl;
          WrapWithTag(file, offset, offset + token_length,
                      MakeTag("span", {"keyword", name}, {}));
        }
        break;
      }

      case tok::comment:
        //      std::cerr << "TOKEN COMMENT " << std::endl;
        WrapWithTag(file, offset, offset + token_length,
                    MakeTag("span", {"comment"}, {}));
        break;
      case tok::utf8_string_literal:
        // Chop off the u part of u8 prefix
        ++offset;
        --token_length;
      // FALL THROUGH to chop the 8
      case tok::wide_string_literal:
      case tok::utf16_string_literal:
      case tok::utf32_string_literal:
        // Chop off the L, u, U or 8 prefix
        ++offset;
        --token_length;
      // FALL THROUGH.
      case tok::string_literal:
        // FIXME: Exclude the optional ud-suffix from the highlighted range.
        WrapWithTag(file, offset, offset + token_length,
                    MakeTag("span", {"string"}, {}));
        break;
      case tok::numeric_constant:
        WrapWithTag(file, offset, offset + token_length,
                    MakeTag("span", {"numeric"}, {}));
        break;
      case tok::utf8_char_constant:
        ++offset;
        --token_length;
      case tok::wide_char_constant:
      case tok::utf16_char_constant:
      case tok::utf32_char_constant:
        // Chop off the L, u, U or 8 prefix
        ++offset;
        --token_length;
      case tok::char_constant:
        WrapWithTag(file, offset, offset + token_length,
                    MakeTag("span", {"char"}, {}));
        break;
      case tok::hash: {
        // If this is a preprocessor directive, all tokens to end of line are
        // too.
        if (!token.isAtStartOfLine()) break;

        // Eat all of the tokens until we get to the next one at the start of
        // line.
        unsigned token_end = offset + token_length;
        Next(token);
        while (!token.isAtStartOfLine() && token.isNot(tok::eof)) {
          token_end = sm.getFileOffset(token.getLocation()) + token.getLength();
          Next(token);
        }

        // Find end of line.  This is a hack.
        WrapWithTag(file, offset, token_end,
                    MakeTag("span", {"directive"}, {}));

        // Don't skip the next token.
        continue;
      }
    }

    Next(token);
    // pp.Lex(token);
    // L.LexFromRawLexer(token);
  }
}

std::string FileRenderer::FormatSource(Preprocessor& pp, FileID fid,
                                       ParsedFile* file) {
  const llvm::MemoryBuffer* buffer = pp.getSourceManager().getBuffer(fid);
  if (!buffer) return "<could-not-retrieve-buffer>";

  RawHighlight(fid, pp, file);
  std::string retval(buffer->getBufferStart(), buffer->getBufferSize());
  retval.shrink_to_fit();
  return retval;
}

bool FileRenderer::OutputJFile(const ParsedDirectory& parent,
                               ParsedFile* file) {
  const auto& path = file->SourcePath(".jhtml");
  std::cerr << "GENERATING JFILE " << file->path << " " << path << std::endl;
  if (!MakeDirs(path, 0777)) {
    std::cerr << "ERROR: FAILED TO MAKE DIRS FOR FILE '" << path << "'"
              << std::endl;
    return false;
  }

  std::ofstream myfile;
  if (file->type == kFileMedia) {
    // We need to maintain the original extension in this case.
    const auto& path = file->SourcePath();
    myfile.open(path);
    // TODO: use hard links, fall back to copy.
    myfile.write(file->body.c_str(), file->body.size());
    return true;
  }

  myfile.open(path);
  {
    json::OStreamWrapper osw(myfile);
    json::Writer<json::OStreamWrapper> writer(osw);

    auto jdata = MakeJsonObject(&writer);
    OutputJNavbar(&writer, file->name, file->path, nullptr, &parent);
  }

  AddJHtmlSeparator(&myfile);
  switch (file->type) {
    case FileRenderer::kFileHtml:
      myfile << html::EscapeText(file->body);
      break;

    case FileRenderer::kFilePrintable:
    case FileRenderer::kFileUtf8:
    case FileRenderer::kFileUnknown:
    case FileRenderer::kFileBinary:
      myfile << file->body;
      break;

    case FileRenderer::kFileParsed:
      file->type = FileRenderer::kFileGenerated;
      file->body = file->rewriter.Generate(file->path, file->body);
      /* NO BREAK HERE */

    case FileRenderer::kFileGenerated:
      myfile << file->body;
      break;
    case FileRenderer::kFileMedia:
      abort();
      break;
  }
  return true;
}

void FileRenderer::OutputJNavbar(json::Writer<json::OStreamWrapper>* writer,
                                 const std::string& name,
                                 const std::string& path,
                                 const FileRenderer::ParsedDirectory* current,
                                 const FileRenderer::ParsedDirectory* parent) {
  // Build stack of parent directories, and find root.
  const FileRenderer::ParsedDirectory* root = stripping_root_;
  static std::deque<const FileRenderer::ParsedDirectory*> stack;
  for (const auto* cursor = current ? current : parent;
       cursor && cursor != root; cursor = cursor->parent) {
    if (!cursor->parent) {
      root = cursor;
      break;
    }
    if (cursor != current) stack.push_back(cursor);
  }

  WriteJsonKeyValue(writer, "name", name);
  WriteJsonKeyValue(writer, "path", GetUserPath(path));
  WriteJsonKeyValue(writer, "root", root->HtmlPath());
  WriteJsonKeyValue(writer, "project", gl_project_name);
  WriteJsonKeyValue(writer, "tag", gl_tag);

  {
    auto parents = MakeJsonArray(writer, "parents");
    while (!stack.empty()) {
      const auto* cursor = stack.back();
      stack.pop_back();

      auto parent = MakeJsonObject(writer);
      WriteJsonKeyValue(writer, "name", cursor->name);
      WriteJsonKeyValue(writer, "href", cursor->HtmlPath());
    }
  }
}

bool FileRenderer::OutputJDirectory(ParsedDirectory* dir) {
  const auto& path = dir->SourcePath(".jhtml");
  std::cerr << "GENERATING JDIR " << dir->path << " " << path << std::endl;
  if (!MakeDirs(path, 0777)) {
    std::cerr << "ERROR: FAILED TO MAKE DIRS FOR '" << path << "'" << std::endl;
    return false;
  }

  std::ofstream myfile;
  myfile.open(path);
  {
    json::OStreamWrapper osw(myfile);
    json::Writer<json::OStreamWrapper> writer(osw);

    auto jdata = MakeJsonObject(&writer);
    OutputJNavbar(&writer, dir->name, dir->path, dir, dir->parent);

    std::string code;
    if (!dir->files.empty()) {
      auto files = MakeJsonArray(&writer, "files");
      for (const auto& it : dir->files) {
        auto& filename = it.first;
        auto& descriptor = it.second;
        auto file = MakeJsonObject(&writer);

        WriteJsonKeyValue(&writer, "name", filename);
        writer.Key("type");
        switch (descriptor.type) {
          case kFileMedia:
            WriteJsonString(&writer, "media");
            break;

          case kFileUtf8:
          case kFilePrintable:
          case kFileHtml:
            WriteJsonString(&writer, "text");
            break;

          case kFileParsed:
          case kFileGenerated:
            WriteJsonString(&writer, "parsed");
            break;

          case kFileUnknown:
          case kFileBinary:
            WriteJsonString(&writer, "blob");
            break;
        }

        WriteJsonKeyValue(&writer, "href", descriptor.HtmlPath());
        WriteJsonKeyValue(&writer, "mtime", ctime(&descriptor.mtime));
        WriteJsonKeyValue(&writer, "size", descriptor.size);
      }
    }

    if (!dir->directories.empty() ||
        (dir->parent && dir != &absolute_root_ && dir != stripping_root_)) {
      auto dirs = MakeJsonArray(&writer, "dirs");

      if (dir->parent && dir != &absolute_root_ && dir != stripping_root_) {
        auto obj = MakeJsonObject(&writer);

        WriteJsonKeyValue(&writer, "href", dir->parent->HtmlPath());
        WriteJsonKeyValue(&writer, "size", dir->parent->files.size());
        WriteJsonKeyValue(&writer, "name", "..");
      }

      for (const auto& it : dir->directories) {
        auto& name = it.first;
        auto& descriptor = it.second;
        auto obj = MakeJsonObject(&writer);

        WriteJsonKeyValue(&writer, "href", descriptor.HtmlPath());
        WriteJsonKeyValue(&writer, "size", descriptor.files.size());
        WriteJsonKeyValue(&writer, "name", name);
      }
    }
  }
  AddJHtmlSeparator(&myfile);
  return true;
}
