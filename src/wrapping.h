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

#ifndef WRAPPING_H
#define WRAPPING_H

#include "base.h"
#include "cache.h"
#include "common.h"
#include "renderer.h"

extern void WrapWithTag(FileRenderer::ParsedFile* file, Tag tag);
extern void WrapWithTag(FileRenderer::ParsedFile* file, int bo, int eo,
                        Tag tag);

extern bool WrapWithTag(const CompilerInstance& ci, FileCache* cache,
                        const SourceLocation& obegin,
                        const SourceLocation& oend, Tag tag);
extern bool WrapWithTag(const CompilerInstance& ci, FileCache* cache,
                        const SourceRange& to_wrap, Tag tag);

extern bool WrapEolSol(const SourceManager& sm, FileCache* cache,
                       SourceLocation start, SourceLocation end, Tag tag);
extern bool WrapEolSol(const CompilerInstance& ci, FileCache* cache,
                       SourceLocation start, SourceLocation end, Tag tag);

#endif /* WRAPPING_H */
