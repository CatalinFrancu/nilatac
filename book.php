<?php

require_once 'smarty/Smarty.class.php';

define('INFINITY', 1000000000);
$PIECE_MAP = array('P' => 'pw', 'N' => 'nw', 'B' => 'bw', 'R' => 'rw', 'Q' => 'qw', 'K' => 'kw',
                   'p' => 'pb', 'n' => 'nb', 'b' => 'bb', 'r' => 'rb', 'q' => 'qb', 'k' => 'kb', '-' => '');

$query = isset($_GET['query']) ? $_GET['query'] : "";

$moveList = splitQuery($query);

$sock = @fsockopen("localhost", 5000);
if (!$sock) {
  die("The server that processes book queries appears to be down. I am " .
      "probably aware of this and working on it already, but if you are " .
      "really really impatient, telnet to freechess.org and message fcatalin.");
}

// Send the query
fputs($sock, $query . " END\n");
list($statusCode) = fscanf($sock, "%d");
if ($statusCode == 1) {
  die("You have issued a malformed query, most likely an invalid move list.");
}

// Read the board and the parent information
list($linearBoard) = fscanf($sock, "%s");
list($proof, $disproof, $score, $numMoves) = fscanf($sock, "%d %d %lf %d");
$position = array('score' => $score, 'proof' => $proof, 'disproof' => $disproof);

$board = array();
for ($i = 0; $i < 8; $i++) {
  for ($j = 0; $j < 8; $j++) {
    $bg = (($i + $j) % 2) ? 'b' : 'w';
    $fg = $PIECE_MAP[$linearBoard[$i * 8 + $j]];
    $board[$i][$j] = $fg . $bg;
  }
}

// Now read the children and make an array
$query = str_replace(' ', '+', $query);
$moves = array();
for ($i = 0; $i < $numMoves; $i++) {
  list($child_mv, $child_proof, $child_disproof, $child_score, $size) = fscanf($sock, "%s %d %d %lf %d");
  $moves[$i]['move'] = $child_mv;
  $moves[$i]['link'] = ($query ? "{$query}+" : "") . $child_mv;
  $moves[$i]['proof'] = $child_proof;
  $moves[$i]['disproof'] = $child_disproof;
  $moves[$i]['score'] = $child_score;
  $moves[$i]['size'] = $size;
}
usort($moves, "moveCmp");

if (!$query) {
  $desc = 'creditsTop.tpl';
} else if (startsWith($query, 'e3+Nc6')) {
  $desc = 'credits_e3Nc6.tpl';
} else if (startsWith($query, 'e3+b5+Bxb5+Nh6') || startsWith($query, 'e3+b5+Bxb5+Ba6') || startsWith($query, 'e3+b5+Bxb5+e6')) {
  $desc = 'credits_e3b5_last3.tpl';
} else if (startsWith($query, 'e3+c6+Bb5+cxb5+b4+b6+Ke2+a5+bxa5+bxa5+c4+bxc4+Kd3+cxd3+a4+Na6+e4+Qc7+e5+Qxc1+Qxc1+Rb8+Qxc8+Rxb1+Rxb1+Nb4+Qxe8+g6+Qxf7+e6+Qxd7+Bd6+Qxh7+Rxh7+exd6+Rxh2+Rxh2+g5+Rxb4+axb4+g4')) {
  $desc = 'credits_e3c6.tpl';
} else {
  $desc = '';
}

fclose($sock);

$smarty = new Smarty();
$smarty->template_dir = 'web/templates';
$smarty->compile_dir = 'web/templates_c';
$smarty->assign('moveList', $moveList);
$smarty->assign('board', $board);
$smarty->assign('position', $position);
$smarty->assign('moves', $moves);
$smarty->assign('desc', $desc);
$smarty->display('book.tpl');

/***************************************************************************/

function moveCmp($a, $b) {
  if ($a['score'] != $b['score']) {
    return ($a['score'] > $b['score']) ? -1 : 1;
  }
  if ($a['proof'] != $b['proof']) {
    return $a['proof'] - $b['proof'];
  }
  if ($a['disproof'] != $b['disproof']) {
    return $b['disproof'] - $a['disproof'];
  }
  if ($a['size'] != $b['size']) {
    return $b['size'] - $a['size'];
  }
  return ($a['move'] < $b['move']) ? -1 : 1;
}

// Returns an array of (move, URL) pairs
function splitQuery($query) {
  if (!$query) {
    return array();
  }
  $result = array();
  $parts = explode(' ', trim($query));
  $acc = '';
  foreach ($parts as $part) {
    if ($acc) {
      $acc .= '+';
    }
    $acc .= $part;
    $result[] = array('move' => $part, 'url' => $acc);
  }
  return $result;
}

function startsWith($str, $prefix) {
  return substr($str, 0, strlen($prefix)) == $prefix;
}

?>
