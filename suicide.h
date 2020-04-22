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
#ifndef _SUICIDE_H
#define _SUICIDE_H

#include "common.h"

void init(); // needs to be here as it calls preprocessMoves()
void restart();
int canmove(tboard* b);
void play_opening();
tmove find_best_move(tboard* b, int centis);
// returns 0 for success, -1 for error
int execute_move(char* s, tboard* b);

#endif /* _SUICIDE_H */
