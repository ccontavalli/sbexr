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

#include "wrapping.h"

void WrapWithTag(FileRenderer::ParsedFile* file, Tag tag) {
  file->rewriter.Add(std::move(tag));
}

void WrapWithTag(FileRenderer::ParsedFile* file, int bo, int eo, Tag tag) {
  tag.open = bo;
  tag.close = eo;
  WrapWithTag(file, std::move(tag));
}

bool WrapWithTag(const CompilerInstance& ci, FileCache* cache,
                 const SourceLocation& obegin, const SourceLocation& oend,
                 Tag tag) {
  SourceManager& sm = ci.getSourceManager();
  if (obegin.isMacroID() || oend.isMacroID()) return false;

  auto begin = sm.getExpansionLoc(obegin);
  auto end = sm.getExpansionLoc(oend);

  auto* file = cache->GetFileFor(sm, begin, end);
  if (!file) return false;

  unsigned bo = sm.getFileOffset(begin);
  unsigned eo = sm.getFileOffset(end);

  // Include the whole end token in the range.
  // This was taken from the WrapRange implementation in clang.
  eo += Lexer::MeasureTokenLength(end, sm, ci.getLangOpts());

  WrapWithTag(file, bo, eo, std::move(tag));
  return true;
}

bool WrapWithTag(const CompilerInstance& ci, FileCache* cache,
                 const SourceRange& to_wrap, Tag tag) {
  return WrapWithTag(ci, cache, to_wrap.getBegin(), to_wrap.getEnd(),
                     std::move(tag));
}

bool WrapEolSol(const SourceManager& sm, FileCache* cache, SourceLocation start,
                SourceLocation end, Tag tag) {
  auto* file = cache->GetFileFor(sm, start, end);
  if (!file) return false;

  // FIXME FIXME FIXME: don't use sm.getBufferData(), get the data from the
  // cache.
  bool invalid = false;
  const auto& buffer = sm.getBufferData(sm.getFileID(start), &invalid);
  const char* data = buffer.data();
  const auto size = buffer.size();
  assert(!invalid);

  // Find end of line, take \\ in consideration.
  auto start_offset = sm.getFileOffset(start);
  while (start_offset < size) {
    if ((data[start_offset] == '\n' || data[start_offset] == '\r') &&
        (start_offset == 0 || data[start_offset - 1] != '\\'))
      break;
    ++start_offset;
  }

  // Find start of line, by looking for previous \n.
  auto end_offset = sm.getFileOffset(end);
  for (; end_offset > 0 && data[end_offset] != '\n' && data[end_offset] != '\r';
       --end_offset)
    ;

  WrapWithTag(file, start_offset, end_offset, std::move(tag));
  return true;
}

bool WrapEolSol(const CompilerInstance& ci, FileCache* cache,
                SourceLocation start, SourceLocation end, Tag tag) {
  return WrapEolSol(ci.getSourceManager(), cache, start, end, std::move(tag));
}
