{if $score == 0}
  <span class="lost">lost</span>
{elseif $score == -1}
  <span class="won">won</span>
{else}
  {$score|string_format:"%.4f"}
{/if}
