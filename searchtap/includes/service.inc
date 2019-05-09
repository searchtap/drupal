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

    foreach ($view->display['default']->display_options['fields'] as $field) {
        if (isset($field['label']))
            if (in_array($field['label'], $selectedLabels)) {
                $selectedFields[] = $field['id'];
                $associatedField[$field['id']] = $field['label'];
            }
    }
    $itemSteps = 1;
    $product_count = 0;
    $loop = true;
    $i = 0;
    while ($loop) {
        $view = views_get_view($selectedViewName);
        $view->set_items_per_page($itemSteps);
        $view->set_offset($i);
        // $view->set_arguments(array(11957));
        $view->execute();
        //  var_dump($view->result);

        watchdog('searchtap', $view->total_rows);
        $i += $itemSteps;
        if (!count($view->result)) {
            $loop = false;
            break;
        }

        foreach ($view->result as $row_index => $row) {
            $item = array();

            foreach ($view->field as $key => $field) {
                //    var_dump($view->field);

                if (in_array($key, $selectedFields)) {
                    $field_value = htmlspecialchars_decode($view->render_field($key, $row_index));


                    if ($key == "field_occasion_new" || $key == "field_available_cities" || $key == "field_bouquet" || $key == "field_gift_type") {
                        $value1 = array_unique(explode(",", $field_value));
                        $item[$associatedField[$key]] = array_map('trim', $value1);

                    } else if ($key == "list_price" || $key == "sell_price") {
                        if (is_string($field_value)) {
                            $item[$associatedField[$key]] = (int)str_replace(',', '', $field_value);;
                        } else {
                            $item[$associatedField[$key]] = (int)$field_value;
                        }

                    } elseif ($key == "uc_product_image") {
                        $doc = new DOMDocument();
                        $doc->loadHTML($field_value);
                        $tags = $doc->getElementsByTagName('img');
                        $imgArr = array();
                        foreach ($tags as $tag) {
                            $imgSrc[] = $tag->getAttribute('data-src');
                        }
                        $item[$associatedField[$key]] = $imgSrc;
                        unset($imgSrc);
                    } else if ($key == "php") {
                        echo "Value of Key: " . $key . "  Field Value: " . $field_value . "\n";
                    } else {
                        if (json_decode($field_value)) {
                            $item[$associatedField[$key]] = json_decode($field_value);

                        } else {

                            echo "Value of Key: " . $key . "  Field Value: " . $field_value . "\n";
                            $item[$associatedField[$key]] = $field_value;
                        }
                    }

                }
            }
            $viewArray[] = $item;
        }
        //  print_r($viewArray);
        $viewJson = json_encode($viewArray);
        //   print_r($viewJson);
        $collectionName = variable_get('searchtap_collection_' . $count);

        $adminKey = variable_get('searchtap_admin_key_' . $count);
        //  $response = addObjects($viewJson, $collectionName, $adminKey);
        // watchdog('searchtap', 'searchtap indexed ' . $i + $itemSteps . ' products ' . $response);
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
        removeObject($itemIds, $collectionName, $adminKey);
        unset($itemIds);
    }
    watchdog('searchtap', 'searchtap cron end');
    $user = $original_user;
    drupal_save_session($old_state);
}

function reindexselectedProduct($productIds, $count)
{
    try {
        $nid = array('242,75,246,247,248');
        $view = views_get_view("jsonproduct");
        $view->set_items_per_page(5);
        $view->set_offset(0);
        $view->set_display('page_1');
        $view->set_arguments($nid);
        $view->execute();

        foreach ($view->result as $row_index => $row) {
            var_dump($row->nid);
        }
    }catch(error $e) {
        var_dump($e);
    }


//    $getJson = array();
//    $collectionName = variable_get('searchtap_collection_' . $count);
//    $adminKey = variable_get('searchtap_admin_key_' . $count);
//    $id = implode(",", $productIds);
//    echo "Value Of Id: " . $id;
//    $getJson = getJsonData($id);
//    $json = json_decode(preg_replace('/[\x00-\x1F\x80-\xFF]/', '', $getJson), true);
//    $formatedValue = formatJson($json);
//    $response = addObjects($formatedValue, $collectionName, $adminKey);
//    if ($response == '200' || $response == '201') {
//        echo "Value Indexed !!";
//        watchdog('Searchtap indexed selected' . ' products ');
//        deleteTableValue($productIds);
//    } else {
//        echo "Error";
//        watchdog($response . ' Response Code !');
//    }

}

function deleteFullSync($productIds, $count)
{
    $itemIds = json_encode($productIds);
    print_r("Value Is: " . $itemIds);
    $collectionName = variable_get('searchtap_collection_' . $count);
    $adminKey = variable_get('searchtap_admin_key_' . $count);
//    $response = removeObject($itemIds, $collectionName, $adminKey);
//    if($response=='200')
//    {
//        echo "Delete";
//        watchdog('product Ids'.$itemIds.' deleted from Searchtap Server!. ');
//        deleteTableValue($productIds);
//    }
//    else {
//         echo "error";
//        watchdog($response.' Response Code !');
//
//    }

}

function deleteFailsafe()
{
    $countval = 1;
    $count = 1000;
    $skip = 0;
    $counter = 0;
    $productIds = array();

    $dbIds = getStoreProductIds();

    $curlReq = searchtapCurl($count, $skip);
    $json = json_decode($curlReq, true);

    if ($json["totalHits"] > 0) {

        while ($skip <= $json['totalHits']) {
            echo "Skip Value : " . $skip;
            $jsondata = searchtapCurl($count, $skip);
            $json1 = json_decode($jsondata, true);

            for ($i = 0; $i < count($json1['results']); $i++) {
                if (in_array($json1['results'][$i]['id'], $dbIds)) {
                    $missingId[] = $json1['results'][$i]['id'];
                }

            }
            $skip += $count;
        }
    } else echo "Collection is empty!!";
    if (count($missingId) > 0) {

        $itemIds = json_encode($productIds);
        print_r("Value Is: " . $itemIds);
        $collectionName = variable_get('searchtap_collection_' . $countval);
        $adminKey = variable_get('searchtap_admin_key_' . $countval);
//            $response = removeObject($itemIds, $collectionName, $adminKey);
//            if($response=='200')
//            {
//             echo "Delete";
//           watchdog('product Ids'.$itemIds.' deleted from Searchtap Server!. ');
//            deleteTableValue($productIds);
//            }
//           else {
//              echo "error";
//                watchdog($response.' Response Code !');
//
//              }
    }


//    foreach (searchtapCurl($count, $skip) as $object) {
//
//        $productIds[] = $object->id;
//
//        $counter++;
//        if ($counter === $count) {
//            $idsToBeDeleted = array_values(array_diff($productIds, $dbIds));
//            if (count($idsToBeDeleted) > 0){
//                 print_r($idsToBeDeleted);
//                }
//            $counter = 0;
//            unset($productIds);
//        }
//        $skip += $count;
//    }
//        print_r($productIds);
//    if (count($productIds) > 0) {
//        $idsToBeDeleted = array_values(array_diff($productIds, $dbIds));
//        if (count($idsToBeDeleted) > 0){
//            $itemIds=json_encode($productIds);
//            print_r("Value Is: ".$itemIds);
//            $collectionName = variable_get('searchtap_collection_' . $countval);
//            $adminKey = variable_get('searchtap_admin_key_' . $countval);
//            $response = removeObject($itemIds, $collectionName, $adminKey);
//            if($response=='200')
//            {
//             echo "Delete";
//           watchdog('product Ids'.$itemIds.' deleted from Searchtap Server!. ');
//            deleteTableValue($productIds);
//            }
//           else {
//              echo "error";
//                watchdog($response.' Response Code !');
//
//              }
//
//
//
//
//           // deleteFullSync($idsToBeDeleted,1);
//            }
//    }

}

function getStoreProductIds()
{
    $allId = getCurlRequstID();
    $json11 = json_decode(preg_replace('/[\x00-\x1F\x80-\xFF]/', '', $allId), true);
    echo "Count Value is: " . count($json11['nodes']);
    if ($json11) {
        for ($i = 0; $i < count($json11['nodes']); $i++) {
            //  var_dump($obh[$i]['node']['nid']);
            $ids[] = $json11['nodes'][$i]['node']['nid'];
        }
    }
    // print_r($ids);
    return $ids;
}

function getTableValue($action, $count)
{
    if ($action == 'add' || $action == 'update') {
        $actiondata = implode("", $action);

        if (db_table_exists('searchtap_queue')) {
            $productIds = db_select('searchtap_queue', 'mm')
                ->fields('mm', ['entity_id'])
                ->condition('action', 'add')
                ->execute()
                ->fetchAll();

            foreach ($productIds as $id) {
                $nodeID[] = $id->entity_id;
            }
            print_r($nodeID);
            reindexselectedProduct($nodeID, $count);
        }
    } else {
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
            deleteFailsafe();
        }

    }
}

function formatJson($json)
{

    foreach ($json as $data) {
        $viewArray = array();

        for ($i = 0; $i < count($data); $i++) {

            $rewriteKeys = array('nid' => 'id', 'title' => 'titleData', 'description' => 'description', 'sell_price' => 'sell_price', 'Image' => 'ImageData', 'path' => 'URL', 'constituents' => 'constituents', 'delivery_through' => 'delivery_through', 'event' => 'event', 'occasion' => 'occasion', 'relationship' => 'relationshipData', 'level' => 'level', 'sku' => 'sku', 'threshold' => 'threshold', 'Stock_Active' => 'Stock_Active', 'Delivery_Type' => 'Delivery_Type', 'Flower_Type' => 'Flower_Type', 'Gift_Category_Type' => 'Gift_Category_Type', 'Product_Category' => 'Product_Category', 'Product_Collection' => 'Product_Collection', 'Product_Type' => 'Product_Type', 'bouquet_type' => 'bouquet_type', 'cake_Type' => 'cakeTypeData', 'gift_type' => 'gift_type', 'plant_type' => 'plantTypeData', 'tag' => 'tagData', 'type' => 'type');
            $newArr = array();

            foreach ($data[$i]["node"] as $key => $value) {

                if ($key == "Image") {
                    foreach ($value as $val) {
                        $imageVal[] = $val['src'];
                    }
                    $value1 = array_unique($imageVal);
                    $newArr[$rewriteKeys[$key]] = $value1;
                    unset($imageVal);

                } elseif ($key == "constituents") {
                    $newArr[$rewriteKeys[$key]] = strip_tags($value);

                } elseif ($key == "occasion") {
                    $occasiondata = explode(',', $value);

                    $newArr[$rewriteKeys[$key]] = array_map('trim', $occasiondata);
                } elseif ($key == "relationship") {
                    $relationData = explode(',', $value);

                    $newArr[$rewriteKeys[$key]] = $relationData;
                } elseif ($key == "bouquet_type") {
                    $bouData = explode(',', $value);

                    $newArr[$rewriteKeys[$key]] = $bouData;
                } else {
                    $newArr[$rewriteKeys[$key]] = $value;
                }
            }

            $viewArray[] = $newArr;
        }

        $viewJson = json_encode($viewArray);

        return $viewJson;
    }


}

function deleteTableValue($produstID)
{

    foreach ($produstID as $pid) {
        $num_deleted = db_delete('searchtap_queue')
            ->condition('entity_id', $pid)
            ->execute();
    }

}

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
}

function getCurlRequstID()
{
    $curl = curl_init();
    curl_setopt_array($curl, array(
        CURLOPT_URL => "http://marketing.floweraura.com/jsonproducts",
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

    var_dump($result1);
    $err = curl_error($curl);
    var_dump($err);
    curl_close($curl);
    return $result1;
}

function removeObject($productIds, $collectionName, $adminKey)
{
    $curl = curl_init();
    // $data_json = json_encode($productIds);
    //  print_r($productIds);

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

    $collectionName = "QS2NFJEFHM67TSIACC3I6HKA";
    $adminKey = "A6FNJP54E9JGCMQUKVKP27JT";
    $applicationId = "Z9VRZQPHURJ2SZDJWUB4KH6G";

    $data = json_encode(array(
        'collection' => $collectionName,
        'fields' => ["id"],
        'count' => $count,
        'skip' => $skip
    ));


    try {
        $curl = curl_init();
        curl_setopt_array($curl, array(
            CURLOPT_URL => "https://" . $applicationId . "-fast.searchtap.net/v2",
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
                "Authorization:  Bearer " . $adminKey
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
