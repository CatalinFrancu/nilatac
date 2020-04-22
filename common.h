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
#ifndef _COMMON_H
#define _COMMON_H
#include <string>
using namespace std;

// Use ifndef to allow these values to be overriden by the Makefile
#ifndef USE_BOOK
#define USE_BOOK    1
#endif

#ifndef USE_EGTB
#define USE_EGTB    1
#endif

#ifndef USE_HASH
#define USE_HASH    1
#endif

#ifndef WEAKENED
#define WEAKENED    0
#endif

// Command-line arguments.
extern string FLAGS_command;
extern char*  FLAGS_movelist;
extern int    FLAGS_port;
extern double FLAGS_pn_ratio;
extern int    FLAGS_max_depth;
extern bool   FLAGS_by_ratio;
extern int    FLAGS_save_every;

// Global variables, mostly for zippy
extern int g_increment; // in centiseconds
extern string g_oppname;
extern bool g_winning_line_found;

enum { EMPTY = 0, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

#define WHITE  1
#define BLACK -1
#define DRAW   0

#define INFTY 3000001
#define WIN 3000000

#define MINDEPTH 4
#define MAXDEPTH 100

extern const char* NEW_BOARD;
extern const char* EMPTY_BOARD;
#define z(a) ((b->side == 1) ? (a) : (-(a)))

// Bit-twiddling functions. Squares are 0-63, ranks and files are 0-7
#define rank(square) ((square) >> 3)
#define file(square) ((square) & 7)
#define assemble(rank, file) (((rank) << 3) ^ (file))
#define isafile(square) (((square) & 7) == 0)
#define ishfile(square) (((square) & 7) == 7)
#define isseventhrank(square) (((square) >> 3) == 1)
#define issecondrank(square) (((square) >> 3) == 6)
#define isedgerank(square) (!((square) >> 3) || ((square) >> 3) == 7)
// Light-square or dark-square? Rank and file must have the same parity.
#define bishop_parity(square) ((((square) >> 3) & 1) ^ ((square) & 1))

/**************************** Board information ******************************/

#define NO_EP_SQUARE 64
typedef unsigned long long u64;
typedef unsigned char byte;
typedef int boolean;
typedef struct {
  char b[64];
  int side, whitecount, blackcount;
  int epsquare; // 0..63 or NO_EP_SQUARE
  // Bishop counts for draw detection
  int wbishop[2], bbishop[2]; // 0 and 1 are for even-squared and odd-squared
  u64 hashValue;
} tboard;

typedef struct {
  char from, to, prom, epsquare;
} tmove;

extern const tmove INVALID_MOVE;

typedef struct {
  int count;
  tmove move[200];
} tmovelist;

// A saverec saves the differences between two boards;
typedef struct {
  char side, whiteCount, blackCount, epsquare, p1, p2;
  u64 hashValue;
  tmove m;
} tsaverec;

void set_alarm(int centis);
extern int timer_expired;

void info(string s);
void fatal(string s);
string board_to_string(tboard* b);
void fen_to_board(const char* s, tboard* b);
void printboard(tboard* b);
int same_move(tmove m1, tmove m2);
void printmovelist(tmovelist m);
void sortmovelist(tmovelist* m, int* values);

// Given a movelist, together with proof and disproof numbers for each
// move, compute p/n ratios and sort in decreasing order of p/n
void sort_by_pns_ratio(tmovelist* m, int* proof, int* disproof, double* ratio);

// Returns true if the board is sane.
int sane_board(tboard* b);

// Save/restore in sr the information about b that m would destroy
void saveboard(tboard*b, tmove mv, tsaverec *sr);
void restoreboard(tboard *b, tsaverec *sr);

void getallvalidmoves(tboard* b, tmovelist* m);
string movetostring(tmove m);
string move_to_san(tboard* b, tmove m);
tmove stringtomove(tboard* b, char* s);
tmove san_to_move(tboard* b, char *s);

// This crashes if the movelist is invalid from the starting position.
void string_to_movelist(char* s, tmovelist* ml);

void move(tboard* b, tmove mv);
int canmove(tboard* b);

void init_common();

#endif /* _COMMON_H */
