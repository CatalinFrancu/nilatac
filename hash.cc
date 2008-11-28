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
#include <iomanip>
#include <stdlib.h>
#include "hash.h"

t_hash_entry* hash_data;

void init_hash() {
  if (USE_HASH) {
    hash_data = (t_hash_entry*)malloc(HASHSIZE * sizeof(t_hash_entry));
    cerr << "[HASH] Initialized, " << HASHSIZE << " entries, "
         << HASHSIZE * sizeof(t_hash_entry) << " bytes" << endl;
  }
}
