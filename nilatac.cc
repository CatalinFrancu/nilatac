/**
 * Copyright 2002-2006 Catalin Francu <cata@francu.com>
 * This file is part of Nilatac.
 *
 * Nilatac is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nilatac is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nilatac; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "suicide.h"

const char* SPC = " ";

inline char* get_token(char* s) {
  return strtok(s, " \t\n");
}

/**
 * Calculates how much time we can afford to think given our remaining time
 * and the opponent's remaining time. All values are in centis.
 */
int allocate_time(int my_time, int opp_time) {
  // If I have under 5 seconds left, or I am more than 3 seconds behind the
  // opponent with less than 20 seconds left, move quickly.
  if (my_time < 500 ||
      (my_time <= 2000 && (my_time < opp_time - 300)))
    return 10; // 0.1 seconds

  return my_time / 15 + g_increment * 2 / 3;
}

void parse_option(char* name, char* value) {
  if (strcmp(strtok(NULL, " "), "name")) {
    fatal("UCI: \"setoption\" should be followed by \"name\"");
  }
  name[0] = value[0] = '\0';
  int onName = true;
  char *t;
  while ((t = strtok(NULL, " ")) != NULL) {
    if (!strcmp(t, "value")) {
      onName = false;
    } else if (onName) {
      if (name[0]) {
        strcat(name, SPC);
      }
      strcat(name, t);
    } else {
      if (value[0]) {
        strcat(value, SPC);
      }
      strcat(value, t);
    }
  }
}

void set_position(tboard* b) {
  char *t = strtok(NULL, " ");
  if (!strcmp(t, "startpos")) {
    fen_to_board(NEW_BOARD, b);
  } else if (!strcmp(t, "fen")) {
    // the FEN notation has exactly six words
    char fen[100] = "";
    for (int i = 0; i < 6; i++) {
      if (i) {
        strcat(fen, SPC);
      }
      strcat(fen, strtok(NULL, " "));
    }
    fen_to_board(fen, b);
  } else {
    fatal((string)"UCI: Unknown position type [" + t + "]");
  }

  t = strtok(NULL, " ");
  if (t && !strcmp(t, "moves")) {
    while ((t = strtok(NULL, " ")) != NULL) {
      int error = execute_move(t, b);
      if (error) {
        fatal((string)"Incorrect move [" + t + "]");
      }
    }
  }

  // printboard(b);
}

void parse_go(tboard* b) {
  char* cmd;
  int my_time = 0, opp_time = 0; // in millis

  while ((cmd = strtok(NULL, " ")) != NULL) {
    if (!strcmp(cmd, "wtime") || !strcmp(cmd, "btime")) {
      if ((b->side == WHITE) ^ !strcmp(cmd, "btime")) {
        my_time = atoi(strtok(NULL, " "));
      } else {
        opp_time = atoi(strtok(NULL, " "));
      }
    } else if (!strcmp(cmd, "movestogo")) {
      strtok(NULL, " "); // ignore one argument
    } else {
      fatal((string)"Unknown go command [" + cmd + "]");
    }
  }

  int centis = allocate_time(my_time / 10, opp_time / 10); // convert to centis
  info((string)"[NILATAC] Thinking for " + to_string(centis) + " centis");

  tmove mv = find_best_move(b, centis);
  if (mv.from != -1) { // not stalemate
    cout << "bestmove " << movetostring(mv) << endl << flush;
  }
}

int main(void) {

  tboard b;

  init_common();
  init();
  restart();

  setbuf(stdout, NULL);
  char s[10000], name[1000], value[1000];
  while (fgets(s, 1000, stdin)) {
    s[strlen(s) - 1] = '\0';
    char* command = get_token(s);

    if (!strcmp(command, "uci")) {
      printf("id name Nilatac\n");
      printf("id author Cătălin Frâncu\n");
      printf("option name UCI_Variant type combo default suicide var suicide\n");
      printf("uciok\n");

    } else if (!strcmp(command, "isready")) {
      printf("readyok\n");

    } else if (!strcmp(command, "setoption")) {
      parse_option(name, value);
      if (!strcasecmp(name, "UCI_Variant")) {
        if (strcasecmp(value, "suicide")) {
          fatal("Nilatac only plays suicide");
        }
      } else {
        // ignore other options
      }

    } else if (!strcmp(command, "ucinewgame")) {
      restart();

    } else if (!strcmp(command, "position")) {
      set_position(&b);

    } else if (!strcmp(command, "go")) {
      parse_go(&b);

    } else if (!strcmp(command, "quit")) {
      break;

    } else if (!strcmp(command, "level")) {
      // We are only interested in the third argument, the increment
      get_token(NULL); get_token(NULL);
      g_increment = 100 * atoi(get_token(NULL));

    } else if (!strcmp(command, "name")) {
      string old_oppname = g_oppname;
      g_oppname = get_token(NULL);
      if (g_oppname != old_oppname)
        printf("tellall Hello %s! This is a *suicide* game. If you don't want "
               "to play suicide, type \"abort\" now.\n", g_oppname.c_str());

    } else if (!strcmp(command, "protover")) {
      info((string)"Protocol version " + get_token(NULL));

    } else if (!strcmp(command, "result")) {

    } else if (!strcmp(command, "xboard")) {
      info("Driven by xboard");

    } else if (!strcmp(command, "accepted") ||
               !strcmp(command, "hard") ||
               !strcmp(command, "random")) {
      // These commands will be silently ignored to produce less garbage

    } else {
      fatal((string)"Ignoring command " + command);
    }
  }

  info("Exiting...");
  return 0;
}
