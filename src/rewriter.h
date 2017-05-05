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

#ifndef REWRITER_H
#define REWRITER_H

#include "common.h"
#include "mempool.h"

#include <map>
#include <set>
#include <string>

extern const char _kTagString[];
using TagString = UniqString<uint32_t, _kTagString>;
static_assert(sizeof(TagString) == sizeof(uint32_t),
              "String is using more space than expected");

struct Tag {
  Tag(Tag&& other) = default;
  Tag(const Tag& other) = default;

  Tag(const char* tag, const std::string& attrs)
    : tag(tag), attributes(attrs) {}

  void Drop() { attributes.Drop(); }
  bool operator==(const Tag& other) {
    return (tag == other.tag || !strcmp(tag, other.tag)) &&
           open == other.open && close == other.close &&
           attributes == other.attributes;
  }

  const char* tag = nullptr;
  int open = -1;
  int close = -1;

  TagString attributes;
};

inline Tag MakeTag(const char* tag, std::initializer_list<StringRef> classes,
                   std::initializer_list<StringRef> attributes) {
  std::string elements;
  if (classes.size()) {
    elements.append("class='");
    int i = 0;
    for (const auto& aclass : classes) {
      if (i++) elements.append(" ");
      elements.append(aclass.data(), aclass.size());
    }

    if (attributes.size())
      elements.append("' ");
    else
      elements.append("'");
  }

  int i = 0;
  for (const auto& attr : attributes) {
    if (i % 2) {
      elements.append("='");
      elements.append(attr);
      elements.append("'");
    } else {
      if (i) elements.append(" ");
      elements.append(attr);
    }
    ++i;
  }

  auto rtag = Tag(tag, elements);
  return std::move(rtag);
}

class HtmlRewriter {
 public:
  void Add(Tag tag);
  std::string Generate(const StringRef& filename, const StringRef& body);

 private:
  std::vector<Tag> tags_;
};

#endif /* REWRITER_H */
