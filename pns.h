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
#ifndef _PNS_H
#define _PNS_H
#include "common.h"
#include <unordered_map>
#define INF_NODES 1000000000

// Hard-coded limit of the PNS tree size
#define PNS_SIZE 2000000
#define PNS_BOOKSIZE 60000000
// A book node can be trimmed if the solution is at most this many nodes:
#define PNS_BOOK_TRIVIAL 2000000

#define BOOK_FILENAME "book.in"

#define pns_finite(x) ((x) && ((x) < INF_NODES))
#define pns_won(p, d) (!(p))
#define pns_lost(p, d) (!(d))
#define pns_drawn(p, d) ((p) == INF_NODES && (d) == INF_NODES)
#define pns_won_drawn(p, d) ((d) == INF_NODES)
#define pns_lost_drawn(p, d) ((p) == INF_NODES)
#define pns_open(p, d) ((pns_finite((p))) || (pns_finite((d))))
#define pns_solved(p, d) (!(pns_finite((p))) && !(pns_finite((d))))

typedef struct {
  int proof, disproof;
  tmove mv; // set only when proof = 0 (to indicate the winning move);
} t_pns_result;

// Definition of the PNS tree
typedef struct _t_pns_node_ {
  tmove mv;                      // How did we reach this node from its parent
  struct _t_pns_node_* parent;
  struct _t_pns_node_** child;   // The links of this node
  u8 num_children;             // = sizeof(child)
  int proof, disproof;           // From the side to move's point of view
  double ratio;
  int size;                      // # of nodes in this tree, including self
} t_pns_node;

typedef struct {
  t_pns_node* root;              // The root node of the PNS tree
  tboard b_orig;                 // The board corresponding to the root
  tboard b_current;              // The boad for the node we evaluate currently
  int size;                      // Number of nodes currently in the tree
  int max_nodes;                 // The maximum allowed size for the tree
} t_pns_data;

extern t_pns_data* pns_space;
extern t_pns_data* book;
extern double book_optimality;

t_pns_data* alloc_pns_data(int max_nodes);
void init_pns(const char* book_file_name);

// Evaluate this node using Proof-Number Search
// Args:
//   - b: The board to analyze
//   - max_nodes: The number of nodes permitted. 0 defaults to the hard-coded
//     limit
t_pns_result pns_main(tboard *b, t_pns_data* data, int max_nodes,
		      tmovelist* subtree);

void print_tree(t_pns_node* node, int all, int max_depth);

// Evaluates each move in ml on board b. Throws away all the losing moves
// and the worse half (or half-1) of the surviving moves. Stores the rest
// in ml, sorted by quality. Returns WIN or -WIN if this position is won/lost,
// 0 otherwise. If all moves lose, returns all of them.
int pns_trim_move_list(tboard* b, tmovelist* ml, int max_nodes);

// Does move mv on board b lead to a lost position? to be used by the opening
// book
int losing_move(tboard *b, tmove mv);

extern void getallvalidmoves(tboard* b, tmovelist* m);
extern void move(tboard* b, tmove* mv);

typedef unordered_map<u64, t_pns_node*> t_pns_hash;
extern t_pns_hash* pns_hash;
void load_pns_tree(const char* filename, t_pns_data* data);
t_pns_hash* populate_hash(t_pns_node* root);
int query_book(tboard *b, tmove* mv);
void browse_pns_tree(const char* filename);

// Lenthep needs these methods... perhaps I should move the lenthep code in
// pns.cc
void update_ancestors(tboard* b, t_pns_node* node, int recompute_ratios);
void expand(t_pns_data* data, t_pns_node* node, int level);

// Print lines of the form 1|10000000 or 1|1
void print_bad_pns_lines();

// Print lines with a very bad disproof/proof ratio (that can probably be
// solved easily)
void print_weak_pns_lines(double threshold, int max_depth);

#endif /* _PNS_H */
