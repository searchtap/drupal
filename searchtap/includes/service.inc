<?php
/*
 * System settings form for searchtap configuration
 */

//error_reporting(0);
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

function searchtap_reindex($count, $pid = null, $action = null)
{
    if (!empty($pid)) {
        $tags = implode(',', $pid);
        $nid = array($tags);
    }
    //Run the script as admin
    global $user;
    $original_user = $user;
    $old_state = drupal_save_session();
    drupal_save_session(FALSE);
    $user = user_load(1);
    watchdog('searchtap', 'searchtap cron start');
    $selectedView = variable_get('searchtap_view_' . $count);
    $selectedLabels = array_filter(variable_get('searchtap_fields_' . $count));
    $viewArray = array();
    $view = views_get_view($selectedView);
    foreach ($view->display['default']->display_options['fields'] as $field) {
        if (isset($field['label']))
            if (in_array($field['label'], $selectedLabels)) {
                $selectedFields[] = $field['id'];
                $associatedField[$field['id']] = $field['label'];
            }
    }
    $itemSteps = 1000;
    $product_count = 0;
    $loop = true;
    $i = 0;
    while ($loop) {

        if ($action == "partial" || $action == "add" || $action == "update") {
            $view = views_get_view($selectedView);
            $view->set_items_per_page($itemSteps);
            $view->set_offset($i);
            $view->set_display('page_1');
            $view->set_arguments($nid);
            $view->execute();
            watchdog('searchtap', $view->total_rows);
            $i += $itemSteps;
            if (!count($view->result)) {
                $loop = false;
                break;
            }

            foreach ($view->result as $row_index => $row) {
                $item = array();
                foreach ($view->field as $key => $field) {
                    if (in_array($key, $selectedFields)) {
                        $field_value = htmlspecialchars_decode($view->render_field($key, $row_index));

                        if (json_decode($field_value)) {
                            $item[$associatedField[$key]] = json_decode($field_value);
                        } else {
                            if ($field_value) {
                                $item[$associatedField[$key]] = htmlspecialchars_decode($field_value, ENT_QUOTES);
                            }
                        }
                    }
                }
                $viewArray[] = $item;
            }
            $viewJson = json_encode($viewArray);
            $collectionName = variable_get('searchtap_collection_' . $count);
            $adminKey = variable_get('searchtap_admin_key_' . $count);
            $response = addObjects($viewJson, $collectionName, $adminKey);
            watchdog('searchtap', 'searchtap indexed ' . $i + $itemSteps . ' products ' . $response);
            unset($viewArray);
        } else {

            $view = views_get_view($selectedView);
            $view->set_items_per_page($itemSteps);
            $view->set_offset($i);
            $view->execute();
            watchdog('searchtap', $view->total_rows);
            $i += $itemSteps;
            if (!count($view->result)) {
                $loop = false;
                break;
            }

            foreach ($view->result as $row_index => $row) {
                $item = array();
                foreach ($view->field as $key => $field) {
                    if (in_array($key, $selectedFields)) {
                        $field_value = htmlspecialchars_decode($view->render_field($key, $row_index));

                        if (json_decode($field_value)) {
                            $item[$associatedField[$key]] = json_decode($field_value);
                        } else {
                            if ($field_value) {
                                $item[$associatedField[$key]] = htmlspecialchars_decode($field_value, ENT_QUOTES);
                            }
                        }

                    }
                }
                $viewArray[] = $item;
            }
            $viewJson = json_encode($viewArray);
            $collectionName = variable_get('searchtap_collection_' . $count);
            $adminKey = variable_get('searchtap_admin_key_' . $count);
            $response = addObjects($viewJson, $collectionName, $adminKey);
            watchdog('searchtap', 'searchtap indexed ' . $i + $itemSteps . ' products ' . $response);
            unset($viewArray);
        }

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
        removeObject($itemIds, $collectionName, $adminKey);
        unset($itemIds);
    }
    echo "Product Added to Searchtap !!"."\n";
    checkQueueForDelete($pid);


    watchdog('searchtap', 'searchtap cron end');
    $user = $original_user;
    drupal_save_session($old_state);
}

// function used in partial indexing for delete

function partialDelete($productIds, $count)
{
    $itemIds = json_encode($productIds);
    $collectionName = variable_get('searchtap_collection_' . $count);
    $adminKey = variable_get('searchtap_admin_key_' . $count);
    $response = removeObject($itemIds, $collectionName, $adminKey);
    if ($response == '200' || $response == '201') {
        echo "Product Deleted !"."\n";
        watchdog('searchtap', 'product Ids' . $itemIds . ' deleted from Searchtap Server!. ');

        //check if there is any product ID in queue that need to be deleted
        checkQueueForDelete($productIds);
    } else {
        echo "Nothing to delete !!";
        watchdog('searchtap', $response . ' Response Code !');

    }
}

function deleteFullsync()
{
    $countval = 1;
    $count = 1000;
    $skip = 0;
    $counter = 0;
    $productIds = array();

    // get product Id's from floweraura server
    $dbIds = getStoreProductIds();

    // get Product Id's from Searchtap server
    $curlReq = searchtapCurl($count, $skip);
    $json = json_decode($curlReq, true);

    if ($json["totalHits"] > 0) {
        while ($skip <= $json['totalHits']) {
            $jsondata = searchtapCurl($count, $skip);
            $productJson = json_decode($jsondata, true);
            for ($i = 0; $i < count($productJson['results']); $i++) {
                if (!in_array($productJson['results'][$i]['id'], $dbIds)) {
                    $missingIds[] = $productJson['results'][$i]['id'];
                }
            }
            $skip += $count;
        }
    } else echo "Collection is empty!!";

    if ($missingIds && count($missingIds) > 0) {
        $itemIds = json_encode($missingIds);

        $collectionName = variable_get('searchtap_collection_' . $countval);
        $adminKey = variable_get('searchtap_admin_key_' . $countval);
        $response = removeObject($itemIds, $collectionName, $adminKey);
        if ($response == '200' || $response == '201') {
            echo " Product Ids'.$itemIds.' Deleted from Searchtap Server !!" . "\n";
            watchdog('searchtap', $itemIds . 'Deleted from Searchtap Server !!');

            //check if there is any product ID in queue that need to be deleted
            checkQueueForDelete($missingIds);
        } else {
            echo "Something Wrong With Searchtap API's Response!";
            watchdog('searchtap', $response . ' Response Code !');

        }
    } else {
        echo "No Match Found !!";
    }

}


function getStoreProductIds()
{
    $allId = getCurlRequstID();
    $json11 = json_decode(preg_replace('/[\x00-\x1F\x80-\xFF]/', '', $allId), true);
    if ($json11) {
        for ($i = 0; $i < count($json11['nodes']); $i++) {
            $ids[] = $json11['nodes'][$i]['node']['nid'];
        }
    }
    return $ids;
}


function getTableValue($action, $count)
{
    $collectionID = variable_get('searchtap_collection_' . $count);
    $adminkey = variable_get('searchtap_admin_key_' . $count);
    $appID = variable_get('searchtap_application_key_' . $count);
    if ($action == 'add' || $action == 'update') {
        $actiondata = implode("", $action);
        if (db_table_exists('searchtap_queue')) {
            $productIds = db_select('searchtap_queue', 'mm')
                ->fields('mm', ['entity_id'])
                ->condition('action', 'add')
                ->execute()
                ->fetchAll();
            foreach ($productIds as $id) {
                $status=node_load($id->entity_id);
                if($status->status){
                    $nodeID[] = $id->entity_id;
                }else{
                    partialDelete($id->entity_id, $count);
                }
            }

            if ($nodeID) {
                searchtap_reindex($count, $nodeID, $action);
            } else {
                echo "No Data to Add/Update !!"."\n";
            }

        }

    } else if ($action == 'delete') {
        $actiondata = implode("", $action);
        if (db_table_exists('searchtap_queue')) {
            $productIds = db_select('searchtap_queue', 'mm')
                ->fields('mm', ['entity_id'])
                ->condition('action', 'delete')
                ->execute()
                ->fetchAll();
            foreach ($productIds as $id) {
                $nodeID[] = $id->entity_id;
            }
            if (!empty($nodeID)) {
                echo "Product". $nodeID." for Delete !" ;
                partialDelete($nodeID, $count);
            } else {
                echo "No data to delete !!";
            }
        }
    } else {
        if (db_table_exists('searchtap_queue')) {
            $productIds = db_select('searchtap_queue', 'mm')
                ->fields('mm', ['entity_id'])
                ->condition('action', 'add')
                ->execute()
                ->fetchAll();
            foreach ($productIds as $id) {
                $status=node_load($id->entity_id);
                if($status->status){
                    $nodeID[] = $id->entity_id;
                }else{
                    $nodeToDelete[]=$id->entity_id;
                }
            }
            if (!empty($nodeID)) {
                searchtap_reindex($count, $nodeID, $action);
            } else {
                echo "No Data to Add/Update !!"."\n";
            }
            unset($nodeID);

            // Check Queue For Delete
            $nodeIdsForDelete=array();
            $productIds = db_select('searchtap_queue', 'mm')
                ->fields('mm', ['entity_id'])
                ->condition('action', 'delete')
                ->execute()
                ->fetchAll();


            foreach ($productIds as $id) {
                $nodeIdsForDelete[] = $id->entity_id;
            }
            $nodeIdsForDelete=array_merge($nodeToDelete,$nodeIdsForDelete);
            if(!empty($nodeIdsForDelete)){
                partialDelete($nodeIdsForDelete, $count);
               // echo "Deleted Product Id ".$nodeID;
            }
            else{
                echo "Nothing To delete !!"."\n";
            }
            unset($nodeIdsForDelete);
            unset($nodeToDelete);


        }

    }
}

// function to check queue that need to be deleted

function checkQueueForDelete($produstID)
{
    foreach ($produstID as $pid) {
        $num_deleted = db_delete('searchtap_queue')
            ->condition('entity_id', $pid)
            ->execute();
    }
}

/*
function getJsonData($id)
{
    $curl = curl_init();
    curl_setopt_array($curl, array(
        CURLOPT_URL => "http://marketing.floweraura.com/jsonproduct/" . $id,
        CURLOPT_SSL_VERIFYPEER => false,
        CURLOPT_SSL_VERIFYHOST => 2,
        CURLOPT_CAINFO => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_ENCODING => "",
        CURLOPT_MAXREDIRS => 10,
        CURLOPT_TIMEOUT => 30,
        CURLOPT_HTTP_VERSION => CURL_HTTP_VERSION_1_1,
        CURLOPT_CUSTOMREQUEST => "GET",
    ));
    $result = curl_exec($curl);
    $result1 = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    $err = curl_error($curl);
    curl_close($curl);
    return $result;
}*/


function getCurlRequstID()
{
    $curl = curl_init();
    $filterbyNid = variable_get('filterby_nid_url_1'); // "http://marketing.floweraura.com/jsonproducts"
    curl_setopt_array($curl, array(
        CURLOPT_URL => $filterbyNid,
        CURLOPT_SSL_VERIFYPEER => false,
        CURLOPT_SSL_VERIFYHOST => 2,
        CURLOPT_CAINFO => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_ENCODING => "",
        CURLOPT_MAXREDIRS => 10,
        CURLOPT_TIMEOUT => 30,
        CURLOPT_HTTP_VERSION => CURL_HTTP_VERSION_1_1,
        CURLOPT_CUSTOMREQUEST => "GET",
    ));
    $result = curl_exec($curl);
    $result1 = curl_getinfo($curl, CURLINFO_HTTP_CODE);
    $err = curl_error($curl);
    curl_close($curl);
    return $result;
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
    $err = curl_error($curl);
    curl_close($curl);
    return $result1;
}

function removeObject($productIds, $collectionName, $adminKey)
{
    $curl = curl_init();
    if ($productIds) {
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
            CURLOPT_POSTFIELDS => $productIds,
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

function searchtapCurl($count, $skip)
{
    $countlocal = $count;
    $count = 1;
    $collectionID = variable_get('searchtap_collection_' . $count);
    $adminkey = variable_get('searchtap_admin_key_' . $count);
    $appID = variable_get('searchtap_application_key_' . $count);
    $count = $countlocal;
    $data = json_encode(array(
        'collection' => $collectionID,
        'fields' => ["id"],
        'count' => $count,
        'skip' => $skip
    ));
    try {
        $curl = curl_init();
        curl_setopt_array($curl, array(
            CURLOPT_URL => "https://" . $appID . "-fast.searchtap.net/v2",
            CURLOPT_SSL_VERIFYPEER => false,
            CURLOPT_SSL_VERIFYHOST => 2,
            CURLOPT_CAINFO => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_ENCODING => "",
            CURLOPT_MAXREDIRS => 10,
            CURLOPT_TIMEOUT => 30,
            CURLOPT_HTTP_VERSION => CURL_HTTP_VERSION_1_1,
            CURLOPT_CUSTOMREQUEST => "POST",
            CURLOPT_POSTFIELDS => $data,
            CURLOPT_HTTPHEADER => array(
                "cache-control: no-cache",
                "content-type: application/json",
                "Authorization:  Bearer " . $adminkey
            ),
        ));
        $result = curl_exec($curl);
        $responseHttpCode = curl_getinfo($curl, CURLINFO_HTTP_CODE);
        $err = curl_error($curl);
        curl_close($curl);
        if ($responseHttpCode != 200) {
            exit;
        }
        //      return (json_decode($result)->results);
        return $result;
    } catch (error $error) {
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
    $form['searchtap_application_key_' . $count] = array(
        '#type' => 'textfield',
        '#title' => t('Enter application key'),
        '#description' => t('Enter the application key provided by searchtap.'),
        '#maxlength' => 256,
        '#required' => TRUE,
        '#default_value' => variable_get('searchtap_application_key_' . $count)
    );

    $form['filterby_nid_url_' . $count] = array(
        '#type' => 'textfield',
        '#title' => t('Enter filter_by_nid URL(API)'),
        '#description' => t('Enter the URL created by view to get node_id.'),
        '#maxlength' => 256,
        '#required' => TRUE,
        '#default_value' => variable_get('filterby_nid_url_' . $count)
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
        '#required' => FALSE,
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
    $applicationkey = $form_state['input']['searchtap_application_key_' . $count];;
    $filterbynid_URL = $form_state['input']['filterby_nid_url_' . $count];;
    $view = $form_state['input']['searchtap_view_' . $count];
    $fields = $form_state['input']['searchtap_fields_' . $count];
    variable_set('searchtap_collection_' . $count, $collection);
    variable_set('searchtap_admin_key_' . $count, $adminKey);
    variable_set('searchtap_application_key_' . $count, $applicationkey);
    variable_set('filterby_nid_url_' . $count, $filterbynid_URL);
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
    variable_del('searchtap_application_key_' . $count);
    variable_del('filterby_nid_url_' . $count);
    variable_del('searchtap_view_' . $count);
    variable_del('searchtap_fields_' . $count);
    drupal_set_message(t('Deleted successfully'));
}

function searchtap_cron_reindex()
{
    $count = variable_get('searchtap_count');
    $selectedViewName = variable_get('searchtap_view_' . $count);
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
