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
#ifndef _EGTB_H
#define _EGTB_H

#include "common.h"

#define EGTB_DIRNAME "egtb"
#define MEN 4

#define EGTB_UNKNOWN -127
#define EGTB_DRAW 127
#define EGTB_MAX_VALUE 125 // Never store wins in more than this many plies
#define EGTB_NOT_IN_INDEX 4000000000u

void init_egtb(const char* egtb_dir);

// Look up a position, return the perfect score
// Assumes that the position is in the index
int egtb_lookup(tboard* b);

// Checks if the position is in the index and solves trivial wins
int egtb_lookup_inclusive(tboard *b);

// Look up a position, return its index
unsigned egtb_get_index(tboard* b);

void egtb_create(int min_config, int max_config);

void clear_egtb_file();

// NNvNP -> number between 0 and 1401
// Dies if it cannot find the config
int string_to_config(string piecenames);

#endif /* _EGTB_H */
