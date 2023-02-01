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
#ifndef _HASH_H
#define _HASH_H

#include "common.h"

#define HASHSIZE (1 << 22)        // Must be a power of 2
#define HASHMASK (HASHSIZE - 1)
#define MIN_HASH_DEPTH 2          // Do not hash positions at lower depths

enum { H_LE, H_EQ, H_GE };

typedef struct {
  u64 hash_value;
  u8 depth;
  int value;
  u8 type;
  u8 best_move;
} t_hash_entry;

extern t_hash_entry* hash_data;

void init_hash();

#define ADD_TO_HASH(_hash_value, _depth, _value, _type, _best_move) { \
  int spot = (_hash_value) & HASHMASK; \
  if (((_depth) >= MIN_HASH_DEPTH) && \
      ((_hash_value) != hash_data[spot].hash_value || \
       (_depth) >= hash_data[spot].depth)) { \
     hash_data[spot].hash_value = (_hash_value); \
     hash_data[spot].depth = (_depth); \
     hash_data[spot].value = (_value); \
     hash_data[spot].type = (_type); \
     hash_data[spot].best_move = (_best_move); \
  } \
}

#endif /* _HASH_H */
