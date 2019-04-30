<?php
/*
 * System settings form for searchtap configuration
 */
error_reporting(0);
use Drupal\commerce_checkout\Event\CheckoutCompleteEvent;
$count = 0;
function getViews()
{
    $viewNames = array();
    $views = views_get_all_views();
    foreach ($views as $view) {
        $viewNames[] = $view->name;
    }
    return $viewNames;
}
function getViewFields($viewName)
{
    $viewFields = array();
    $empty = array();
    if ($viewName) {
        $view = views_get_view($viewName);
        foreach ($view->display['default']->display_options['fields'] as $field) {
            if (isset($field['label']))
                $viewFields[] = $field['label'];
        }
        return $viewFields;
    }
    return $empty;
}
function searchtap_config_form($form, &$form_state)
{
    $header = array(
        'S.No.',
        'Collection Name',
        'View Name',
        'Reindex',
        'Edit',
        'Delete'
    );
    $rows = array();
    $instanceCount = variable_get('searchtap_count');
    for ($i = 1; $i <= $instanceCount; $i++) {
        if (variable_get('searchtap_collection_' . $i))
            $rows[] = array(
                $i,
                variable_get('searchtap_collection_' . $i),
                variable_get('searchtap_view_' . $i),
                l('Reindex', 'admin/config/development/searchtap/' . $i . '/reindex'),
                l('Edit', 'admin/config/development/searchtap/' . $i . '/edit'),
                l('Delete', 'admin/config/development/searchtap/' . $i . '/delete')
            );
    }
    return array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
        '#attributes' => array('class' => array('searchtap-overview')),
        '#empty' => t('There is no searchtap instance created yet.'),
    );
}
function searchtap_reindex($count)
{
    //Run the script as admin
    global $user;
    $original_user = $user;
    $old_state = drupal_save_session();
    drupal_save_session(FALSE);
    $user = user_load(1);
    watchdog('searchtap', 'searchtap cron start');
    $selectedViewName = variable_get('searchtap_view_' . $count);
    $selectedLabels = array_filter(variable_get('searchtap_fields_' . $count));
    $viewArray = array();
    $view = views_get_view($selectedViewName);
    $view->set_arguments(array(11957));;
    foreach ($view->display['default']->display_options['fields'] as $field) {
        if (isset($field['label']))
            if (in_array($field['label'], $selectedLabels)) {
                $selectedFields[] = $field['id'];
                $associatedField[$field['id']] = $field['label'];
            }
    }
    $itemSteps = 500;
    $product_count = 0;
    $loop = true;
    $i = 0;
    while ($loop) {
        $view = views_get_view($selectedViewName);
        $view->set_items_per_page($itemSteps);
        $view->set_offset($i);
        // $view->set_arguments(array(11957));
        $view->execute();
        //   var_dump($view->result);
        watchdog('searchtap', $view->total_rows);
        $i += $itemSteps;
        if (!count($view->result)) {
            $loop = false;
            break;
        }

        foreach ($view->result as $row_index => $row) {
            $item = array();

            foreach ($view->field as $key=>$field1) {
                //   var_dump($view->field);

                if (in_array($key, $selectedFields)) {
                    $field_value = htmlspecialchars_decode($view->render_field($key, $row_index));

                    if (json_decode($field_value)) {
                        $item[$associatedField[$key]] = json_decode($field_value);
                        //  echo "\nKey Value: ".$key."           Field Value:  ".$field_value."\n";
                    }

                    else{
                        $item[$associatedField[$key]] = $field_value;
                    }

                }
            }
            $viewArray[] = $item;
        }
        print_r($viewArray);
        $viewJson = json_encode($viewArray);
        //  print_r($viewJson);
        $collectionName = variable_get('searchtap_collection_' . $count);

        $adminKey = variable_get('searchtap_admin_key_' . $count);

        //   $response = addObjects($viewJson, $collectionName, $adminKey);
        //   watchdog('searchtap', 'searchtap indexed ' . $i + $itemSteps . ' products ' . $response);
        unset($viewArray);
    }
    //Remove disabled products
    $selectedViewDisabledName = variable_get('searchtap_view_disabled' . $count);
    $loop = true;
    $j = 0;
    while ($loop) {
        $view2 = views_get_view($selectedViewDisabledName);
        $view2->set_items_per_page($itemSteps);
        $view2->set_offset($j);
        $view2->execute();
        $j += $itemSteps;
        $itemIds = array();
        if (!count($view2->result)) {
            $loop = false;
            break;
        }
        foreach ($view2->result as $row_index => $row) {
            foreach ($view2->field as $key => $field) {
                $field_value = $view2->render_field($key, $row_index);
                $itemIds[] = $field_value;
            }
        }
        $collectionName = variable_get('searchtap_collection_' . $count);
        $adminKey = variable_get('searchtap_admin_key_' . $count);
        // removeObject($itemIds, $collectionName, $adminKey);
        unset($itemIds);
    }
    watchdog('searchtap', 'searchtap cron end');
    $user = $original_user;
    drupal_save_session($old_state);
}
function addObjects($product_json, $collectionName, $adminKey)
{
    $curl = curl_init();
    curl_setopt_array($curl, array(
        CURLOPT_URL => "https://manage.searchtap.net/v2/collections/" . $collectionName . "/records",
        CURLOPT_SSL_VERIFYPEER => false,
        CURLOPT_SSL_VERIFYHOST => 2,
        CURLOPT_CAINFO => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_ENCODING => "",
        CURLOPT_MAXREDIRS => 10,
        CURLOPT_TIMEOUT => 30,
        CURLOPT_HTTP_VERSION => CURL_HTTP_VERSION_1_1,
        CURLOPT_CUSTOMREQUEST => "POST",
        CURLOPT_POSTFIELDS => $product_json,
        CURLOPT_HTTPHEADER => array(
            "cache-control: no-cache",
            "content-type: application/json",
            "Authorization: Bearer " . $adminKey
        ),
    ));
    $result = curl_exec($curl);
    $result1 = curl_getinfo($curl, CURLINFO_HTTP_CODE);

    var_dump($result1);
    $err = curl_error($curl);
    var_dump($err);
    curl_close($curl);
    return $result1;
}
function removeObject($productIds, $collectionName, $adminKey)
{
    $curl = curl_init();
    $data_json = json_encode($productIds);
    if ($data_json) {
        curl_setopt_array($curl, array(
            CURLOPT_URL => "https://manage.searchtap.net/v2/collections/" . $collectionName . "/records/",
            CURLOPT_SSL_VERIFYPEER => false,
            CURLOPT_SSL_VERIFYHOST => 2,
            CURLOPT_CAINFO => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_ENCODING => "",
            CURLOPT_MAXREDIRS => 10,
            CURLOPT_TIMEOUT => 30,
            CURLOPT_HTTP_VERSION => CURL_HTTP_VERSION_1_1,
            CURLOPT_CUSTOMREQUEST => "DELETE",
            CURLOPT_POSTFIELDS => $data_json,
            CURLOPT_HTTPHEADER => array(
                "cache-control: no-cache",
                "content-type: application/json",
                "Authorization: Bearer " . $adminKey
            ),
        ));
        curl_exec($curl);
        $response = curl_getinfo($curl, CURLINFO_HTTP_CODE);
        $err = curl_error($curl);
        if ($err)
            echo $err;
        curl_close($curl);
        return $response;
    }
}
function searchtap_add_instance(array $form, array &$form_state, $setCount = 0)
{
    drupal_set_title(t('Add searchtap instance'));
    global $count;
    $count = $setCount ? $setCount : (int)variable_get('searchtap_count') + 1;
    if (!$setCount)
        $viewSelect = !empty($form_state['values']['searchtap_view_' . $count]) ?
            $form_state['values']['searchtap_view_' . $count] : '';
    else
        $viewSelect = variable_get('searchtap_view_' . $count);
    if (!$setCount)
        $form['searchtap_count'] = array('#type' => 'hidden', '#value' => $count);
    $form['searchtap_collection_' . $count] = array(
        '#type' => 'textfield',
        '#title' => t('Enter collection name'),
        '#description' => t('Enter the collection name provided by searchtap.'),
        '#required' => TRUE,
        '#default_value' => variable_get('searchtap_collection_' . $count)
    );
    $form['searchtap_admin_key_' . $count] = array(
        '#type' => 'textfield',
        '#title' => t('Enter admin key'),
        '#description' => t('Enter the admin key provided by searchtap.'),
        '#maxlength' => 256,
        '#required' => TRUE,
        '#default_value' => variable_get('searchtap_admin_key_' . $count)
    );
    $form['searchtap_view_' . $count] = array(
        '#type' => 'select',
        '#title' => t('Select view'),
        '#options' => drupal_map_assoc(getViews()),
        '#default_value' => variable_get('searchtap_view_' . $count),
        '#required' => TRUE,
        '#ajax' => array(
            'callback' => 'searchtap_ajax_select_view',
            'wrapper' => 'st-checkbox'
        )
    );
    $form['searchtap_fields_' . $count] = array(
        '#type' => 'checkboxes',
        '#title' => t('Select fields'),
        '#prefix' => '<div id="st-checkbox">',
        '#suffix' => '</div>',
        '#options' => drupal_map_assoc(getViewFields($viewSelect)),
        '#default_value' => variable_get('searchtap_fields_' . $count),
    );
    $form['searchtap_view_disabled' . $count] = array(
        '#type' => 'select',
        '#title' => t('Select view (for disabled products)'),
        '#options' => drupal_map_assoc(getViews()),
        '#default_value' => variable_get('searchtap_view_disabled' . $count),
        '#required' => TRUE,
    );
    if ($setCount) {
        $form['submit'] = array(
            '#type' => 'submit',
            '#value' => 'Save',
            '#attributes' => array(
                'count' => $setCount
            )
        );
    }
    if ($setCount)
        return $form;
    else
        return (system_settings_form($form));
}
function searchtap_add_instance_submit($form, $form_state)
{
    $count = $form_state['clicked_button']['#attributes']['count'];
    $collection = $form_state['input']['searchtap_collection_' . $count];
    $adminKey = $form_state['input']['searchtap_admin_key_' . $count];
    $view = $form_state['input']['searchtap_view_' . $count];
    $fields = $form_state['input']['searchtap_fields_' . $count];
    variable_set('searchtap_collection_' . $count, $collection);
    variable_set('searchtap_admin_key_' . $count, $adminKey);
    variable_set('searchtap_view_' . $count, $view);
    variable_set('searchtap_fields_' . $count, $fields);
    drupal_set_message(t('The configuration options have been saved.'));
}
function searchtap_ajax_select_view($form, $form_state)
{
    global $count;
    return $form['searchtap_fields_' . $count];
}
function searchtap_add_instance_reindex($form, &$form_state, $count)
{
    searchtap_reindex($count);
    drupal_set_message(t('Entities indexed successfully'));
}
function searchtap_add_instance_edit($form, &$form_state, $count)
{
    return drupal_get_form('searchtap_add_instance', $count);
}
function searchtap_add_instance_delete($form, &$form_state, $count)
{
    variable_del('searchtap_collection_' . $count);
    variable_del('searchtap_admin_key_' . $count);
    variable_del('searchtap_view_' . $count);
    variable_del('searchtap_fields_' . $count);
    drupal_set_message(t('Deleted successfully'));
}

function searchtap_cron_reindex()
{
    $count = variable_get('searchtap_count');
    for ($i = 1; $i <= $count; $i++) {
        if (variable_get('searchtap_collection_' . $i))
            searchtap_reindex($i);
    }
}
function searchtap_add_template(array $form, array &$form_state)
{
    $form['searchtap_template'] = array(
        '#type' => 'textarea',
        '#title' => t('Add HTML Template'),
        '#description' => t('Enter the html template provided by Searchtap'),
        '#required' => TRUE,
        '#default_value' => variable_get('searchtap_template')
    );
    return (system_settings_form($form));
}
