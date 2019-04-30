<?php

function demo_drush_drush_command() {
    $items  = array();
    $items['sarchtap_trigger'] = array(
      //  'callback'    => 'demo_drush_create_node',
        'description' => 'This Alias Use By Searchtap for full/parser indexing.',
        'aliases' => array('searchtap'),
        'examples' => array(      // List these example when user types : drush help searchtap
            'Add Items to Searchtap Server' => 'drush searchtap add',
            'Update Items to Searchtap Server' => 'drush searchtap update',
            'Remove Items from Searchtap Server' => 'drush searchtap delete',
            'For Partial Indexing' => 'drush searchtap partial',
            'For Full Indexeing' => 'drush searchtap',
        ),
    );
    return $items;
}

//function demo_drush_create_node() {
//    $args = func_get_args();
//    demo_drush_print_statement($args[0]);
//
//}
function drush_demo_drush_sarchtap_trigger() {
    $args = func_get_args();
    demo_drush_print_statement($args[0]);

}