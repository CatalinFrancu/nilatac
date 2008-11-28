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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <sys/time.h>
#include <stdlib.h>
#include "common.h"

// Command-line arguments. Default values here.
string FLAGS_command = "";
char*  FLAGS_movelist = NULL;
int    FLAGS_port = 5000;
double FLAGS_pn_ratio = 0.001;
int    FLAGS_max_depth = 6;
bool   FLAGS_by_ratio = false;  // Select book mpn by ratio or by proof number
int    FLAGS_save_every = 5;

// Global variables, mostly for zippy
int g_time = 0;
int g_opptime = 0;
int g_increment = 0;     // all in centiseconds
bool g_force = false;    // when true, do not automatically move
string g_oppname = "";
int g_reversible = 0;    // number of reversible half-moves
bool g_offered_draw = false;
bool g_winning_line_found = false;
bool g_playing = false;

const tmove INVALID_MOVE = { -1, -1, -1, -1 };
const char piecename[8] = " PNBRQK";
const char extended_piecename[14] = "kqrbnp-PNBRQK";
int namepiece[128];
tboard b;
int timer_expired;

typedef struct {
  u64 z[2 * KING + 1][64];
  u64 sideZ; // This is only XOR'ed when side == BLACK
  u64 epZ[NO_EP_SQUARE + 1]; // Values for the en passant square
} tzobrist;

tzobrist zobrist;

/********************************* I/O ***************************************/

void info(string s) {
  cerr << "[NILATAC] " << s << endl;
}

void fatal(string s) {
  cerr << "[FATAL] " << s << endl;
  exit(1);
}

string board_to_string(tboard* b) {
  string s = "";
  for (int i = 0; i < 64; i++)
    s += extended_piecename[b->b[i] + KING];
  s += (b->side == WHITE) ? "W" : "B";
  s += (b->epsquare == NO_EP_SQUARE) ? '8' : ('0' + file(b->epsquare));
  return s;
}

void recompute_zobrist(tboard* b) {
  b->whitecount = b->blackcount = b->hashValue = 0;
  b->wbishop[0] = b->wbishop[1] = b->bbishop[0] = b->bbishop[1] = 0;
  for (int i = 0; i < 64; i++) {
    if (b->b[i] != EMPTY) b->hashValue ^= zobrist.z[b->b[i] + KING][i];
    if (b->b[i] > 0)
      b->whitecount++;
    if (b->b[i] < 0)
      b->blackcount++;
    if (b->b[i] == BISHOP)
      b->wbishop[bishop_parity(i)]++;
    if (b->b[i] == -BISHOP)
      b->bbishop[bishop_parity(i)]++;
  }
  if (b->side == BLACK) b->hashValue ^= zobrist.sideZ;
  b->hashValue ^= zobrist.epZ[b->epsquare];
}

// Sets a board according to a FEN notation. If the FEN is incorrect, no
// results are guaranteed.
void fen_to_board(char* s, tboard *b) {
  // board
  int i = 0;
  while (i < 64) {
    char c = *(s++);
    if (c >= '1' && c <= '8') {
      int num_spaces = c - '0';
      if ((i / 8) - ((i + num_spaces - 1) / 8) > 0) {
	cerr << "Bad FEN1\n"; return;
      }
      while (num_spaces--) b->b[i++] = EMPTY;
    } else if (namepiece[c] != EMPTY) {
      b->b[i++] = namepiece[c];
    } else {
      cerr << "Bad FEN2\n"; return;
    }

    if ((file(i) == 0) && (i != 64) && (*(s++) != '/')) {
      cerr << "Bad FEN3\n" << i << "[" << s << "]" << endl; return;
    }
  }

  // space side
  if (*(s++) != ' ') {
    cerr << "Bad FEN4\n"; return;
  }
  switch (toupper(*(s++))) {
  case 'W': b->side = WHITE; break;
  case 'B': b->side = BLACK; break;
  default: cerr << "Bad FEN5\n"; return;
  }

  // space castles
  if (*(s++) != ' ') {
    cerr << "Bad FEN6\n"; return;
  }
  s++;
  while (*s != '\0' && *s != ' ') s++;

  // space en-passant target
  if (*(s++) != ' ') {
    cerr << "Bad FEN7\n"; return;
  }

  char c = *(s++);
  if (c == '-') {
    b->epsquare = NO_EP_SQUARE;
  } else {
    if (c < 'a' || c > 'h') {
    cerr << "Bad FEN8\n"; return;
    }
    char r = *(s++);
    if (r < '1' || r > '8') {
      cerr << "Bad FEN9\n"; return;
    }
    b->epsquare = assemble('8' - r, c - 'a');
    if (b->side == WHITE) b->epsquare += 8; else b->epsquare -= 8;
  }

  // space junk
  if (*(s++) != ' ') {
    cerr << "Bad FEN10\n"; return;
  }  
  recompute_zobrist(b);
}

/************************ Print the board in colors **************************/

// fg is from 0 to 15, bg is from 0 to 7
void colors(char *s, int fg, int bg) {
  fprintf(stderr, "\033[%d;%d;%dm%s", fg/8, 30 + fg%8, 40 + bg, s);
}

// Colors to print the board in
#define WHITESQUARE 6
#define BLACKSQUARE 0
#define WHITEPIECE 15
#define BLACKPIECE 9

void blankline(int i) {
  int j;

  fprintf(stderr, "  ");
  for (j = 0; j < 8; j++) {
    colors("     ", 0, (i + j) % 2 ? BLACKSQUARE : WHITESQUARE);
  }
  fprintf(stderr, "\033[0m\n");
}

void printboard(tboard* b) {
  char s[10];
  fprintf(stderr, "    A    B    C    D    E    F    G    H\n");
  for (int i = 0; i < 8; i++) {
    blankline(i);
    fprintf(stderr, "%d ", 8 - i);
    for (int j = 0; j < 8; j++) {
      sprintf(s, "  %c  ", piecename[abs(b->b[assemble(i, j)])]);
      colors(s, b->b[assemble(i, j)] > 0 ? WHITEPIECE : BLACKPIECE,
	     (i + j) % 2 ? BLACKSQUARE : WHITESQUARE);
    }
    fprintf(stderr, "\033[0m %d\n", 8 - i);

    blankline(i);
  }
  fprintf(stderr, "    A    B    C    D    E    F    G    H\n");
  fprintf(stderr, "epsquare: %d side: %d wc: %d bc: %d hash: %llu\n",
	  b->epsquare, b->side, b->whitecount, b->blackcount, b->hashValue);
}

int same_move(tmove m1, tmove m2) {
  return m1.from == m2.from &&
    m1.to == m2.to &&
    m1.prom == m2.prom &&
    m1.epsquare == m2.epsquare;
}

void printmovelist(tmovelist m) {
  for (int i = 0; i < m.count; i++) {
    cerr << movetostring(m.move[i]) << " ";
  }
  cerr << endl;
}

// Sort a move list in descending order of the  values associated with each
// move.
void sortmovelist(tmovelist* m, int* values) {
  int i, j, k, iaux;
  tmove maux;

  for (i = 0; i < m->count - 1; i++) {
    k = i;
    for (j = i + 1; j < m->count; j++)
      if (values[j] > values[k])
	k = j;
    maux = m->move[i]; m->move[i] = m->move[k]; m->move[k] = maux;
    iaux = values[i]; values[i] = values[k]; values[k] = iaux;
  }
}

void sort_by_pns_ratio(tmovelist* m, int* proof, int* disproof,
		       double* ratio) {

  for (int i = 0; i < m->count; i++)
    // we assume disproof isn't 0, or this move would have been a win for us
    // and thus find_best_move_pns would have returned it
    ratio[i] = 1.0 * proof[i] / disproof[i];

  for (int i = 0; i < m->count - 1; i++) {
    int k = i;
    for (int j = i + 1; j < m->count; j++)
      if (ratio[j] > ratio[k])
	k = j;
    tmove maux = m->move[i]; m->move[i] = m->move[k]; m->move[k] = maux;
    double raux = ratio[i]; ratio[i] = ratio[k]; ratio[k] = raux;
    int paux = proof[i]; proof[i] = proof[k]; proof[k] = paux;
    int daux = disproof[i]; disproof[i] = disproof[k]; disproof[k] = daux;
  }  
}

u64 rand64(void) {
  return rand() ^ ((u64)rand() << 30) ^ ((u64)rand() << 60);
}

void saveboard(tboard*b, tmove mv, tsaverec *sr) {
  sr->m = mv;
  sr->side = b->side;
  sr->p1 = b->b[mv.from];
  sr->p2 = b->b[mv.to];
  sr->whiteCount = b->whitecount;
  sr->blackCount = b->blackcount;
  sr->hashValue = b->hashValue;
  sr->epsquare = b->epsquare;
}

void restoreboard(tboard *b, tsaverec *sr) {
  b->side = sr->side;
  b->b[sr->m.from] = sr->p1;
  b->b[sr->m.to] = sr->p2;
  if (sr->m.epsquare != NO_EP_SQUARE)
    b->b[sr->m.epsquare] = -z(PAWN);
  b->whitecount = sr->whiteCount;
  b->blackcount = sr->blackCount;
  b->hashValue = sr->hashValue;
  b->epsquare = sr->epsquare;

  // Increase the bishop counts if a bishop was captured and is placed back
  if (sr->p2 == BISHOP)
    b->wbishop[bishop_parity(sr->m.to)]++;
  else if (sr->p2 == -BISHOP)
    b->bbishop[bishop_parity(sr->m.to)]++;

  // Decrease the bishop counts if a pawn is unpromoted from a bishop
  if (sr->m.prom == BISHOP)
    b->wbishop[bishop_parity(sr->m.to)]--;
  else if (sr->m.prom == -BISHOP)
    b->bbishop[bishop_parity(sr->m.to)]--;
}

int sane_board(tboard* b) {
  int white_count = 0, black_count = 0;
  int wb[2] = {0, 0}, bb[2] = {0, 0}; // Bishop counts
  for (int i = 0; i < 64; i++) {
    if (b->b[i] > 0) white_count++;
    else if (b->b[i] < 0) black_count++;

    if (b->b[i] == BISHOP) wb[bishop_parity(i)] ++;
    else if (b->b[i] == -BISHOP) bb[bishop_parity(i)] ++;
  }
  int sane =
    b->whitecount == white_count &&
    b->blackcount == black_count &&
    b->wbishop[0] == wb[0] &&
    b->wbishop[1] == wb[1] &&
    b->bbishop[0] == bb[0] &&
    b->bbishop[1] == bb[1];

  if (!sane) {
    cerr << "BEEEEEEP! BEEEEEEEP! INSANE BOARD:\n";
    printboard(b);
  }
  return sane;
}

/*********************** Preprocessing move generation ***********************/

// This enum names the fields in table freedom[][]
enum t_fieldtype {
  PIECE = 0,  // What type of piece
  DR = 1,
  DF = 2,     // The trajectory it can move on
  RANGE = 3   // The distance it can go
};

// A freedom indicates a possible path that a piece may follow. For example,
// knights have 8 freedoms, whereas bishops only have 4 (the diagonals).
// Also note the range. Kings and knights have a range of 1, while the other
// pieces have a range of 7 (i.e. infinite).
#define FREEDOMS 32
const int freedom[FREEDOMS][4] = {
  { KNIGHT,  1,  2, 1 },
  { KNIGHT,  1, -2, 1 },
  { KNIGHT, -1,  2, 1 },
  { KNIGHT, -1, -2, 1 },
  { KNIGHT,  2,  1, 1 },
  { KNIGHT,  2, -1, 1 },
  { KNIGHT, -2,  1, 1 },
  { KNIGHT, -2, -1, 1 },
  { BISHOP, -1, -1, 7 },
  { BISHOP,  1, -1, 7 },
  { BISHOP, -1,  1, 7 },
  { BISHOP,  1,  1, 7 },
  { ROOK,   -1,  0, 7 },
  { ROOK,    1,  0, 7 },
  { ROOK,    0, -1, 7 },
  { ROOK,    0,  1, 7 },
  { QUEEN,  -1, -1, 7 },
  { QUEEN,   1, -1, 7 },
  { QUEEN,  -1,  1, 7 },
  { QUEEN,   1,  1, 7 },
  { QUEEN,  -1,  0, 7 },
  { QUEEN,   1,  0, 7 },
  { QUEEN,   0, -1, 7 },
  { QUEEN,   0,  1, 7 },
  { KING,   -1, -1, 1 },
  { KING,    1, -1, 1 },
  { KING,   -1,  1, 1 },
  { KING,    1,  1, 1 },
  { KING,   -1,  0, 1 },
  { KING,    1,  0, 1 },
  { KING,    0, -1, 1 },
  { KING,    0,  1, 1 }
};

// A ray stores all the possible moves for one piece in one given direction,
// assuming an empty board. Square numbers are stored radially outwards, and
// the ray is terminated with the square of the piece itself (to simulate a
// block).
typedef int t_ray[8];

typedef struct {
  int size;
  t_ray ray[8]; // A piece can have as many as 8 rays of movement
} t_moveset;

t_moveset premove[KING+1][64]; // This stores our preprocessed data

void preprocessMoves() {
  int piece, from_square, range;
  t_ray ray;

  for (piece = KNIGHT; piece <= KING; piece++)
    for (from_square = 0; from_square < 64; from_square++) {
      // Generate all the valid moves on an empty board for that piece
      // (regardless of color) standing on from_square. Moves are generated
      // radially outwards from the piece.
      premove[piece][from_square].size = 0; // No rays found yet.
      // Iterate through all the freedoms that the piece has
      for (int k = 0; k < FREEDOMS; k++)
        if (freedom[k][PIECE] == piece) {
          // Keep going until we exceed the range or we run off the board
          int i, j, size = 0;
          for (range = 1,
		 i = rank(from_square) + freedom[k][DR],
		 j = file(from_square) + freedom[k][DF];
               (range <= freedom[k][RANGE]) &&
		 (i >= 0) && (i <= 7) &&
		 (j >= 0) && (j <= 7);
               range++, i += freedom[k][DR], j += freedom[k][DF])
            ray[size++] = assemble(i, j);
          ray[size] = from_square;

          // Has this ray given us some actual moves? If not, forget it.
          if (ray[0] != from_square)
            memcpy(premove[piece][from_square]
                   .ray[premove[piece][from_square].size++],
                   ray, 8 * sizeof(int));
        }
    }
}

/****************************** Board handling *******************************/

void move(tboard *b, tmove mv) {
  // Was this an EP capture?
  if (mv.epsquare != NO_EP_SQUARE) {
    b->hashValue ^= zobrist.z[b->b[mv.epsquare] + KING][mv.epsquare];
    b->b[mv.epsquare] = EMPTY;
    if (b->side == WHITE) b->blackcount--; else b->whitecount--;
  }

  // Now set the new epsquare value, if this was a double pawn push
  b->hashValue ^= zobrist.epZ[b->epsquare];
  b->epsquare = (abs(b->b[mv.from]) == PAWN &&
                abs(mv.from - mv.to) == 16) ?
               mv.to : NO_EP_SQUARE;
  b->hashValue ^= zobrist.epZ[b->epsquare];

  // Increase the bishop counts for promotions, decrease for captures
  if (b->b[mv.to] == BISHOP)
    b->wbishop[bishop_parity(mv.to)]--;
  else if (b->b[mv.to] == -BISHOP)
    b->bbishop[bishop_parity(mv.to)]--;

  if (mv.prom == BISHOP)
    b->wbishop[bishop_parity(mv.to)]++;
  else if (mv.prom == -BISHOP)
    b->bbishop[bishop_parity(mv.to)]++;

  // If this is a capture, remove the old piece  
  if (b->b[mv.to]) {
    b->hashValue ^= zobrist.z[b->b[mv.to] + KING][mv.to];
    if (b->side == WHITE) b->blackcount--; else b->whitecount--;
  }

  // Now copy the piece to its destination, perhaps by promoting it
  b->b[mv.to] = mv.prom ? mv.prom : b->b[mv.from];
  b->hashValue ^= zobrist.z[b->b[mv.to] + KING][mv.to];

  // Remove the piece from its original location
  b->hashValue ^= zobrist.z[b->b[mv.from] + KING][mv.from];
  b->b[mv.from] = EMPTY;

  // Flip the side
  b->side = -b->side;
  b->hashValue ^= zobrist.sideZ;
}

// This is a dirty hack, but it works in 99.99% of the games.
// It assumes that stalemate isn't possible if you have anything besides pawns
// It may go wrong and return true when it should return false, but only
// in the case when (1) it is stalemate and (2) you have other blocked
// pieces besides pawns, e.g a bishop.
// Caveat: we are ignoring a situation like white: d5; black: d6, c5 where
// white can capture en-passant. Fix this.
int canmove(tboard* b) {
  int i;
  for (i = 0; i < 64; i++) {
    if ((z(b->b[i]) >= KNIGHT && z(b->b[i]) <= KING))
      return 1;
    else if (b->b[i] == PAWN && b->side == WHITE) {
      if (b->b[i - 8] == EMPTY ||
          (!isafile(i) && b->b[i - 9] <= -PAWN) ||
          (!ishfile(i) && b->b[i - 7] <= -PAWN))
        return 1;
    }
    else if (b->b[i] == -PAWN && b->side == BLACK) {
      if (b->b[i + 8] == EMPTY ||
          (!isafile(i) && b->b[i + 7] >= PAWN) ||
          (!ishfile(i) && b->b[i + 9] >= PAWN))
        return 1;
    }
  }
  return 0;
}

/***************************** Move generation *******************************/

void legalpawn(tboard* b, tmovelist* m, tmove* mv) {
  int i;

  if (isedgerank(mv->to)) {
    // This is a promotion, there are 5 possible moves
    for (i = KNIGHT; i <= KING; i++) {
      m->move[m->count] = *mv;
      m->move[m->count++].prom = z(i);
    }
  } else {
    m->move[m->count++] = *mv;
  }
}

// If this is the first capture, clear all previous moves
#define CHECK_FIRST_CAPTURE \
  if (no_captures) { m->count = 0; no_captures = 0; }

void getallvalidmoves(tboard* b, tmovelist *m) {
  tmove mv = { 0, 0, 0, NO_EP_SQUARE };
  int no_captures = 1;
  
  m->count = 0;
  char* board_end = b->b + 64;
  for (char* sq = b->b; sq != board_end; sq++) {
    if (!*sq) continue;
    int p = z(*sq);
    mv.from = sq - b->b;
    if (p == PAWN) {
      mv.to = mv.from - (b->side << 3);
      if (no_captures && b->b[mv.to] == EMPTY) {
        legalpawn(b, m, &mv); // One step forward
        if (b->side == WHITE && issecondrank(mv.from) ||
            b->side == BLACK && isseventhrank(mv.from)) {
          mv.to -= b->side << 3;
          if (b->b[mv.to] == EMPTY)
            m->move[m->count++] = mv; // Two steps forward
          mv.to += b->side << 3;
        }
      }
      if (!isafile(mv.from)) {
        mv.to--;
        if (z(b->b[mv.to]) < 0) {
          CHECK_FIRST_CAPTURE;
          legalpawn(b, m, &mv); // Capture left
        }
        mv.to++;
      }
      if (!ishfile(mv.from)) {
        mv.to++;
        if (z(b->b[mv.to]) < 0) {
          CHECK_FIRST_CAPTURE;
          legalpawn(b, m, &mv); // Capture right
        }
        mv.to--;
      }
      
      if (b->epsquare != NO_EP_SQUARE &&
          abs(mv.from - b->epsquare) == 1 &&
          rank(mv.from) == rank(b->epsquare)) {
        mv.to += b->epsquare - mv.from;
        mv.epsquare = b->epsquare;
        CHECK_FIRST_CAPTURE;
        m->move[m->count++] = mv; // En passant
        mv.epsquare = NO_EP_SQUARE;
      }
    } else if (p >= KNIGHT && p <= KING) {
      int x;
      for (int r = premove[p][mv.from].size - 1; r >= 0; r--) // for each ray
        for (int *square = premove[p][mv.from].ray[r];
             (x = z(b->b[*square])) <= 0;
             square++) {
          mv.to = *square;
          if (x < 0) {
            CHECK_FIRST_CAPTURE;
            m->move[m->count++] = mv;
            break;
          } else if (no_captures) {
            m->move[m->count++] = mv;
          }
        }
    }
  }
}

string movetostring(tmove m) {
  if (m.from == -1) {
    return "none";
  }

  char s[10];
  s[0] = file(m.from) + 'a'; s[1] = (7 - rank(m.from)) + '1';
  s[2] = file(m.to) + 'a'; s[3] = (7 - rank(m.to)) + '1';
  if (m.prom != EMPTY) {
    s[4] = piecename[abs(m.prom)]; // Generate lowercase letter for promotion
    s[5] = 0;
  }
  else s[4] = 0;
  return s;
}

tmove stringtomove(tboard* b, char *s) {
  tmove m;
  m.from = assemble(7 - (s[1] - '1'), s[0] - 'a');
  m.to = assemble(7 - (s[3] - '1'), s[2] - 'a');
  if (s[4] != '\0') { // promotion
    m.prom = namepiece[s[4]];
    // Regardless of the spelling case, promote to a piece of the correct color
    if (z(m.prom) < 0) m.prom = -m.prom;
  }
  else m.prom = EMPTY;
  if (abs(b->b[m.from]) == PAWN && file(m.from) != file(m.to) &&
      b->b[m.to] == EMPTY)
    m.epsquare = assemble(rank(m.from), file(m.to));
  else
    m.epsquare = NO_EP_SQUARE;
  return m;
}

tmove san_to_move(tboard* b, char *s) {
  tmovelist ml;
  getallvalidmoves(b, &ml);

  for (int i = 0; i < ml.count; i++)
    if (!strcmp(s, move_to_san(b, ml.move[i]).c_str()))
      return ml.move[i];
  return INVALID_MOVE;
}

string move_to_san(tboard* b, tmove m) {
  const string INVALID("invalid");
  if (m.from < 0 || m.from >= 64 || m.to < 0 || m.to >= 64) return INVALID;

  string san = "";

  // Add the piece name
  const char piece_name = piecename[abs(b->b[m.from])];
  if (piece_name == ' ') return INVALID;
  if (piece_name != 'P') san += piece_name;

  // Add the file/rank if necessary: ambiguities or pawn (or ep) captures
  int f = 0; // how many pieces of the same type on the same file attack m.to
  int r = 0; // how many pieces of the same type on the same rank attack m.to
  int t = 0; // how many pieces of the same type attack m.to
  tmovelist ml;
  getallvalidmoves(b, &ml);
  for (int i = 0; i < ml.count; i++)
    if (b->b[ml.move[i].from] == b->b[m.from] &&   // same piece type
	ml.move[i].to == m.to &&                   // same landing square
        ml.move[i].from != m.from) {               // different start square
      if (file(ml.move[i].from) == file(m.from)) f++;
      if (rank(ml.move[i].from) == rank(m.from)) r++;
      t++;
    }
  if ((piece_name == 'P' && (file(m.from) != file(m.to))) || // pawn captures
      (t && (!f || (f && r)))) // disambiguate
    san += (char)('a' + file(m.from));
  if (t && f)
    san += (char)('8' - rank(m.from));

  // Add an x for captures
  if (b->b[m.to] != EMPTY || m.epsquare != NO_EP_SQUARE)
    san += 'x';

  // Add the destination square
  san += (char)('a' + file(m.to));
  san += (char)('8' - rank(m.to));

  // Add the promotion if any
  if (m.prom != EMPTY) {
    san += '=';
    san += piecename[abs(m.prom)];
  }
  return san;
}

void string_to_movelist(char* s, tmovelist* ml) {
  ml->count = 0;
  if (!s) return;

  tboard puzzle;  // We need a board to extract each move in SAN format.
  fen_to_board(NEW_BOARD, &puzzle);
  char* tok = strtok(s, " \t");
  while (tok) {
    tmove mv = san_to_move(&puzzle, tok);
    if (same_move(mv, INVALID_MOVE)) {
      cerr << "Cannot parse move [" << tok << "]\n";
      exit(1);
    }
    ml->move[ml->count] = mv;
    move(&puzzle, mv);
    ml->count++;
    tok = strtok(NULL, " \t");
  }
}

void signalHandler (int sig) {
  timer_expired = 1;
  if (!g_playing) {
    puts("tellics withdraw");
    puts("tellics resume");
    set_alarm(KEEP_ALIVE * 100);
  }
}

void set_alarm(int centis) {
  timer_expired = 0;
  itimerval value = { { 0, 0 }, { centis / 100, (centis % 100) * 10000 } };
  cerr << "[NILATAC] Alarm set at " << centis << " centis\n";
  setitimer(ITIMER_REAL, &value, NULL);
}

void init_zobrist() {
  for (int p = 0; p <= 2 * KING; p++)
    for (int i = 0; i < 64; i++)
	zobrist.z[p][i] = rand64();
  zobrist.sideZ = rand64();
  for (int i = 0; i <= NO_EP_SQUARE; i++)
    zobrist.epZ[i] = rand64();
}

void init_common() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand(tv.tv_usec);
  memset(&b, 0, sizeof(tboard));
  memset(namepiece, EMPTY, sizeof(namepiece));
  for (int i = PAWN; i <= KING; i++) {
    namepiece[piecename[i]] = i;
    namepiece[tolower(piecename[i])] = -i;
  }
  signal(SIGALRM, signalHandler);
  init_zobrist();
  preprocessMoves();
}
