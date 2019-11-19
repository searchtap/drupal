<?php
error_reporting(0);
function demo_drush_drush_command() {
    $items  = array();
    $items['sarchtap_trigger'] = array(
      //  'callback'    => 'demo_drush_create_node',
        'description' => 'This Alias Use By Searchtap for full/parser indexing.',
        'aliases' => array('searchtap'),
        'examples' => array(      // List these example when user types : drush help searchtap         
            'For Partial(add/update/delete) Indexing' => 'drush searchtap partial',
            'For Delete Full Sync'=>'drush searchtap deletefullsync',
            'For Full Indexeing' => 'drush searchtap',

        ),
    );
    return $items;
}

function drush_demo_drush_sarchtap_trigger() {
    $args = func_get_args();
    demo_drush_print_statement($args[0]);
}
