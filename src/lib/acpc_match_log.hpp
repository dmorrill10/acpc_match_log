#pragma once

#include <exception>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include <vector>
#include <functional>
#include <thread>

extern "C" {
#include <print_debugger.h>
#include <game.h>
}

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
};

class LogFile {
public:
  LogFile(const std::string &name) : name_(name) {}

  void open(std::function<void(std::ifstream &)> doFn) const {
    std::ifstream stream;
    stream.open(name_, std::ifstream::in);
    if (!stream.is_open()) {
      throw invalid_argument("Unable to open log file \"" + name_ + "\"");
    }

    doFn(stream);

    stream.close();
  }

//  [this](const LogFile& /*log*/, ifstream& logStream, ofstream& featuresStream, ofstream& actionsStream) {
//          assert(logStream.is_open());
//          assert(featuresStream.is_open());
//
//          string line;
//          std::getline(logStream, line);
//
//          vector<string> parts;
//          boost::split(parts, line, boost::is_any_of(" "));
//   assert(parts.size() > 3);
//
//  path gameDefPath = BIN_DIR / "../vendor/game_defs" / path(parts[3]).leaf();
//  GameDef gameDef(gameDefPath.string());
//
//  while (logStream.good()) {
//    std::getline(logStream, line);
//
//    if (regex_match(line, COMMENT_PATTERN)) { continue; }
//
//    const Hand hand(line, gameDef);
//
//    for (auto& player : playerNames_) {
//      if (hand.playerParticipated(player)) {
//        hand.replay(player, [&featuresStream, &actionsStream](const StateWithHistory& sWithHistory) {
//          featuresStream << sWithHistory.featuresToTextRow() << endl;
//          actionsStream << sWithHistory.currentState().action_ << endl;
//          return false;
//        });
//      }
//    }

  const std::string &name() const { return name_; }

private:
  const std::string name_;
};

class LogFileSet {
public:
  LogFileSet(std::vector<std::string>&& filePaths):logFiles_(filePaths) {}

  void processLogsInParallel(
      std::function<void(const LogFile&, std::ifstream&)> doOnLogFn
  ) {
    std::vector<std::thread> threads;
    for (auto& log : logFiles_) {
      threads.emplace_back([&log, &doOnLogFn]() {
        log.open(doOnLogFn);
      });
    }
    for (auto& t : threads) { t.join(); }
  }
private:
  const std::vector<std::string>& logFiles_;
};

}

