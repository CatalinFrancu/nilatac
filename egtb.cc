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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <iostream>
#include <iomanip>
#include "common.h"
#include "egtb.h"

// Comments beginning with OPTIMIZING mark changes I've made so I can start
// optimizing the egtb. For example, I only iterate once per table in an
// attempt to reduce the distances.

/**************** Combinations table and handling functions ******************/

unsigned choose[65][65]; // combinations table
// Only defined for sum_comb[i][j][k] for j > i, k <= MEN
// which stores c(i, k) + ... + c(j, k)
unsigned sum_comb[65][65][MEN + 1];

unsigned comb_to_int2[64][64];
unsigned comb_to_int3[64][64][64];
unsigned comb_to_int_pp2[64][64];
unsigned comb_to_int_pp3[64][64][64];

unsigned int_to_comb2[2016][2];
unsigned int_to_comb3[41664][3];
unsigned int_to_comb_pp2[2016][2];
unsigned int_to_comb_pp3[41664][3];

#define comb_to_int(x, size, pawns)                       \
  (((size) == 1)                                          \
   ? ((pawns) ? ((x)[0] - 8) : (x)[0])                    \
   : (((size) == 2) ? ((pawns)                            \
                       ? comb_to_int_pp2[(x)[0]][(x)[1]]  \
                       : comb_to_int2[(x)[0]][(x)[1]])    \
      : ((pawns)                                          \
         ? comb_to_int_pp3[(x)[0]][(x)[1]][(x)[2]]        \
         : comb_to_int3[(x)[0]][(x)[1]][(x)[2]])))

#define int_to_comb(c, x, size, pawns)          \
  if ((size) == 1)                              \
    if ((pawns))                                \
      (x)[0] = (c) + 8;                         \
    else                                        \
      (x)[0] = (c);                             \
  else if ((size) == 2)                         \
    if ((pawns))                                \
      memcpy((x), int_to_comb_pp2[(c)], 8);     \
    else                                        \
      memcpy((x), int_to_comb2[(c)], 8);        \
  else                                          \
    if ((pawns))                                \
      memcpy((x), int_to_comb_pp3[(c)], 12);    \
    else                                        \
      memcpy((x), int_to_comb3[(c)], 12);

// Given size sorted numbers between 0 and 63, return the number of the
// combination (0 through C(64, size) - 1)
inline unsigned func_comb_to_int(int* x, int size) {
  int num = 0, num_squares = 64;
  for (int i = 0; i < size; i++) {
    // diff is the index of this piece relative to the one before it
    int diff = i ? (x[i] - x[i-1] - 1) : x[i];
    num += sum_comb[num_squares - diff][num_squares - 1][size - i - 1];
    num_squares -= (diff + 1);
  }
  return num;
}

// Given size sorted numbers between 8 and 55, return the number of the
// combination (0 through C(48, size) - 1. Pawns only have 48 legal squares.
inline unsigned func_comb_to_int_pp(int* x, int size) {
  int num = 0, num_squares = 48;
  for (int i = 0; i < size; i++) {
    // diff is the index of this piece relative to the one before it
    int diff = i ? (x[i] - x[i-1] - 1) : (x[i] - 8);
    num += sum_comb[num_squares - diff][num_squares - 1][size - i - 1];
    num_squares -= (diff + 1);
  }
  return num;
}

// Precompute the combinations table. Some of the values overflow, but we only
// need choose[n][k] for small values of k, so we're safe.
void precompute_comb() {
  for (int i = 0; i <= 64; i++)
    for (int j = 0; j <= i; j++)
      if (j == 0 || j == i)
        choose[i][j] = 1;
      else
        choose[i][j] = choose[i-1][j] + choose[i-1][j-1];

  // Now precompute sum_comb also;
  for (int k = 0; k <= MEN; k++)
    for (int i = 0; i <= 64; i++)
      for (int j = i; j <= 64; j++)
        if (j == i)
          sum_comb[i][j][k] = choose[j][k];
        else
          sum_comb[i][j][k] = sum_comb[i][j-1][k] + choose[j][k];

  // Now precompute comb_to_int and comb_to_int_pp and viceversa
  // Only precompute for ordered combinations
  int x[4];
  for (x[0] = 0; x[0] < 63; x[0]++)
    for (x[1] = x[0] + 1; x[1] < 64; x[1]++) {
      unsigned u = comb_to_int2[x[0]][x[1]] = func_comb_to_int(x, 2);
      int_to_comb2[u][0] = x[0];
      int_to_comb2[u][1] = x[1];
    }

  for (x[0] = 0; x[0] < 62; x[0]++)
    for (x[1] = x[0] + 1; x[1] < 63; x[1]++)
      for (x[2] = x[1] + 1; x[2] < 64; x[2]++) {
        unsigned u = comb_to_int3[x[0]][x[1]][x[2]] = func_comb_to_int(x, 3);
        int_to_comb3[u][0] = x[0];
        int_to_comb3[u][1] = x[1];
        int_to_comb3[u][2] = x[2];
      }

  for (x[0] = 8; x[0] < 55; x[0]++)
    for (x[1] = x[0] + 1; x[1] < 56; x[1]++) {
      unsigned u = comb_to_int_pp2[x[0]][x[1]] = func_comb_to_int_pp(x, 2);
      int_to_comb_pp2[u][0] = x[0];
      int_to_comb_pp2[u][1] = x[1];
    }

  for (x[0] = 8; x[0] < 54; x[0]++)
    for (x[1] = x[0] + 1; x[1] < 55; x[1]++)
      for (x[2] = x[1] + 1; x[2] < 56; x[2]++) {
        unsigned u =
          comb_to_int_pp3[x[0]][x[1]][x[2]] = func_comb_to_int_pp(x, 3);
        int_to_comb_pp3[u][0] = x[0];
        int_to_comb_pp3[u][1] = x[1];
        int_to_comb_pp3[u][2] = x[2];
      }
}

/***************************** Board transforms ******************************/

int transform_go[8][64]; // where does a square go after a transform
int transform_inverse[8] = { 0, 3, 2, 1, 4, 5, 6, 7 }; // Inverse transforms

// Tells us where square x goes after a transform. See below the transform
// types. Do not use this as-is. Instead, use transform_g[type][x], which is
// precomputed.
int transform(int x, int type) {
  int r = rank(x), f = file(x);
  switch (type) {
    case 0: return x;                      // Identity
    case 1: return assemble(f, 7 - r);     // Rot 90 clockwise
    case 2: return assemble(7 - r, 7 - f); // Rot 180
    case 3: return assemble(7 - f, r);     // Rot 270
    case 4: return assemble(r, 7 - f);     // Mirror against Oy
    case 5: return assemble(7 - f, 7 - r); // Mirror, Rot 90 clockwise
    case 6: return assemble(7 - r, f);     // Mirror, Rot 180
    case 7: return assemble(f, r);         // Mirror, Rot 270
    default: assert(0);
  }
}

// In-place bubble sort
inline void bubble_sort(int* x, int size) {
  int changed;
  do {
    changed = 0;
    for (int i = 0; i < size - 1; i++)
      if (x[i] > x[i+1]) {
        int t = x[i]; x[i] = x[i+1]; x[i+1] = t;
        changed = 1;
      }
  } while (changed);
}

// In-place transform that also sorts the vector after renumbering
void transform_combo(int *x, int size, int type) {
  for (int i = 0; i < size; i++)
    x[i] = transform_go[type][x[i]];
  bubble_sort(x, size);
}

void precompute_transforms() {
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 64; j++)
      transform_go[i][j] = transform(j, i);
}

void transform_board(tboard* b1, tboard *b2, int type) {
  b2->side = b1->side;
  b2->whitecount = b1->whitecount;
  b2->blackcount = b1->blackcount;
  if (b1->epsquare == NO_EP_SQUARE)
    b2->epsquare = NO_EP_SQUARE;
  else
    b2->epsquare = transform_go[type][b1->epsquare];
  for (int i = 0; i < 64; i++)
    b2->b[transform_go[type][i]] = b1->b[i];
}

// Basically apply a type 6 transform, but also switch colors and counts
void transform_switch_side(tboard* b1, tboard *b2) {
  b2->side = -b1->side;
  b2->whitecount = b1->blackcount;
  b2->blackcount = b1->whitecount;
  if (b1->epsquare == NO_EP_SQUARE)
    b2->epsquare = NO_EP_SQUARE;
  else
    b2->epsquare = transform_go[6][b1->epsquare];
  for (int i = 0; i < 64; i++)
    b2->b[transform_go[6][i]] = -b1->b[i];
}

/** Compute canonic combinations for pieces and pawns and discard symmetries */

typedef struct {
  int num_combos;      // 64 choose size
  int num_combos_pp;   // 48 choose size
  int num_canonics_p;  // How many remain after we cut all symmetries - pawns
  int num_canonics_np; // How many remain - no pawns
  int num_canonics_pp; // How many remain - pawns only (48 squares)
  // If a combo is canonic, then data stores its order number; otherwise,
  // data stores the *negative* of the transform we need to apply to
  // canonicalize it (-1 ... -7)
  int* data_p;
  int* data_np;
  int* data_pp;
} t_canonic_info;

t_canonic_info canonic[MEN];

void init_canonic(t_canonic_info* canonic, int size) {
  canonic->num_combos = choose[64][size];
  canonic->num_combos_pp = choose[48][size];
  canonic->num_canonics_p = canonic->num_canonics_np = 0;
  canonic->data_p = (int*)malloc(sizeof(int) * canonic->num_combos);
  canonic->data_np = (int*)malloc(sizeof(int) * canonic->num_combos);
  canonic->data_pp = (int*)malloc(sizeof(int) * canonic->num_combos_pp);

  int x[size], y[size];

  // CASE 1: PAWNS ALLOWED
  // For each possible combo, see if it is canonical (no pawns)
  for (int i = 0; i < canonic->num_combos; i++) {
    int_to_comb(i, x, size, 0);

    // Now try transform 4 and see if we get a combo that we already had
    memcpy(y, x, 16);
    transform_combo(y, size, 4); // Apply transform 4 (mirror against Oy)
    int new_i = comb_to_int(y, size, 0);
    if (new_i < i && canonic->data_p[new_i] >= 0) {
      canonic->data_p[i] = -4; // Remember the transform
    } else {
      canonic->data_p[i] = canonic->num_canonics_p++;
    }
  }

  // CASE 2: NO PAWNS ALLOWED
  // For each possible combo, see if it is canonical (no pawns)
  for (int i = 0; i < canonic->num_combos; i++) {
    int_to_comb(i, x, size, 0);

    // Now try every transform and see if we get a combo that we already had
    int j = 1, found = 0;
    while (j < 8 && !found) {
      memcpy(y, x, 16);
      transform_combo(y, size, j); // Apply transform j
      int new_i = comb_to_int(y, size, 0);
      if (new_i < i && canonic->data_np[new_i] >= 0) {
        found = 1;
        canonic->data_np[i] = -j; // Remember the transform
      }
      j++;
    }

    // It is canonical
    if (!found)
      canonic->data_np[i] = canonic->num_canonics_np++;
  }

  // CASE 3: PAWNS ALLOWED, and we're placing pawns on the board
  // For each possible combo, see if it is canonical (no pawns)
  for (int i = 0; i < canonic->num_combos_pp; i++) {
    int_to_comb(i, x, size, 1);

    // Now try transform 4 and see if we get a combo that we already had
    memcpy(y, x, 16);
    transform_combo(y, size, 4); // Apply transform 4 (mirror against Oy)
    int new_i = comb_to_int(y, size, 1);
    if (new_i < i && canonic->data_pp[new_i] >= 0)
      canonic->data_pp[i] = -4; // Remember the transform
    else
      canonic->data_pp[i] = canonic->num_canonics_pp++;
  }
}

// Initialize the canonic[] array
void init_canonics() {
  for (int i = 1; i < MEN; i++)
    init_canonic(canonic + i, i);
}

/************************** Compute the egtb index ***************************/

int val[12];             // Used by the backtracking below
unsigned egtb_file_size; // The total number of known positions
// The indexing order is KING .. PAWN, -KING .. -PAWN, EMPTY
// At which position (0..3.2G) does this piece configuration start and end?
unsigned egtb_start[12][12][13][13];
unsigned egtb_end[12][12][13][13];

// What combination of pieces begins at this index?
typedef struct {
  unsigned start;
  char p[MEN];
} t_index_to_pieceset_map;
t_index_to_pieceset_map index_to_pieceset[1500];
int index_to_pieceset_size;

// This is a simple backtracking that generates all possible combinations
// of up to MEN pieces and counts the number of placement for each combination
void backtrack(int men, int level) {
  if (level == 12) {
    if (men == MEN) return; // No pieces placed
    if (!val[0] && !val[1] && !val[2] && !val[3] && !val[4] && !val[5])
      return; // White has no pieces -- impossible
    if (!val[6] && !val[7] && !val[8] && !val[9] && !val[10] && !val[11])
      return; // Black has no pieces -- impossible
    int first = 1;
    int any_pawns = val[5] || val[11];
    int total = 1;

    for (int i = 0; i < 12; i++)
      if (val[i]) {
        if (first) {
          total = any_pawns
            ? ((i == 5 || i == 11) ? canonic[val[i]].num_canonics_pp
               : canonic[val[i]].num_canonics_p)
            : canonic[val[i]].num_canonics_np;
          first = 0;
        } else {
          if (i == 5 || i == 11) // pawns
            total *= choose[48][val[i]];
          else
            total *= choose[64][val[i]];
        }
      }

    int p[4] = { 12, 12, 12, 12 }; // get the piece types

    int k = 0;
    for (int i = 0; i < 12; i++)
      for (int j = 1; j <= val[i]; j++)
        p[k++] = i;

    egtb_start[p[0]][p[1]][p[2]][p[3]] = egtb_file_size;
    egtb_end[p[0]][p[1]][p[2]][p[3]] = egtb_file_size + total;
    index_to_pieceset[index_to_pieceset_size].start = egtb_file_size;
    for (int i = 0; i < MEN; i++)
      index_to_pieceset[index_to_pieceset_size].p[i] = p[i];
    index_to_pieceset_size++;
    egtb_file_size += total;

  } else {
    for (int i = men; i >= 0; i--) {
      val[level] = i;
      backtrack(men - i, level + 1);
    }
  }
}

// This precomputes a table of the form egtb_start[p1][p2][p3][p4]
// which tells us at what position in the file the egtb for p1p2p3p4 starts
// (e.g. KKnn)
void init_egtb_index() {
  memset(egtb_start, 0, sizeof(egtb_start));
  memset(egtb_end, 0, sizeof(egtb_end));
  egtb_file_size = 0;
  index_to_pieceset_size = 0;
  backtrack(MEN, 0);
  index_to_pieceset[index_to_pieceset_size].start = egtb_file_size; // sentry
  cerr << "[EGTB] Initialized, " << egtb_file_size << " positions\n";
}

/************************ EGTB file handling *********************************/

FILE* egtb_files[4];

void open_egtb_files() {
  egtb_files[0] = fopen("egtb/egtb0.in", "rt");
  egtb_files[1] = fopen("egtb/egtb1.in", "rt");
  egtb_files[2] = fopen("egtb/egtb2.in", "rt");
  egtb_files[3] = fopen("egtb/egtb3.in", "rt");
}

// We store the distance to mate in plies, one byte per position. 0 through 125
// mean white wins, -1 through -126 mean black wins and they are SHIFTED BY 1.
// -127 means unknown and 127 means drawn.

// This is low-level, so it doesn't call write_egtb_byte() nicely
// This DELETES your egtb.
void clear_egtb_files() {
  // Commenting out: Convert to multiple files before using
  //   int desc = open(EGTB_FILENAME, O_RDWR);
  //   FILE* f = fdopen(desc, "wt");
  //   char buf[1 << 16];

  //   memset(buf, EGTB_UNKNOWN, sizeof(buf));

  //   for (unsigned i = 0; i < egtb_file_size / sizeof(buf); i++)
  //     assert(fwrite(buf, sizeof(buf), 1, f) == 1);
  //   assert(fwrite(buf, egtb_file_size % sizeof(buf), 1, f) == 1);
  //   fclose(f);
}

int read_egtb_byte(unsigned pos) {
  FILE* f = egtb_files[pos >> 30];
  assert(!fseek(f, pos & 0x3FFFFFFF, SEEK_SET));
  return (char)fgetc(f);
}

void write_egtb_byte(unsigned index, int c) {
  FILE* f = egtb_files[index >> 30];
  assert(!fseek(f, index & 0x3FFFFFFF, SEEK_SET));
  fputc(c, f);
}

void write_egtb_sequence(char* src, unsigned index, unsigned count) {
  // Commenting out: Convert to multiple files before using
  // Pay attention, you may need to cross a gigabyte boundary
  //   if (!count || !src) return;
  //   assert(!fseek(egtb_file, index, SEEK_SET));
  //   assert(fwrite(src, count, 1, egtb_file) == 1);
}

/***************************** Graph storage *********************************/

#define MAX_NODES 13000000
#define MAX_EDGES 480000000 // For 160M edges
unsigned char* edges = NULL;
int num_edges;
char list_size[MAX_NODES];
char old_score[MAX_NODES]; // Some nodes have no children
unsigned start1, start2, size1, size2;

void int_list_init(int index1, int index2) {
  start1 = index_to_pieceset[index1].start;
  size1 = index_to_pieceset[index1 + 1].start - start1;
  if (index2 != index1) {
    start2 = index_to_pieceset[index2].start;
    size2 = index_to_pieceset[index2 + 1].start - start2;
  } else {
    start2 = size2 = 0;
  }
  assert(size1 + size2 <= MAX_NODES);
  for (unsigned i = 0; i < size1 + size2; i++)
    list_size[i] = 0;
  num_edges = 0;

  for (unsigned i = 0; i < size1; i++)
    old_score[i] = read_egtb_byte(i + start1);
  for (unsigned i = 0; i < size2; i++)
    old_score[size1 + i] = read_egtb_byte(i + start2);
}

void int_list_clear() {
  cerr << "Dumping tables to egtb file...\n";
  write_egtb_sequence(old_score, start1, size1);
  write_egtb_sequence(old_score + size1, start2, size2);
  for (unsigned i = 0; i < size1 + size2; i++) {
    list_size[i] = 0;
    old_score[i] = EGTB_UNKNOWN;
  }
  num_edges = 0;
}

inline unsigned get_edge() {
  unsigned x = (edges[num_edges] << 16) | (edges[num_edges + 1] << 8)
    | edges[num_edges + 2];
  num_edges += 3;
  return x;
}

inline void set_edge(unsigned x) {
  assert(num_edges + 3 <= MAX_EDGES);
  edges[num_edges] = x >> 16;
  edges[num_edges + 1] = (x >> 8) & 0xff;
  edges[num_edges + 2] = x & 0xff;
  num_edges += 3;
  //  cerr << "Added edge " << x << endl;
}

// Config k is a predecessor of config x
// Store size1 + size2 elements; start1 and start2 are the offsets
// Trivial configurations (no descendants) should be stored as
// 4,000,000,000 + score
inline void int_list_add(unsigned k, unsigned x) {
  assert((k >= start1 && k < start1 + size1) ||
         (k >= start2 && k < start2 + size2));
  int place = (k >= start1 && k < start1 + size1)
    ? k - start1
    : size1 + (k - start2);
  list_size[place]++;

  //  cerr << "Place " << place << " x " << x << endl;
  if (x >= start1 && x < start1 + size1)
    set_edge(x - start1);
  else if (x >= start2 && x < start2 + size2)
    set_edge(x - start2 + size1);
  else if (x + 200 > 4000000000u)
    set_edge(x - 4000000000u + 16000000);
  else
    set_edge(16000000 + read_egtb_byte(x));
}

/***************************** Exploiting EGTB *******************************/

#define index_convert(x) (((x) > 0) ? (6 - (x)) : ((x) + 12))
int index_revert[12] = { KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN,
                         -KING, -QUEEN, -ROOK, -BISHOP, -KNIGHT, -PAWN };

// From board b, constructs
// - a sorted array (in the order above) with the 4 pieces, including empty
// - a sorted array with the positions of the pieces
void egtb_build_vectors(tboard* b, int* index, int* position, int* any_pawns) {
  for (int i = 0; i < MEN; index[i++] = 12); // No pieces yet
  int num_pieces = 0;
  *any_pawns = 0;
  for (int i = 0; i < 64; i++)
    if (b->b[i]) {
      int piece = index_convert(b->b[i]);
      index[num_pieces] = piece;
      position[num_pieces++] = i;
      if (abs(b->b[i]) == PAWN) *any_pawns = 1;
    }

  // Bubble sort the indices and the positions together: first by index, then
  // by positions as a tie breaker
  int changed;
  do {
    changed = 0;
    for (int i = 0; i < num_pieces - 1; i++)
      if (index[i] > index[i+1] ||
          (index[i] == index[i+1] && position[i] > position[i+1])) {
        int t = index[i]; index[i] = index[i+1]; index[i+1] = t;
        t = position[i]; position[i] = position[i+1]; position[i+1] = t;
        changed = 1;
      }
  } while (changed);
}

// Sets some values:
// b2: the canonicalized board
// transform_applied: number of transform we applied to make b2 canonic
// num_canonics: number of existing categories based on the first set of
//   indexing pieces
// index_canonic: the index of b2 within this array of categories
// if transform > 0, it CLOBBERS position!
void transform_to_canonic(tboard *b1, tboard *b2, int* position,
                          int size, int any_pawns, int* transform_applied,
                          unsigned* num_canonics, unsigned* index_canonic) {
  *transform_applied = 0;
  if (abs(b1->b[position[0]]) == PAWN) {
    // Index by pawns
    *num_canonics = canonic[size].num_canonics_pp;
    *index_canonic = comb_to_int(position, size, 1);
    *transform_applied = canonic[size].data_pp[*index_canonic];
  } else if (any_pawns) {
    *num_canonics = canonic[size].num_canonics_p;
    *index_canonic = comb_to_int(position, size, 0);
    *transform_applied = canonic[size].data_p[*index_canonic];
  } else { // no pawns at all
    *num_canonics = canonic[size].num_canonics_np;
    *index_canonic = comb_to_int(position, size, 0);
    *transform_applied = canonic[size].data_np[*index_canonic];
  }

  if (*transform_applied >= 0) {
    // We don't need to apply any transform and we already have the index.
    *index_canonic = *transform_applied;
    *transform_applied = 0;
  } else {
    *transform_applied = -*transform_applied;
  }

  // Apply the transform anyway, even if only to copy the board
  transform_board(b1, b2, *transform_applied);

  // Now, if any transform was applied, then recompute the index
  if (*transform_applied) {
    transform_combo(position, size, *transform_applied);
    if (abs(b2->b[position[0]]) == PAWN)
      *index_canonic = canonic[size].data_pp[comb_to_int(position, size, 1)];
    else if (any_pawns)
      *index_canonic = canonic[size].data_p[comb_to_int(position, size, 0)];
    else // no pawns
      *index_canonic = canonic[size].data_np[comb_to_int(position, size, 0)];

    assert(*index_canonic >= 0);
  }
}

// Restrict the interval start-end to a chunk thereof, based on the combination
// given by position[lo]..position[lo+size-1]
inline void egtb_narrow_interval(int piece, int* position, int lo, int size,
                                 unsigned* start, unsigned* end) {
  unsigned my_index, num_chunks;
  if (piece == 5 || piece == 11) {
    my_index = comb_to_int(position + lo, size, 1);
    num_chunks = choose[48][size];
  } else {
    my_index = comb_to_int(position + lo, size, 0);
    num_chunks = choose[64][size];
  }

  unsigned chunk_size = (*end - *start) / num_chunks;
  *start += my_index * chunk_size;
  *end = *start + chunk_size;
}

// Look up a position, return its index
unsigned egtb_get_index(tboard* b) {
  // We only store positions where white is to move
  tboard b_switched, b_canonic;
  if (b->side == BLACK) {
    transform_switch_side(b, &b_switched);
    b = &b_switched;
  }

  // Build the characteristic vectors
  // index: what the pieces are, in sorted order
  // positions: the pieces' position, in the same order as index
  int piece[MEN], position[MEN], any_pawns;
  egtb_build_vectors(b, piece, position, &any_pawns);

  // Find and apply the needed transform
  int num_indexed_pieces = 1;
  while (piece[num_indexed_pieces] == piece[0])
    num_indexed_pieces++;

  unsigned num_canonic_combos, my_index;
  int transform_applied;
  transform_to_canonic(b, &b_canonic, position, num_indexed_pieces, any_pawns,
                       &transform_applied, &num_canonic_combos, &my_index);

  if (transform_applied > 0)
    egtb_build_vectors(&b_canonic, piece, position, &any_pawns);

  unsigned start = egtb_start[piece[0]][piece[1]][piece[2]][piece[3]];
  unsigned end = egtb_end[piece[0]][piece[1]][piece[2]][piece[3]];
  unsigned chunk_size = (end - start) / num_canonic_combos;
  start += my_index * chunk_size;
  end = start + chunk_size;

  // Now continue shrinking the interval based on the other piece types.
  int j = num_indexed_pieces;
  while (j < MEN && piece[j] < 12) { // still have piece types to handle
    int k = j + 1;
    while (k < MEN && piece[k] == piece[j]) k++;
    egtb_narrow_interval(piece[j], position, j, k - j, &start, &end);
    j = k;
  }

  return start;
}

// Look up a position, return the perfect score
// Safe to call on a position that has more than MEN pieces
// Assumes that the position is in the index, i.e. there is at least one
// piece of each color and there are at most MEN pieces in total
int egtb_lookup(tboard* b) {
  unsigned index = egtb_get_index(b);
  return read_egtb_byte(index);
}

// Checks if the position is in the index and solves trivial wins
int egtb_lookup_inclusive(tboard *b) {
  if (!b->whitecount)
    return (b->side == WHITE) ? 0 : -1;
  if (!b->blackcount)
    return (b->side == BLACK) ? 0 : -1;
  if (b->whitecount + b->blackcount > MEN)
    return EGTB_UNKNOWN;
  unsigned index = egtb_get_index(b);
  return read_egtb_byte(index);
}

int egtb_lookup_by_index(unsigned index) {
  if (index == EGTB_NOT_IN_INDEX)
    return EGTB_UNKNOWN;
  else
    return read_egtb_byte(index);
}

/****************************** Create a table *******************************/

int board_count, solved_count;
int max_update; // Maximum decrease in distance

// Returns 1 iff there was a conflict (and changes b anyway)
int place_pieces(tboard* b, int piece_type, int* coords, int size) {
  assert(piece_type != 12);
  int piece = index_revert[piece_type];
  for (int i = 0; i < size; i++) {
    if (b->b[coords[i]] != EMPTY) return 1;
    b->b[coords[i]] = piece;
  }
  if (piece > 0)
    b->whitecount += size;
  else b->blackcount += size;
  return 0;
}

// This procedure takes an index and produces the corresponding board.
// It also needs to know its index in the index_to_pieceset map, so
// that it knows the pieceset. Finally, it needs to know the index of
// the noncanonic piece combo, so that it can place the first bunch of
// pieces correctly. Returns 1 if there was a conflict (which means
// this index is invalid), 0 otherwise
int egtb_get_position(tboard* b, int map_index, int noncanonic_index,
                      unsigned index_rest, unsigned size_rest) {
  fen_to_board(EMPTY_BOARD, b);
  char* pieces = index_to_pieceset[map_index].p;
  int set_size = 1;
  while (pieces[set_size] == pieces[0]) set_size++;

  int position[MEN];
  int_to_comb(noncanonic_index, position, set_size,
              (pieces[0] == 5 || pieces[0] == 11));

  // Place the first set of pieces
  assert(!place_pieces(b, pieces[0], position, set_size));
  unsigned start = 0;
  unsigned end = size_rest;

  // Now place the other sets of pieces;
  int j = set_size;
  while (j < MEN && pieces[j] < 12) { // still have piece types to handle
    int k = j + 1;
    while (k < MEN && pieces[k] == pieces[j]) k++;
    // Place set pieces[j..k-1]
    unsigned pawn = (pieces[j] == 5 || pieces[j] == 11);
    unsigned num_chunks = pawn ? choose[48][k - j] : choose[64][k - j];
    unsigned chunk_size = (end - start) / num_chunks;
    unsigned my_chunk = (index_rest - start) / chunk_size;
    int_to_comb(my_chunk, position, k - j, pawn);

    if (place_pieces(b, pieces[j], position, k - j))
      return 1; // Conflict

    start += my_chunk * chunk_size;
    end = start + chunk_size;
    j = k;
  }

  assert(start + 1 == end);
  return 0;
}


// Take an array of chars between 0 and 11 describing a configuration and
// return a printable string of the form KQvBN
string config_to_string(char* pieceset) {
  char piece_names[13] = "KQRBNPKQRBNP";
  int first_opponent = 1;
  string s = "";
  for (int i = 0; i < MEN && pieceset[i] != 12; i++) {
    if (pieceset[i] >= 6 && first_opponent) {
      s += "v";
      first_opponent = 0;
    }
    s += piece_names[pieceset[i]];
  }
  return s;
}

int complement_map_index(int map_index) {
  char* pieces = index_to_pieceset[map_index].p;
  char pieces2[MEN];
  int j = 0;

  int i = 0;
  while (pieces[i] <= 5) i++; // Find the first piece of the opponent

  while (pieces[i] <= 11 && i < MEN)
    pieces2[j++] = pieces[i++] - 6; // Copy opponent's pieces

  i = 0;
  while (pieces[i] <= 5)
    pieces2[j++] = pieces[i++] + 6; // Copy my pieces

  while (j < MEN) pieces2[j++] = 12; // Fill up the vector

  for (int k = 0; k < index_to_pieceset_size; k++)
    if (!memcmp(index_to_pieceset[k].p, pieces2, MEN))
      return k;

  assert(0);
}

void egtb_compute_graph_node(tboard* b, int index) {
  tmovelist ml;
  getallvalidmoves(b, &ml);
  for (int i = 0; i < ml.count; i++) {
    tboard new_b = *b;
    move(&new_b, ml.move[i]);

    if (!new_b.whitecount)
      int_list_add(index, 4000000000u + ((new_b.side == WHITE) ? 0 : -1));
    else if (!new_b.blackcount)
      int_list_add(index, 4000000000u + ((new_b.side == BLACK) ? 0 : -1));
    else {
      unsigned child_index = egtb_get_index(&new_b);
      int_list_add(index, child_index);
    }
  }
}

void egtb_compute_graph(int map_index) {
  char* pieces = index_to_pieceset[map_index].p;

  // Compute the indexing set size and see if we have any pawns
  int set_size = 1;
  while (pieces[set_size] == pieces[0]) set_size++;
  int any_pawns = 0;
  for (int i = 0; i < 4; i++)
    if (pieces[i] == 5 || pieces[i] == 11)
      any_pawns = 1;

  // Find the number of chunks and the list of canonic configurations for
  // the indexing set
  unsigned num_chunks;
  int *data;
  if (pieces[0] == 5 || pieces[0] == 11) {
    num_chunks = canonic[set_size].num_canonics_pp;
    data = canonic[set_size].data_pp;
  } else if (any_pawns) {
    num_chunks = canonic[set_size].num_canonics_p;
    data = canonic[set_size].data_p;
  } else {
    num_chunks = canonic[set_size].num_canonics_np;
    data = canonic[set_size].data_np;
  }
  unsigned chunk_size = (index_to_pieceset[map_index + 1].start -
                         index_to_pieceset[map_index].start) / num_chunks;
  assert(chunk_size * num_chunks == index_to_pieceset[map_index + 1].start -
         index_to_pieceset[map_index].start);

  int noncanonic_index = -1;
  cerr << num_chunks << " chunks:";
  for (unsigned i = 0; i < num_chunks; i++) {
    cerr << " " << i;
    do noncanonic_index++;
    while (data[noncanonic_index] < 0);
    for (unsigned j = 0; j < chunk_size; j++) {
      tboard b;
      if (!egtb_get_position(&b, map_index, noncanonic_index, j,
                             chunk_size))
        egtb_compute_graph_node(&b, index_to_pieceset[map_index].start +
                                i * chunk_size + j);
    }
  }
  cerr << endl;
}

void egtb_loop() {
  num_edges = 0; // So we can start reading the edges
  for (unsigned i = 0; i < size1 + size2; i++) {
    int old = old_score[i];
    if (old != EGTB_UNKNOWN && list_size[i]) {
      int child_score;
      int unknown = 0, won = 0, lost = 0, drawn = 0;
      int shortest_win = 1000, longest_loss = -1;

      for (int l = 0; l < list_size[i]; l++) {
        unsigned child = get_edge();
        if (child + 200 > 16000000)
          // This node is final;
          child_score = child - 16000000;
        else
          child_score = old_score[child];
        assert(child_score >= -128 && child_score < 128) ;

        if (child_score == EGTB_UNKNOWN)
          unknown++;
        else if (child_score == EGTB_DRAW)
          drawn++;
        else if (child_score >= 0) { // opponent is winning this one
          lost++;
          if (child_score > longest_loss) longest_loss = child_score;
        } else { // opponent is losing this one
          won++;
          child_score = -child_score - 1;
          if (child_score < shortest_win) shortest_win = child_score;
        }
      }

      int new_score;
      if (won)
        new_score = (shortest_win < 124)
          ? (shortest_win + 1)
          : EGTB_DRAW;
      else if (!unknown && ! drawn && !won) // All lost, all lost
        new_score = (longest_loss >= 124)
          ? EGTB_DRAW
          : (-longest_loss - 2);
      else if (!unknown)
        new_score = EGTB_DRAW;
      else
        new_score = EGTB_UNKNOWN;

      if (new_score != old) {
        old_score[i] = new_score;
        if (abs(new_score - old) > max_update)
          max_update = abs(new_score - old);
        solved_count++;
      }
    }
  }
}

void solve_configuration(int map_index) {
  int map_index2 = complement_map_index(map_index);
  if (map_index2 < map_index) return; // We've been here already
  cerr << "Analyzing " << config_to_string(index_to_pieceset[map_index].p);
  if (map_index2 > map_index)
    cerr << " and " << config_to_string(index_to_pieceset[map_index2].p);
  cerr << "...\n";
  // Repeatedly generate all the positions with this piece set until we cannot
  // learn anything new.
  int iteration = 0;
  int t1 = time(NULL);

  cerr << "Computing " << (map_index2 > map_index ? 2 : 1) << " graphs...\n";
  int_list_init(map_index, map_index2);
  egtb_compute_graph(map_index);
  if (map_index2 > map_index)
    egtb_compute_graph(map_index2);
  cerr << "Graphs computed! " << size1 + size2 << " nodes, "
       << (num_edges / 3) << " edges.\n";

  do {
    iteration++;
    // Per-iteration statistics;
    board_count = solved_count = max_update = 0;
    egtb_loop();
    cerr << "Iteration " << iteration << " done. "
         << "Solved " << solved_count
         << ", max improvement " << max_update << endl;
  } while (solved_count && iteration < 124);
  int_list_clear();

  // Now loop once more to mark everything that's still UNKNOWN with DRAW
  // OPTIMIZING: Uncomment these
  // board_count = solved_count = 0;
  // egtb_loop(map_index, 1);
  // if (map_index2 != map_index) egtb_loop(map_index2, 1);

  // This configuration is completed. Print some statistics.
  int t2 = time(NULL);
  cerr << t2 - t1 << " seconds.\n";
}

void egtb_create(int min_config, int max_config) {
  if (!edges) edges = (unsigned char*)malloc(MAX_EDGES);
  int config = 0;
  for (int size = 2; size <= MEN; size++)
    for (int allow_pawns = 0; allow_pawns <= size; allow_pawns++)
      for (int map_index = 0; map_index < index_to_pieceset_size;
           map_index++) {
        // First, make sure that this configuration does have size pieces and
        // has the requested number of pawns
        char* pieces = index_to_pieceset[map_index].p;
        int piece_count = 0;
        int has_pawns = 0;
        for (piece_count = 0;
             piece_count < MEN && pieces[piece_count] != 12;
             piece_count++) {
          if (pieces[piece_count] == 5 || pieces[piece_count] == 11)
            has_pawns++;
        }
        if (piece_count == size && has_pawns == allow_pawns) {
          if (config >= min_config) {
            // Yes, we can solve this configuration
            cerr << "Configuration " << config << " ----------------\n";
            solve_configuration(map_index);
          }
          if (++config == max_config)
            return;
        }
      }
}

// NNvNP -> number between 0 and 1401
// Returns -1 if it cannot find the config
int string_to_config(string piecenames) {
  int config = 0;
  for (int size = 2; size <= MEN; size++)
    for (int allow_pawns = 0; allow_pawns <= size; allow_pawns++)
      for (int map_index = 0; map_index < index_to_pieceset_size;
           map_index++) {
        // First, make sure that this configuration does have size pieces and
        // has the requested number of pawns
        char* pieces = index_to_pieceset[map_index].p;
        int piece_count = 0;
        int has_pawns = 0;
        for (piece_count = 0;
             piece_count < MEN && pieces[piece_count] != 12;
             piece_count++) {
          if (pieces[piece_count] == 5 || pieces[piece_count] == 11)
            has_pawns++;
        }
        if (piece_count == size && has_pawns == allow_pawns) {
          if (config_to_string(index_to_pieceset[map_index].p) == piecenames)
            return config;
          if (++config == index_to_pieceset_size)
            return -1;
        }
      }
  return -1;
}

int egtb_inited = 0;

void init_egtb() {
  if (egtb_inited) return; // Don't initialize twice
  precompute_comb();
  precompute_transforms();
  init_canonics();
  init_egtb_index();
  open_egtb_files();
  egtb_inited = 1;
}
