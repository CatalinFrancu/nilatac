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
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <iomanip>
#include "hash.h"
#include "pns.h"
#include "suicide.h"
#include "egtb.h"

int depth_completed, best_score;
tmove killer[MAXDEPTH + 1];
int positions; // The number of positions evaluated
int kibitzed; // What have we kibitzed most recently?

typedef enum {
  KIB_NONE = 0,
  KIB_WIN,
  KIB_DRAW,
  KIB_LOSS,
} kib_reason;

void restart(void) {
  fen_to_board(NEW_BOARD, &b);
  kibitzed = KIB_NONE;
  book_optimality = 1.0;

  g_force = false;
  g_reversible = 0;
  g_winning_line_found = false;
  g_offered_draw = false;
}

void init(void) {
  init_hash();
  init_pns();

  if (USE_EGTB && !WEAKENED) {
    init_egtb();
  }
}

void kibitz(int kib, const char* reason) {
  switch (kib) {
  case KIB_WIN:
    if (kibitzed == KIB_LOSS)
      cout << "kibitz You could have won this!\n";
    if (kibitzed == KIB_DRAW)
      cout << "kibitz You could have drawn this!\n";
    if (kibitzed != KIB_WIN) {
      cout << "kibitz " << reason << endl;
      g_winning_line_found = true;
    }
    break;
  case KIB_DRAW:
    if (kibitzed == KIB_WIN) {
      cout << "kibitz You have probably found a bug in me. I should have won "
           << "this game and I missed it. Lucky you!\n";
      cout << "resign\n";
    }
    if (kibitzed == KIB_LOSS)
      cout << "kibitz You could have won this!\n";
    if (kibitzed != KIB_DRAW) {
      cout << "kibitz " << reason << endl;
      puts("offer draw");
      g_offered_draw = true;
    }
    break;
  case KIB_LOSS:
    if (kibitzed == KIB_WIN || kibitzed == KIB_DRAW) {
      cout << "kibitz You have probably found a bug in me. I should have won "
           << "this game and I missed it. Lucky you!\n";
      cout << "resign\n";
    }
    if (kibitzed != KIB_LOSS)
      cout << "kibitz " << reason << endl;
    break;
  case KIB_NONE:
    cout << "kibitz " << reason << endl;
    break;
  default: assert(0);
  }
  kibitzed = kib;
}

/******************************* Game engine *********************************/

#define MATERIAL_PANIC_COUNT 7
#define MATERIAL_PANIC_PENALTY 0
#define KING_BONUS 10000
#define PAWN_BONUS 100
#define MOBILITY_FACTOR 50
const int piecevalue[7] = {0, -100, 50, -200, 500, 100, 500};

int static_eval(tboard* b) {
  if (USE_EGTB) {
    int egtb_score = egtb_lookup_inclusive(b);
    if (egtb_score != EGTB_UNKNOWN) {
      if (egtb_score < 0) return  -WIN;
      else if (egtb_score == EGTB_DRAW) return DRAW;
      else return WIN;
    }
  }

  tmovelist ml;
  getallvalidmoves(b, &ml);

  if (ml.count == 0) {
    return (b->whitecount < b->blackcount) ?
      z(WIN) : ((b->whitecount == b->blackcount) ? DRAW : -z(WIN));
  }

  int score = ml.count * MOBILITY_FACTOR;
  b->side = -b->side;
  getallvalidmoves(b, &ml);
  score -= ml.count * MOBILITY_FACTOR;
  b->side = -b->side;

  // By how many pieces I'm ahead
  int material_diff = (b->side == WHITE)
                      ? (b->whitecount - b->blackcount)
                      : (b->blackcount - b->whitecount);

  // Freak out if I'm ahead by more than 7 pieces
  if (material_diff > MATERIAL_PANIC_COUNT)
    score -= MATERIAL_PANIC_PENALTY;
  else if (material_diff < -MATERIAL_PANIC_COUNT)
    score += MATERIAL_PANIC_PENALTY;

  int my_king = 0, his_king = 0;
  int piece_sum = 0;
  for (int i = 0; i < 64; i++) {
    int piece = z(b->b[i]);
    if (piece == KING)
      my_king = 1;
    else if (piece == -KING)
      his_king = 1;

    if (piece > 0) piece_sum += piecevalue[piece];
    else piece_sum -= piecevalue[-piece];

    if (piece == PAWN || piece == -PAWN) {
      int r = (b->b[i] == PAWN) ? 7 - rank(i) : rank(i);
      // Give a bonus for advanced pawns. The pawn was advanced by r - 1 steps
      // (regardless of color).
      if (piece == PAWN) piece_sum += (r -1) * PAWN_BONUS;
      else piece_sum -= (r - 1) * PAWN_BONUS;
    }
  }

  score += piece_sum;
  if (my_king && !his_king)
    score += KING_BONUS;
  else if (!my_king && his_king)
    score -= KING_BONUS;

  return score;
}

int hs_hits, hs_reads;

int eval_board(tboard* b, int depth, int alpha, int beta) {
  positions++;
  if (!depth || timer_expired) return static_eval(b);

  if ((b->side == WHITE && !b->whitecount) ||
      (b->side == BLACK && !b->blackcount))
    return WIN;
  if (!canmove(b))
    return (b->whitecount < b->blackcount) ? z(WIN):
      ((b->whitecount == b->blackcount) ? DRAW : -z(WIN));

  // Search for the position in the hash
  if (depth >= MIN_HASH_DEPTH) hs_reads++;
  t_hash_entry* spot = hash_data + (b->hashValue & HASHMASK);
  if (spot->hash_value == b->hashValue &&
      spot->depth >= depth &&
      (spot->type == H_EQ ||
       (spot->type == H_GE && spot->value >= beta) ||
       (spot->type == H_LE && spot->value <= alpha))) {
    hs_hits++;
    return spot->value;
  }

  tmovelist m;
  getallvalidmoves(b, &m);

  // Move the transposition move to the front
  int preferredMoves = 0;
  if (spot->hash_value == b->hashValue && spot->best_move != 0xff) {
    assert(spot->best_move < m.count);
    tmove maux = m.move[preferredMoves];
    m.move[preferredMoves] = m.move[spot->best_move];
    m.move[spot->best_move] = maux;
    preferredMoves++;
  }

  // Move the killer move to the front
//   if (preferredMoves < m.count)
//     for (int i = 0; i < m.count; i++)
//       if (!memcmp(m.move + i, killer + depth, sizeof(tmove))) {
// 	tmove maux = m.move[preferredMoves];
// 	m.move[preferredMoves] = m.move[i];
// 	m.move[i] = maux;
// 	i = m.count;
//       }

  // Recurrence
  int best_move = 0xff;
  boolean is_exact = 0;
  for (int i = 0; i < m.count; i++) {
    tsaverec sr;
    saveboard(b, m.move[i], &sr);
    move(b, m.move[i]);
    int value = -eval_board(b, depth - 1, -beta, -alpha);
    restoreboard(b, &sr);
    if (value >= beta) {
      //      killer[depth] = m.move[i];
      ADD_TO_HASH(b->hashValue, depth, value, H_GE, i);
      return value;
    }
    if (value >= alpha) {
      alpha = value;
      is_exact = 1;
      best_move = i;
    }
  }

  ADD_TO_HASH(b->hashValue, depth, alpha, (is_exact ? H_EQ : H_LE), best_move);
  return alpha;
}

// Makes the best move it finds. If sec is 0, runs a fixed minimum-depth
// search.
tmove find_best_move_alpha_beta(tmovelist* m, int centis) {
  depth_completed = best_score = positions = 0;
  hs_hits = hs_reads = 0;

  // Drop the losing moves and half of the surviving moves
  set_alarm(centis / 2);
  int pns_score = pns_trim_move_list(&b, m, 4000000);
  if (pns_score == WIN) {
    kibitz(KIB_WIN, "I got lucky!");
    return m->move[0];
  }
  if (pns_score == -WIN)
    kibitz(KIB_LOSS, "Those who are about to die salute you");

  if (m->count == 1) return m->move[0];

  set_alarm(centis / 3);
  memset(killer, 0, sizeof(killer));
  int values[1000]; // Values of the moves
  int moves_completed = 0;
  for (int depth = MINDEPTH;
       depth <= MAXDEPTH && (!timer_expired || depth == MINDEPTH) && best_score != WIN;
       depth++) {
    int level_score = -INFTY;

    for (int i = 0; i < m->count && level_score != WIN && (!timer_expired || depth == MINDEPTH);
	 i++) {
      tsaverec sr;
      saveboard(&b, m->move[i], &sr);
      move(&b, m->move[i]);
      int value = -eval_board(&b, depth, -INFTY, -level_score + 1);
      // Do not overwrite values if we timed out
      if (!timer_expired || depth == MINDEPTH) {
	values[i] = value;
	moves_completed = i;
	cerr << movetostring(m->move[i]) << " -> " << values[i] << endl;
	if (value > level_score) level_score = values[i];
      }
      restoreboard(&b, &sr);	
    }

    if (!timer_expired || depth == MINDEPTH) {
      cerr << "Depth " << depth << " complete............" << endl;
      best_score = level_score;
      depth_completed = depth;
    }

    m->count = moves_completed + 1;
    sortmovelist(m, values);
    if (values[1] == -WIN)
      return m->move[0]; // Offer as much resistance as possible
  }

  int bestMoveCount;
  for (bestMoveCount = 0; bestMoveCount < m->count &&
	 values[0] == values[bestMoveCount]; bestMoveCount++);

  cerr << "[HASH] reads:" << hs_reads << " hits:" << hs_hits << endl;

  // Move the selected move to the front of the list
  int choice = rand() % bestMoveCount;
  tmove maux = m->move[0]; m->move[0] = m->move[choice];
  m->move[choice] = maux;

  // Find the first move that does not lead to a forced loss
  tboard baux;
  for (int i = 0; i < m->count; i++) {
    baux = b;
    move(&baux, m->move[i]);
    
    set_alarm(centis / 6);
    t_pns_result res = pns_main(&baux, pns_space, 300000, NULL);
    cerr << "[PNS] Doublecheck: " << res.proof << "|" << res.disproof
	 << endl;
    if (res.proof) return m->move[i];
    
    cerr << "[PNS] Doublecheck: avoiding " << movetostring(m->move[i])
	 << " because " << movetostring(res.mv) << " would kill us.\n";
  }
  info("[PNS] Doublecheck: all moves lead to forced losses");
  kibitz(KIB_LOSS, "Those who are about to die salute you");

  return m->move[0];
}

// Returns WIN, -WIN or DRAW and sets mv accordingly. If mv is INVALID, then
// the return value should be ignored.
int query_egtb(tmove *mv) {
  tmovelist ml;
  tboard new_b;
  int egtb_score = egtb_lookup(&b);
  cerr << "            EGTB score: " << egtb_score << endl;
  if (egtb_score != EGTB_UNKNOWN) {
    if (egtb_score == EGTB_DRAW || egtb_score < 0) {
      // Find a move that draws, or loses in as many moves as possible
      int longest_loss = -1, longest_move = -1;
      getallvalidmoves(&b, &ml);
      if (ml.count == 0) {
	*mv = INVALID_MOVE; return EGTB_DRAW;
      }
      for (int i = 0; i < ml.count; i++) {
	new_b = b;
	move(&new_b, ml.move[i]);
	int child_score = egtb_lookup_inclusive(&new_b);
	assert(child_score != EGTB_UNKNOWN);
	if (child_score == EGTB_DRAW) {
	  *mv = ml.move[i];
	  return EGTB_DRAW;
	}
	if (child_score >= 0) {
	  if (child_score > longest_loss) {
	    longest_loss = child_score;
	    longest_move = i;
	  }
	} else {
	  assert(0); // The child cannot lose, because we are supposed to lose!
	}
      }
      assert(longest_move != -1);
      *mv = ml.move[longest_move];
      return egtb_score;
    }

    if (egtb_score >= 0) {
      // Find a move for the shortest win
      getallvalidmoves(&b, &ml);
      if (ml.count == 0) {
	*mv = INVALID_MOVE; return EGTB_DRAW;
      }
      for (int i = 0; i < ml.count; i++) {
	new_b = b;
	move(&new_b, ml.move[i]);
	int child_score = egtb_lookup_inclusive(&new_b);
	if (child_score < 0 && child_score >= -egtb_score) {
	  // aka == -(score - 1) - 1, but allow for gaps due to unoptimal
	  // egtbs. E.g. I win in 8, but this child loses in 5
	  *mv = ml.move[i];
	  return egtb_score;
	}
      }
      assert(0); // Hmmm, cannot find a winning path
    }

    assert(0); // We should have covered all cases!
  } else {
    *mv = INVALID_MOVE;
    return EGTB_UNKNOWN;
  }  
}

// m - a non-trivial move list (at least two moves to choose from)
// centis - how much time we're allowed to think
tmove find_best_move_pns(tmovelist* m, int centis) {
  // If we've found a winning line from the book, give ourselves a lot of
  // nodes to make sure we find it (the opening book solves lines down
  // to nodes that are 1000000-pns-solvable)
  if (g_winning_line_found || kibitzed == KIB_WIN) {
    t_pns_result res = pns_main(&b, pns_space, 0, NULL);
    if (!res.proof)
      return res.mv;
    else
      info("Failed to solve trivial node!");
  }

  int proof[m->count], disproof[m->count];
  double ratio[m->count];
  tboard resulting_b[m->count];
  string s[m->count];

  for (int i = 0; i < m->count; i++) {
    proof[i] = disproof[i] = 1;
    resulting_b[i] = b;
    move(resulting_b + i, m->move[i]);
    s[i] = movetostring(m->move[i]);
  }

  int allowed_nodes = WEAKENED ? 1500 : 10000;
  int first_iteration = 1;
  int any_moves = m->count; // How many moves left to research
  // If all moves have proof INF_NODES, see if there are any dead draws
  int draw_move = -1;

  // Now run pns on each resulting move with increasing memory
  while ((first_iteration || !timer_expired)
	 && (!WEAKENED || first_iteration)
	 && allowed_nodes <= 8000000 &&
	 // either i have several choices left, or i have a single choice left,
	 // but i also have a proven draw
	 (any_moves > 1 || any_moves == 1 && draw_move != -1)) {
    cerr << "PNS @ " << allowed_nodes << " nodes........................\n";
    for (int i = 0; i < m->count && (first_iteration || !timer_expired);
	 i++) {
      // If there's anything left to research...
      if (proof[i] && proof[i] != INF_NODES ||
	  disproof[i] && disproof[i] != INF_NODES) {
	t_pns_result res = pns_main(resulting_b + i, pns_space, allowed_nodes,
                                    NULL);
	if (!res.disproof) {
	  kibitz(KIB_WIN, "I got lucky!");
	  return m->move[i];
	}
	proof[i] = res.proof, disproof[i] = res.disproof;
	cerr << s[i] << " " << proof[i] << " | " << disproof[i] << endl;
	if (proof[i] == INF_NODES && disproof[i] == INF_NODES)
	  draw_move = i;
	if (disproof[i] == INF_NODES &&
	    (proof[i] == 0 || proof[i] == INF_NODES))
	  any_moves--;
      }
    }
    first_iteration = 0;
    allowed_nodes *= 3;
  }

  if (!any_moves) {
    if (draw_move != -1) { // Best we can get is a dead draw
      kibitz(KIB_DRAW, "Dead draw.");
      return m->move[draw_move];
    } else {  // All moves lose, return any one
      kibitz(KIB_LOSS, "Those who are about to die salute you");
      return m->move[rand() % m->count];
    }
  }

  // We want to choose among the largest ratios, because the numbers are
  // computed on the opponent's turn.
  sort_by_pns_ratio(m, proof, disproof, ratio);

  // Accept suboptimal moves if they are at least half as good as the optimal
  int i = 1;
  while (i < m->count &&
	 (ratio[i] >= 0.75 * ratio[0] ||
	  WEAKENED && (ratio[i] >= 0.05 * ratio[0])))
    i++;

  cerr << "Choosing from: ";
  for (int k = 0; k < i; k++)
    cerr << movetostring(m->move[k]) << " ";
  cerr << endl;

  return m->move[rand() % i];
}

// A general routine that does a few generic tasks (like querying the
// EGTB and the opening book) and then invokes a more specific algorithm
// to find the best move in a non-trivial position.
tmove find_best_move(int centis) {
  // First, look up the position in the EGTB
  if (USE_EGTB && !WEAKENED) {
    if (b.whitecount && b.blackcount && b.whitecount + b.blackcount <= MEN) {
      tmove mv;
      int egtb_score = query_egtb(&mv);
      if (mv.from != -1) {
	if (egtb_score == EGTB_DRAW) {
	  kibitz(KIB_DRAW, "Precomputed draw");
	} else if (egtb_score >= 0) {
	  char s[100];
	  sprintf(s, "Precomputed win in %d moves", egtb_score / 2);
	  kibitz(KIB_WIN, s);
	} else if (egtb_score != EGTB_UNKNOWN) {
	  char s[100];
	  sprintf(s, "Precomputed loss in %d moves", (1 - egtb_score) / 2);
	  kibitz(KIB_LOSS, s);
	} else assert(0);
	return mv;
      }
    }
  }

  // Second, look up the position in the opening book.
  tmovelist m;
  if (USE_BOOK && !WEAKENED) {
    tmove book_move;
    int book_score = query_book(&b, &book_move);
    if (book_score == WIN) {
      kibitz(KIB_WIN, "This opening is a known loss");
      return book_move;
    }

    // Open position, query_book selected one of the best continuations
    if (book_score == DRAW) return book_move;

    // If the position is losing or not in the book, keep all the moves
    getallvalidmoves(&b, &m);
  } else {
    // No book, just get the valid moves
    getallvalidmoves(&b, &m);
  }

  // Trivial cases: zero or one moves in the list
  if (m.count == 0) return INVALID_MOVE;
  if (m.count == 1) return m.move[0];

  return WEAKENED
    ? find_best_move_pns(&m, centis)
    : find_best_move_alpha_beta(&m, centis);
}

// Move mv is about to be made on board b. Increase or reset g_reversible.
inline void update_reversible(tboard* b, tmove mv) {
  if (z(b->b[mv.from]) == PAWN ||
      b->b[mv.to] != EMPTY)
    g_reversible = 0;
  else
    g_reversible++;
}

void play_best_move(int centis) {
  char ss[100];
  sprintf(ss, "[NILATAC] Thinking for %d centis", centis);
  info(ss);

  tmove mv = find_best_move(centis);
  if (mv.from == -1) return; // Nothing to move
  update_reversible(&b, mv);
  move(&b, mv);

  string s = movetostring(mv);
  info(s);
  cout << "move " << s << endl << flush;

  // Claim a draw on the 50 move rule
  if (g_reversible >= 100)
    puts("offer draw");
}

// Blindly executes the specified move.
int execute_move(char *s, tboard* b) {
  tmove mv = stringtomove(b, s);
  tmovelist ml;
  getallvalidmoves(b, &ml);
  for (int i = 0; i < ml.count; i++)
    if (same_move(mv, ml.move[i])) {
      update_reversible(b, mv);
      move(b, mv);
      return 0;
    }
  cerr << "Illegal move: " << s << endl;
  return -1;
}
