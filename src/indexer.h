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

#ifndef INDEX_H
#define INDEX_H

#include "base.h"
#include "cache.h"
#include "cindex.h"
#include "mempool.h"
#include "renderer.h"

#include <sparsehash/sparse_hash_map>

extern const char _kIndexString[];
using IndexString = UniqString<uint32_t, _kIndexString>;
static_assert(sizeof(IndexString) == sizeof(uint32_t),
              "String is using more space than expected");

extern const char _kSnippetString[];
using SnippetString = UniqString<uint32_t, _kSnippetString>;
static_assert(sizeof(SnippetString) == sizeof(uint32_t),
              "String is using more space than expected");

extern const char _kNameString[];
using NameString = UniqString<uint32_t, _kNameString>;
static_assert(sizeof(NameString) == sizeof(uint32_t),
              "String is using more space than expected");

struct ObjectId {
  uint64_t sl;
  uint64_t el;

  bool operator==(const ObjectId& other) const {
    return sl == other.sl && el == other.el;
  }
  bool operator<(const ObjectId& other) const {
    if (sl == other.sl) return el < other.el;
    return sl < other.sl;
  }
};
ObjectId MakeObjectId(const SourceManager& sm, const SourceRange& location);

static inline std::ostream& operator<<(std::ostream& stream,
                                       const ObjectId& objid) {
  stream << objid.sl;
  stream << objid.el;
  return stream;
}

class Indexer {
 public:
  Indexer(FileCache* cache) : cache_(cache) {}

  struct Id {
    Id() = default;
    Id(FileCache* cache, const SourceManager& sm, const SourceRange& target);
    bool operator<(const Id& other) const;
    bool operator==(const Id& other) const;

    FileRenderer::ParsedFile* file = nullptr;
    ObjectId object{0, 0};
  };
  struct IdHasher {
    std::size_t operator()(const Indexer::Id& id) const {
      return id.file->hash ^ id.object.sl ^ (id.object.el << 9);
    }
  };

  struct Properties {
    struct User {
      User(const Id& location) : location(location) {}
      // Something like: /usr/lib.../:12 -> full path, and line #.
      Id location;
    };

    enum {
      kFlagNone = 0,
      // If set, it means that the provider is giving a definition.
      kFlagDefinition = 1 << 0,
    };
    struct Provider {
      Provider(uint32_t flags, const Id& location, const std::string& name,
               const std::string& snippet, const std::string& kind,
               const uint8_t access, const Linkage& linkage)
          : location(location),
            name(name),
            kind(kind),
            snippet(snippet),
            access(access),
            flags(flags),
            linkage(linkage) {}
      Id location;

      NameString name;
      IndexString kind;

      SnippetString snippet;

      uint8_t access = 255;
      uint8_t flags = 0;
      Linkage linkage;
    };

    // Think about a macro like:
    //   #define foo struct foo { struct bar { int values[2]; } } }
    // Every time 'foo' is used, on the same line at the same position we'll
    // have multiple definitions.

    // When can something have multiple names?
    // Examples:
    //   - inner data type / method of inherited class, for example:
    //     class foo {
    //       struct bar {};
    //     }
    //     class baz : foo {
    //     }
    //
    //     struct 'bar' can now be known as baz::bar or foo::bar. It is the
    //     same struct.

    std::vector<User> users;
    std::vector<Provider> providers;
    std::vector<std::string> exceptions;
  };

  void RecordUse(const SourceManager& sm, const clang::SourceRange& target,
                 const clang::SourceRange& user, const char* description);

  void RecordDeclares(const SourceManager& sm,
                      const clang::SourceRange& declared,
                      const clang::SourceRange& declarer, const char* kind,
                      const std::string& name, const StringRef& snippet,
                      AccessSpecifier access, clang::Linkage linkage);
  void RecordDefines(const SourceManager& sm, const clang::SourceRange& defined,
                     const clang::SourceRange& definer, const char* kind,
                     const std::string& name, const StringRef& snippet,
                     AccessSpecifier access, clang::Linkage linkage);
  void RecordException(const SourceManager& sm,
                       const clang::SourceRange& target,
                       const std::string& exception);

  void OutputTree();

  void OutputJsonIndex();
  void OutputJsonIndex(const char* path);

  void OutputBinaryIndex(const char* path, const char* name);

  void Clear() {
    google::sparse_hash_map<Id, Properties, IdHasher>().swap(index_);
    IndexString::Clear();
    SnippetString::Clear();
    NameString::Clear();
  }

 private:
  FileCache* cache_;

  // Maps an ObjectId to its properties.
  // Note that in a large project, there will be many symbols.
  // Keeping this map small is critical in terms of memory usage.
  google::sparse_hash_map<Id, Properties, IdHasher> index_;
  StlSizePrinter<decltype(index_)> printer_{&index_, "Index"};
};

static inline std::ostream& operator<<(std::ostream& stream,
                                       const Indexer::Id& location) {
  stream << location.file->path;
  const uint64_t el = location.object.el;

  stream << ":" << std::to_string((el >> kBeginLineShift) & kLineMask) << ":"
         << std::to_string((el >> kBeginColumnShift) & kColumnMask) << "-"
         << std::to_string((el >> kEndLineShift) & kLineMask) << ":"
         << std::to_string((el >> kEndColumnShift) & kColumnMask);
  return stream;
}

std::string MakeIdName(const ObjectId& objid);
std::string MakeIdName(const SourceManager& sm, SourceRange location);

std::string ObjIdToLink(const Indexer::Id& id);
std::string MakeIdLink(const SourceManager& sm, FileRenderer* renderer,
                       SourceRange location);

#endif /* INDEX_H */
