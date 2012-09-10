<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Suicide chess book browser</title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
    <link href="web/book.css?v=2" rel="stylesheet" type="text/css"/>

    <style type="text/css">
    </style>

  </head>

  <body>
    <div class="title">
      Suicide chess book browser by <span class="plug">nilatac</span>
    </div>

    {if count($moveList)}
      <div class="moveList">
        [<a href="?query=">Home</a>]
        {foreach from=$moveList item=move key=i}
          {if $i % 2 == 0}<span class="moveNo">{$i/2+1}.</span>{/if}
          {if $i == count($moveList) - 1}
            {$move.move}
          {else}
            <a href="?query={$move.url}">{$move.move}</a>
          {/if}
        {/foreach}
      </div>
    {/if}
  
    <div class="contents">
      <div class="boardContainer">
        <table class="board">
          {foreach from=$board item=rank}
            <tr>
              {foreach from=$rank item=square}
                <td><img src="web/images/{$square}.png" alt="{$square}"/></td>
              {/foreach}
            </tr>
          {/foreach}
        </table>
      </div>
  
      <div class="summary">
        Score: {include file="score.tpl" score=$position.score}<br/>
        Proof: {include file="proofNumber.tpl" pn=$position.proof}<br/>
        Disproof: {include file="proofNumber.tpl" pn=$position.disproof}<br/>
      </div>

      <div style="clear: both"></div>

      <div class="moveDiv">
        {if count($moves)}
          <table class="moves">
            <tr>
              <th>Move</th>
              <th>Score</th>
              <th>Proof</th>
              <th>Disproof</th>
              <th>Size</th>
            </tr>
            {foreach from=$moves item=move}
              <tr class="{cycle values="oddRow,evenRow"}">
      	  <td><a href="?query={$move.link}">{$move.move}</a></td>
      	  <td class="number">{include file="score.tpl" score=$move.score}</td>
      	  <td class="number">{include file="proofNumber.tpl" pn=$move.proof}</td>
      	  <td class="number">{include file="proofNumber.tpl" pn=$move.disproof}</td>
      	  <td class="number">{$move.size}</td>	  
      	</tr>
            {/foreach}
          </table>
        {else}
          No further information in this branch.
        {/if}
      </div>
    </div>

    {if $desc}
      <div class="description">
        {include file=$desc}
      </div>
    {/if}
  </body>
</html>
