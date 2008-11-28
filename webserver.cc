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
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "webserver.h"
#include "pns.h"

void* handle_connection(void* fd_ptr) {
  int fd = (int)fd_ptr;
  FILE *f_in = fdopen(fd, "r+t");
  FILE *f_out = fdopen(fd, "wt");
  char s[100];

  tboard b;
  fen_to_board(NEW_BOARD, &b);
  t_pns_node* current_node = book->root;
  int legal = 1;

  while (legal && fscanf(f_in, "%100s", s) == 1 && strcmp(s, "END")) {
    tmove mv = san_to_move(&b, s);
    int i = 0, found = 0;
    while (i < current_node->num_children && !found) {
      if (same_move(mv, current_node->child[i]->mv)) {
	current_node = current_node->child[i];
	move(&b, mv);
	found = 1;
      } else {
	i++;
      }
    }
    legal &= found;
  }

  fprintf(f_out, "%d\n", legal ? WS_OK : WS_BAD_MOVELIST);
  if (legal) {
    fprintf(f_out, "%s\n", board_to_string(&b).c_str());
    fprintf(f_out, "%d %d %.9lf %d\n",
	    current_node->proof, current_node->disproof,
	    (current_node->proof ? 1 / current_node->ratio : -1),
	    current_node->num_children);
    for (int i = 0; i < current_node->num_children; i++)
      fprintf(f_out, "%s %d %d %.9lf %d\n",
	      move_to_san(&b, current_node->child[i]->mv).c_str(),
	      current_node->child[i]->disproof, current_node->child[i]->proof,
	      (current_node->child[i]->disproof ?
	       current_node->child[i]->ratio : -1),
	      current_node->child[i]->size);
  }
  fflush(f_out);
  fclose(f_in);
  fclose(f_out);
  close(fd);
  return NULL;
}

void start_server(int port) {
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  int sock = socket (AF_INET, SOCK_STREAM, 0);
  assert(sock != -1);
  int n = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof (n));
  fcntl(sock, F_SETFD, 1);
  bind(sock, (struct sockaddr*) &sin, sizeof(sin));
  assert(!listen(sock, 10));
  cerr << "[WEB] Started server on port " << port << endl;

  while (1) {
    int conn = accept(sock, NULL, NULL);
    assert(conn != -1);
    pthread_t* thread = new pthread_t;
    pthread_create(thread, NULL, handle_connection, (void*)conn);
    pthread_detach(*thread);
  }

}

void* start_server_thread_helper(void* port) {
  start_server((int)port);
  return NULL;
}

/* Start the webserver in a different thread, so that we can research
 * the book and while running the webserver
 */
void start_server_thread(int port) {
  pthread_t* thread = new pthread_t;
  pthread_create(thread, NULL, start_server_thread_helper, (void*)port);
}
