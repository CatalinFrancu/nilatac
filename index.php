<?php

require_once 'smarty-4.3.0/Smarty.class.php';

$smarty = new Smarty();
$smarty->template_dir = 'web/templates';
$smarty->compile_dir = '/tmp/templates_c';
$smarty->display('index.tpl');

?>
