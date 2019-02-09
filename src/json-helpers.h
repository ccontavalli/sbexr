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

#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace json = rapidjson;

// RAII Wrappers around some of the common rapidjson constructs.
template <typename Writer>
class JsonArray {
 public:
  JsonArray(Writer* writer, const char* key = nullptr) : writer_(writer) {
    if (key) writer->Key(key);
    writer->StartArray();
  }
  ~JsonArray() { writer_->EndArray(); }

 private:
  Writer* writer_;
};
template <typename Writer>
inline JsonArray<Writer> MakeJsonArray(Writer* writer,
                                       const char* key = nullptr) {
  return JsonArray<Writer>(writer, key);
}

template <typename Writer>
class JsonObject {
 public:
  JsonObject(Writer* writer, const char* key = nullptr) : writer_(writer) {
    if (key) writer->Key(key);
    writer->StartObject();
  }
  ~JsonObject() { writer_->EndObject(); }

 private:
  Writer* writer_;
};
template <typename Writer>
inline JsonObject<Writer> MakeJsonObject(Writer* writer,
                                         const char* key = nullptr) {
  return JsonObject<Writer>(writer, key);
}

template <typename Writer>
void WriteJsonString(Writer* writer, const std::string& str) {
  writer->String(str.c_str(), str.length());
}

template <typename Writer, typename Base, typename Offset, const char* Id>
void WriteJsonString(Writer* writer,
                     const ConstStringBase<Base, Offset, Id>& str) {
  writer->String(str.data(), str.size());
}

template <typename Writer>
void WriteJsonKeyValue(Writer* writer, const char* key,
                       const std::string& str) {
  writer->Key(key);
  writer->String(str.data(), str.size());
}

template <typename Writer>
void WriteJsonKeyValue(Writer* writer, const char* key, int value) {
  writer->Key(key);
  writer->Uint(value);
}

#endif /* JSON_HELPERS_H */
