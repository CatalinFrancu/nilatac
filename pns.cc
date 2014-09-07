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
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pns.h"
#include "hash.h"
#include "egtb.h"
#include "suicide.h"

t_pns_data* pns_space;
t_pns_data* book;

// we hash these nodes by zobrist keys so we can look them up easily
t_pns_hash* pns_hash = NULL;
double book_optimality; // how much we gave away due to suboptimal book moves

t_pns_data* alloc_pns_data(int max_nodes) {
  t_pns_data* data = (t_pns_data*)malloc(sizeof(t_pns_data));
  data->max_nodes = max_nodes;
  data->root = (t_pns_node*)malloc((max_nodes + 1000) * sizeof(t_pns_node));
  data->size = 0;
  return data;
}

void init_pns() {
  pns_space = alloc_pns_data(PNS_SIZE);
  if (USE_BOOK && !WEAKENED) {
    book = alloc_pns_data(PNS_BOOKSIZE);
    load_pns_tree(BOOK_FILENAME, book);
    pns_hash = populate_hash(book->root);
  }
  else {
    book = NULL;
  }
}

inline t_pns_node* my_malloc(t_pns_data* data) {
  return data->root + data->size++;
}

void init_pns_data(t_pns_data* data, tboard* b) {
  data->root = my_malloc(data);
  data->root->mv = INVALID_MOVE;
  data->root->parent = NULL;
  data->root->child = NULL;
  data->root->num_children = 0;
  data->root->proof = 1;
  data->root->disproof = 1;
  data->root->ratio = 1.0;
  data->size = 1;
  data->b_orig = data->b_current = *b;
}

void delete_pns_node(t_pns_node* node) {
  if (!node) return;
  for (int i = 0; i < node->num_children; i++)
    delete_pns_node(node->child[i]);
  if (node->child && node->num_children) free(node->child);
  node->num_children = 0;
}

// Delete one child of this node and compact the remaining children.
// Returns 1 for success, 0 for failure. Fails if the node only has one child,
// because we would be unable to update its p/n values after that
int delete_pns_child(t_pns_node* node, int i) {
  assert(node && i >= 0 && i < node->num_children);
  if (node->num_children == 1) return 0;
  delete_pns_node(node->child[i]);
  node->child[i] = node->child[--node->num_children];
  // Fit the vector of pointers to children to match the decrease in size
  node->child = (t_pns_node**)realloc(node->child,
				      node->num_children *
				      sizeof(t_pns_node*));
  assert(node->child != NULL);
  update_ancestors(NULL, node, 0);
  return 1;
}

void delete_pns_data(t_pns_data* data) {
  delete_pns_node(data->root);
  data->size = 0;
  if (data == book) {
    pns_hash->clear();
    delete pns_hash;
  }
}

int num_seen = 0;
int num_trimmed = 0;

// Given a node that is either losing or winning, remove the uninteresting
// branches. For winning nodes, we only need to retain one winning move. For
// losing nodes, we must retain all the possible defenses.
// With trivialize set, returns 1 if this node is trivially solvable with pns
int trim_uninteresting_branches(t_pns_node* node, tboard* b, int trivialize) {
  num_seen++;
  if (num_seen % 100 == 0)
    cerr << "Seen: " << num_seen
         << " Trimmed: " << num_trimmed << endl;

  if (!node->num_children)
    return 1; // leaf

  int triv_children = 1;
  if (node->proof) {                                                    // lost
    for (int i = 0; i < node->num_children; i++) {
      tboard new_b = *b;
      move(&new_b, node->child[i]->mv);
      triv_children &= trim_uninteresting_branches(node->child[i], &new_b,
						   trivialize);
    }
  } else {                                                              // won
    int winning_child = 0;
    while (winning_child < node->num_children &&
	   node->child[winning_child]->disproof)
      winning_child++;
    if (winning_child == node->num_children)
      printboard(b);
    assert(winning_child < node->num_children);

    // Delete all children except the winning one
    num_trimmed += node->num_children - 1;
    if (node->num_children > 1) cerr << "Trimmed: " << num_trimmed << endl;
    t_pns_node* aux = node->child[0];
    node->child[0] = node->child[winning_child];
    node->child[winning_child] = aux;
    while (node->num_children > 1)
      delete_pns_child(node, 1);

    tboard new_b = *b;
    move(&new_b, node->child[0]->mv);
    triv_children = trim_uninteresting_branches(node->child[0], &new_b,
						trivialize);
  }

  // Now, if all children are trivial, try to solve this node as well and, if
  // it is trivial, delete all its children.
  if (triv_children && trivialize) {
    t_pns_result res = pns_main(b, pns_space, 2000000, NULL);
    if (!res.proof || !res.disproof) {
      num_trimmed += node->num_children;
      if (node->num_children) cerr << "Trimmed: " << num_trimmed << endl;
      delete_pns_node(node);     // Delete all its children
      node->num_children = 0;    // But leave its P & D values intact
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}

// Stop at the maximum depth, but keep going for solved nodes with proofs
// larger than PNS_BOOK_TRIVIAL nodes.
t_pns_node* replicate_pns_tree(t_pns_node* node, t_pns_data* data, int depth,
                               int forced) {
  if (node == NULL) return NULL;
  t_pns_node* rep = my_malloc(data);
  *rep = *node;
  if (forced ||
      (node->num_children &&
       ((depth > 0 && pns_open(rep->proof, rep->disproof)) ||
        (pns_solved(rep->proof, rep->disproof) &&
         // Copy at least one level for solved nodes
         (rep->size > PNS_BOOK_TRIVIAL || depth == 1))))) {
    rep->child = (t_pns_node**)malloc(rep->num_children * sizeof(t_pns_node*));
    for (int i = 0; i < rep->num_children; i++) {
      rep->child[i] = replicate_pns_tree(node->child[i], data, depth - 1,
                                         forced);
      if (rep->child[i]) rep->child[i]->parent = rep;
    }
  } else {
    rep->num_children = 0;
    rep->child = NULL;
  }
  return rep;
}

/********************** Loading / saving of pns trees. ***********************/

inline void recompute_ratio(t_pns_node* node) {
  if (node->num_children == 0) {
    node->ratio = 1.0 * node->proof / node->disproof;
  } else {
    double x = 1.0; x--;     // Supress a warning
    node->ratio = 1.0 / x;
    for (int i = 0; i < node->num_children; i++) {
      if (1 / node->child[i]->ratio < node->ratio)
	node->ratio = 1 / node->child[i]->ratio;
    }
  }
}

t_pns_node* load_pns_node(FILE* f, t_pns_data* data) {
  t_pns_node* node = my_malloc(data);
  unsigned char buf[13];
  assert(fread(buf, 1, 13, f) == 13);
  node->mv.from = buf[0];
  node->mv.to = buf[1];
  node->mv.prom = buf[2];
  node->mv.epsquare = buf[3];
  node->proof = *(int*)(buf + 4);
  node->disproof = *(int*)(buf + 8);
  node->parent = NULL;
  node->num_children = buf[12];
  node->size = 1;
  node->child = (t_pns_node**)malloc(node->num_children * sizeof(t_pns_node*));
  for (int i = 0; i < node->num_children; i++) {
    node->child[i] = load_pns_node(f, data);
    node->child[i]->parent = node;
    node->size += node->child[i]->size;
  }

  recompute_ratio(node);
  return node;
}

void save_pns_node(t_pns_node* node, FILE* f) {
  unsigned char buf[13];
  buf[0] = node->mv.from;
  buf[1] = node->mv.to;
  buf[2] = node->mv.prom;
  buf[3] = node->mv.epsquare;
  *(int*)(buf + 4) = node->proof;
  *(int*)(buf + 8) = node->disproof;
  buf[12] = node->num_children;
  assert(fwrite(buf, 1, 13, f) == 13);
  for (int i = 0; i < node->num_children; i++)
    save_pns_node(node->child[i], f);
}

void load_pns_tree(const char* filename, t_pns_data* data) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    tboard b;
    fen_to_board(NEW_BOARD, &b);
    init_pns_data(data, &b);
  } else {
    load_pns_node(f, data);
    fen_to_board(NEW_BOARD, &data->b_orig);
    fen_to_board(NEW_BOARD, &data->b_current);
    fclose(f);
  }
  cerr << "[BOOK] Initialized, " << data->size << " nodes loaded from "
       << filename << endl;
}

void save_pns_tree(const char* filename, t_pns_data* data) {
  char temp[] = "book.tmp";
  FILE *f = fopen(temp, "wb");
  save_pns_node(data->root, f);
  fclose(f);
  assert(!rename(temp, filename));
  cerr << "Book saved to " << filename << " (" << data->size << " nodes)\n";
}

void add_to_pns_hash_recursively(t_pns_hash* pns_hash, t_pns_node* node,
				 tboard* b) {
  if (node == NULL || !node->num_children) return;

  tboard orig_b = *b;
  if (node->mv.from != -1)
    move(&orig_b, node->mv);
  (*pns_hash)[orig_b.hashValue] = node;
  for (int i = 0; i < node->num_children; i++) {
    tboard new_b = orig_b;
    add_to_pns_hash_recursively(pns_hash, node->child[i], &new_b);
  }
}

t_pns_hash* populate_hash(t_pns_node* root) {
  tboard b;
  fen_to_board(NEW_BOARD, &b);
  t_pns_hash* pns_hash = new t_pns_hash;
  add_to_pns_hash_recursively(pns_hash, root, &b);
  return pns_hash;
}

// Finds the most proving node for this PNS tree.
// Changes data->b_current to reflect the moves from the root to the MPN.
// Assumes this move sequence is valid from data->b_orig
// Do NOT assume that node == data->root, although node appears in data.
t_pns_node* select_mpn(t_pns_node* node, t_pns_data* data) {
  if (data == book)
    cerr << "Selecting mpn:";
  data->b_current = data->b_orig;
  while (node->num_children) {
    int best_child = 0;
    int rnd = rand();
    if (node->proof == INF_NODES) {
      // If node is INF | FIN, choose one of the surviving children at random.
      int numChoices = 0;
      for (int i = 0; i < node->num_children; i++) {
        if (node->child[i]->proof) {
          numChoices++;
          if (rnd % numChoices == 0) {
            best_child = i;
          }
        }
      }
    } else if (data == book && FLAGS_by_ratio) {
      for (int i = 1; i < node->num_children; i++)
        if (node->child[i]->ratio > node->child[best_child]->ratio)
          best_child = i;
    } else {
      // If node is FIN | x, search for a child of the form x | FIN.
      // If there are multiple choices, pick one at random.
      // The other cases are either absurd (e.g. 0 | FIN) or they cannot
      // happen as they do not require analysis (e.g. INF | INF).
      int numChoices = 0;
      int rnd = rand();
      for (int i = 0; i < node->num_children; i++) {
        if (node->child[i]->disproof == node->proof) {
          numChoices++;
          if (rnd % numChoices == 0) {
            best_child = i;
          }
        }
      }
    }

    node = node->child[best_child];
    if (data == book)
      cerr << " " << move_to_san(&data->b_current, node->mv);
    move(&data->b_current, node->mv);
  }
  if (data == book)
    cerr << endl << "Original values: " << node->proof << "|" << node->disproof
	 << " (" << node->ratio << ")" << endl;
  return node;
}

// Sets node->proof and node->disproof if b is a final position.
// Assumes there are no legal moves on b.
inline void set_values_leaf(t_pns_node* node, tboard* b) {
  node->proof = 
    ((b->side == WHITE && b->whitecount < b->blackcount) ||
     (b->side == BLACK && b->whitecount > b->blackcount)) ?
    0 : INF_NODES;
  node->disproof = 
    ((b->side == BLACK && b->whitecount < b->blackcount) ||
     (b->side == WHITE && b->whitecount > b->blackcount)) ?
    0 : INF_NODES;
}

// Sets the P & D values for a newly expanded node in PNS
void set_values_level1(t_pns_node* node, tboard* b) {
  if (!b->whitecount) {
    // Final configuration, white wins
    if (b->side == WHITE) {
      node->proof = 0; node->disproof = INF_NODES;
    } else {
      node->proof = INF_NODES; node->disproof = 0;
    }
    return;
  }

  if (!b->blackcount) {
    // Final configuration, black wins
    if (b->side == BLACK) {
      node->proof = 0; node->disproof = INF_NODES;
    } else {
      node->proof = INF_NODES; node->disproof = 0;
    }
    return;
  }

  if (b->whitecount + b->blackcount <= MEN && !WEAKENED && USE_EGTB) {
    // EGTB position
    int egtb_score = egtb_lookup(b);
    assert(egtb_score != EGTB_UNKNOWN);
    if (egtb_score == EGTB_DRAW) {
      node->proof = node->disproof = INF_NODES;
    } else if (egtb_score >= 0) {
      node->proof = 0; node->disproof = INF_NODES;
    } else {
      node->proof = INF_NODES; node->disproof = 0;
    }
    return;
  }

  // Count the moves
  tmovelist ml;
  getallvalidmoves(b, &ml);
  if (!ml.count) {
    // Final position, either side may win or it may be a draw
    set_values_leaf(node, b);
    return;
  }

  // Open position
  node->proof = 1; node->disproof = ml.count;
  // Possibly set P or D to INF by checking bishops
  if ((b->wbishop[0] == b->whitecount && b->bbishop[1]) ||
      (b->wbishop[1] == b->whitecount && b->bbishop[0])) {
    // White cannot lose
    if (b->side == WHITE) node->disproof = INF_NODES;
    else node->proof = INF_NODES;
  }

  if ((b->bbishop[0] == b->blackcount && b->wbishop[1]) ||
      (b->bbishop[1] == b->blackcount && b->wbishop[0])) {
    // Black cannot lose
    if (b->side == WHITE) node->proof = INF_NODES;
    else node->disproof = INF_NODES;
  }
}

void recompute_sizes(t_pns_node* node) {
  if (!node) return;
  node->size = 1; // For the node itself
  for (int i = 0; i < node->num_children; i++) {
    recompute_sizes(node->child[i]);
    node->size += node->child[i] ? node->child[i]->size : 0;
  }
}

// If all is set, print all children of a node, even if a winning move exists
void print_tree_helper(t_pns_node* node, int all, int depth, int max_depth) {
  for (int i = 0; i < 3 * depth; i++)
    cerr << " ";
  cerr << movetostring(node->mv).c_str() << " " << node->proof << "|"
       << node->disproof << " (" << node->ratio << ")" << endl;

  if (!node->num_children || depth == max_depth)
    return;

  if (!node->proof && !all) { // Winning node, just search for one winning move
    int i = 0;
    while (i < node->num_children && node->child[i]->disproof) i++;
    assert(i < node->num_children);
    print_tree_helper(node->child[i], all, depth + 1, max_depth);
  } else {        // Either a losing node or an open node -- print all children
    for (int i = 0; i < node->num_children; i++)
      print_tree_helper(node->child[i], all, depth + 1, max_depth);
  }
}

void print_tree(t_pns_node* node, int all, int max_depth) {
  print_tree_helper(node, all, 0, max_depth);
}

// Assuming src is solved, copies the proof in dest. Does NOT do any trimming.
// Wastes 1 node. Does not modify src.
// forced -- if true, force complete copy
void copy_proof_to_book(t_pns_node* dest, t_pns_node* src, int depth,
                        int forced) {
  recompute_sizes(src);
  t_pns_node* copy = replicate_pns_tree(src, book, depth, forced);
  dest->child = copy->child;
  dest->num_children = copy->num_children;
  for (int i = 0; i < dest->num_children; i++)
    if (dest->child[i]) dest->child[i]->parent = dest;
}

// Create a node's children and initialize them using the lower-level PNS
void expand(t_pns_data* data, t_pns_node* node, int quick) {
  if (data == book && !quick) {
    t_pns_result res = pns_main(&data->b_current, pns_space, 0, NULL);
    copy_proof_to_book(node, pns_space->root, 1, 0);
    // Sort the children in increasing order of disproof number and decreasing
    // order of proof number
    for (int i = 0; i < node->num_children; i++) {
      int k = i;
      for (int j = i + 1; j < node->num_children; j++)
        if (node->child[j]->disproof < node->child[k]->disproof ||
            (node->child[j]->disproof == node->child[k]->disproof &&
             node->child[j]->proof > node->child[k]->proof))
          k = j;
      t_pns_node* aux;
      aux = node->child[i];
      node->child[i] = node->child[k];
      node->child[k] = aux;
    }
    // Set the children's ratios
    for (int i = 0; i < node->num_children; i++)
      node->child[i]->ratio = 1.0 * node->child[i]->proof /
                              node->child[i]->disproof;
    
    // Print the children
    for (int i = 0; i < node->num_children; i++) {
      cerr << move_to_san(&data->b_current, node->child[i]->mv) << " -> "
           << node->child[i]->proof << "|" << node->child[i]->disproof
           << " (" << node->child[i]->ratio << ")" << endl;
    }
  } else {
    tmovelist ml;
    getallvalidmoves(&data->b_current, &ml);
    node->num_children = ml.count;
    if (ml.count)
      node->child = (t_pns_node**)malloc(ml.count * sizeof(t_pns_node*));

    for (int i = 0; i < ml.count; i++) {
      t_pns_node* c = my_malloc(data);
      c->mv = ml.move[i];
      c->parent = node;
      c->child = NULL;
      c->num_children = 0;
      node->child[i] = c;
      tsaverec sr;
      saveboard(&data->b_current, ml.move[i], &sr);
      move(&data->b_current, ml.move[i]);
      set_values_level1(c, &data->b_current);
      restoreboard(&data->b_current, &sr);
    }
  }
}

t_pns_node* pns_follow_path(t_pns_data* data, tmovelist* ml) {
  t_pns_node* root = data->root;
  for (int i = 0; i < ml->count; i++) {
    if (!root->num_children) {
      cerr << "Expanding node so we can follow "
           << move_to_san(&data->b_current, ml->move[i])
	   << endl;
      expand(data, root, 1);
    }
    int j = 0;
    while (j < root->num_children &&
	   !same_move(ml->move[i], root->child[j]->mv))
      j++;
    assert(j != root->num_children);
    root = root->child[j];
    move(&data->b_orig, root->mv);
    data->b_current = data->b_orig;
  }
  return root;
}

// Hash all the won and lost positions of a tree top
int hash_tree_top(t_pns_node* root, tboard* b, int depth) {
  if (!depth) return 0;
  if (!root->proof || !root->disproof) {
    ADD_TO_HASH(b->hashValue, MAXDEPTH, (root->proof ? -WIN : WIN), H_EQ,
		0xff);
    return 1;
  }

  tsaverec rec;
  int total = 0;
  for (int i = 0; i < root->num_children; i++) {
    saveboard(b, root->child[i]->mv, &rec);
    move(b, root->child[i]->mv);
    total += hash_tree_top(root->child[i], b, depth - 1);
    restoreboard(b, &rec);
  }
  return total;
}

// Sets the P & D numbers for a node based on the values of its children.
// This should only happen for nodes between the most recently expanded node
// and the PNS root.
// Returns true if anything has changed, false otherwise.
int set_proof_numbers(tboard* b, t_pns_node* node, int update_ratios) {
  if (!node->num_children) {
    // Unless we've already found this node to be a draw by repetition
    if (node->proof != INF_NODES || node->disproof != INF_NODES)
      // This is the node we've just expanded and there are no legal moves.
      // The board is still in data->b_current
      set_values_leaf(node, b);
    if (update_ratios) recompute_ratio(node);
    return 1;
  } else {
    // P = min child_D; D = sum child_P
    int orig_proof = node->proof;
    int orig_disproof = node->disproof;
    node->proof = INF_NODES;
    node->disproof = 0;
    for (int i = 0; i < node->num_children; i++) {
      if (node->child[i]->disproof < node->proof)
	node->proof = node->child[i]->disproof;
      node->disproof += node->child[i]->proof;
      if (node->disproof > INF_NODES)
	node->disproof = INF_NODES;
    }
    if (update_ratios) recompute_ratio(node);

    // When updating ratios, go all the way to the root. The ratios may change
    // even when the P/D numbers stay the same.
    return (update_ratios || node->proof != orig_proof ||
            node->disproof != orig_disproof);
  }
}

void update_ancestors(tboard* b, t_pns_node* node, int update_ratios) {
  while (node != NULL) {
    if (!set_proof_numbers(b, node, update_ratios))
      return; // No more changes above this point
    node = node->parent;
  }
}

inline int opposite_moves(tmove a, tmove b) {
  return (a.from == b.to && a.to == b.from &&
          a.prom == EMPTY && b.prom == EMPTY);
}

int four_useless_moves(t_pns_node* node) {
  if (!node ||
      !node->parent ||
      !node->parent->parent ||
      !node->parent->parent->parent)
    return 0; // Not even 4 moves deep
  tmove mv1 = node->parent->parent->parent->mv;
  tmove mv2 = node->parent->parent->mv;
  tmove mv3 = node->parent->mv;
  tmove mv4 = node->mv;
  return opposite_moves(mv1, mv3) && opposite_moves(mv2, mv4);
}

// b -- the board to analyze
// data -- the pns memory in which to work
// max_nodes -- how many nodes are allowed (0 means use all available memory)
// set_values_routine -- either simple 1|1 initializaion (for pn search) or
//                    -- call to outer search (for pn2 search)
t_pns_result pns_main(tboard* b, t_pns_data* data, int max_nodes,
                      tmovelist* subtree) {
  if (!max_nodes || max_nodes > data->max_nodes)
    max_nodes = data->max_nodes;

  if (data != book) {
    delete_pns_data(data); // Never wipe the book contents
    init_pns_data(data, b);
  }

  t_pns_node* root = (subtree && data == book)
    ? pns_follow_path(data, subtree) : data->root;

  int round_robin = 0; // In lenthep mode, save book more seldom
  while ((pns_finite(root->proof) || pns_finite(root->disproof)) &&
 	 data->size < max_nodes) {
    if (data == book)
      cerr << "\nProof " << root->proof << " Disproof " << root->disproof
	   << "       Ratio " << root->ratio << endl;
    t_pns_node* mpn = select_mpn(root, data);

    // Check whether the last 4 moves revert to the position 4 moves ago
    if (four_useless_moves(mpn)) {
      mpn->proof = mpn->disproof = INF_NODES;
    } else {
      // Look for a duplicate solution we have solved before
      int found = 0;
      if (data == book) {
        t_pns_hash::iterator it = pns_hash->find(book->b_current.hashValue);
        if (it != pns_hash->end()) {
          found = 1;
          cerr << "We've seen this node before! Copying proof!\n";
          copy_proof_to_book(mpn, it->second, 100, 1);
        }
      }
      if (!found) expand(data, mpn, 0);
    }

    update_ancestors(&data->b_current, mpn, data == book);
    if (data == book && (!mpn->proof || !mpn->disproof))
      (*pns_hash)[data->b_current.hashValue] = mpn;
    if (data == book) {
      if (++round_robin == FLAGS_save_every) {
	round_robin = 0;
	save_pns_tree(BOOK_FILENAME, data);
      } else {
	cerr << "Saving in " << (FLAGS_save_every - round_robin) << " iterations\n";
      }
    }
    if (timer_expired) break; // Loop at least once.
  }

  if (data == book) {
    trim_uninteresting_branches(root, &data->b_orig, 0);
    save_pns_tree(BOOK_FILENAME, data);
  }

  // Now construct the result and return it;
  t_pns_result res;
  res.proof = root->proof;
  res.disproof = root->disproof;

  if (pns_won_drawn(res.proof, res.disproof)) {
    // This returns the actual move that leads to a win or draw
    int i = 0;
    // Search for a win first
    while (i < root->num_children && root->child[i]->disproof) i++;
    if (i == root->num_children) {
      // No win found; search for a draw next
      i = 0;
      while (i < root->num_children && root->child[i]->proof != INF_NODES) i++;
    }
    res.mv = (i < root->num_children) ? root->child[i]->mv : INVALID_MOVE;
  } else {
    res.mv = INVALID_MOVE;
  }

  return res;
}

int pns_trim_move_list(tboard *b, tmovelist* ml, int max_nodes) {
  t_pns_result res = pns_main(b, pns_space, max_nodes, NULL);
  int hashed = hash_tree_top(pns_space->root, b, 100);
  cerr << "[PNS] Hashed " << hashed << " positions from trimming\n";

  // Just return the winning move;
  if (!res.proof) {
    ml->count = 1;
    ml->move[0] = res.mv;
    return WIN;
  }

  ml->count = pns_space->root->num_children;
  int proof[ml->count], disproof[ml->count];
  double ratio[ml->count];
  for (int i = 0; i < ml->count; i++) {
    ml->move[i] = pns_space->root->child[i]->mv;
    proof[i] = pns_space->root->child[i]->proof;
    disproof[i] = pns_space->root->child[i]->disproof;
  }

  // Return the entire list and let alpha-beta decide
  if (!res.disproof) {
    cerr << "[PNS] Lost\n";
    return -WIN;
  }

  sort_by_pns_ratio(ml, proof, disproof, ratio);
  assert(proof[0]); // or else we would have returned before.
  cerr << "[PNS]";
  for (int i = 0; i < ml->count; i++)
    cerr << " " << move_to_san(b, ml->move[i]) << " " << proof[i] << "|"
	 << disproof[i];
  cerr << endl;

  // Now drop everything that's worse than 5% of the best node
  int num_kept = 1;
  while (num_kept < ml->count && ratio[num_kept] > 0.05 * ratio[0])
    num_kept++;
  ml->count = num_kept;
  cerr << "[PNS] Keeping " << num_kept << " moves\n";
  return 0;
}

int losing_move(tboard *b, tmove mv) {
  tboard b_new = *b;
  move(&b_new, mv);
  set_alarm(2000); // Do not spend more than 2 seconds
  t_pns_result res = pns_main(&b_new, pns_space, 300000, NULL);
  return !res.proof;
}

int query_book(tboard *b, tmove* mv) {
  // Do we know anything about this position?
  t_pns_hash::iterator it = pns_hash->find(b->hashValue);
  if (it == pns_hash->end()) {
    info("Position not in book");
    return -1;
  }

  t_pns_node* node = it->second;
  assert(node && node->disproof);  // We cannot have played a loss!

  // Winning node, return a winning move
  if (!node->proof) {
    for (int i = 0; i < node->num_children; i++)
      if (!node->child[i]->disproof) {
	*mv = node->child[i]->mv;
        return WIN;
      }
    assert(0); // Node is winning, but we couldn't find a winning move
  }

  // Open node, return a move that has a ratio of at least 40% of the optimum.
  double best_ratio = 1 / node->ratio;
  int allowed[node->num_children];
  int count = 0;

  cerr << "[BOOK] Open, choosing from:";
  for (int i = 0; i < node->num_children; i++) {
    if (node->child[i]->ratio >= 0.4 * best_ratio &&
        book_optimality * node->child[i]->ratio / best_ratio >= 0.2) {
      allowed[count++] = i;
      cerr << " " << move_to_san(b, node->child[i]->mv) << "("
	   << node->child[i]->ratio << ")";
    }
  }
  cerr << endl;

  // Now make sure the move we return isn't losing
  int i = rand() % count;
  while (node->child[allowed[i]]->size < 100 &&
         losing_move(b, node->child[allowed[i]]->mv)) {
    cout << "tell fcatalin Move "
         << move_to_san(b, node->child[allowed[i]]->mv)
         << " is losing. Add it to the book!\n";
    i = (i + 1) % count;
  }
  // Count the fact that we're playing a non-optimal move
  book_optimality *= node->child[allowed[i]]->ratio / best_ratio;
  cerr << "[BOOK] Optimality at " << book_optimality << endl;
  *mv = node->child[allowed[i]]->mv;

  return DRAW;
}

void browse_pns_tree(const char* filename) {
  tmovelist ml;
  ml.count = 0;
  string move_names[100];

  t_pns_node* current_node = book->root;
  char s[500], command[100], arg[100];

  tboard boards[200]; // The levels
  int level = 0;
  boards[0] = book->b_current;

  // Parse simple commands with 0 or 1 arguments
  while (1) {
    cerr << "Command: ";
    command[0] = arg[0] = '\0';
    fgets(s, sizeof(s), stdin);
    s[strlen(s) - 1] = '\0';
    sscanf(s, "%s", command);
    sscanf(s + strlen(command), "%s", arg);
    if (!strcmp(command, "print") || !strcmp(command, "p")) {
      int depth = atoi(arg);
      if (depth < 1) depth = 1;
      print_tree(current_node, 1, depth);
    } else if (!strcmp(command, "down") || !strcmp(command, "d")) {
      char *s2 = s + strlen(command);
      while (sscanf(s2, "%s", arg) == 1) {
	tmove mv = san_to_move(&book->b_current, arg);
	int i = 0, found = 0;
	while (i < current_node->num_children && !found) {
	  if (same_move(mv, current_node->child[i]->mv)) {
	    current_node = current_node->child[i];
            cerr << "Going down [" << move_to_san(&book->b_current, mv) << "]"
                 << endl;
            move_names[ml.count] = move_to_san(&book->b_current, mv);
	    ml.move[ml.count++] = mv;
	    move(&book->b_current, mv);
	    found = 1;
	    boards[++level] = book->b_current;
	  } else {
	    i++;
	  }
	}
	
	if (!found) {
          long x = strtol(arg, NULL, 10);
          if (x >= 1 && x <= current_node->num_children) {
            cerr << "Going down ["
                 << move_to_san(&book->b_current,
                                current_node->child[x - 1]->mv)
                 << "]\n";
	    current_node = current_node->child[x - 1];
            move_names[ml.count] = move_to_san(&book->b_current,
                                               current_node->mv);
	    ml.move[ml.count++] = current_node->mv;
	    move(&book->b_current, current_node->mv);
	    boards[++level] = book->b_current;
          } else {
            cerr << "No such child: [" << arg << "]" << endl;
          }
        }
        while (isspace(*s2)) s2++;
	s2 += strlen(arg);
      }
    } else if (!strcmp(command, "delete")) {
      tmove mv = san_to_move(&book->b_current, arg);
      int i = 0, found = 0;
      while (i < current_node->num_children && !found) {
	if (same_move(mv, current_node->child[i]->mv)) {
	  delete_pns_child(current_node, i);
	  found = 1;
	} else {
	  i++;
	}
      }

      if (found)
	cerr << "Deleted child [" << move_to_san(&book->b_current, mv) << "]"
             << endl;
      else
	cerr << "No such child: [" << arg << "]" << endl;
    } else if (!strcmp(command, "top") || !strcmp(command, "t")) {
      current_node = book->root;
      book->b_current = book->b_orig;
      ml.count = 0;
      level = 0;
      cerr << "Back to top.\n";
    } else if (!strcmp(command, "up") || !strcmp(command, "u")) {
      int delta = atoi(arg);
      if (!delta) delta = 1;
      if (delta > level) {
	cerr << "You cannot go up by that many levels\n";
      } else {
	for (int i = 0; i < delta; i++)
	  current_node = current_node->parent;
	level -= delta;
	book->b_current = boards[level];
	ml.count -= delta;
      }
    } else if (!strcmp(command, "path")) {
      if (ml.count) {
	cerr << "You're looking at";
        for (int i = 0; i < ml.count; i++)
          cerr << " " << move_names[i];
        cerr << endl;
      } else {
	cerr << "You're at root level in the book\n";
      }
    } else if (!strcmp(command, "board") || !strcmp(command, "b")) {
      printboard(&book->b_current);
    } else if (!strcmp(command, "expand") || !strcmp(command, "e")) {
      if (current_node ->num_children)
	cerr << "This node already has some children\n";
      else {
	expand(book, current_node, 1);
	update_ancestors(&book->b_current, current_node, 1);
      }
    } else if (!strcmp(command, "unexpand")) {
      delete_pns_node(current_node);
      current_node->proof = current_node->disproof = 1;
      update_ancestors(&book->b_current, current_node->parent, 1);
    } else if (!strcmp(command, "draw")) {
      delete_pns_node(current_node);
      current_node->proof = current_node->disproof = INF_NODES;
      update_ancestors(&book->b_current, current_node->parent, 1);
    } else if (!strcmp(command, "trim")) {
      trim_uninteresting_branches(current_node, &book->b_current, atoi(arg));
      cerr << "Seen: " << num_seen << " Trimmed: " << num_trimmed << endl;
    } else if (!strcmp(command, "proof")) {
      current_node->proof = atoi(arg);
      update_ancestors(&book->b_current, current_node->parent, 1);
    } else if (!strcmp(command, "disproof")) {
      current_node->disproof = atoi(arg);
      update_ancestors(&book->b_current, current_node->parent, 1);
    } else if (!strcmp(command, "reload") || !strcmp(command, "r")) {
      delete_pns_data(book);
      load_pns_tree(filename, book);
      pns_hash = populate_hash(book->root);
      current_node = book->root;
      level = 0;
      for (int k = 0; k < ml.count; k++) {
	int i = 0, found = 0;
	while (i < current_node->num_children && !found) {
	  if (same_move(ml.move[k], current_node->child[i]->mv)) {
	    current_node = current_node->child[i];
            cerr << "Going down [" << move_to_san(&book->b_current, ml.move[k])
                 << "]" << endl;
	    move(&book->b_current, ml.move[k]);
	    level++;
	    found = 1;
	  } else {
	    i++;
	  }
	}
	
	if (!found) {
	  cerr << "No such child: [" << arg << "]" << endl;
          break;
	}
      }
      print_tree(current_node, 1, 1);
    } else if (!strcmp(command, "save")) {
      save_pns_tree(filename, book);
    } else if (!strcmp(command, "quit")) {
      return;
    } else {
      cerr << "Unknown command\n";
    }
  }
}

void traverse_bad_lines(t_pns_node* node, int depth, tboard* b) {
  if (!node) return;

  if (node->num_children == 0 &&
      (node->proof == 1 || node->proof == 10000000) &&
      (node->disproof == 1 || node->disproof == 10000000)) {
    tmovelist ml;
    ml.count = 0;
    for (t_pns_node* t = node; t != NULL; t = t->parent)
      ml.move[ml.count++] = t->mv;
    ml.count--;
    for (int i = 0, j = ml.count - 1; i < j; i++, j--) {
      tmove maux = ml.move[i]; ml.move[i] = ml.move[j]; ml.move[j] = maux;
    }
    cerr << "CALLING PNS ON: ";
    printmovelist(ml);
    fen_to_board(NEW_BOARD, &book->b_orig);
    fen_to_board(NEW_BOARD, &book->b_current);
    pns_main(NULL, book, book->size + 1, &ml);
  } else {
    for (int i = 0; i < node->num_children; i++) {
      tsaverec rec;
      saveboard(b, node->child[i]->mv, &rec);
      move(b, node->child[i]->mv);
      traverse_bad_lines(node->child[i], depth + 1, b);
      restoreboard(b, &rec);
    }
  }
}

void print_bad_pns_lines() {
  tboard b;
  fen_to_board(NEW_BOARD, &b);
  traverse_bad_lines(book->root, 0, &b);
}

void traverse_weak_lines(t_pns_node* node, int depth, int max_depth,
                         tboard* b, string* mv, double threshold) {
  if (depth > max_depth) return;

  if (node->num_children && depth < max_depth) {
    // Non-terminal, recursively call each child
    for (int i = 0; i < node->num_children; i++) {
      tsaverec rec;
      saveboard(b, node->child[i]->mv, &rec);
      mv[depth] = move_to_san(b, node->child[i]->mv);
      move(b, node->child[i]->mv);
      traverse_weak_lines(node->child[i], depth + 1, max_depth, b, mv,
                          threshold);
      restoreboard(b, &rec);
    }
  } else {
    // Terminal. If node is very weak, but not lost, print out line.
    if (node->proof > 0 && node->disproof > 0 &&
        (double)node->disproof / node->proof < threshold) {
      // cout << node->proof << " " << node->disproof;
      for (int i = 0; i < depth; i++)
        cout << " " << mv[i];
      cout << endl;
    }
  }
}

void print_weak_pns_lines(double threshold, int max_depth) {
  tboard b;
  string mv[200];
  fen_to_board(NEW_BOARD, &b);
  traverse_weak_lines(book->root, 0, max_depth, &b, mv, threshold);
}
