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
// NOT CONVERTED YET
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include "pns.h"
#include "suicide.h"

// Set depth to 0 if no move is given. Otherwise, convert n. to 2n-1 and
// n... to 2n (black's move)
int read_lenthep_move(FILE* f, tboard *b, tmove* mv, int depth) {
  int new_depth;
  int depth_given = fscanf(f, "%d", &new_depth);
  int c;

  if (depth_given == -1) {  // End of file
    *mv = INVALID_MOVE;
    return depth;
  }

  if (depth_given) {
    assert(getc(f) == '.');
    if ((c = getc(f)) == '.') {
      assert (getc(f) == '.');
      new_depth = 2 * new_depth;
    } else {
      ungetc(c, f);
      new_depth = 2 * new_depth - 1;
    }
  } else {
    new_depth = depth;
  }
  assert(new_depth <= depth);

  char s[20];
  assert(fscanf(f, "%s", s));
  int len = strlen(s);
  while (s[len - 1] == '?' || s[len - 1] == '!')
    s[--len] = '\0';
  *mv = san_to_move(b + new_depth, s);
  return new_depth;
}

void load_lenthep_book(char* filename) {
  tboard boards[100];
  FILE *f = fopen(filename, "rt");

  int current_depth = 1;
  t_pns_node* current_node = book->root;
  tmove mv;

  fen_to_board(NEW_BOARD, boards + 1);
  do {
    int new_depth = read_lenthep_move(f, boards, &mv, current_depth);

    if (mv.from != -1) {
      // Go up to the correct depth; before that, set the leaf to 10M|1 to
      // indicate it is a winning leaf
      if (new_depth < current_depth) {
	assert(current_node->proof = INF_NODES ||  // make sure it's a leaf
	       (current_node->num_children == 0 && current_node->proof == 1));
	cout << "proof " << INF_NODES << endl;
	cout << "up " << current_depth - new_depth << endl;
	for (int i = 0; i < current_depth - new_depth; i++)
	  current_node = current_node->parent;
	current_depth = new_depth;
	book->b_current = boards[current_depth + 1];
      }

      // Go down the indicated move; before that, expand the node and set all
      // the new leaves to 1|10M to indicate they are losing (so pn^3 doesn't
      // spend time analyzing them
      boards[current_depth + 1] = boards[current_depth];
      move(boards + current_depth + 1, mv);
      cout << "expand\n";

      // We need to actually exp
      if (current_node->num_children == 0) {
	expand(book, current_node, 1);
	update_ancestors(&book->b_current, current_node, 0);
      }

      int child = -1;
      for (int i = 0; i < current_node->num_children; i++) {
	if (same_move(mv, current_node->child[i]->mv))
	  child = i;
	else {
	  if (current_node->child[i]->proof == 1 &&
	      current_node->child[i]->num_children == 0)
	    cout << "down " << move_to_san(boards + current_depth,
                                           current_node->child[i]->mv) << endl
		 << "disproof " << INF_NODES << endl
		 << "up" << endl;
	}
      }
      cout << "down " << move_to_san(boards + current_depth, mv) << endl;
      assert(child != -1);
      current_node = current_node->child[child];
      book->b_current = boards[current_depth + 1];
      current_depth++;
    }
  } while (mv.from != -1); // Read till EOF

  fclose(f);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " <file>\n";
    return 1;
  }

  init();
  init_common(NULL);
  load_lenthep_book(argv[1]);
  cout << "save" << endl
       << "quit" << endl;
  return 0;
}
