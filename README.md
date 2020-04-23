# Nilatac

A suicide chess engine with UCI support

## Quick links

* [Online opening book](http://catalin.francu.com/nilatac/book.php)
* [About page](http://catalin.francu.com/nilatac/)
* [Nilatac's Lichess profile](https://lichess.org/@/NilatacBot) (see also its weakened sibling, [CatNail](https://lichess.org/@/CatNail))

## Status

Nilatac is not under active development. I have moved the code here because GitHub is more popular than Subversion, but I haven't worked on the code for many years (except for occasional flurries of book research).

If you have questions or contributions, please contact me - I still have a general interest in suicide chess.

## Installation instructions (GNU/Linux)

* Get the code from this page.
* Optionally, get the [end-game table files](http://catalin.francu.com/egtb.tar.xz). Unpack the archive into the `egtb/` subdirectory. There already exists an `egtb` symlink as a memento, but feel free to change that.
* Choose a make target (read the Makefile for details).
* Run `make <target>`.

## Single-player mode

Nilatac has basic UCI support, so it should work (or almost work...) with any variant-aware GUI. I have tested it with [cutechess](https://github.com/cutechess/cutechess). If you would like to see better support for your favorite GUI, please let me know and I can work on it.

## Book browser

You can load Nilatac's book in memory and use your web browser to query it:

* Run `./nilatac-pn webserver`. It'll take a few seconds to load the book and open a local port.
* Point your browser to `/path/to/nilatac/book.php`.

## Command-line browser

You probably won't need this, but it's there. It allows you to expand leaf nodes, collapse (delete all children of) analyzed nodes, mark nodes as won/drawn/lost and more.

* Run `./nilatac-pn browse`
  * This also opens port 5000 as in the previous section.
* There is no documentation here, sorry. Please read the function `browse_pns_tree()` in `pns.cc` for a list of commands.

## Book research

If you'd like to let Nilatac investigate some openings, run something like:

```
./nilatac-pn --save_every=100 analyze --movelist="c3 e6 Na3"
```

You can stop the research at any time with Ctrl-C, but you will lose unsaved progress (hence the flag `--save-every`).
