<!DOCTYPE html>
<html>
  <head>
    <title>Nilatac suicide chess engine</title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <link href="web/book.css?v=3" rel="stylesheet" type="text/css"/>

    <style type="text/css">
    </style>

  </head>

  <body>
    <div class="title">
      <span class="plug">Nilatac</span> suicide chess engine
    </div>

    <h3>About Nilatac</h3>

<p>Nilatac is a <a href="http://en.wikipedia.org/wiki/Antichess">suicide chess</a> engine. It plays according to FICS rules (stalemate is a win for the
player with the fewer pieces remaining -- variant 5 on Wikipedia).</p>

<p>The fun thing about suicide chess, from a software engineer's point of view, is that it appears to be solvable, which means a computer could
produce a game tree ("proof") in which White wins regardless of Black's defense. Some lines, such as <b>1. e3 d5</b> or <b>1. e3 e5</b>, are trivially
solvable by average players. Other lines, like <b>1. e3 b5</b> took years of computing time from several different chess programs.</p>

<p>Nilatac was developed mostly between 2001 and 2003. I haven't worked on the code recently, although I did do some minor opening research, notably
solving <b>1. e3 Nh6</b> in 2012. Nilatac is registered on FICS, but it is mostly offline. Do let me know if you want to play it and I will gladly
bring it online. Nilatac's weaker brother, CatNail, is (almost) always on FICS and has played over 1.000.000 games.</p>

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

<p>Copyright 2001-2012 <a href="http://catalin.francu.com">Cătălin Frâncu</a></p>

<p>Nilatac's source code is released under the <a href="http://www.gnu.org/licenses/gpl-2.0.html">GNU General Public License, version 2</a>.</p>

<p>Nilatac's opening book and endgame tables are released under the <a href="http://creativecommons.org/licenses/by-sa/3.0/">Creative Commons
Attribution-ShareAlike 3.0 Unported License</a>.</p>

  </body>
</html>
