<?php

require_once 'smarty3/Smarty.class.php';

$smarty = new Smarty();
$smarty->template_dir = 'web/templates';
$smarty->compile_dir = '/tmp/templates_c';
$smarty->display('index.tpl');

?>
