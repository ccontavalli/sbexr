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

#include "indexer.h"
#include "counters.h"

#include "json-helpers.h"

auto& c_invalid_object_id = MakeCounter(
    "indexer/object-id/invalid-file",
    "Link lead to an #invalid-id, as there was no file set in the Id objecT");

auto& c_discarded_define_range =
    MakeCounter("indexer/record/define/invalid-range",
                "Ranges passed to RecordDefines are not valid");
auto& c_discarded_define_file =
    MakeCounter("indexer/record/define/invalid-file",
                "Ranges passed to RecordDefines refer to an invalid file");
auto& c_discarded_declare_range =
    MakeCounter("indexer/record/declare/invalid-range",
                "Ranges passed to RecordDefines are not valid");
auto& c_discarded_declare_file =
    MakeCounter("indexer/record/declare/invalid-file",
                "Ranges passed to RecordDefines refer to an invalid file");

const char _kIndexString[] = "Generic";
const char _kSnippetString[] = "Snippet";
const char _kNameString[] = "Name";

std::string ObjIdToLink(const Indexer::Id& id) {
  if (!id.file) {
    c_invalid_object_id.Add() << id;
    return "#invalid-id";
  }
  return MakeHtmlPath(id.file->hash) + "#" + MakeIdName(id.object);
}

Indexer::Id::Id(FileCache* cache, const SourceManager& sm,
                const SourceRange& target)
    : file(cache->GetFileFor(sm, target.getBegin())),
      object(MakeObjectId(sm, NormalizeSourceRange(target))) {}

bool Indexer::Id::operator<(const Id& other) const {
  if (file != other.file) return file->path < other.file->path;
  return object < other.object;
}
bool Indexer::Id::operator==(const Id& other) const {
  return file == other.file && object == other.object;
}

bool Indexer::RecordException(const SourceManager& sm,
                              const clang::SourceRange& target,
                              const std::string& exception) {
  if (!target.isValid()) return false;

  Id tid(cache_, sm, target);
  if (!tid.file) return false;

  auto& properties = index_[tid];
  properties.exceptions.push_back(exception);
  return true;
}

bool Indexer::RecordUse(const SourceManager& sm,
                        const clang::SourceRange& target,
                        const clang::SourceRange& user,
                        const char* description) {
  if (!target.isValid() || !user.isValid()) return false;

  Id tid(cache_, sm, target);
  Id uid(cache_, sm, user);

  if (!tid.file || !uid.file) return false;

  auto& properties = index_[tid];
  properties.users.emplace_back(Id(cache_, sm, user));
  return true;
}

bool Indexer::RecordDefines(const SourceManager& sm,
                            const clang::SourceRange& defined,
                            const clang::SourceRange& definer, const char* kind,
                            const std::string& name, const StringRef& snippet,
                            AccessSpecifier access,
                            const clang::Linkage linkage) {
  if (!defined.isValid() || !definer.isValid()) {
    c_discarded_define_range.Add(defined)
        << "name: " << name << ", snippet: " << snippet.str();
    return false;
  }

  Id definedid(cache_, sm, defined);
  Id definerid(cache_, sm, definer);

  if (!definedid.file || !definerid.file) {
    c_discarded_define_file.Add(defined)
        << "name: " << name << ", snippet: " << snippet.str();
    return false;
  }

  auto& properties = index_[definedid];
  properties.providers.emplace_back(Properties::kFlagDefinition, definerid,
                                    name, snippet, kind, access, linkage);
  return true;
}

bool Indexer::RecordDeclares(const SourceManager& sm,
                             const clang::SourceRange& declared,
                             const clang::SourceRange& declarer,
                             const char* kind, const std::string& name,
                             const StringRef& snippet,
                             const AccessSpecifier access,
                             const clang::Linkage linkage) {
  if (!declared.isValid() || !declarer.isValid()) {
    c_discarded_declare_range.Add(declared)
        << "name: " << name << ", snippet: " << snippet.str();
    return false;
  }

  Id declaredid(cache_, sm, declared);
  Id declarerid(cache_, sm, declarer);

  if (!declaredid.file || !declarerid.file) {
    c_discarded_declare_file.Add(declared)
        << "name: " << name << ", snippet: " << snippet.str();
    return false;
  }

  auto& properties = index_[declaredid];
  properties.providers.emplace_back(Properties::kFlagNone, declarerid, name,
                                    snippet, kind, access, linkage);
  return true;
}

class JsonWriter {
 public:
  typedef char Ch;

  JsonWriter(std::ofstream* myfile) : myfile_(myfile) {}
  void Put(char c) { myfile_->put(c); }
  void PutN(char c, int n) {
    for (int i = 0; i < n; ++i) Put(c);
  }
  void Flush() {}

 private:
  std::ofstream* myfile_;
};

template <typename MemPoolT>
void OutputPool(const char* path, const MemPoolT& mempool) {
  std::ofstream myfile;
  myfile.open(
      path, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

  const auto& storage = mempool.GetStorage();

  myfile.write(storage.data(), storage.size());
}

void Indexer::OutputJsonIndex(const char* path) {
  struct LinkageKind {
    bool operator<(const LinkageKind& other) const {
      bool klower = kind < other.kind;
      if (klower) return true;
      if (kind != other.kind) return false;
      bool llower = linkage < other.linkage;
      if (llower) return true;
      if (linkage != other.linkage) return false;
      return access < other.access;
    }

    IndexString kind;
    Linkage linkage;
    uint8_t access;
  };

  std::map<NameString,
           std::map<LinkageKind, std::map<const Id, const Properties*>>>
      locations;
  for (const auto& objit : index_) {
    const auto& objid = objit.first;
    const auto& objdata = objit.second;
    for (const auto& provider : objdata.providers) {
      LinkageKind lk = {provider.kind, provider.linkage, provider.access};
      locations[provider.name][lk][objid] = &objdata;
    }
  }

  std::ofstream myfile;
  myfile.open(
      path, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

  JsonWriter outputter(&myfile);
  json::PrettyWriter<JsonWriter> writer(outputter);

  auto data = MakeJsonObject(&writer);
  auto symbols = MakeJsonArray(&writer, "data");

  auto OutputProvider = [&writer, this](const NameString& name,
                                        const Properties::Provider& provider) {
    auto jprovider = MakeJsonObject(&writer);
    if (name != provider.name) return;

    writer.Key("href");
    WriteJsonString(&writer, ObjIdToLink(provider.location));

    writer.Key("location");
    WriteJsonString(&writer,
                    cache_->GetUserPath(std::to_string(provider.location)));

    writer.Key("snippet");
    WriteJsonString(&writer, provider.snippet);
  };

  auto OutputUser = [&writer, this](const NameString& name,
                                        const Properties::User& user) {
    auto juser = MakeJsonObject(&writer);

    writer.Key("href");
    WriteJsonString(&writer, ObjIdToLink(user.location));

    writer.Key("location");
    WriteJsonString(&writer,
                    cache_->GetUserPath(std::to_string(user.location)));
  };

  for (const auto& objit : locations) {
    const auto& objname = objit.first;
    const auto& linkkinds = objit.second;

    auto symbol = MakeJsonObject(&writer);

    writer.Key("name");
    WriteJsonString(&writer, objname);

    writer.Key("kinds");
    auto jlinkkinds = MakeJsonArray(&writer);

    for (const auto& kindit : linkkinds) {
      const auto& linkkind = kindit.first;
      const auto& objdatas = kindit.second;

      auto jlinkkind = MakeJsonObject(&writer);

      writer.Key("kind");
      WriteJsonString(&writer, linkkind.kind);
      writer.Key("linkage");
      writer.Uint(linkkind.linkage);
      if (linkkind.access != 255 && linkkind.access != AS_none) {
        writer.Key("access");
        writer.Uint(linkkind.access);
      }

      {
        writer.Key("defs");
        auto defs = MakeJsonArray(&writer);
        std::set<Id> deduper;

        for (const auto& objdata : objdatas) {
          const auto* objprops = objdata.second;
          for (const auto& provider : objprops->providers)
            if ((provider.flags & Properties::kFlagDefinition) &&
                deduper.find(provider.location) == deduper.end()) {
              OutputProvider(objname, provider);
              deduper.insert(provider.location);
            }
        }
      }

      {
        writer.Key("decls");
        auto decls = MakeJsonArray(&writer);
        std::set<Id> deduper;

        for (const auto& objdata : objdatas) {
          const auto* objprops = objdata.second;
          for (const auto& provider : objprops->providers)
            if ((!(provider.flags & Properties::kFlagDefinition)) &&
                deduper.find(provider.location) == deduper.end()) {
              OutputProvider(objname, provider);
              deduper.insert(provider.location);
            }
        }
      }

      {
        writer.Key("users");
        auto decls = MakeJsonArray(&writer);
        std::set<Id> deduper;

        for (const auto& objdata : objdatas) {
          const auto* objprops = objdata.second;
          for (const auto& user : objprops->users)
            if (deduper.find(user.location) == deduper.end()) {
              OutputUser(objname, user);
              deduper.insert(user.location);
            }
        }
      }
    }
  }
}

void Indexer::OutputBinaryIndex(const char* path, const char* tag) {
  struct LinkageKind {
    bool operator<(const LinkageKind& other) const {
      if (kind != other.kind) return kind < other.kind;
      if (linkage != other.linkage) return linkage < other.linkage;
      return access < other.access;
    }

    IndexString kind;
    Linkage linkage;
    uint8_t access;
  };

  struct Symbol {
    uint64_t score = 0ULL;
    std::map<LinkageKind, std::map<const Id, const Properties*>> kinds;
  };

  std::map<NameString, Symbol> locations;
  std::map<FileRenderer::ParsedFile*, FileOffsetT> allfiles;

  if (!MakeAllDirs(path, 0777)) {
    std::cerr << "FAILED TO MAKE INDEX PATH '" << path << "'" << std::endl;
    return;
  }

  // 1) Re-index objects by name rather than unique identifier.
  for (const auto& objit : index_) {
    const auto& objid = objit.first;
    const auto& objdata = objit.second;

    for (const auto& provider : objdata.providers) {
      LinkageKind lk = {provider.kind, provider.linkage, provider.access};

      allfiles.insert(std::make_pair(provider.location.file, 0));
      auto& symbol = locations[provider.name];
      symbol.kinds[lk][objid] = &objdata;
    }
  }
  // 2) Compute score for each object. This is done as a second pass
  // to avoid holding onto many sets / maps for a long time.
  for (auto& symbolit : locations) {
    std::set<FileRenderer::ParsedFile*> files;
    uint32_t appearances = 0;

    auto& symbol = symbolit.second;
    for (const auto& kindit : symbol.kinds) {
      for (const auto& propertiesit : kindit.second) {
        auto& properties = propertiesit.second;
        for (const auto& user : properties->users) {
          files.insert(user.location.file);
          appearances += 1;
        }
      }
    }
    symbol.score = (files.size() << 32) + appearances;
    for (const auto& file : files) allfiles.insert(std::make_pair(file, 0));
  }
  // 3) Sort symbols by score, and output them.
  using SymbolPair = std::pair<NameString, Symbol>;
  std::vector<SymbolPair> symbols;
  symbols.reserve(locations.size());
  for (const auto& locationsit : locations)
    symbols.emplace_back(
        std::make_pair(locationsit.first, std::move(locationsit.second)));
  locations.clear();

  std::sort(
      symbols.begin(), symbols.end(),
      [&locations](const SymbolPair& first, const SymbolPair& second) -> bool {
        // Shortest symbols first, no matter what.
        if (first.first.size() != second.first.size())
          return first.first.size() < second.first.size();
        // Within symbols with the same length, higher scored ones first.
        if (first.second.score != second.second.score)
          return first.second.score < second.second.score;
        // If everything else is equal, sort by name.
        return first.first < second.first;
      });

  std::string basename = tag ? std::string("index.") + tag : "index";

  // Output list of files.
  {
    std::ofstream ffile;
    const auto& filesfile = JoinPath({path, basename + ".files"});
    ffile.open(filesfile, std::ofstream::out | std::ofstream::trunc |
                              std::ofstream::binary);

    FileOffsetT offset = 0;
    for (auto& fileit : allfiles) {
      auto& fileptr = fileit.first;
      auto& fileoffset = fileit.second;

      fileoffset = offset;
      const auto& path = cache_->GetUserPath(fileptr->path);

      if (path.size() > std::numeric_limits<uint16_t>::max()) {
        std::cerr << "ERROR: Path " << path.str()
                  << " longer than uint16_t, cannot be added to index!\n";
        continue;
      }

      FileDetail detail = {fileptr->hash, static_cast<uint16_t>(path.size())};
      ffile.write(reinterpret_cast<const char*>(&detail), sizeof(detail));
      ffile.write(path.data(), path.size());

      offset += sizeof(detail) + path.size();
    }
  }

  std::vector<SymbolHashToDetails> hashtodetails;
  {
    std::ofstream symfile;
    const auto& symboldetailsfile =
        JoinPath({path, basename + ".symbol-details"});
    symfile.open(symboldetailsfile, std::ofstream::out | std::ofstream::trunc |
                                        std::ofstream::binary);

    std::ofstream detfile;
    const auto& detailsfile = JoinPath({path, basename + ".details"});
    detfile.open(detailsfile, std::ofstream::out | std::ofstream::trunc |
                                  std::ofstream::binary);

    NameOffsetT symboloff = 0;
    DetailOffsetT detailoff = 0;
    for (const auto& symbolit : symbols) {
      const auto& name = symbolit.first;
      const auto& symbol = symbolit.second;

      if (name.size() > std::numeric_limits<uint16_t>::max()) {
        std::cerr << "ERROR: Symbol " << name
                  << " longer than uint16_t, cannot be added to index!\n";
        continue;
      }
      if (symbol.kinds.size() > std::numeric_limits<uint16_t>::max()) {
        std::cerr
            << "ERROR: too many instantiations for " << name
            << ", overflows uint16_t counter, cannot be added to index!\n";
        continue;
      }
      uint64_t symbolhash = hash_value(StringRef(name.data(), name.size()));
      hashtodetails.emplace_back(SymbolHashToDetails{symbolhash, detailoff});

      SymbolNameToDetails symdata = {detailoff,
                                     static_cast<uint16_t>(name.size())};
      symfile.write((const char*)&symdata, sizeof(symdata));
      symfile.write(name.data(), name.size());

      SymbolDetail detdata = {symboloff, symbolhash,
                              static_cast<uint16_t>(symbol.kinds.size())};
      detfile.write((const char*)&detdata, sizeof(detdata));
      detailoff += sizeof(detdata);

      for (const auto& kindit : symbol.kinds) {
        const auto& linkkind = kindit.first;
        const auto& idprops = kindit.second;

        std::map<Id, const Properties::Provider*> defs;
        std::map<Id, const Properties::Provider*> decls;

        for (const auto& idprop : idprops) {
          // const auto& id = idprop.first;
          const auto& prop = idprop.second;

          for (const auto& provider : prop->providers) {
            if (provider.name != name) continue;

            if (provider.flags & Properties::kFlagDefinition) {
              defs.emplace(std::make_pair(provider.location, &provider));
            } else {
              decls.emplace(std::make_pair(provider.location, &provider));
            }
          }
        }

        // Produce set of decls and defs.
        auto defsize = defs.size();
        if (defsize > std::numeric_limits<uint16_t>::max()) {
          std::cerr << "ERROR: Symbol " << name
                    << " has too many definitions, would overflow uint16, "
                       "cannot be added to index!\n";
          defsize = 0;
        }
        auto declsize = decls.size();
        if (declsize > std::numeric_limits<uint16_t>::max()) {
          std::cerr << "ERROR: Symbol " << name
                    << " has too many declarations, would overflow uint16, "
                       "cannot be added to index!\n";
          declsize = 0;
        }

        SymbolDetailKind kinddata = {
            linkkind.kind.GetOffset(), linkkind.linkage, linkkind.access,
            static_cast<uint16_t>(defsize), static_cast<uint16_t>(declsize)};
        detfile.write((const char*)&kinddata, sizeof(kinddata));
        detailoff += sizeof(kinddata);

        auto OutputProvider = [&detailoff, &detfile, &allfiles](
                                  const Properties::Provider& provider) {
          const auto& fileit = allfiles.find(provider.location.file);
          FileOffsetT foffset = 0;
          if (fileit != allfiles.end()) {
            foffset = fileit->second;
          } else {
            std::cerr << "ERROR: File " << provider.location.file->path
                      << " could not be found in index, leaving 0 offset!\n";
          }
          SymbolDetailProvider towrite;
          towrite.fid = {provider.location.file->hash, foffset};
          towrite.sid = {provider.location.object.sl,
                         provider.location.object.el};
          towrite.snippet = provider.snippet.GetOffset();

          detfile.write((const char*)&towrite, sizeof(towrite));
          detailoff += sizeof(towrite);
        };

        if (defsize) {
          for (const auto& defit : defs) {
            const auto& provider = *defit.second;
            OutputProvider(provider);
          }
        }
        if (declsize) {
          for (const auto& declit : decls) {
            const auto& provider = *declit.second;
            OutputProvider(provider);
          }
        }
      }

      symboloff += sizeof(symdata) + name.size();
    }
  }

  {
    std::sort(hashtodetails.begin(), hashtodetails.end(),
              [](const SymbolHashToDetails& first,
                 const SymbolHashToDetails& second) -> bool {
                if (first.hash != second.hash) return first.hash < second.hash;
                return first.Detailoffset < second.Detailoffset;
              });

    std::ofstream hashfile;
    const auto& hashdetailsfile = JoinPath({path, basename + ".hash-details"});
    hashfile.open(hashdetailsfile, std::ofstream::out | std::ofstream::trunc |
                                       std::ofstream::binary);

    hashfile.write(reinterpret_cast<const char*>(hashtodetails.data()),
                   sizeof(SymbolHashToDetails) * hashtodetails.size());
  }

  // FIXME! TODO!
  const auto& iddetailsfile = JoinPath({path, basename + ".id-details"});

  // Now output:
  // - .snippet file, with snippets.
  const auto& snippetfile = JoinPath({path, basename + ".snippets"});
  OutputPool(snippetfile.c_str(), *SnippetString::GetPool());

  // - .strings file, with all other text.
  const auto& textfile = JoinPath({path, basename + ".strings"});
  OutputPool(textfile.c_str(), *IndexString::GetPool());

  // - .json file with integers instead of strings
  // Output this one last as the server uses its timestamp to determine
  // when to re-load the index.
  const auto& jsonfile = JoinPath({path, basename + ".symbols.json"});
  OutputJsonIndex(jsonfile.c_str());
}

ObjectId MakeObjectId(const SourceManager& sm, const SourceRange& location) {
  auto sb_line = sm.getSpellingLineNumber(location.getBegin());
  auto sb_column = sm.getSpellingColumnNumber(location.getBegin());
  auto se_line = sm.getSpellingLineNumber(location.getEnd());
  auto se_column = sm.getSpellingColumnNumber(location.getEnd());

  auto eb_line = sm.getExpansionLineNumber(location.getBegin());
  auto eb_column = sm.getExpansionColumnNumber(location.getBegin());
  auto ee_line = sm.getExpansionLineNumber(location.getEnd());
  auto ee_column = sm.getExpansionColumnNumber(location.getEnd());

  if (((ee_line & 0xfffff) == 0 && (ee_column & 0xfff) == 0) ||
      ((se_line & 0xfffff) == 0 && (se_column & 0xfff) == 0)) {
    std::cerr << "INVALID LINE / COLUMN: " << ee_line << " " << ee_column
              << std::endl;
    abort();
  }

  uint64_t ekey =
      static_cast<uint64_t>(eb_line & kLineMask) << kBeginLineShift |
      static_cast<uint64_t>(eb_column & kColumnMask) << kBeginColumnShift |
      static_cast<uint64_t>(ee_line & kLineMask) << kEndLineShift |
      static_cast<uint64_t>(ee_column & kColumnMask) << kEndColumnShift;

  uint64_t skey =
      static_cast<uint64_t>(sb_line & kLineMask) << kBeginLineShift |
      static_cast<uint64_t>(sb_column & kColumnMask) << kBeginColumnShift |
      static_cast<uint64_t>(se_line & kLineMask) << kEndLineShift |
      static_cast<uint64_t>(se_column & kColumnMask) << kEndColumnShift;
  return {skey, ekey};
}

std::string MakeIdName(const ObjectId& objid) {
  if (objid.sl == 0) return ToHex(objid.el);
  if (objid.sl == objid.el) return ToHex(objid.el);
  return (ToHex(objid.sl).operator StringRef() +
          ToHex(objid.el).operator StringRef())
      .str();
}

std::string MakeIdName(const SourceManager& sm, SourceRange location) {
  const auto& objid = MakeObjectId(sm, location);
  return MakeIdName(objid);
}
