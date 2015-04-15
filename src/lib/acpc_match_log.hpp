#pragma once

#include <exception>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <functional>
#include <thread>

extern "C" {
#include <print_debugger.h>
#include <game.h>
}

#include <lib/encapsulated_match_state.hpp>

namespace AcpcMatchLog {
namespace Utils {
#define DELETE_POINTER(ptr)                                                    \
  {                                                                            \
    if (ptr) {                                                                 \
      delete ptr;                                                              \
      ptr = nullptr;                                                           \
    }                                                                          \
  }
#define DELETE_ARRAY(array)                                                    \
  {                                                                            \
    if (array) {                                                               \
      delete[] array;                                                          \
      array = nullptr;                                                         \
    }                                                                          \
  }

template <class ElemSrc, class ElemDest, class MapFn>
void map(const std::vector<ElemSrc> &src, std::vector<ElemDest> &dest,
         MapFn mapFn) {
  dest.resize(src.size());
  transform(src.begin(), src.end(), dest.begin(), mapFn);
}

template <class SrcArrayType, class DestArrayType, class ArraySize, class MapFn>
void map(const SrcArrayType *src, ArraySize srcArraySize, DestArrayType *dest,
         MapFn mapFn) {
  assert(src);
  assert(dest);

  for (size_t i = 0; i < srcArraySize; ++i) {
    dest[i] = mapFn(src[i]);
  }
}

template <class DestArrayType, class SrcArrayType, class ArraySize, class MapFn,
          class DoFn>
auto mapDo(const SrcArrayType *src, ArraySize srcArraySize, MapFn mapFn,
           DoFn doFn) {
  DestArrayType dest[srcArraySize];

  map(src, srcArraySize, dest, mapFn);

  return doFn(dest);
}

template <class SrcArrayType, class ResultType, class ArraySize, class ReduceFn>
ResultType reduce(const SrcArrayType *src, ArraySize srcArraySize,
                  ReduceFn reduceFn, ResultType result) {
  assert(src);

  for (size_t i = 0; i < srcArraySize; ++i) {
    result = reduceFn(result, src[i]);
  }
  return result;
}

template <class Integral, class ArraySize>
Integral sum(const Integral *src, ArraySize srcArraySize) {
  return reduce(src, srcArraySize, [](Integral count, Integral numBc) {
    return count + numBc;
  }, Integral(0));
}

class File {
public:
  File(const std::string &name) : name_(name) {}
  virtual ~File(){};

  virtual void
  open(std::function<void(const File &f, std::ifstream &stream)> doFn) const {
    std::ifstream stream;
    stream.open(name_, std::ifstream::in);
    if (!stream.is_open()) {
      throw std::invalid_argument("Unable to open log file \"" + name_ + "\"");
    }

    doFn((*this), stream);

    stream.close();
  }
  const std::string &name() const { return name_; }

protected:
  const std::string name_;
};
}

class LogFile : public Utils::File {
public:
  LogFile(const std::string &name, const Acpc::GameDef &gameDef)
      : Utils::File(name), gameDef_(gameDef) {}
  virtual ~LogFile(){};

  virtual void eachState(const std::function<
      bool(const Acpc::EncapsulatedMatchState &ms,
           const std::vector<std::string> playerNames)> &doFn) {
    open([&doFn, this](const File & /*f*/, std::ifstream &stream) {
      std::string line;
      while (stream.good()) {
        std::getline(stream, line);
        try {
          Acpc::EncapsulatedMatchState ms(line, gameDef_);
          std::vector<std::string> playerNames = players(line, gameDef_);
          if (doFn(ms, playerNames)) {
            break;
          };
        } catch (const std::runtime_error &e) {
          // Ignore comments
        }
      }
    });
  }

protected:
  const Acpc::GameDef &gameDef_;
};

class LogFileSet {
public:
  LogFileSet(const std::vector<std::string> &filePaths, const Acpc::GameDef &gameDef)
      : filePaths_(filePaths), gameDef_(gameDef) {}
  virtual ~LogFileSet() {}

  virtual void processFilesInParallel(const std::function<
      bool(const Acpc::EncapsulatedMatchState &ms,
           const std::vector<std::string> playerNames)> &doFn) {
    std::vector<std::thread> threads;
    for (auto &f : filePaths_) {
      threads.emplace_back(
          [&f, &doFn, this]() { LogFile(f, gameDef_).eachState(doFn); });
    }
    for (auto &t : threads) {
      t.join();
    }
  }

  virtual void processFiles(const std::function<
      bool(const Acpc::EncapsulatedMatchState &ms,
           const std::vector<std::string> playerNames)> &doFn) {
    for (auto &f : filePaths_) {
      LogFile(f, gameDef_).eachState(doFn);
    }
  }

protected:
  const std::vector<std::string> &filePaths_;
  const Acpc::GameDef &gameDef_;
};
}
