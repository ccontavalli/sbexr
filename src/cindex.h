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

#ifndef CINDEX_H
#define CINDEX_H

#include <stdint.h>

// File:
//  + .snippets - all snippets, mempool with <length><snippet> format.
//  + .strings - all random strings, mempool with <length><string> format.
//
//  + .symbol-details - all symbol names. Main struct is SymbolNameToDetails.
//    Sorted by shortest symbol first. Within shortest symbol, sorted by best
//    symbol.
//  + .hash-details - goes from a symbol hash to SymbolDetail. Main struct is
//  SymbolHashToDetails.
//    Sorted by hash number, to allow binsearching.
//
//  - .id-details - all symbol ids. Main struct is SymbolIdToDetails.
//    Sorted by symbol id, numerically.
//
//  + .details - all details of each symbol. Main struct SymbolDetail.
//  - .files - all files. Main struct FileDetail.
//
//  + .json - struct representing the object and hierarchy.
//
// Two main lookups for details:
//  1) from search box, user types a name. Unknown which instance
//     of that name he is referring to.
//     Name -> list of possibly definitions/declarations
//             (each definition/declaration tied to some code id)
//
//  2) from source code browser, when trying to gather details about
//     a symbol. In this case, we know what the symbol is referring to.
//

// Zero-sized arrays/flexible array are not allowed by C++ standard.
// gcc / clang both support this. If sbexr is ever qualified or built
// with a different compiler, this will need to be fixed.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

typedef uint32_t DetailOffsetT;
typedef uint32_t NameOffsetT;
typedef uint32_t KindOffsetT;
typedef uint32_t SnippetOffsetT;
typedef uint32_t FileOffsetT;

typedef struct {
  uint64_t hash;
  FileOffsetT pathoffset;
} FileId;

enum SidConstantsT {
  kLineMask = 0xfffff,
  kLineShift = 20,

  kColumnMask = 0xfff,
  kColumnShift = 12,

  kEndColumnShift = 0,
  kEndLineShift = 12,

  kBeginColumnShift = 32,
  kBeginLineShift = 44,
};

typedef struct {
  uint64_t sid;
  uint64_t eid;
} SymbolId;

// In .symbol-details file.
// Sorted by symbol relevance. Looked up by symbol name.
typedef struct {
  DetailOffsetT Detailoffset;

  uint16_t namesize;
  const char name[];
} SymbolNameToDetails;

// In .hash-details file.
// Sorted by hash value. Looked up by hash value.
typedef struct {
  uint64_t hash;
  DetailOffsetT Detailoffset;
} SymbolHashToDetails;

// In .id-details file.
// Sorted by filehash, sid, eid. Looked up by symbol::Id.
typedef struct {
  FileId fid;
  SymbolId sid;

  DetailOffsetT Detailoffset;
  // FIXME: which specific detail?
} SymbolIdToDetails;

// In .files file.
// Sorted by hash. Not normally looked up.
typedef struct {
  uint64_t filehash;

  uint16_t pathsize;
  const char path[];
} FileDetail;

typedef struct {
  SymbolId sid;
  FileId fid;
  SnippetOffsetT snippet;
} SymbolDetailProvider;

typedef struct {
  KindOffsetT name;
  uint8_t linkage;
  uint8_t access;

  uint16_t defsize;
  uint16_t declsize;

  const SymbolDetailProvider provider[];
} SymbolDetailKind;

// Main element in .details file.
typedef struct {
  NameOffsetT nameoffset;
  uint64_t hash;

  uint16_t kindsize;
  const SymbolDetailKind kind[];
} SymbolDetail;

#pragma GCC diagnostic push

#endif /* CINDEX_H */
