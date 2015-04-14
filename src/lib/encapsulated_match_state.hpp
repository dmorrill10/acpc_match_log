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

#include <lib/acpc.hpp>

extern "C" {
#include <game.h>
#include <print_debugger.h>
}
namespace AcpcMatchLog {
namespace Acpc {
class EncapsulatedMatchState {
public:
  static const int OMNISCIENT_VIEWER = -2;
  static const int OUTSIDE_OBSERVER_VIEWER = -1;
  explicit EncapsulatedMatchState(const MatchState &view,
                                  const GameDef &gameDef)
      : viewer_(view.viewingPlayer), state_(view.state), gameDef_(gameDef){};
  explicit EncapsulatedMatchState(const State &state, const GameDef &gameDef,
                                  int viewer = OUTSIDE_OBSERVER_VIEWER)
      : viewer_(viewer), state_(state), gameDef_(gameDef){};
  explicit EncapsulatedMatchState(State &&state, const GameDef &gameDef,
                                  int viewer = OUTSIDE_OBSERVER_VIEWER)
      : viewer_(viewer), state_(state), gameDef_(gameDef){};
  explicit EncapsulatedMatchState(const std::string &resultLogStateLine,
                                  const GameDef &gameDef, int viewer = OUTSIDE_OBSERVER_VIEWER)
      : viewer_(viewer), state_(newState(resultLogStateLine, gameDef)),
        gameDef_(gameDef){};
  virtual ~EncapsulatedMatchState(){};

  const State& state() const { return state_; }

  int32_t potSize() const {
    return Acpc::potSize(state_, gameDef_.game_->numPlayers);
  }
  std::string toString() const {
    return isObserver()
               ? Acpc::stateToString(state_, gameDef_.game_)
               : Acpc::matchStateToString(MatchState{state_, uint8_t(viewer_)},
                                          gameDef_.game_);
  }

  /// Hand number counting from 1
  uint32_t handNum() const { return Acpc::handNum(state_); }

  bool isFinished() const { return state_.finished; }

  /**
   * Yields every time a player is about to act, starting at the
   * beginning of the hand.
   */
  template <typename AbstractMatchState = EncapsulatedMatchState>
  void replay(std::function<bool(const AbstractMatchState &, const Action &)>
                  doOnState) const {
    State s;
    initState(gameDef_.game_, state_.handId, &s);
    memcpy(s.holeCards, state_.holeCards, sizeof(state_.holeCards));
    AbstractMatchState replayState(s, gameDef_, viewer_);

    for (uint8_t replayRound = 0; replayRound <= roundIndex(); ++replayRound) {
      for (uint8_t actionIndex = 0; actionIndex < numActions(replayRound);
           ++actionIndex) {
        const Action &action_ = action(replayRound, actionIndex);
        if (doOnState(replayState, action_)) {
          return;
        }

        replayState.applyAction(action_);
      }
    }
    return;
  }

  uint8_t roundIndex() const { return state_.round; }

  template <typename RoundIndex = uint8_t>
  uint8_t numActions(const RoundIndex roundToQuery) const {
    return state_.numActions[roundToQuery];
  }

  template <typename RoundIndex = uint8_t, typename ActionIndex = uint8_t>
  const Action &action(const RoundIndex roundToQuery,
                       const ActionIndex actionToQuery) const {
    return state_.action[roundToQuery][actionToQuery];
  }

  virtual EncapsulatedMatchState &applyAction(const Action &action) {
    doAction(gameDef_.game_, &action, &state_);
    return (*this);
  }

  bool isBeginningOfRound() const { return Acpc::isBeginningOfRound(state_); }

  bool yetToActThisRound(const uint8_t player, const uint round) const {
    return Acpc::yetToActThisRound(state_, player, round);
  }

  bool yetToActThisHand() const { return yetToActThisHand(viewer_); }

  bool yetToActThisHand(const uint8_t player) const {
    return Acpc::yetToActThisHand(state_, player);
  }

  bool actionIsAtEndOfSequence(const Action &action_) const {
    const auto &numActionsInCurrentRound = numActions(roundIndex());
    if (numActionsInCurrentRound <= 0) {
      if (roundIndex() <= 0) {
        return false;
      }
      const auto prevRound = roundIndex() - 1;

      return actionsEqual(action(prevRound, numActions(prevRound) - 1),
                          action_);
    }
    return actionsEqual(action(roundIndex(), numActionsInCurrentRound - 1),
                        action_);
  }

  uint8_t actor() const { return currentPlayer(gameDef_.game_, &state_); }

  bool isBeginningOfHand() const { return Acpc::isBeginningOfHand(state_); }

  const GameDef &gameDef() const { return gameDef_; }

  bool isObserver() const { return viewer() < 0; }
  bool isOmniscient() const { return viewer() == OMNISCIENT_VIEWER; }
  bool isPlayer() const { return !isObserver(); }

  bool handRevealed(const uint8_t player) const {
    return isOmniscient() || player == viewer() ||
           !(state_.playerFolded[player] ||
             Acpc::allOthersFolded(state_, player, gameDef_.game_->numPlayers));
  }
  int viewer() const { return viewer_; }
  int viewer(const int newViewer) { return (viewer_ = newViewer); }

protected:
  int viewer_;
  State state_;
  const GameDef &gameDef_;
};
}
}
