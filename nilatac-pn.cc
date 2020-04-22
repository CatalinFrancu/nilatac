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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "egtb.h"
#include "pns.h"
#include "suicide.h"
#include "webserver.h"

// displays usage info and exits
void print_usage(char* progname) {
  cerr << "USAGE: " << progname << " [options] command" << endl;
  cerr << endl;
  cerr << "OPTIONS: " << endl;
  cerr << "\t--by_ratio\t\tSelect mpn by ratio rather than by PN" << endl;
  cerr << "\t--movelist\t\tMove list to analyze or browse" << endl;
  cerr << "\t--port\t\t\tWebserver port" << endl;
  cerr << "\t--pn_ratio\t\tThreshold disproof/proof ratio" << endl;
  cerr << "\t--save_every\t\tFrequency of book saves" << endl;
  cerr << endl;
  cerr << "COMMANDS: " << endl;
  cerr << "\t analyze\t\tAnalyze with PN-search and add results to book\n";
  cerr << "\t browse\t\t\tBrowse the book\n";
  cerr << "\t create_egtb\t\tRegenerate the endgame table bases\n";
  cerr << "\t create_egtb_config\tRegenerate a specific endgame table\n";
  cerr << "\t print_bad_lines\tPrint the suspicious lines in the book\n";
  cerr << "\t print_weak_lines\tPrint the weakest lines in the book\n";
  cerr << "\t webserver\t\tStart a webserver for online querying\n";
  exit(1);
}

// Crashes if the arguments are bad.
void parse_command_line(int argc, char** argv) {
  // Start at 1, skip the file name
  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "--movelist=", strlen("--movelist="))) {
      FLAGS_movelist = argv[i] + strlen("--movelist=");

    } else if (!strncmp(argv[i], "--port=", strlen("--port="))) {
      char* endptr;
      FLAGS_port = strtol(argv[i] + strlen("--port="), &endptr, 10);
      if (*endptr != '\0') {
        cerr << "ERROR: bad port number\n";
        exit(1);
      }

    } else if (!strncmp(argv[i], "--max_depth=", strlen("--max_depth="))) {
      char* endptr;
      FLAGS_max_depth = strtol(argv[i] + strlen("--max_depth="), &endptr, 10);
      if (*endptr != '\0') {
        cerr << "ERROR: bad max_depth\n";
        exit(1);
      }

    } else if (!strncmp(argv[i], "--pn_ratio=", strlen("--pn_ratio="))) {
      char* endptr;
      FLAGS_pn_ratio = strtod(argv[i] + strlen("--pn_ratio="), &endptr);
      if (*endptr != '\0') {
        cerr << "ERROR: bad pn_ratio\n";
        exit(1);
      }

    } else if (!strcmp(argv[i], "--by_ratio")) {
      FLAGS_by_ratio = true;

    } else if (!strncmp(argv[i], "--save_every=", strlen("--save_every="))) {
      char* endptr;
      FLAGS_save_every = strtol(argv[i] + strlen("--save_every="),
                                &endptr, 10);
      if (*endptr != '\0') {
        cerr << "ERROR: bad save_every\n";
        exit(1);
      }

    } else if (!strncmp(argv[i], "--", 2)) {
      cerr << "ERROR: Unknown flag: " << argv[i] << endl;
      exit(1);

    } else {
      if (FLAGS_command != "") print_usage(argv[0]);
      FLAGS_command = argv[i];
    }
  }

  if (FLAGS_command == "") print_usage(argv[0]);
}

int main(int argc, char** argv) {
  parse_command_line(argc, argv);

  init_common();
  init(BOOK_FILENAME);

  if (FLAGS_command == "analyze") {
    tmovelist ml;
    string_to_movelist(FLAGS_movelist, &ml);
    start_server_thread(FLAGS_port);
    pns_main(NULL, book, 0, &ml);

  } else if (FLAGS_command == "browse") {
    start_server_thread(FLAGS_port);
    browse_pns_tree(BOOK_FILENAME);

  } else if (FLAGS_command == "webserver") {
    start_server(FLAGS_port);

  } else if (FLAGS_command == "print_bad_lines") {
    print_bad_pns_lines();

  } else if (FLAGS_command == "print_weak_lines") {
    print_weak_pns_lines(FLAGS_pn_ratio, FLAGS_max_depth);

  } else if (FLAGS_command == "create_egtb") {
    init_egtb();
    // Prevent overwriting of the egtb file. Uncomment if you know what you're
    // doing
    // egtb_create(atoi(argv[2]), atoi(argv[3]));

  } else if (FLAGS_command == "create_egtb_config") {
    // Create one specific EGTB (e.g. KPPvK)
    init_egtb();
    for (int i = 2; i < argc; i++) {
      int config = string_to_config(argv[i]);
      if (config == -1) {
        cerr << "Cannot find config " << argv[i] << endl;
        exit(1);
      }
      else {
        egtb_create(config, config + 1);
      }
    }

  } else { // Unknown command
    print_usage(argv[0]);
  }
}
