<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Suicide chess book browser</title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <meta name="robots" content="nofollow"/>
  </head>

<body>

<?php

$PIECE_MAP = array('P' => 'pw', 'N' => 'nw', 'B' => 'bw', 'R' => 'rw', 'Q' => 'qw', 'K' => 'kw',
                   'p' => 'pb', 'n' => 'nb', 'b' => 'bb', 'r' => 'rb', 'q' => 'qb', 'k' => 'kb', '-' => '');

function pretty_print_query($query) {
  $halfmove = 0;
  $reassembled = "";
  $pp_query = ""; // pretty-printed
  while ($query != "") {
    $query = trim($query);
    if (sscanf($query, "%s", $token) == 1) {
      $query = trim(substr($query, strlen($token)));
      if ($halfmove % 2 == 0)
	$pp_query = $pp_query . "<b>" . ($halfmove / 2 + 1) . ".</b> ";
      $halfmove++;
      $reassembled = $reassembled . ($reassembled ? " " : "") . $token;
      // Do not add a link to the latest move -- that is where we are now.
      if ($query) {
	$pp_query = $pp_query . "<a href=\"?query=" .
	  $reassembled . "\">" . $token . "</a> ";
      } else {
	$pp_query = $pp_query . $token;
      }
    }
  }
  return $halfmove ? $pp_query : "Starting position";
}

function interpret_score($score) {
  if ($score == 0.0) return "LOST";
  if ($score == -1.0) return "WON";
  return sprintf("%.4lf", $score);
}

function interpret_pn($pn) {
  if ($pn == 1000000000) return "&infin;";
  return $pn;
}

/************************************ MAIN ***********************************/

$query = isset($_GET['query']) ? $_GET['query'] : "";
if ($query) {
  print "[<a href=\"?query=\">Top</a>] ";
}
print pretty_print_query($query) . "<p/>";

$sock = fsockopen("localhost", 5000);

if (!$sock) {
  print "The server that processes book queries appears to be down. I am " .
    "probably aware of this and working on it already, but if you are " .
    "really really impatient, telnet to freechess.org and message fcatalin.";

  print_footer();
}

// Send the query
fputs($sock, $query . " END\n");
list($status_code) = fscanf($sock, "%d");

if ($status_code == 1) {
  print "You have issued a malformed query, most likely an invalid move list.";
  print_footer();
}

// Read the board and the parent information
list($board) = fscanf($sock, "%s");
list($proof, $disproof, $score, $num_children) = fscanf($sock, "%d %d %lf %d");

print "<table cellspacing=\"0\">";
print "<tr><td style=\"padding: 0px; border: 1px solid gray; white-space: no-wrap\">";
for ($i = 0; $i < 8; $i++) {
  for ($j = 0; $j < 8; $j++) {
    $bg = (($i + $j) % 2) ? 'b' : 'w';
    $fg = $PIECE_MAP[$board[$i * 8 + $j]];
    if ($fg == '-') $fg = "";
    print "<img src=\"images/{$fg}{$bg}.png\" alt=\"{$fg}{$bg}\"/>";
  }
  print "<br/>\n";
}
print "</td><td valign=\"middle\" align=\"center\" width=\"200\" nowrap=\"nowrap\" style=\"border-width:0;\">";

print "<b>Score</b>: " . interpret_score($score) . "<br/>";
print "<b>Proof</b>: " . interpret_pn($proof) . "<br/>";
print "<b>Disproof</b>: " . interpret_pn($disproof) . "<br/>";
print "</td></tr></table>";

if ($num_children == 0) {
  print "<p/>No further information in this branch.";
  print_footer();
}

// Now read the children and make an array;
$moves = array();
$proofs = array();
$disproofs = array();
$scores = array();

for ($i = 0; $i < $num_children; $i++) {
  list($child_mv, $child_proof, $child_disproof, $child_score, $size) =
    fscanf($sock, "%s %d %d %lf %d");
  $moves[$i] = $child_mv;
  $proofs[$i] = $child_proof;
  $disproofs[$i] = $child_disproof;
  $scores[$i] = $child_score;
  $sizes[$i] = $size;
}

array_multisort($scores, SORT_DESC, $proofs, $disproofs, $sizes, SORT_DESC,
		$moves);

$color = array("#efefff", "#dfdfef");
print "<p/><table border=\"1\" cellspacing=\"0\" cellpadding=\"1\">";
print "<tr bgcolor=\"" . $color[1] . "\">";
print "<th width=\"100\">Move</th>";
print "<th width=\"100\">Score</th>";
print "<th width=\"100\">Proof</th>";
print "<th width=\"100\">Disproof</th>";
print "<th width=\"100\">Size</th></tr>";
for ($i = 0; $i < $num_children; $i++) {
  print "<tr bgcolor=\"" . $color[$i % 2] . "\">";
  print "<td align=\"center\"><a href=\"?query=" .
    str_replace(" ", "+", $query) . ($query ? "+" : "") . $moves[$i] . "\">" .
    $moves[$i] . "</a></td>";
  print "<td>" . interpret_score($scores[$i]) . "</td>";
  print "<td>" . interpret_pn($proofs[$i]) . "</td>";
  print "<td>" . interpret_pn($disproofs[$i]) . "</td>";
  print "<td>" . $sizes[$i] . "</td>";
  print "</tr>";
}
print "</table>";

if ($query == "") {
  include("book_credits.html");
}

print_footer();

fclose($sock);

// FOOTER
function print_footer() {
  print("</body></html>");
  exit();
}
?>
