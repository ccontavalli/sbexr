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

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include "base.h"
#include "common.h"

#include <sparsehash/sparse_hash_set>
#include <string>
#include <vector>

class MemoryPrinter {
 public:
  MemoryPrinter(const std::string& name, std::function<void()> function) {
    if (!printers_) printers_ = new PrinterList();
    auto result = printers_->emplace(std::make_pair(name, std::move(function)));
    it_ = result.first;
  }
  ~MemoryPrinter() {
    if (it_ != printers_->end()) printers_->erase(it_);
    it_ = printers_->end();
  }

  MemoryPrinter(const MemoryPrinter& other) = delete;
  MemoryPrinter& operator=(const MemoryPrinter& other) = delete;

  MemoryPrinter(MemoryPrinter&& other) { *this = std::move(other); }
  MemoryPrinter& operator=(MemoryPrinter&& other) {
    if (it_ != printers_->end()) printers_->erase(it_);
    it_ = other.it_;
    other.it_ = printers_->end();
    return *this;
  }

  static void OutputStats();

 private:
  using PrinterList = std::map<std::string, std::function<void()>>;
  static PrinterList* printers_;
  PrinterList::iterator it_;
};

template <typename T>
class StlSizePrinter : public MemoryPrinter {
 public:
  StlSizePrinter(const T* obj, const char* name)
      : MemoryPrinter(name, [obj]() { Print(obj); }) {}
  static void Print(const T* obj) {
    std::cerr << "size " << GetSuffixedValueIS(obj->size()) << " ("
              << obj->size() << ") ";
  }
};

template <typename T>
class StlCapacityPrinter : public MemoryPrinter {
 public:
  StlCapacityPrinter(const T* obj, const char* name)
      : MemoryPrinter(name, [obj]() { Print(obj); }) {}

  static void Print(const T* obj) {
    StlSizePrinter<T>::Print(obj);
    std::cerr << "capacity " << GetSuffixedValueBytes(obj->capacity()) << " ("
              << obj->capacity() << ")";
  }
};

template <typename ObjectT, typename OffsetT>
class MemPool {
 public:
  MemPool(const char* name)
      : printer_(std::string(name) + ":mempool", [this]() {
          StlCapacityPrinter<decltype(memory_)>::Print(&memory_);
          std::cerr << " entries " << GetSuffixedValueIS(elements_) << " ("
                    << elements_ << ")";
        }) {}

  OffsetT Allocate(OffsetT size) {
    ++elements_;
    OffsetT retval = memory_.size();
    memory_.resize(size + memory_.size());
    return retval;
  }

  ObjectT* Get(OffsetT offset) { return &memory_[offset]; }

  bool Return(OffsetT offset, OffsetT size) {
    if (offset + size == memory_.size()) {
      memory_.resize(offset);
      return true;
    }
    return false;
  }

  void Clear() {
    std::vector<ObjectT>().swap(memory_);
    elements_ = 0;
  }

  std::vector<ObjectT> GetStorage() const { return memory_; }

 private:
  std::vector<ObjectT> memory_;
  OffsetT elements_ = 0;

  MemoryPrinter printer_;
};

template <typename Derived, typename OffsetT, const char* Instance = -1>
class ConstStringBase {
 public:
  using Type = ConstStringBase<Derived, OffsetT, Instance>;
  using Pool = MemPool<char, OffsetT>;

  ConstStringBase() {}

  ConstStringBase(const Type& other) { offset_ = other.offset_; }
  Type& operator=(const Type& other) {
    offset_ = other.offset_;
    return *this;
  }

  bool operator==(const Type& other) const {
    return offset_ == other.offset_ ||
           (size() == other.size() && !memcmp(data(), other.data(), size()));
  }
  bool operator!=(const Type& other) const { return !(*this == other); }
  bool operator<(const Type& other) const {
    auto result = memcmp(data(), other.data(), std::min(size(), other.size()));
    if (result < 0) return true;
    if (result > 0) return false;
    return size() < other.size() ? true : false;
  }

  ConstStringBase(const char* data, OffsetT size) {
    static_cast<Derived*>(this)->Create(data, size);
  }
  explicit ConstStringBase(const char* str) {
    static_cast<Derived*>(this)->Create(str, strlen(str));
  }
  explicit ConstStringBase(const std::string& str) {
    static_cast<Derived*>(this)->Create(str.c_str(),
                                        static_cast<OffsetT>(str.size()));
  }
  explicit ConstStringBase(const StringRef& str) {
    static_cast<Derived*>(this)->Create(str.data(),
                                        static_cast<OffsetT>(str.size()));
  }

  bool Drop() { return GetPool()->Return(offset_, size() + sizeof(OffsetT)); }

  const char* data() const { return GetPool()->Get(offset_) + sizeof(OffsetT); }
  OffsetT size() const {
    return *reinterpret_cast<OffsetT*>(GetPool()->Get(offset_));
  }
  bool empty() const { return !size(); }

  static Pool* GetPool() { return Derived::GetPool(); }
  OffsetT GetOffset() const { return offset_; }

 protected:
  void Create(const char* str, OffsetT size) {
    offset_ = GetPool()->Allocate(size + sizeof(OffsetT));

    auto* ptr = GetPool()->Get(offset_);
    memcpy(ptr, &size, sizeof(OffsetT));
    memcpy(ptr + sizeof(OffsetT), str, size);
  }

  OffsetT offset_ = 0;
};

template <typename Derived, typename OffsetT, const char* Instance = -1>
bool operator==(const ConstStringBase<Derived, OffsetT, Instance>& first,
                const std::string& second) {
  return first.size() == second.size() &&
         !memcmp(first.data(), second.data(), first.size());
}
template <typename Derived, typename OffsetT, const char* Instance = -1>
bool operator==(const std::string& first,
                const ConstStringBase<Derived, OffsetT, Instance>& second) {
  return first.size() == second.size() &&
         !memcmp(first.data(), second.data(), first.size());
}
template <typename Derived, typename OffsetT, const char* Instance = -1>
bool operator!=(const ConstStringBase<Derived, OffsetT, Instance>& first,
                const std::string& second) {
  return first.size() != second.size() ||
         memcmp(first.data(), second.data(), first.size());
}
template <typename Derived, typename OffsetT, const char* Instance = -1>
bool operator!=(const std::string& first,
                const ConstStringBase<Derived, OffsetT, Instance>& second) {
  return first.size() != second.size() ||
         memcmp(first.data(), second.data(), first.size());
}

template <typename Derived, typename OffsetT, const char* Instance>
struct ConstStringBaseHasher {
  std::size_t operator()(
      const ConstStringBase<Derived, OffsetT, Instance>& base) const {
    const char* ptr = base.data();
    const char* end = base.data() + base.size();

    size_t hash = 0xcbf29ce484222325;
    for (; ptr < end; ++ptr) {
      hash = hash * 0x100000001b3;
      hash = hash ^ *ptr;
    }
    return hash;
  }
};

template <typename OffsetT, const char* Instance = -1>
class ConstString : public ConstStringBase<ConstString<OffsetT, Instance>,
                                           OffsetT, Instance> {
 public:
  using Base =
      ConstStringBase<ConstString<OffsetT, Instance>, OffsetT, Instance>;

  using Base::ConstStringBase;
  using typename Base::Pool;

  static Pool* GetPool() { return &pool_; }

 protected:
  static Pool pool_;
};

template <typename OffsetT, const char* Instance>
typename ConstString<OffsetT, Instance>::Pool
    ConstString<OffsetT, Instance>::pool_(Instance);

template <typename Base, typename OffsetT, const char* Instance>
struct Deduper {
  Deduper() {}

  using Map =
      google::sparse_hash_set<Base,
                              ConstStringBaseHasher<Base, OffsetT, Instance>>;
  Map table;

  void Clear() {
    saved_bytes = 0;
    saved_strings = 0;
    table.clear();
  }

  uint64_t saved_bytes = 0;
  uint64_t saved_strings = 0;
  MemoryPrinter printer{std::string(Instance) + ":deduper", [this]() {
                          StlSizePrinter<Map>::Print(&table);
                          std::cerr << "savings "
                                    << GetSuffixedValueBytes(saved_bytes)
                                    << " (" << saved_bytes << ") entries "
                                    << GetSuffixedValueIS(saved_strings) << " ("
                                    << saved_strings << ")";
                        }};
};

template <typename OffsetT, const char* Instance = -1>
class UniqString
    : public ConstStringBase<UniqString<OffsetT, Instance>, OffsetT, Instance> {
 public:
  using Base =
      ConstStringBase<UniqString<OffsetT, Instance>, OffsetT, Instance>;
  using String = ConstString<OffsetT, Instance>;

  using Base::ConstStringBase;

  static typename Base::Pool* GetPool() { return String::GetPool(); }

  static void Clear() {
    deduper_.Clear();
    GetPool()->Clear();
  }

 protected:
  friend Base;

  void Create(const char* data, OffsetT size) {
    String str(data, size);

    auto result = deduper_.table.insert(str);
    if (result.second) {
      this->offset_ = str.GetOffset();
    } else {
      str.Drop();

      deduper_.saved_bytes += size;
      deduper_.saved_strings += 1;
      this->offset_ = result.first->GetOffset();
    }
  }

 private:
  // Private cheap stuff.
  static Deduper<String, OffsetT, Instance> deduper_;
};

template <typename OffsetT, const char* Instance>
Deduper<typename UniqString<OffsetT, Instance>::String, OffsetT, Instance>
    UniqString<OffsetT, Instance>::deduper_;

template <typename Base, typename OffsetT, const char* Instance>
static inline std::ostream& operator<<(
    std::ostream& stream,
    const ConstStringBase<Base, OffsetT, Instance>& toprint) {
  stream.write(toprint.data(), toprint.size());
  return stream;
}

#endif /* MEMPOOL_H */
