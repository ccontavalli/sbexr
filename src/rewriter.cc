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

#include "rewriter.h"
#include <iostream>

const char _kTagString[] = "Tag";

struct TagSet {
  // HTML TAGS need to be nested correctly, for example:
  //   <a><span></span></a>
  // we need to open first the tags that are closed the latest,
  // and close first the tags that were opened the latest.

  // Lists the tags to open, sorted by the largest closing offset first.
  std::multimap<ssize_t, Tag*, std::greater<ssize_t>> opens;

  // Lists the tags to close, sorted by the largest opening offset first.
  // Additionally, closes for opens in the same position should preserve
  // the correct order.
  std::multimap<ssize_t, Tag*, std::greater<ssize_t>> closes;
};

// Key is the offset, value is a set of tags to open or close at that offset.
using TagSetsMap = std::map<ssize_t, std::unique_ptr<TagSet>>;

std::unique_ptr<TagSet>& GetTagset(TagSetsMap* ts, ssize_t position);
bool AddOpen(TagSetsMap* ts, Tag* tag);
bool AddClose(TagSetsMap* ts, Tag* tag, int order);

uint64_t gl_bytes_wasted_on_duplication = 0;

void HtmlRewriter::Add(Tag tag) { tags_.emplace_back(std::move(tag)); }

std::unique_ptr<TagSet>& GetTagset(TagSetsMap* ts, ssize_t position) {
  auto& tagset = (*ts)[position];
  if (tagset == nullptr) tagset = std::move(llvm::make_unique<TagSet>());
  return tagset;
}

bool AddOpen(TagSetsMap* ts, Tag* tag) {
  if (tag->open < 0) return false;

  auto& tagset = GetTagset(ts, tag->open);
  auto it = tagset->opens.lower_bound(tag->close);
  for (; it != tagset->opens.end(); ++it) {
    if (it->first != tag->close) break;
    if (*it->second == *tag) {
      // gl_bytes_wasted_on_duplication += to_print.size();
      // std::cerr << "AVOIDING DUP TAG " << filename.str() << " " << to_print
      //           << " offset " << retval.size() << " wasted "
      //           << GetHumanValue(gl_bytes_wasted_on_duplication) <<
      //           std::endl;
      return false;
    }
  }

  tagset->opens.emplace_hint(it, std::make_pair(tag->close, tag));
  return true;
}

bool AddClose(TagSetsMap* ts, Tag* tag, int order) {
  if (tag->close < 0) return false;

  auto& tagset = GetTagset(ts, tag->close);
  tagset->closes.emplace(std::make_pair((tag->open << 10) + order, tag));
  return true;
}

std::string HtmlRewriter::Generate(const StringRef& filename,
                                   const StringRef& body) {
  TagSetsMap tagsets;
  for (auto& tag : tags_) AddOpen(&tagsets, &tag);

  const char* data = body.data();
  const auto size = body.size();

  std::string retval;
  auto CloseTag = [&retval](const Tag& tag) {
    retval.append("</");
    retval.append(tag.tag);
    retval.append(">");
  };

  int pos = 0, start = 0;
  for (const auto& it : tagsets) {
    auto nextstop = std::min<ssize_t>(size, it.first);
    const auto& tagset = it.second;

    for (; pos < nextstop; ++pos) {
      if (data[pos] == '&' || data[pos] == '<' || data[pos] == '>') {
        retval.append(data + start, pos - start);
        start = pos + 1;

        switch (data[pos]) {
          case '&':
            retval.append("&amp;");
            break;
          case '<':
            retval.append("&lt;");
            break;
          case '>':
            retval.append("&gt;");
            break;
        }
      }
    }

    retval.append(data + start, pos - start);
    start = pos;

    for (const auto& ait : tagset->closes) {
      const auto& tag = ait.second;
      if (tag == nullptr) continue;
      CloseTag(*tag);
    }

    int order = 0;
    for (auto& ait : tagset->opens) {
      std::string to_print;

      auto& tag = ait.second;
      if (tag == nullptr) continue;

      ++order;
      to_print.append("<");
      to_print.append(tag->tag);
      if (!tag->attributes.empty()) {
        to_print.append(" ");
        to_print.append(tag->attributes.data(), tag->attributes.size());
      }
      to_print.append(">");

      if (tag->close >= 0) {
        if (pos >= tag->close) {
          CloseTag(*tag);
        } else {
          AddClose(&tagsets, tag, order);
        }
      }

      retval.append(to_print);
    }
  }

  // Ensure deletion / cleaning the vector.
  std::vector<Tag>().swap(tags_);
  retval.shrink_to_fit();
  return retval;
}
