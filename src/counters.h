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

#ifndef COUNTERS_H
#define COUNTERS_H

#include "common.h"

#include <map>
#include <ostream>
#include <string>

class DebugStream {
 public:
  explicit DebugStream(std::ostream& stream) : stream_(stream) {}
  ~DebugStream() { stream_ << "\n"; }

  DebugStream(const DebugStream& other) = delete;
  DebugStream& operator=(const DebugStream& other) = delete;

  DebugStream(DebugStream&& other) : stream_(other.stream_) {}
  // DebugStream& operator=(DebugStream&& other) { stream_ = std::move(other.stream_); }

  std::ostream& GetStream() const { return stream_; }

 private:
  std::ostream& stream_;
};

// Allows a DebugStream to be used as a plain stream, with the usual << fun. 
template<typename Whatever>
std::ostream& operator<<(DebugStream&& dstream, Whatever value) {
  dstream.GetStream() << value;
  return dstream.GetStream();
}

class Counter {
 public:
  Counter(const char* name, const char* description, std::ostream* capture)
      : name_(name), description_(description), capture_(capture) {}

  DebugStream Add();
  DebugStream Add(SourceRange range);
  DebugStream Add(SourceLocation begin, SourceLocation end);

  void Capture(std::ostream* capture);

  uint64_t Value() const { return counter_; }

 private:
  const std::string name_;
  const std::string description_;
  std::ostream* capture_;
  uint64_t counter_ = 0;
};

class Register {
 public:
  typedef std::map<std::string, Counter> CounterMap;

  Counter& MakeCounter(const char* path, const char* description);
  const CounterMap& GetCounters() const { return counters_; }
  int Capture(const std::string& match, std::ostream* stream);

  bool OutputJson(const std::string& path) const;

 private:
  CounterMap counters_;
};

Register& GlobalRegister();
Counter& MakeCounter(const char* path, const char* description);

#endif
