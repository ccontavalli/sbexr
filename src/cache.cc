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

#include "cache.h"

Counter& c_begin_end_different_files =
    MakeCounter("cache/nullreturn/begin-end-different-files",
                "Returned a nullptr because a source and dest location were in "
                "different files");
Counter& c_internal_buffer = MakeCounter(
    "cache/nullreturn/internal-buffer",
    "Returned a nullptr because a location referred to an internal buffer");
Counter& c_no_content_cache =
    MakeCounter("cache/nullreturn/no-content-cache",
                "Returned a nullptr because the SourceManager had no content "
                "associated to the file");
Counter& c_no_sloc_entry =
    MakeCounter("cache/nullreturn/no-sloc-entry",
                "Returned a nullptr because the SourceManager had no SLocEntry "
                "associated to the file");
Counter& c_empty_path = MakeCounter(
    "cache/nullreturn/empty-path",
    "Returned a nullptr because an empty path was passed to GetFileFor");
Counter& c_invalid_fid = MakeCounter(
    "cache/nullreturn/invalid-fid",
    "Returned a nullptr because an invalid FileID was passed to GetFileFor");
