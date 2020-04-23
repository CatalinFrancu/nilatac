<!DOCTYPE html>
<html>
  <head>
    <title>Nilatac suicide chess engine</title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <link href="web/book.css?v=4" rel="stylesheet" type="text/css"/>

    <style type="text/css">
    </style>

  </head>

  <body>
    <div class="title">
      <span class="plug">Nilatac</span> suicide chess engine
    </div>

    <h3>About Nilatac</h3>

    <p>
      Nilatac is a <a href="http://en.wikipedia.org/wiki/Antichess">suicide chess</a>
      engine. It plays according to international rules (the player being
      stalemated wins). See the Footnote for caveats.
    </p>

    <p>
      The fun thing about suicide chess, from a software engineer's point of
      view, is that it was long believed to be solvable, meaning that a
      computer could produce a game tree ("proof") in which White wins
      regardless of Black's defense. In fact, <a href="http://magma.maths.usyd.edu.au/~watkins/LOSING_CHESS/">Mark Watkins solved it</a> in
      2016 (I would estimate that around 20% of the work was done by other
      engines, including Nilatac). Some lines, such as <b>1. e3 d5</b> or <b>1. e3 e5</b>,
      are trivially solvable by average players. Other lines, like <b>1. e3 b6</b>,
      took years of computing time from several different chess programs.
    </p>

    <p>
      Nilatac was developed mostly between 2001 and 2003. I haven't worked on
      the code recently, although I did do some minor opening research,
      notably solving <b>1. e3 Nh6</b> in 2012.
      <a href="https://lichess.org/@/NilatacBot">Nilatac</a> and its weakened
      sibling <a href="https://lichess.org/@/CatNail">CatNail</a> are
      registered on Lichess and usually online. Before, they were active on
      FICS (where CatNail played over 1.2 million games).
    </p>

    <h3>Features</h3>

    <ul>
      <li>4-men <a href="http://en.wikipedia.org/wiki/Endgame_tablebase">endgame tablebases</a> (EGTB) (3 GB);</li>

      <li><a href="book.php">opening book</a> (click to browse online);</li>

      <li>alpha-beta with transposition tables and various optimizations;</li>

      <li>proof number search.</li>
    </ul>

    <h3>Issues</h3>

    <ul>
      <li>Slow move generator; the board is stored as a char matrix (no bitboards);</li>
      <li>Crashes and infinite loops in the proof-number search algorithm, since it ignores transpositions and repetitions; </li>
      <li>EGTB bugs, at least due to the lack of en passant support, and possibly more since the EGTB were never verified;</li>
      <li>slow EGTB generation -- it would take forever to generate the 5-men EGTB, because it didn't use retrograde analysis;</li>
      <li>no EGTB compression;</li>
      <li>no unit tests.</li>
    </ul>

    <h3>Colibri</h3>

    <p>In 2012 and 2013 I spent some time rewriting a new version from scratch, called <a href="http://catalin.francu.com/colibri/www">Colibri</a>. So far
      the only working component are the 5-men endgame tables (browsable online). Time permitting, I'd like to rewrite the PNS engine as well to try and
      solve a few more openings. But it's the bad kind of "time permitting", the kind that tends never to happen.</p>

    <h3>Downloads</h3>

    <p>Nilatac is free software. <a href="https://github.com/CatalinFrancu/nilatac">Get the source code!</a></p>

    <p>I will seed the EGTB files on torrents once anybody asks. :-)</p>

    <h3>License</h3>

    <p>Copyright 2001-2020 <a href="http://catalin.francu.com">Cătălin Frâncu</a></p>

    <p>Nilatac's source code is released under the <a href="http://www.gnu.org/licenses/gpl-2.0.html">GNU General Public License, version 2</a>.</p>

    <p>Nilatac's opening book and endgame tables are released under the <a href="http://creativecommons.org/licenses/by-sa/3.0/">Creative Commons
      Attribution-ShareAlike 3.0 Unported License</a>.</p>

    <h3>Footnote</h3>

    <p>
      Nilatac originally worked according to FICS rules, in which stalemate is
      a win for the player with fewer remaining pieces, or a draw if both
      sides have the same number of pieces. When I moved to Lichess I updated
      the rules only partially due to lack of time.
    </p>

    <ul>
      <li>
        I fixed the alpha-beta and proof number search algorithms. That was
        the easy part.
      </li>

      <li>
        I did not rebuild the opening book, so there can be discrepancies.  It
        is even conceivable that some lines that are won under FICS rules are
        still viable under international rules, although this seems very
        unlikely.
      </li>

      <li>
        I did not rebuild the endgame tables. I expect this to cause issues
        occasionally, for example when the engine, having fewer pieces, will
        stalemate its opponent. Worse yet, these positions can occur in the
        alpha-beta or PNS trees and the engine will actively pursue them.
      </li>

    </ul>

  </body>
</html>
