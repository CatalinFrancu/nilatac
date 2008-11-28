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
#include <stdio.h>
#include "common.h"
#include "suicide.h"

inline char* get_token(char* s) {
  return strtok(s, " \t\n");
}

int allocate_time() {
  // If I have under 5 seconds left, or I am more than 3 seconds behind the
  // opponent with less than 20 seconds left, move quickly.
  if (g_time < 500 ||
      (g_time <= 2000 && (g_time < g_opptime - 300)))
    return 10; // 0.1 seconds

  return g_time / 15 + g_increment * 2 / 3;
}

int main(void) {
  // Quickly send features to xboard
  setbuf(stdout, NULL);
  puts("feature analyze=0");
  puts("feature colors=0");
  puts("feature myname=\"Nilatac\"");
  puts("feature name=1");
  puts("feature setboard=1");
  puts("feature sigint=0");
  puts("feature sigterm=0");
  puts("feature usermove=1");
  puts("feature variants=\"suicide\"");
  puts("feature done=1");

  char s[100];
  init_common();
  init();
  restart();
  set_alarm(KEEP_ALIVE * 100);

  while (fgets(s, 100, stdin)) {
    s[strlen(s) - 1] = '\0';
    char* command = get_token(s);
    
    if (!strcmp(command, "draw")) {
      // Accept draw (a) if we offered one, (b) if 7 moves have passed, but
      // unless (c) we have found a win
      if ((g_offered_draw || g_reversible >= 14) && !g_winning_line_found) {
	printf("tellall Agreed, %s!\n", g_oppname.c_str());
	printf("offer draw");
      } else if (g_winning_line_found) {
	puts("tellall Nice try, but I already have a winning line");
	puts("tellics decline");
      } else { // g_reversible < 14 && !g_offered_draw
	printf("tellall Sorry %s, but I only accept draws after 7 "
               "reversible moves (no captures, no pawn pushes)\n",
               g_oppname.c_str());
	puts("tellics decline");
      }

    } else if (!strcmp(command, "force")) {
      g_force = true;

    } else if (!strcmp(command, "go")) {
      g_force = false;
      play_best_move(allocate_time());

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
      g_playing = true;

    } else if (!strcmp(command, "new")) {
      restart();

    } else if (!strcmp(s, "otim")) {
      g_opptime = atoi(get_token(NULL));

    } else if (!strcmp(command, "protover")) {
      info((string)"Protocol version " + get_token(NULL));

    } else if (!strcmp(command, "quit")) {
      break;

    } else if (!strcmp(command, "result")) {
      g_playing = false;
      set_alarm(KEEP_ALIVE * 100);

    } else if (!strcmp(command, "setboard")) {
      fen_to_board(s + strlen("setboard "), &b); // skip the space as well

    } else if (!strcmp(command, "time")) {
      g_time = atoi(get_token(NULL));

    } else if (!strncmp(s, "usermove ", 5)) {
      // TODO: Warn xboard of illegal moves
      int error = execute_move(get_token(NULL), &b);
      if (!error && !g_force)
	play_best_move(allocate_time());

    } else if (!strcmp(command, "variant")) {
      if (strcmp(get_token(NULL), "suicide"))
	fatal("Nilatac only plays suicide");

    } else if (!strcmp(command, "xboard")) {
      info("Driven by xboard");

    } else if (!strcmp(command, "accepted") ||
	       !strcmp(command, "hard") ||
	       !strcmp(command, "random")) {
      // These commands will be silently ignored to produce less garbage

    } else {
      info((string)"Ignoring command " + command);
    }
  }

  info("Exiting...");
  return 0;
}
