/*
Copyright (C) 2011 by the Computer Poker Research Group, University of Alberta
*/

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
#include <getopt.h>
#include "dealer.h"
#include "game.h"
#include "net.h"


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


/* update the number of invalid actions for seat
   returns >= 0 if match should continue, -1 for failure */
static int checkErrorInvalidAction( const uint8_t seat, ErrorInfo *info )
{
  ++( info->numInvalidActions[ seat ] );

  if( info->numInvalidActions[ seat ] > info->maxInvalidActions ) {
    return -1;
  }

  return 0;
}

/* update the time used by seat
   returns >= 0 if match should continue, -1 for failure */
static int checkErrorTimes( const uint8_t seat,
			    const struct timeval *sendTime,
			    const struct timeval *recvTime,
			    ErrorInfo *info )
{
  uint64_t responseMicros;

  /* calls to gettimeofday can return earlier times on later calls :/ */
  if( recvTime->tv_sec < sendTime->tv_sec
      || ( recvTime->tv_sec == sendTime->tv_sec
	   && recvTime->tv_usec < sendTime->tv_usec ) ) {
    return 0;
  }

  /* figure out how many microseconds the response took */
  responseMicros = ( recvTime->tv_sec - sendTime->tv_sec ) * 1000000
    + recvTime->tv_usec - sendTime->tv_usec;

  /* update usage counts */
  info->usedHandMicros[ seat ] += responseMicros;
  info->usedMatchMicros[ seat ] += responseMicros;

  /* check time used for the response */
  if( responseMicros > info->maxResponseMicros ) {
    return -1;
  }

  /* check time used in the current hand */
  if( info->usedHandMicros[ seat ] > info->maxUsedHandMicros ) {
    return -1;
  }

  /* check time used in the entire match */
  if( info->usedMatchMicros[ seat ] > info->maxUsedMatchMicros ) {
    return -1;
  }

  return 0;
}

/* note that there is a new hand
   returns >= 0 if match should continue, -1 for failure */
static int checkErrorNewHand( const Game *game, ErrorInfo *info )
{
  uint8_t p;

  for( p = 0; p < game->numPlayers; ++p ) {
    info->usedHandMicros[ p ] = 0;
  }

  return 0;
}


static uint8_t seatToPlayer( const Game *game, const uint8_t player0Seat,
			     const uint8_t seat )
{
  return ( seat + game->numPlayers - player0Seat ) % game->numPlayers;
}

static uint8_t playerToSeat( const Game *game, const uint8_t player0Seat,
			     const uint8_t player )
{
  return ( player + player0Seat ) % game->numPlayers;
}

/* returns >= 0 if match should continue, -1 for failure */
static int sendPlayerMessage( const Game *game, const MatchState *state,
			      const int quiet, const uint8_t seat,
			      const int seatFD, struct timeval *sendTime )
{
  int c;
  char line[ MAX_LINE_LEN ];

  /* prepare the message */
  c = printMatchState( game, state, MAX_LINE_LEN, line );
  if( c < 0 || c > MAX_LINE_LEN - 3 ) {
    /* message is too long */

    fprintf( stderr, "ERROR: state message too long\n" );
    return -1;
  }
  line[ c ] = '\r';
  line[ c + 1 ] = '\n';
  line[ c + 2 ] = 0;
  c += 2;

  /* send it to the player and flush */
  if( write( seatFD, line, c ) != c ) {
    /* couldn't send the line */

    fprintf( stderr, "ERROR: could not send state to seat %"PRIu8"\n",
	     seat + 1 );
    return -1;
  }

  /* note when we sent the message */
  gettimeofday( sendTime, NULL );

  /* log the message */
  if( !quiet ) {
    fprintf( stderr, "TO %d at %zu.%.06zu %s", seat + 1,
	     sendTime->tv_sec, sendTime->tv_usec, line );
  }

  return 0;
}

/* returns >= 0 if action/size has been set to a valid action
   returns -1 for failure (disconnect, timeout, too many bad actions, etc) */
static int readPlayerResponse( const Game *game,
			       const MatchState *state,
			       const int quiet,
			       const uint8_t seat,
			       const struct timeval *sendTime,
			       ErrorInfo *errorInfo,
			       ReadBuf *readBuf,
			       Action *action,
			       struct timeval *recvTime )
{
  int c, r;
  MatchState tempState;
  char line[ MAX_LINE_LEN ];

  while( 1 ) {

    /* read a line of input from player */
    struct timeval start;
    gettimeofday( &start, NULL );
    if( getLine( readBuf, MAX_LINE_LEN, line,
		 errorInfo->maxResponseMicros ) <= 0 ) {
      /* couldn't get any input from player */

      struct timeval after;
      gettimeofday( &after, NULL );
      uint64_t micros_spent =
	(uint64_t)( after.tv_sec - start.tv_sec ) * 1000000
	+ ( after.tv_usec - start.tv_usec );
      fprintf( stderr, "ERROR: could not get action from seat %"PRIu8"\n",
	       seat + 1 );
      // Print out how much time has passed so we can see if this was a
      // timeout as opposed to some other sort of failure (e.g., socket
      // closing).
      fprintf( stderr, "%.1f seconds spent waiting; timeout %.1f\n",
	       micros_spent / 1000000.0,
	       errorInfo->maxResponseMicros / 1000000.0);
      return -1;
    }

    /* note when the message arrived */
    gettimeofday( recvTime, NULL );

    /* log the response */
    if( !quiet ) {
      fprintf( stderr, "FROM %d at %zu.%06zu %s", seat + 1,
	       recvTime->tv_sec, recvTime->tv_usec, line );
    }

    /* ignore comments */
    if( line[ 0 ] == '#' || line[ 0 ] == ';' ) {
      continue;
    }

    /* check for any timeout issues */
    if( checkErrorTimes( seat, sendTime, recvTime, errorInfo ) < 0 ) {

      fprintf( stderr, "ERROR: seat %"PRIu8" ran out of time\n", seat + 1 );
      return -1;
    }

    /* parse out the state */
    c = readMatchState( line, game, &tempState );
    if( c < 0 ) {
      /* couldn't get an intelligible state */

      fprintf( stderr, "WARNING: bad state format in response\n" );
      continue;
    }

    /* ignore responses that don't match the current state */
    if( !matchStatesEqual( game, state, &tempState ) ) {

      fprintf( stderr, "WARNING: ignoring un-requested response\n" );
      continue;
    }

    /* get the action */
    if( line[ c++ ] != ':'
	|| ( r = readAction( &line[ c ], game, action ) ) < 0 ) {

      if( checkErrorInvalidAction( seat, errorInfo ) < 0 ) {

	fprintf( stderr, "ERROR: bad action format in response\n" );
      }

      fprintf( stderr,
	       "WARNING: bad action format in response, changed to call\n" );
      action->type = a_call;
      action->size = 0;
      goto doneRead;
    }
    c += r;

    /* make sure the action is valid */
    if( !isValidAction( game, &state->state, 1, action ) ) {

      if( checkErrorInvalidAction( seat, errorInfo ) < 0 ) {

	fprintf( stderr, "ERROR: invalid action\n" );
	return -1;
      }

      fprintf( stderr, "WARNING: invalid action, changed to call\n" );
      action->type = a_call;
      action->size = 0;
    }

    goto doneRead;
  }

 doneRead:
  return 0;
}

/* returns >= 0 if match should continue, -1 for failure */
static int setUpNewHand( const Game *game, const uint8_t fixedSeats,
			 uint32_t *handId, uint8_t *player0Seat,
			 rng_state_t *rng, ErrorInfo *errorInfo, State *state )
{
  ++( *handId );

  /* rotate the players around the table */
  if( !fixedSeats ) {

    *player0Seat = ( *player0Seat + 1 ) % game->numPlayers;
  }

  if( checkErrorNewHand( game, errorInfo ) < 0 ) {

    fprintf( stderr, "ERROR: unexpected game\n" );
    return -1;
  }
  initState( game, *handId, state );
  dealCards( game, rng, state );

  return 0;
}

/* returns >= 0 if match should continue, -1 for failure */
static int processTransactionFile( const Game *game, const int fixedSeats,
				   uint32_t *handId, uint8_t *player0Seat,
				   rng_state_t *rng, ErrorInfo *errorInfo,
				   double totalValue[ MAX_PLAYERS ],
				   MatchState *state, FILE *file )
{
  int c, r;
  uint32_t h;
  uint8_t s;
  Action action;
  struct timeval sendTime, recvTime;
  char line[ MAX_LINE_LEN ];

  while( fgets( line, MAX_LINE_LEN, file ) ) {

    /* get the log entry */

    /* ACTION */
    c = readAction( line, game, &action );
    if( c < 0 ) {

      fprintf( stderr, "ERROR: could not parse transaction action %s", line );
      return -1;
    }

    /* ACTION HANDID SEND RECV */
    if( sscanf( &line[ c ], " %"SCNu32" %zu.%06zu %zu.%06zu%n", &h,
		&sendTime.tv_sec, &sendTime.tv_usec,
		&recvTime.tv_sec, &recvTime.tv_usec, &r ) < 4 ) {

      fprintf( stderr, "ERROR: could not parse transaction stamp %s", line );
      return -1;
    }
    c += r;

    /* check that we're processing the expected handId */
    if( h != *handId ) {

      fprintf( stderr, "ERROR: handId mismatch in transaction log: %s", line );
      return -1;
    }

    /* make sure the action is valid */
    if( !isValidAction( game, &state->state, 0, &action ) ) {

      fprintf( stderr, "ERROR: invalid action in transaction log: %s", line );
      return -1;
    }

    /* check for any timeout issues */
    s = playerToSeat( game, *player0Seat,
		      currentPlayer( game, &state->state ) );
    if( checkErrorTimes( s, &sendTime, &recvTime, errorInfo ) < 0 ) {

      fprintf( stderr,
	       "ERROR: seat %"PRIu8" ran out of time in transaction file\n",
	       s + 1 );
      return -1;
    }

    doAction( game, &action, &state->state );

    if( stateFinished( &state->state ) ) {
      /* hand is finished */

      /* update the total value for each player */
      for( s = 0; s < game->numPlayers; ++s ) {

	totalValue[ s ]
	  += valueOfState( game, &state->state,
			   seatToPlayer( game, *player0Seat, s ) );
      }

      /* move on to next hand */
      if( setUpNewHand( game, fixedSeats, handId, player0Seat,
			rng, errorInfo, &state->state ) < 0 ) {

	return -1;
      }
    }
  }

  return 0;
}

/* returns >= 0 if match should continue, -1 on failure */
static int logTransaction( const Game *game, const State *state,
			   const Action *action,
			   const struct timeval *sendTime,
			   const struct timeval *recvTime,
			   FILE *file )
{
  int c, r;
  char line[ MAX_LINE_LEN ];

  c = printAction( game, action, MAX_LINE_LEN, line );
  if( c < 0 ) {

    fprintf( stderr, "ERROR: transaction message too long\n" );
    return -1;
  }

  r = snprintf( &line[ c ], MAX_LINE_LEN - c,
		" %"PRIu32" %zu.%06zu %zu.%06zu\n",
		state->handId, sendTime->tv_sec, sendTime->tv_usec,
		recvTime->tv_sec, recvTime->tv_usec );
  if( r < 0 ) {

    fprintf( stderr, "ERROR: transaction message too long\n" );
    return -1;
  }
  c += r;

  if( fwrite( line, 1, c, file ) != c ) {

    fprintf( stderr, "ERROR: could not write to transaction file\n" );
    return -1;
  }
  fflush( file );

  return c;
}

/* returns >= 0 if match should continue, -1 on failure */
static int checkVersion( const uint8_t seat,
			 ReadBuf *readBuf )
{
  uint32_t major, minor, rev;
  char line[ MAX_LINE_LEN ];


  if( getLine( readBuf, MAX_LINE_LEN, line, -1 ) <= 0 ) {

    fprintf( stderr,
	     "ERROR: could not read version string from seat %"PRIu8"\n",
	     seat + 1 );
    return -1;
  }

  if( sscanf( line, "VERSION:%"SCNu32".%"SCNu32".%"SCNu32,
	      &major, &minor, &rev ) < 3 ) {

    fprintf( stderr,
	     "ERROR: invalid version string %s", line );
    return -1;
  }

  if( major != VERSION_MAJOR || minor > VERSION_MINOR ) {

    fprintf( stderr, "ERROR: this server is currently using version %"SCNu32".%"SCNu32".%"SCNu32"\n", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION );
  }

  return 0;
}

/* returns >= 0 if match should continue, -1 on failure */
static int addToLogFile( const Game *game, const State *state,
			 const double value[ MAX_PLAYERS ],
			 const uint8_t player0Seat,
			 char *seatName[ MAX_PLAYERS ], FILE *logFile )
{
  int c, r;
  uint8_t p;
  char line[ MAX_LINE_LEN ];

  /* prepare the message */
  c = printState( game, state, MAX_LINE_LEN, line );
  if( c < 0 ) {
    /* message is too long */

    fprintf( stderr, "ERROR: log state message too long\n" );
    return -1;
  }

  /* add the values */
  for( p = 0; p < game->numPlayers; ++p ) {

    r = snprintf( &line[ c ], MAX_LINE_LEN - c,
		  p ? "|%.6f" : ":%.6f", value[ p ] );
    if( r < 0 ) {

      fprintf( stderr, "ERROR: log message too long\n" );
      return -1;
    }
    c += r;

    /* remove trailing zeros after decimal-point */
    while( line[ c - 1 ] == '0' ) { --c; }
    if( line[ c - 1 ] == '.' ) { --c; }
    line[ c ] = 0;
  }

  /* add the player names */
  for( p = 0; p < game->numPlayers; ++p ) {

    r = snprintf( &line[ c ], MAX_LINE_LEN - c,
		  p ? "|%s" : ":%s",
		  seatName[ playerToSeat( game, player0Seat, p ) ] );
    if( r < 0 ) {

      fprintf( stderr, "ERROR: log message too long\n" );
      return -1;
    }
    c += r;
  }

  /* print the line to log and flush */
  if( fprintf( logFile, "%s\n", line ) < 0 ) {

    fprintf( stderr, "ERROR: logging failed for game %s\n", line );
    return -1;
  }
  fflush( logFile );

  return 0;
}

/* returns >= 0 if match should continue, -1 on failure */
static int printFinalMessage( const Game *game, char *seatName[ MAX_PLAYERS ],
			      const double totalValue[ MAX_PLAYERS ],
			      FILE *logFile )
{
  int c, r;
  uint8_t s;
  char line[ MAX_LINE_LEN ];

  c = snprintf( line, MAX_LINE_LEN, "SCORE" );
  if( c < 0 ) {
    /* message is too long */

    fprintf( stderr, "ERROR: value state message too long\n" );
    return -1;
  }

  for( s = 0; s < game->numPlayers; ++s ) {

    r = snprintf( &line[ c ], MAX_LINE_LEN - c,
		  s ? "|%.6f" : ":%.6f", totalValue[ s ] );
    if( r < 0 ) {

      fprintf( stderr, "ERROR: value message too long\n" );
      return -1;
    }
    c += r;

    /* remove trailing zeros after decimal-point */
    while( line[ c - 1 ] == '0' ) { --c; }
    if( line[ c - 1 ] == '.' ) { --c; }
    line[ c ] = 0;
  }

  /* add the player names */
  for( s = 0; s < game->numPlayers; ++s ) {

    r = snprintf( &line[ c ], MAX_LINE_LEN - c,
		  s ? "|%s" : ":%s", seatName[ s ] );
    if( r < 0 ) {

      fprintf( stderr, "ERROR: log message too long\n" );
      return -1;
    }
    c += r;
  }

  fprintf( stdout, "%s\n", line );
  fprintf( stderr, "%s\n", line );

  if( logFile ) {

    fprintf( logFile, "%s\n", line );
  }

  return 0;
}

void initErrorInfo( const uint32_t maxInvalidActions,
         const uint64_t maxResponseMicros,
         const uint64_t maxUsedHandMicros,
         const uint64_t maxUsedMatchMicros,
         ErrorInfo *info )
{
  int s;

  info->maxInvalidActions = maxInvalidActions;
  info->maxResponseMicros = maxResponseMicros;
  info->maxUsedHandMicros = maxUsedHandMicros;
  info->maxUsedMatchMicros = maxUsedMatchMicros;

  for( s = 0; s < MAX_PLAYERS; ++s ) {
    info->numInvalidActions[ s ] = 0;
    info->usedHandMicros[ s ] = 0;
    info->usedMatchMicros[ s ] = 0;
  }
}

/* run a match of numHands hands of the supplied game

   cards are dealt using rng, error conditions like timeouts
   are controlled and stored in errorInfo

   actions are read/sent to seat p on seatFD[ p ]

   if quiet is not zero, only print out errors, warnings, and final value

   if logFile is not NULL, print out a single line for each completed
   match with the final state and all player values.  The values are
   printed in player, not seat order.

   if transactionFile is not NULL, a transaction log of actions made
   is written to the file, and if there is any input left to read on
   the stream when gameLoop is called, it will be processed to
   initialise the state

   returns >=0 if the match finished correctly, -1 on error */
int gameLoop( const Game *game, char *seatName[ MAX_PLAYERS ],
		     const uint32_t numHands, const int quiet,
		     const int fixedSeats, rng_state_t *rng,
		     ErrorInfo *errorInfo, const int seatFD[ MAX_PLAYERS ],
		     ReadBuf *readBuf[ MAX_PLAYERS ],
		     FILE *logFile, FILE *transactionFile )
{
  uint32_t handId;
  uint8_t seat, p, player0Seat, currentP, currentSeat;
  struct timeval t, sendTime, recvTime;
  Action action;
  MatchState state;
  double value[ MAX_PLAYERS ], totalValue[ MAX_PLAYERS ];

  /* check version string for each player */
  for( seat = 0; seat < game->numPlayers; ++seat ) {

    if( checkVersion( seat, readBuf[ seat ] ) < 0 ) {
      /* error messages already handled in function */

      return -1;
    }
  }

  gettimeofday( &sendTime, NULL );
  if( !quiet ) {
    fprintf( stderr, "STARTED at %zu.%06zu\n",
	     sendTime.tv_sec, sendTime.tv_usec );
  }

  /* start at the first hand */
  handId = 0;
  if( checkErrorNewHand( game, errorInfo ) < 0 ) {

    fprintf( stderr, "ERROR: unexpected game\n" );
    return -1;
  }
  initState( game, handId, &state.state );
  dealCards( game, rng, &state.state );
  for( seat = 0; seat < game->numPlayers; ++seat ) {
    totalValue[ seat ] = 0.0;
  }

  /* seat 0 is player 0 in first game */
  player0Seat = 0;

  /* process the transaction file */
  if( transactionFile != NULL ) {

    if( processTransactionFile( game, fixedSeats, &handId, &player0Seat,
				rng, errorInfo, totalValue,
				&state, transactionFile ) < 0 ) {
      /* error messages already handled in function */

      return -1;
    }
  }

  if( handId >= numHands ) {
    goto finishedGameLoop;
  }

  /* play all the (remaining) hands */
  while( 1 ) {

    /* play the hand */
    while( !stateFinished( &state.state ) ) {

      /* find the current player */
      currentP = currentPlayer( game, &state.state );

      /* send state to each player */
      for( seat = 0; seat < game->numPlayers; ++seat ) {

	state.viewingPlayer = seatToPlayer( game, player0Seat, seat );
	if( sendPlayerMessage( game, &state, quiet, seat,
			       seatFD[ seat ], &t ) < 0 ) {
	  /* error messages already handled in function */

	  return -1;
	}

	/* remember the seat and send time if player is acting */
	if( state.viewingPlayer == currentP ) {

	  sendTime = t;
	}
      }

      /* get action from current player */
      state.viewingPlayer = currentP;
      currentSeat = playerToSeat( game, player0Seat, currentP );
      if( readPlayerResponse( game, &state, quiet, currentSeat, &sendTime,
			      errorInfo, readBuf[ currentSeat ],
			      &action, &recvTime ) < 0 ) {
	/* error messages already handled in function */

	return -1;
      }

      /* log the transaction */
      if( transactionFile != NULL ) {

	if( logTransaction( game, &state.state, &action,
			    &sendTime, &recvTime, transactionFile ) < 0 ) {
	  /* error messages already handled in function */

	  return -1;
	}
      }

      /* do the action */
      doAction( game, &action, &state.state );
    }

    /* get values */
    for( p = 0; p < game->numPlayers; ++p ) {

      value[ p ] = valueOfState( game, &state.state, p );
      totalValue[ playerToSeat( game, player0Seat, p ) ] += value[ p ];
    }

    /* add the game to the log */
    if( logFile != NULL ) {

      if( addToLogFile( game, &state.state, value, player0Seat,
			seatName, logFile ) < 0 ) {
	/* error messages already handled in function */

	return -1;
      }
    }

    /* send final state to each player */
    for( seat = 0; seat < game->numPlayers; ++seat ) {

      state.viewingPlayer = seatToPlayer( game, player0Seat, seat );
      if( sendPlayerMessage( game, &state, quiet, seat,
			     seatFD[ seat ], &t ) < 0 ) {
	/* error messages already handled in function */

	return -1;
      }
    }

    if ( !quiet ) {
      if ( handId % 100 == 0) {
	for( seat = 0; seat < game->numPlayers; ++seat ) {
	  fprintf(stderr, "Seconds cumulatively spent in match for seat %i: "
		  "%i\n", seat,
		  (int)(errorInfo->usedMatchMicros[ seat ] / 1000000));
	}
      }
    }

    /* start a new hand */
    if( setUpNewHand( game, fixedSeats, &handId, &player0Seat,
		      rng, errorInfo, &state.state ) < 0 ) {
      /* error messages already handled in function */

      return -1;
    }
    if( handId >= numHands ) {
      break;
    }
  }

 finishedGameLoop:
  /* print out the final values */
  if( !quiet ) {
    gettimeofday( &t, NULL );
    fprintf( stderr, "FINISHED at %zu.%06zu\n",
	     sendTime.tv_sec, sendTime.tv_usec );
  }
  if( printFinalMessage( game, seatName, totalValue, logFile ) < 0 ) {
    /* error messages already handled in function */

    return -1;
  }

  return 0;
}
