#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>
#include <bitset>
#include <random>

extern "C" {
#include <game.h>
#include <print_debugger.h>

#include <dealer.h>
#include <stdlib.h>
#include <stdio.h>
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net.h>
}


namespace AcpcMatchLog {

#define FREE_POINTER(ptr) {\
    if (ptr) {    \
      free(static_cast<void*>(ptr)); \
      ptr = nullptr; \
    }             \
}
#define DELETE_POINTER(ptr) {\
    if (ptr) {    \
      delete ptr; \
      ptr = nullptr; \
    }             \
}
#define DELETE_ARRAY(array) {\
    if (array) {    \
      delete[] array; \
      array = nullptr; \
    }             \
}

namespace Acpc {
  // Thanks to hobbs
  // (http://stackoverflow.com/questions/4169981/logsumexp-implementation-in-c)
  // for this.
  template <typename List>
  double logsumexp(List nums) {
    double max_exp = nums[0], sum = 0.0;
    size_t i;

    for (i = 1 ; i < nums.size() ; i++) {
      if (nums[i] > max_exp) {
        max_exp = nums[i];
      }
    }

    for (i = 0; i < nums.size() ; i++) {
      sum += exp(nums[i] - max_exp);
    }

    return log(sum) + max_exp;
  }

  template<class ElemSrc, class ElemDest, class MapFn>
  void map(const std::vector<ElemSrc>& src, std::vector<ElemDest>& dest, MapFn mapFn) {
    dest.resize(src.size());
    transform(src.begin(), src.end(), dest.begin(), mapFn);
  }

  template<class SrcArrayType, class ResultType, class ReduceFn>
  ResultType reduce(
      const SrcArrayType src,
      ReduceFn reduceFn,
      ResultType result
  ) {
    for (auto elem : src) {
      result = reduceFn(result, elem);
    }
    return result;
  }

  template<class SrcArrayType, class DestArrayType, class ArraySize, class MapFn>
  void map(
      const SrcArrayType* src,
      ArraySize srcArraySize,
      DestArrayType* dest,
      MapFn mapFn
  ) {
    assert(src);
    assert(dest);

    for (size_t i = 0; i < srcArraySize; ++i) {
      dest[i] = mapFn(src[i]);
    }
  }

  template<class SrcArrayType, class ResultType, class ArraySize, class ReduceFn>
  ResultType reduce(
      const SrcArrayType* src,
      ArraySize srcArraySize,
      ReduceFn reduceFn,
      ResultType result
  ) {
    assert(src);

    for (size_t i = 0; i < srcArraySize; ++i) {
      result = reduceFn(result, src[i]);
    }
    return result;
  }

  template<class Integral, class ArraySize>
  Integral sum(const Integral* src, ArraySize srcArraySize) {
    return reduce(src, srcArraySize, [](Integral count, Integral numBc) { return count + numBc; }, Integral(0));
  }

  template<class ArraySize>
  int32_t potSize(const MatchState& view, ArraySize numPlayers) {
    return potSize(view.state, numPlayers);
  }
  template<class ArraySize>
  int32_t potSize(const State& state, ArraySize numPlayers) {
    return sum(state.spent, numPlayers);
  }

  bool actionsEqual(const Action& a1, const Action& a2) {
    return a1.size == a2.size && a1.type == a2.type;
  }

  Game* gameDef(const std::string& game_def_file_name) {
    /* get the game definition */
    FILE* file = fopen(game_def_file_name.c_str(), "r" );
    if (!file) {
      throw std::runtime_error(std::string("Could not open game definition ") + game_def_file_name + "\n");
    }
    Game* game = readGame( file );
    if(!game) {
      fclose(file);
      throw std::runtime_error(std::string("Could not read game ") + game_def_file_name + "\n");
    }
    fclose(file);

    return game;
  }

  std::string stateToString(const State& state, const Game* game) {
    char line[MAX_LINE_LEN];
    printState(game, &state, MAX_LINE_LEN, line);
    return std::string(line);
  }

  std::string matchStateToString(const MatchState& state, const Game* game) {
    char line[MAX_LINE_LEN];
    printMatchState(game, &state, MAX_LINE_LEN, line);
    return std::string(line);
  }

  std::string actionToString(const Action& action, const Game* game) {
    char line[MAX_LINE_LEN];
    printAction(game, &action, MAX_LINE_LEN, line);
    return std::string(line);
  }

  std::string actionToString(const ActionType action, const Game* game) {
    char line[MAX_LINE_LEN];
    const Action a{action, 0};
    printAction(game, &a, MAX_LINE_LEN, line);
    return std::string(line);
  }

  /// Hand number counting from 1
  constexpr uint32_t handNum(const State& state) {
    return state.handId + 1;
  }

  /// Hand number counting from 1
  constexpr uint32_t handNum(const MatchState& view) {
    return handNum(view.state);
  }

  typedef double ChipBalance;

  uint randomRandomSeed() {
    std::random_device rd;
    return rd();
  }

  bool flipCoin(double probTrue, std::mt19937* randomEngine) {
    assert(randomEngine);
    std::bernoulli_distribution coin(probTrue);
    return coin(*randomEngine);
  }

  bool allOthersFolded(const State& state, const size_t pos, const size_t numPlayers) {
    bool allOthersFolded_ = true;
    for (size_t otherPos = 0; otherPos < numPlayers; ++otherPos) {
      if (pos != otherPos && !state.playerFolded[otherPos]) {
        allOthersFolded_ = false;
        break;
      }
    }
    return allOthersFolded_;
  }

  class GameDef {
  public:
    static int32_t bigBlind(const Game* game) {
      return (
          reduce(
              game->blind, game->numPlayers,
              [](int32_t maxSoFar, int32_t blind) {
        return blind > maxSoFar ? blind : maxSoFar;
      },
      0
          )
      );
    }
    GameDef(const GameDef& toCopy)
    :game_(nullptr),
     bigBlind_(bigBlind(toCopy.game_)) {
      game_ = static_cast<Game*>(malloc(sizeof(*toCopy.game_)));
      assert(game_);
      memcpy(game_, toCopy.game_, sizeof(*game_));
    }
    GameDef(const std::string& gameDefPath)
    :game_(gameDef(gameDefPath)),
     bigBlind_(bigBlind(game_)) {};
    virtual ~GameDef() { FREE_POINTER(game_); }

    std::string toString(const State& state) const {
      char line[MAX_LINE_LEN];
      printState(game_, &state, MAX_LINE_LEN, line);
      return std::string(line);
    }

    std::string toString(const MatchState& state) const {
      char line[MAX_LINE_LEN];
      printMatchState(game_, &state, MAX_LINE_LEN, line);
      return std::string(line);
    }

    const Game* game() const { return game_; }
    Game* game() { return game_; }
    int32_t bigBlind() const { return bigBlind_; }

    size_t numCards() const { return game_->numRanks * game_->numSuits; }
    size_t numPrivateCards() const {
      return game_->numHoleCards * game_->numPlayers;
    }

    // @todo Make this private
    Game* game_;
    const int32_t bigBlind_;
  };

  State newState(const std::string& stateString, const GameDef& gameDef) {
    State s;
    int charsRead = readState(stateString.c_str(), gameDef.game_, &s);
    if (charsRead <= 0) {
      throw std::runtime_error("Unable to read state \"" + stateString + "\"");
    }
    return s;
  };

  MatchState newMatchState(const std::string& s, const GameDef& gameDef) {
    MatchState ms;
    int charsRead = readMatchState(s.c_str(), gameDef.game_, &ms);
    if (charsRead <= 0) {
      throw std::runtime_error("Unable to read match state \"" + s + "\"");
    }
    return ms;
  };

  std::vector<std::string> players(const std::string& stateString, const GameDef& gameDef) {
    State s;
    int charsRead = readState(stateString.c_str(), gameDef.game_, &s);
    if (charsRead <= 0) {
      throw std::runtime_error("Unable to read state \"" + stateString + "\"");
    }
    // Looks like "player1|player2|player3"
    std::string playersString = stateString.substr(charsRead + 1 + stateString.substr(charsRead+1).find(":") + 1);

    std::vector<std::string> players_(gameDef.game_->numPlayers);
    size_t startingPos = 0;
    for (size_t p = 0; p < gameDef.game_->numPlayers - 1; ++p) {
      size_t endPos = playersString.find("|");
      players_[p] = playersString.substr(0, endPos - startingPos);
      playersString = playersString.substr(endPos + 1);
    }
    players_[gameDef.game_->numPlayers - 1] = playersString;
    return players_;
  };

  /**
   * Yields every time a player is about to act.
   */
  void replay(
      const MatchState& view,
      const GameDef& gameDef,
      std::function<bool(const MatchState&, const Action&)> doOnState
  ) {
    MatchState ms;
    ms.viewingPlayer = view.viewingPlayer;
    initState(gameDef.game_, view.state.handId, &ms.state);
    memcpy(ms.state.holeCards, view.state.holeCards, sizeof(view.state.holeCards));

    for (uint8_t round = 0; round <= view.state.round; ++round) {
      for (uint8_t actionIndex = 0; actionIndex < view.state.numActions[round]; ++actionIndex) {
        const Action& action = view.state.action[round][actionIndex];
        if (doOnState(ms, action)) { return; }

        doAction(gameDef.game_, &action, &(ms.state));
      }
    }
    return;
  }

  struct DealerConnection {
    DealerConnection(
        uint16_t port,
        const std::string& host = "localhost")
    :port_(port),
     host_(),
     toServer_(nullptr),
     fromServer_(nullptr) {
      std::memcpy(host_, host.c_str(), sizeof(*host_) * host.size());
    }
    virtual ~DealerConnection() {
      fclose(toServer_);
      fclose(fromServer_);
    };

    void connect() {
      DEBUG_VARIABLE("%s", host_);
      int sock = connectTo(host_, port_);
      if( sock < 0 ) {
        exit( EXIT_FAILURE );
      }
      toServer_ = fdopen( sock, "w" );
      fromServer_ = fdopen( sock, "r" );
      if(!(toServer_ && fromServer_)) {
        fprintf( stderr, "ERROR: could not get socket streams\n" );
        exit( EXIT_FAILURE );
      }

      /* send version string to dealer */
      if(
          fprintf(
              toServer_,
              "VERSION:%" PRIu32 ".%" PRIu32 ".%" PRIu32 "\n",
              VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION
          ) != 14
      ) {
        fprintf( stderr, "ERROR: could not get send version to server\n" );
        exit(EXIT_FAILURE);
      }
      fflush(toServer_);
    }

    uint16_t port_;
    char host_[HOST_NAME_MAX];
    FILE* toServer_;
    FILE* fromServer_;
  };

  class Configuration {
  public:
    Configuration(
        const std::string& gameDefPath,
        uint16_t port,
        const std::string& host = "localhost"
    ):gameDef_(gameDefPath),dealer_(port, host) {
      dealer_.connect();
    }
    Configuration(
        const GameDef& gameDef,
        uint16_t port,
        const std::string& host = "localhost"
    ):gameDef_(gameDef),dealer_(port, host) {
      dealer_.connect();
    }
    virtual ~Configuration() {};

    int nextMatchState(char* line) {
      assert(line);
      while( fgets( line, MAX_LINE_LEN, dealer_.fromServer_ ) ) {
        /* ignore comments */
        if( line[ 0 ] == '#' || line[ 0 ] == ';' ) { continue; }

        int len = readMatchState( line, gameDef_.game_, &state_ );
        if( len < 0 ) {
          fprintf( stderr, "ERROR: could not read state_ %s", line );
          exit( EXIT_FAILURE );
        }
        return len;
      }
      return 0;
    }

    void sendAction(Action actionToSend, char* line, int len) {
      /* add a colon (guaranteed to fit because we read a new-line in fgets) */
      line[ len ] = ':';
      ++len;

      assert(isValidAction(gameDef_.game_, &state_.state, 0, &actionToSend));
      int r = printAction(gameDef_.game_, &actionToSend, MAX_LINE_LEN - len - 2, &line[len]);

      if( r < 0 ) {
        fprintf( stderr, "ERROR: line too long after printing action\n" );
        exit( EXIT_FAILURE );
      }
      len += r;
      line[ len ] = '\r';
      ++len;
      line[ len ] = '\n';
      ++len;

      if( fwrite(line, 1, len, dealer_.toServer_) != static_cast<size_t>(len) ) {
        fprintf( stderr, "ERROR: could not get send response to server\n" );
        exit(EXIT_FAILURE);
      }
      fflush(dealer_.toServer_);
    }

    void forEveryMatchState(
        std::function<Action(const MatchState&)> generateAction,
        std::function<void(const MatchState&)> doAtEndOfHand
    ) {
      int len;
      char line[ MAX_LINE_LEN ];

      /* play the game! */
      while ((len = nextMatchState(line)) > 0) {
        if (handFinished()) {
          doAtEndOfHand(state_);
        }
        else if(mustAct()) {
          sendAction(generateAction(state_), line, len);
        }
      }
    }

    bool mustAct() const {
      return (
          !handFinished() &&
          currentPlayer(gameDef_.game_, &state_.state) == state_.viewingPlayer
      );
    }

    bool handFinished() const { return stateFinished(&state_.state); }

    const MatchState& mostRecentState() const { return state_; }

  public:
    const GameDef gameDef_;

  protected:
    DealerConnection dealer_;
    MatchState state_;
  };

  constexpr double complementaryProb(double p) {
    return 1.0 - p;
  }

  constexpr double sampledImmediateCfr(double infoSetW, double probTakingAction, bool actionIsPartOfSeq) {
    return actionIsPartOfSeq ? infoSetW * complementaryProb(probTakingAction) : -infoSetW * probTakingAction;
  }

  bool isBeginningOfRound(const State& state) {
    return state.numActions[state.round] < 1;
  }
  bool isBeginningOfRound(const MatchState& view) {
    return isBeginningOfRound(view.state);
  }
  bool isBeginningOfHand(const State& state) {
    return state.numActions[0] < 1;
  }
  bool isBeginningOfHand(const MatchState& view) {
    return isBeginningOfHand(view.state);
  }

  bool yetToActThisRound(const State& state, uint8_t viewingPlayer, uint round) {
    if (round > state.round) { return true; }
    for (
        int actionIndex = 0;
        actionIndex < state.numActions[round];
        ++actionIndex
    ) {
      if (state.actingPlayer[round][actionIndex] == viewingPlayer) {
        return false;
      }
    }
    return true;
  }
  bool yetToActThisHand(const State& state, uint8_t viewingPlayer) {
    for (int round = state.round; round >= 0; --round) {
      if (!yetToActThisRound(state, viewingPlayer, round)) {
        return false;
      }
    }
    return true;
  }
  bool yetToActThisRound(const MatchState& view, uint8_t viewingPlayer, uint round) {
    return yetToActThisRound(view.state, viewingPlayer, round);
  }
  bool yetToActThisHand(const MatchState& view, uint8_t viewingPlayer) {
    return yetToActThisHand(view.state, viewingPlayer);
  }
  bool yetToActThisRound(const MatchState& view, uint round) {
    return yetToActThisRound(view.state, view.viewingPlayer, round);
  }
  bool yetToActThisHand(const MatchState& view) {
    return yetToActThisHand(view.state, view.viewingPlayer);
  }

  bool actionIsAtEndOfSequence(const Action& action, const MatchState& view) {
    const auto& numActionsInCurrentRound = view.state.numActions[view.state.round];
    if (numActionsInCurrentRound <= 0) {
      if (view.state.round <= 0) { return false; }
      const auto& prevRound = view.state.round - 1;

      return actionsEqual(view.state.action[prevRound][view.state.numActions[prevRound] - 1], action);
    }
    return actionsEqual(view.state.action[view.state.round][numActionsInCurrentRound - 1], action);
  }

  template <class Container>
  double regretMatching(const Container regrets, size_t actionIndex) {
    assert(actionIndex < regrets.size());

    const double sumOfPosRegrets = reduce(
        regrets,
        [](double posRegret, double r) {
      return r > 0.0 ? posRegret + r : posRegret;
    },
    0.0
    );

    return (
        sumOfPosRegrets > 0.0 ? (
            regrets[actionIndex] > 0.0 ? (
                regrets[actionIndex] / sumOfPosRegrets
            ) : 0.0
        ) : (1.0 / regrets.size())
    );
  }

  template <class Container>
  double normalize(const Container values, size_t subjectIndex) {
    assert(subjectIndex < values.size());

    const double sumOfValues = reduce(values, [](double s, double val) { return s + val; }, 0.0);

    return (
        sumOfValues > 0.0 ? (
            values[subjectIndex] / sumOfValues
        ) : (1.0 / values.size())
    );
  }

  template <class Count>
  double optimisticAveragingWeight(uint32_t handNum, Count infoSetCount, double probIPlayToCurrentInfoSet) {
    return (handNum - infoSetCount) * probIPlayToCurrentInfoSet;
  }

  template <size_t numCards>
  class Deck {
  public:
    constexpr Deck():cardsRevealed_() {};
    virtual ~Deck() {};

    constexpr void reveal(const size_t cardIndex) {
      cardsRevealed_[cardIndex] = 1;
    }
    constexpr bool isRevealed(const size_t cardIndex) const {
      return cardsRevealed_[cardIndex];
    }
    constexpr size_t numHiddenCards() const {
      return (~cardsRevealed_).count();
    }

    template <typename CardIndex>
    std::vector<CardIndex> hiddenCards() const {
      std::vector<CardIndex> hiddenCards_(numHiddenCards());
      size_t i = 0;
      for (size_t card = 0; card < cardsRevealed_.size(); ++card) {
        if (!isRevealed(card)) {
          hiddenCards_[i] = CardIndex(card);
          ++i;
        }
      }
      return hiddenCards_;
    }
  private:
    std::bitset<numCards> cardsRevealed_;
  };

namespace Dealer {
  int startMatch(
      const std::string& matchName,
      const GameDef& gameDef,
      const std::vector<std::string>& players,
      const std::vector<int>& listenSocket,
      const std::string& workingDirectory,
      uint32_t numHands = 3000,
      uint32_t seed = 98723209,
      uint32_t maxInvalidActions = DEFAULT_MAX_INVALID_ACTIONS,
      uint64_t maxResponseMicros = DEFAULT_MAX_RESPONSE_MICROS,
      uint64_t maxUsedHandMicros = DEFAULT_MAX_USED_HAND_MICROS,
      uint64_t maxUsedPerHandMicros = DEFAULT_MAX_USED_PER_HAND_MICROS,
      // 10 seconds. Set to negative for no timeout
      int64_t startTimeoutMicros = 10000000,
      /* players rotate around the table */
      bool fixedSeats = 0,
      /* print all messages */
      bool quiet = 1,
      /* by default, overwrite preexisting log/transaction files */
      bool append = 0,
      /* use log file, don't use transaction file */
      bool useLogFile = 0,
      bool useTransactionFile = 0
  ) {
    Game *game = gameDef.game_;

    FILE *logFile, *transactionFile;

    int i, v;
    struct sockaddr_in addr;
    socklen_t addrLen;
    ReadBuf *readBuf[ MAX_PLAYERS ];
    int seatFD[ MAX_PLAYERS ];

    rng_state_t rng;
    ErrorInfo errorInfo;
    char *seatName[ MAX_PLAYERS ];

    struct timeval startTime, tv;

    char name[ MAX_LINE_LEN ];

    for(int i = 0; i < game->numPlayers; ++i ) {
      seatName[i] = static_cast<char*>(calloc(players[i].size(), sizeof(*seatName[i])));
      memcpy(seatName[ i ], players[i].data(), sizeof(*seatName[i])*players[i].size());
    }

    init_genrand( &rng, seed );
    srandom( seed ); /* used for random port selection */

    if( useLogFile ) {
      const auto logFileName = workingDirectory + "/" + matchName + ".log";
      memcpy(name, logFileName.data(), sizeof(*name)*logFileName.size());

      /* create/open the log */
      if (append) {
        logFile = fopen( name, "a+" );
      } else {
        logFile = fopen( name, "w" );
      }
      if( logFile == NULL ) {

        fprintf( stderr, "ERROR: could not open log file %s\n", name );
        exit( EXIT_FAILURE );
      }
    } else {
      /* no log file */

      logFile = NULL;
    }

    if( useTransactionFile ) {
      const auto transactionFileName = workingDirectory + "/" + matchName + ".tlog";
      memcpy(name, transactionFileName.data(), sizeof(*name)*transactionFileName.size());

      /* create/open the transaction log */
      if (append) {
        transactionFile = fopen( name, "a+" );
      } else {
        transactionFile = fopen( name, "w" );
      }
      if( transactionFile == NULL ) {

        fprintf( stderr, "ERROR: could not open transaction file %s\n", name );
        exit( EXIT_FAILURE );
      }
    } else {
      /* no transaction file */

      transactionFile = NULL;
    }

    /* set up the error info */
    initErrorInfo( maxInvalidActions, maxResponseMicros, maxUsedHandMicros,
        maxUsedPerHandMicros * numHands, &errorInfo );

    /* wait for each player to connect */
    gettimeofday( &startTime, NULL );
    for( i = 0; i < game->numPlayers; ++i ) {

      if( startTimeoutMicros >= 0 ) {
        int64_t startTimeLeft;
        fd_set fds;

        gettimeofday( &tv, NULL );
        startTimeLeft = startTimeoutMicros
            - (uint64_t)( tv.tv_sec - startTime.tv_sec ) * 1000000
            - ( tv.tv_usec - startTime.tv_usec );
        if( startTimeLeft < 0 ) {

          startTimeLeft = 0;
        }
        tv.tv_sec = startTimeLeft / 1000000;
        tv.tv_usec = startTimeLeft % 1000000;

        FD_ZERO( &fds );
        FD_SET( listenSocket[ i ], &fds );

        if( select( listenSocket[ i ] + 1, &fds, NULL, NULL, &tv ) < 1 ) {
          /* no input ready within time, or an actual error */

          fprintf( stderr, "ERROR: timed out waiting for seat %d to connect\n",
              i + 1 );
          return EXIT_FAILURE;
        }
      }

      addrLen = sizeof( addr );
      seatFD[ i ] = accept( listenSocket[ i ], (struct sockaddr *)&addr, &addrLen );
      if( seatFD[ i ] < 0 ) {
        fprintf( stderr, "ERROR: seat %d could not connect\n", i + 1 );
        return EXIT_FAILURE;
      }
      close( listenSocket[ i ] );

      v = 1;
      setsockopt( seatFD[ i ], IPPROTO_TCP, TCP_NODELAY,
          (char *)&v, sizeof(int) );

      readBuf[ i ] = createReadBuf( seatFD[ i ] );
    }

    for(int i = 0; i < game->numPlayers; ++i ) {
      readBuf[ i ] = createReadBuf( seatFD[ i ] );
    }

    /* play the match */
    if( gameLoop( game, seatName, numHands, quiet, fixedSeats, &rng, &errorInfo,
        seatFD, readBuf, logFile, transactionFile ) < 0 ) {
      /* should have already printed an error message */

      return EXIT_FAILURE;
    }

    //fflush( stderr );
    //fflush( stdout );
    // Otherwise the last line or two of the log file
    // won't be written sometimes when run through a
    // Ruby interface.
    fflush(NULL);
    if( transactionFile != NULL ) {
      fclose( transactionFile );
    }
    if( logFile != NULL ) {
      fclose( logFile );
    }

    for(int i = 0; i < game->numPlayers; ++i ) {
      close(seatFD[ i ]);
      FREE_POINTER(seatName[i]);
    }

    return EXIT_SUCCESS;
  }
}
}
}
