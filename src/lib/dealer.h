/*
Copyright (C) 2011 by the Computer Poker Research Group, University of Alberta
*/

#include <stdlib.h>
#include <stdio.h>
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <unistd.h>
#include "game.h"


/* the ports for players to connect to will be printed on standard out
   (in player order)

   if log file is enabled, matchName.log will contain finished states
   and values, followed by the final total values for each player

   if transaction file is enabled, matchName.tlog will contain a list
   of actions taken and timestamps that is sufficient to recreate an
   interrupted match

   if the quiet option is not enabled, standard error will print out
   the messages sent to and receieved from the players

   the final total values for each player will be printed to both
   standard out and standard error

   exit value is EXIT_SUCCESS if the match was a success,
   or EXIT_FAILURE on any failure */


#define DEFAULT_MAX_INVALID_ACTIONS UINT32_MAX
#define DEFAULT_MAX_RESPONSE_MICROS 600000000
#define DEFAULT_MAX_USED_HAND_MICROS 600000000
#define DEFAULT_MAX_USED_PER_HAND_MICROS 7000000


typedef struct {
  uint32_t maxInvalidActions;
  uint64_t maxResponseMicros;
  uint64_t maxUsedHandMicros;
  uint64_t maxUsedMatchMicros;

  uint32_t numInvalidActions[ MAX_PLAYERS ];
  uint64_t usedHandMicros[ MAX_PLAYERS ];
  uint64_t usedMatchMicros[ MAX_PLAYERS ];
} ErrorInfo;

void initErrorInfo( const uint32_t maxInvalidActions,
         const uint64_t maxResponseMicros,
         const uint64_t maxUsedHandMicros,
         const uint64_t maxUsedMatchMicros,
         ErrorInfo *info );
int gameLoop( const Game *game, char *seatName[ MAX_PLAYERS ],
		     const uint32_t numHands, const int quiet,
		     const int fixedSeats, rng_state_t *rng,
		     ErrorInfo *errorInfo, const int seatFD[ MAX_PLAYERS ],
		     ReadBuf *readBuf[ MAX_PLAYERS ],
		     FILE *logFile, FILE *transactionFile );
