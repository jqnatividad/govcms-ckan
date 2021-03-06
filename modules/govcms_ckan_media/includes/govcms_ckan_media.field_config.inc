<?php
/**
 * @file
 * Defines the configuration widget field.
 */

/**
 * Implements hook_field_info().
 */
function govcms_ckan_media_field_info() {
  return array(
    'govcms_ckan_media_config' => array(
      'label' => t('CKAN Visualisation'),
      'description' => t('Configuration and display of a CKAN dataset.'),
      'default_widget' => 'govcms_ckan_media_config_widget',
      'default_formatter' => 'govcms_ckan_media_config_formatter',
      // Not working yet, @see https://www.drupal.org/node/680910.
      'entity_types' => array('file'),
    ),
  );
}

/**
 * Implements hook_field_instance_settings_form().
 */
function govcms_ckan_media_field_instance_settings_form($field, $instance) {
  // Placeholder for if and when we want to add additional field config.
  // Return settings form.
  return array();
}

/**
 * Implements hook_field_is_empty().
 */
function govcms_ckan_media_field_is_empty($item, $field) {
  return empty($item['config']) && (string) $item['config'] !== '0';
}

/**
 * Implements hook_field_widget_info().
 */
function govcms_ckan_media_field_widget_info() {
  return array(
    'govcms_ckan_media_config_widget' => array(
      'label' => t('Visualisation configuration'),
      'field types' => array('govcms_ckan_media_config'),
      'settings' => array(),
    ),
  );
}

/**
 * Implements hook_field_validate().
 */
function govcms_ckan_media_field_validate($entity_type, $entity, $field, $instance, $langcode, $items, &$errors) {
  // TODO: Let the plugin invoke validation.
}

/**
 * Implements hook_field_presave().
 *
 * Serialise configuration on field save.
 */
function govcms_ckan_media_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'govcms_ckan_media_config') {
    foreach ($items as $delta => $item) {
      // Only serialise if item['value'] is an array.
      if (is_array($item['config'])) {
        $items[$delta]['config'] = serialize($item['config']);
      }
    }
  }
}

/**
 * Implements hook_field_presave().
 *
 * Unserialise configuration on field load.
 */
function govcms_ckan_media_field_load($entity_type, $entities, $field, $instances, $langcode, &$items, $age) {
  if ($field['type'] == 'govcms_ckan_media_config') {
    foreach ($entities as $id => $entity) {
      foreach ($items[$id] as $delta => $item) {
        // Only unserialise if item['config'] is a string.
        if (is_string($item['config'])) {
          $items[$id][$delta]['config'] = unserialize($item['config']);
        }
      }
    }
  }
}

/**
 * Implements hook_preprocess_HOOK().
 */
function govcms_ckan_media_preprocess_html(&$variables) {
  // Add the admin stylesheet if on an admin page.
  if (path_is_admin(current_path())) {
    drupal_add_css(drupal_get_path('module', 'govcms_ckan_media') . '/css/govcms_ckan_media.admin.css');
  }
}

/**
 * Implements hook_field_widget_form().
 */
function govcms_ckan_media_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {

  // If not dealing with a config widget, exit here.
  // TODO: Also check we only dealing with a file_entity type.
  if ($instance['widget']['type'] != 'govcms_ckan_media_config_widget') {
    return $element;
  }

  // Things we use often.
  $field_name = $field['field_name'];
  $file = isset($form_state['file']) ? $form_state['file'] : (object) array();

  // Get the config values.
  $config = array();
  // As much of this is ajax loaded we check input first for current state.
  if (isset($form_state['input'][$field_name])) {
    $config = $form_state['input'][$field_name][$langcode][$delta]['config'];
  }
  elseif (isset($file->{$field_name}[$langcode][$delta])) {
    // Fallback to saved values.
    $config = $file->{$field_name}[$langcode][$delta]['config'];
  }

  // Base widget is a fieldset tree.
  $widget = $element + array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
  );

  // Ajax wrapper for including the visualisation settings.
  $vis_wrapper_id = 'visualisation-settings-' . $delta;

  // The visualisation selector. This will ajax load the configuration form for
  // the selected visualisation.
  $widget['visualisation'] = array(
    '#title' => t('Visualisation'),
    '#type' => 'select',
    '#default_value' => isset($config['visualisation']) ? $config['visualisation'] : NULL,
    '#required' => ($form_state['build_info']['form_id'] != 'field_ui_field_edit_form'),
    '#options' => _govcms_ckan_media_field_widget_visualisation_options(),
    '#description' => t('How the data from this CKAN dataset will be presented when rendered on the page.'),
    '#ajax' => array(
      'callback' => 'govcms_ckan_media_field_widget_visualisation_form',
      'wrapper' => $vis_wrapper_id,
    ),
  );

  // The wrapper element for visualisation configuration.
  $widget['visualisation_config'] = array(
    '#type' => 'container',
    '#attributes' => array(
      'id' => $vis_wrapper_id,
      'class' => array('govcms-ckan-media--settings-form'),
    ),
  );

  // If config says there is a configuration, load the form from the plugin.
  if (!empty($config['visualisation'])) {
    $config_form = govcms_ckan_get_plugin_config_form($config['visualisation'], $form, $form_state, $config['visualisation_config']);
    $widget['visualisation_config'] = array_merge($widget['visualisation_config'], $config_form);
  }

  $element['config'] = $widget;
  return $element;
}

/**
 * Ajax callback for changing visualisations.
 */
function govcms_ckan_media_field_widget_visualisation_form($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  $parents = $form_state['triggering_element']['#parents'];
  array_pop($parents);
  $el = drupal_array_get_nested_value($form, $parents);
  return $el['visualisation_config'];
}


/************
 * Helpers.
 ************/

/**
 * Find the config field on a file entity and return its config.
 *
 * @param object $file
 *   File object.
 *
 * @return array
 *   The configuration, will be empty array if not found.
 */
function _govcms_ckan_media_get_visualisation_config($file) {
  // Only deal with ckan files.
  $scheme = file_uri_scheme($file->uri);
  if ($scheme !== 'ckan') {
    return array();
  }

  // Attempt to retrieve field name from static cache.
  $config_field = &drupal_static(__FUNCTION__ . $file->type, NULL);

  // Lookup config field name.
  if (empty($config_field)) {
    $fields_info = field_info_instances('file', $file->type);
    foreach ($fields_info as $field) {
      if (empty($config_field) && $field['widget']['type'] == 'govcms_ckan_media_config_widget') {
        $config_field = $field['field_name'];
      }
    }
  }

  // Return the first configuration. A visualisation shouldn't have multiples
  // deltas for config but if for some reason it will, change here.
  return !empty($config_field) && isset($file->{$config_field}[LANGUAGE_NONE]) ? $file->{$config_field}[LANGUAGE_NONE][0]['config'] : array();
}

/**
 * Get all the available visualisations as select options.
 *
 * @return array
 *   A key/value array of vis name/human name.
 */
function _govcms_ckan_media_field_widget_visualisation_options() {
  $plugins = govcms_ckan_get_all_plugins();
  $options = array();
  foreach ($plugins as $plugin) {
    $options[$plugin['name']] = $plugin['title'];
  }
  return $options;
}


/******************************
 * Config form default elements.
 ******************************/

/**
 * A helper for visualisations to easily include required form widgets.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state.
 * @param array $config
 *   Current configuration.
 * @param array $widgets
 *   An array of widgets to include. Eg
 *   - govcms_ckan_media_visualisation_default_key_config
 *   - govcms_ckan_media_visualisation_default_axis_config
 *   - govcms_ckan_media_visualisation_default_chart_config
 *   - govcms_ckan_media_visualisation_default_grid_config
 *   This enables a visualisation to only include widgets it needs.
 *
 * @return mixed
 *   A form array with all widgets included.
 */
function govcms_ckan_media_visualisation_include_form_widgets($form, &$form_state, $config, $widgets = array()) {
  $elements = array();
  foreach ($widgets as $callback) {
    if (function_exists($callback)) {
      $elements += $callback($form, $form_state, $config);
    }
  }
  return $elements;
}

/**
 * This helper provides the common elements for selecting the keys.
 *
 * This is implemented by the visualisation and merged into the form structure
 * for that plugin.  Most visualisations will use this.
 *
 * @param array $form
 *   Form array for the parent form.
 * @param array $form_state
 *   Current form state array from the parent form.
 * @param array $config
 *   The current configuration values.
 *
 * @return array
 *   Form structure for new elements.
 */
function govcms_ckan_media_visualisation_default_key_config($form, &$form_state, $config = array(), $key_type = 'numeric') {
  $form = array();

  // We need a resource_id, however this file might not be saved yet, if it is
  // we can get it from the entity, otherwise, need to re-parse the embed code.
  if (isset($form_state['file'])) {
    $resource_id = $form_state['file']->resource_id;
  }
  else {
    $embed_code = $form_state['storage']['embed_code'];
    $handler = new MediaInternetGovCmsCkanHandler($embed_code);
    $uri = $handler->parse($embed_code);
    $resource_id = $handler->parseIdFromUri($uri);
  }

  // Make a request to obtain the fields.
  $client = govcms_ckan_client();
  $result = $client->get('action/datastore_search', array('id' => $resource_id));

  // If something went wrong with request we cannot continue.
  if ($result->valid === FALSE || !isset($result->data->fields)) {
    drupal_set_message(t('Unable to retrieve data for this package'), 'error');
    return $form;
  }

  // Get an instance of the parser.
  $parser = govcms_ckan_dataset_parser($result->data);

  // Loop over fields to build some lists for options.
  $options = array();
  foreach ($result->data->fields as $field) {
    // Cleanse the value (stripping new lines).
    $field->id = $parser->cleanseValue($field->id);
    // Ignore the ID field.
    if ($field->id == '_id') {
      continue;
    }
    // Group by field type.
    $options[$field->type][$field->id] = $field->id;
    // All fields.
    $options['all'][$field->id] = $field->id;
  }

  $form['title_data_settings'] = govcms_ckan_media_form_title_element(t('Data settings'));

  // Optional filter and query parameters.
  $form['ckan_filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('CKAN data filters'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => '<p>' . t('Filters can be used to refine/reduce the records returned from the CKAN datasource. !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'ckan-filters'))) . '</p>',
  );

  // Default values for query string.
  if (isset($config['ckan_filters']['search'])) {
    $ckan_query_default = $config['ckan_filters']['search'];
  }
  else {
    $ckan_query_default = '';
  }

  $form['ckan_filters']['search'] = array(
    '#type' => 'textfield',
    '#title' => t('Full text query'),
    '#description' => t('Optionally query entire dataset for any string value.'),
    '#default_value' => $ckan_query_default,
  );

  // Default values for key/value filters.
  if (isset($config['ckan_filters']['filters'])) {
    $ckan_filters_default = $config['ckan_filters']['filters'];
  }
  else {
    $ckan_filters_default = '';
  }

  $form['ckan_filters']['filters'] = array(
    '#type' => 'textfield',
    '#title' => t('Filters'),
    '#description' => t('Filter on key/value dictionary. For example: {"code": "4000", "year": "2016"}.'),
    '#default_value' => $ckan_filters_default,
  );

  // Build the elements.
  $form['keys'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Keys'),
    '#options' => $options[$key_type],
    '#default_value' => $config['keys'],
    '#multiple' => TRUE,
    '#required' => TRUE,
    '#description' => t('What data to include in the visualisation. If X axis grouping is "Group by keys" these values will form the data groups lines/bars/legend, if grouping is "group by label values" these values will be values on the X axis. <strong>Do not include the key used in "Label key" below</strong>. !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'keys'))),
  );

  $col_override_id = 'col-override-settings';
  $form['labels'] = array(
    '#type' => 'select',
    '#title' => t('Label key'),
    '#options' => $options['all'],
    '#default_value' => $config['labels'],
    '#description' => t('What key contains the labels. Ensure this key is not selected in "Keys" above. !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'label-key'))),
    '#ajax' => array(
      'callback' => 'govcms_ckan_media_visualisation_default_key_config_col_override_callback',
      'wrapper' => $col_override_id,
    ),
  );

  $x_axis_grouping = isset($config['x_axis_grouping']) ? $config['x_axis_grouping'] : 'keys';
  $form['x_axis_grouping'] = array(
    '#type' => 'select',
    '#title' => t('X Axis Grouping'),
    '#options' => array(
      'keys' => t('Group by keys'),
      'values' => t('Group by label values'),
    ),
    '#default_value' => $x_axis_grouping,
    '#description' => t('Will the X axis use the <em>keys</em> or the <em>label key value</em> as tick values. Changing this swaps what is displayed on the X axis (or table header if viewed as a table). !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'x-axis-grouping'))),
    '#ajax' => array(
      'callback' => 'govcms_ckan_media_visualisation_default_key_config_col_override_callback',
      'wrapper' => $col_override_id,
    ),
  );

  $form['split'] = array(
    '#type' => 'select',
    '#title' => t('Split field'),
    '#options' => $options['all'],
    '#default_value' => $config['split'],
    '#empty_option' => t('None'),
    '#description' => t('Optionally spit into multiple visualisations based on the value of this key. Eg. a column of the data might contain the year, with this you could split the visualisations into one per year. !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'split'))),
  );

  $column_override_examples = array(
    'type|line', 'color|#000000', 'legend|hide', 'style|dashed', 'weight|20', 'class|hide-points',
  );
  $form['column_overrides'] = array(
    '#prefix' => '<div id="' . $col_override_id . '">',
    '#suffix' => '</div>',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Column/Group overrides'),
    '#description' => '<p>' . t('Optionally override a style for a specific column, add one key|value per line and separate key value with a pipe. !help.<br />Examples: <strong>!examples</strong>.',
      array(
        '!examples' => implode('</strong> or <strong>', $column_override_examples),
        '!help' => govcms_ckan_help_link('govcms_ckan_media', 'column-overrides'),
      )) . '</p>',
  );

  // Get the columns in use and make a text area for each to store its settings.
  $columns = $x_axis_grouping == 'keys' ? $options['all'] : $parser->getUniqueLabels($config['labels']);
  foreach ($columns as $column) {
    $form['column_overrides'][$column] = array(
      '#type' => 'textarea',
      '#rows' => 2,
      '#title' => filter_xss($column),
      '#default_value' => isset($config['column_overrides'][$column]) ? $config['column_overrides'][$column] : NULL,
    );
  }

  $form['label_settings'] = array(
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#type' => 'fieldset',
    '#title' => t('Label options'),
  );

  $form['label_settings']['overrides'] = array(
    '#type' => 'textarea',
    '#rows' => 2,
    '#title' => t('Label overrides'),
    '#default_value' => isset($config['label_settings']['overrides']) ? $config['label_settings']['overrides'] : NULL,
    '#description' => t('Optionally override one or more labels. Add one original_label|new_label per line and separate with a pipe. This will apply to X axis labels and legend labels. !help.',
      array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'label-overrides'))),
  );

  // This indicates to future parsers that these elements have been included.
  $form['key_defaults'] = array(
    '#type' => 'hidden',
    '#value' => 1,
  );

  return $form;
}

/**
 * Ajax callback for changing the label key or x grouping.
 */
function govcms_ckan_media_visualisation_default_key_config_col_override_callback($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  $parents = $form_state['triggering_element']['#parents'];
  array_pop($parents);
  $el = drupal_array_get_nested_value($form, $parents);
  return $el['column_overrides'];
}

/**
 * This helper provides the common elements for graph display axis settings.
 *
 * Eg: labels, tick counts, etc.
 *
 * @param array $form
 *   Form array for the parent form.
 * @param array $form_state
 *   Current form state array from the parent form.
 * @param array $config
 *   The current configuration values.
 *
 * @return array
 *   Form structure for new elements.
 */
function govcms_ckan_media_visualisation_default_axis_config($form, &$form_state, $config = array()) {
  $element = array();

  $element['title_axis_settings'] = govcms_ckan_media_form_title_element(t('Axis settings'));

  $element['rotated'] = array(
    '#type' => 'select',
    '#title' => t('Orientation'),
    '#default_value' => govcms_ckan_get_config_value($config, 'rotated'),
    '#description' => t('Default orientation is vertical, changing to horizontal will rotate the graph 90deg clockwise with the Y axis at the bottom and X axis on the left.'),
    '#options' => array(
      'false' => t('Vertical'),
      'true' => t('Horizontal'),
    ),
  );

  // Labels.
  $element['x_label'] = array(
    '#type' => 'textfield',
    '#title' => t('X axis label'),
    '#default_value' => govcms_ckan_get_config_value($config, 'x_label'),
    '#description' => t('Add a label to the Y axis to describe what the data values relate to. Eg Height (m) or Weight (kg).'),
  );
  $element['y_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Y axis label'),
    '#default_value' => govcms_ckan_get_config_value($config, 'y_label'),
    '#description' => t('Add a label to the X axis to describe what the values realte to. Eg Year or Category.'),
  );

  // Advanced settings.
  $element['axis_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced axis settings'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
  );

  $element['axis_settings']['x_axis_title'] = govcms_ckan_media_form_title_element(t('X Axis'));

  // Tick settings.
  $element['axis_settings']['x_tick_rotate'] = array(
    '#title' => t('X labels vertical'),
    '#type' => 'checkbox',
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/x_tick_rotate'),
    '#description' => t('Check to rotate the X axis labels 90 degrees. This is useful when there is not enough space for the labels to display horizontally.'),
  );

  $element['axis_settings']['x_tick_count'] = array(
    '#title' => t('X Tick Count'),
    '#type' => 'textfield',
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/x_tick_count'),
    '#element_validate' => array('element_validate_integer'),
    '#description' => t('Limit the number of ticks on the X axis. This only relates to the number of ticks on the X axis and not the values.'),
  );

  // Limit label count.
  $element['axis_settings']['x_tick_cull'] = array(
    '#title' => t('X Label count (max)'),
    '#type' => 'textfield',
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/x_tick_cull'),
    '#element_validate' => array('element_validate_integer'),
    '#description' => t('The number of X ticks and labels will be adjusted to <strong>less than this value</strong>. Labels will be rounded to nearest whole number.<br /><strong>NOTE: This will only work if the X axis labels are numbers</strong> as maths is used to reduce the label count.'),
  );

  $element['axis_settings']['x_tick_values'] = array(
    '#type' => 'textfield',
    '#title' => t('X Tick Values'),
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/x_tick_values'),
    '#description' => t('Override X axis tick values. Separate values with a comma, <strong>only numeric values supported</strong> and they should be within the range of the dataset.<br><strong>NOTE: If defining values here do not use X tick count or X label count.</strong>'),
  );

  $element['axis_settings']['x_tick_value_format'] = array(
    '#type' => 'select',
    '#title' => t('X Tick Value Number Format'),
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/x_tick_value_format'),
    '#description' => t('How are the Y axis values are formatted. Only works if X values are numeric.'),
    '#options' => array(
      '' => t('None'),
      ' ' => t('Space separated Eg. 10 000'),
      ',' => t('Comma separated Eg. 10,000'),
    ),
  );

  $element['axis_settings']['y_axis_title'] = govcms_ckan_media_form_title_element(t('Y Axis'));

  $element['axis_settings']['y_tick_count'] = array(
    '#title' => t('Y Tick Count'),
    '#type' => 'textfield',
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/y_tick_count'),
    '#element_validate' => array('element_validate_integer'),
    '#description' => t('Limit the number of ticks on the Y axis.'),
  );

  $element['axis_settings']['y_tick_values'] = array(
    '#type' => 'textfield',
    '#title' => t('Y Tick Values'),
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/y_tick_values'),
    '#description' => t('Override Y axis tick values. Separate values with a comma, values should be numeric and within the range of the dataset.'),
  );

  $element['axis_settings']['y_tick_value_format'] = array(
    '#type' => 'select',
    '#title' => t('Y Tick Value Number Format'),
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/y_tick_value_format'),
    '#description' => t('How are the Y axis values are formatted.'),
    '#options' => array(
      '' => t('None'),
      ' ' => t('Space separated Eg. 10 000'),
      ',' => t('Comma separated Eg. 10,000'),
    ),
  );

  $element['axis_settings']['x_y_axis_title'] = govcms_ckan_media_form_title_element(t('X and Y Axis'));

  // Tick visibility.
  $element['axis_settings']['tick_visibility'] = array(
    '#type' => 'select',
    '#title' => t('Tick visibility'),
    '#default_value' => govcms_ckan_get_config_value($config, 'axis_settings/tick_visibility', 'show'),
    '#description' => t('Applies to the axis ticks only and not the tick labels.'),
    '#options' => array(
      'show' => t('Show all ticks'),
      'hide-x' => t('Hide X ticks'),
      'hide-y' => t('Hide Y ticks'),
      'hide-xy' => t('Hide X and Y ticks'),
    ),
  );

  return $element;
}

/**
 * This helper provides the common elements for graph display grid settings.
 *
 * Eg: labels, tick counts, etc.
 *
 * @param array $form
 *   Form array for the parent form.
 * @param array $form_state
 *   Current form state array from the parent form.
 * @param array $config
 *   The current configuration values.
 *
 * @return array
 *   Form structure for new elements.
 */
function govcms_ckan_media_visualisation_default_grid_config($form, &$form_state, $config = array()) {
  $element = array();

  $element['title_grid_settings'] = govcms_ckan_media_form_title_element(t('Grid settings'));

  $element['grid'] = array(
    '#type' => 'select',
    '#title' => t('Enable grid'),
    '#default_value' => govcms_ckan_get_config_value($config, 'grid'),
    '#empty_option' => t('None'),
    '#description' => t('Optionally add grid lines to a chart.'),
    '#options' => array(
      'x' => t('X Lines'),
      'y' => t('Y Lines'),
      'xy' => t('Both X and Y lines'),
    ),
  );

  // Additional grind line configuration.
  $grid_lines_id = 'axis-lines-settings';
  $grid_line_values = govcms_ckan_get_config_value($config, 'grid_lines/lines', array());
  $grid_line_values = govcms_ckan_array_valid_rows($grid_line_values, 'value');
  if (empty($form_state['grid_line_count'])) {
    $form_state['grid_line_count'] = count($grid_line_values) > 0 ? count($grid_line_values) : 1;
  }
  $element['grid_lines'] = array(
    '#collapsible' => TRUE,
    // Open if more than 1 line or we have values.
    '#collapsed' => ($form_state['grid_line_count'] < 2 && empty($grid_line_values)),
    '#type' => 'fieldset',
    '#title' => t('Additional Grid / Trend Lines'),
    '#description' => '<p>' . t('This allows you to draw additional grid lines on the chart. Eg Trend lines or intersects. !help.',
        array('!help' => govcms_ckan_help_link('govcms_ckan_media', 'grid-lines'))) . '</p>',
    '#prefix' => '<div id="' . $grid_lines_id . '">',
    '#suffix' => '</div>',
    'lines' => array(
      '#type' => 'container',
    ),
  );

  // For each line make a fieldset containing line settings.
  for ($i = 0; $i < $form_state['grid_line_count']; $i++) {
    $grid_line_value = govcms_ckan_get_config_value($grid_line_values, $i);
    $element['grid_lines']['lines'][$i] = array(
      '#type' => 'fieldset',
      '#title' => t('Grid line @num', array('@num' => ($i + 1))),
      '#attributes' => array('class' => array('container-inline')),
    );
    $element['grid_lines']['lines'][$i]['axis'] = array(
      '#title' => 'Axis',
      '#type' => 'select',
      '#options' => array('x' => 'X', 'y' => 'Y'),
      '#default_value' => govcms_ckan_get_config_value($grid_line_value, 'axis', 'y'),
    );
    $element['grid_lines']['lines'][$i]['value'] = array(
      '#title' => 'Value',
      '#type' => 'textfield',
      '#default_value' => govcms_ckan_get_config_value($grid_line_value, 'value'),
      '#size' => 20,
    );
    $element['grid_lines']['lines'][$i]['text'] = array(
      '#title' => 'Label',
      '#type' => 'textfield',
      '#default_value' => govcms_ckan_get_config_value($grid_line_value, 'text'),
      '#size' => 20,
    );
    $element['grid_lines']['lines'][$i]['position'] = array(
      '#title' => 'Label position',
      '#type' => 'select',
      '#options' => array(
        'start' => 'Start',
        'middle' => 'Middle',
        'end' => 'End',
      ),
      '#default_value' => govcms_ckan_get_config_value($grid_line_value, 'position', 'end'),
    );
    $element['grid_lines']['lines'][$i]['class'] = array(
      '#title' => 'CSS Class',
      '#type' => 'textfield',
      '#default_value' => govcms_ckan_get_config_value($grid_line_value, 'class'),
      '#size' => 20,
    );
  }

  // Add a new grid line.
  $element['grid_lines']['add'] = array(
    '#type' => 'submit',
    '#value' => 'Add line',
    '#submit' => array('govcms_ckan_media_visualisation_default_grid_lines_config_add_one'),
    '#ajax' => array(
      'wrapper' => $grid_lines_id,
      'callback' => 'govcms_ckan_media_visualisation_default_grid_lines_config_ajax_callback',
    ),
  );

  // Remove the last grids line.
  $element['grid_lines']['remove'] = array(
    '#type' => 'submit',
    '#value' => 'Remove line',
    '#submit' => array('govcms_ckan_media_visualisation_default_grid_lines_config_remove_one'),
    '#ajax' => array(
      'wrapper' => $grid_lines_id,
      'callback' => 'govcms_ckan_media_visualisation_default_grid_lines_config_ajax_callback',
    ),
  );

  return $element;
}

/**
 * Ajax callback for adding an axis line.
 */
function govcms_ckan_media_visualisation_default_grid_lines_config_ajax_callback($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  $parents = $form_state['triggering_element']['#parents'];
  array_pop($parents);
  $el = drupal_array_get_nested_value($form, $parents);
  return $el;
}

/**
 * Submit callback for adding a new line.
 */
function govcms_ckan_media_visualisation_default_grid_lines_config_add_one($form, &$form_state) {
  $form_state['grid_line_count']++;
  $form_state['rebuild'] = TRUE;
}

/**
 * Submit callback for removing a line.
 */
function govcms_ckan_media_visualisation_default_grid_lines_config_remove_one($form, &$form_state) {
  if ($form_state['grid_line_count'] > 1) {
    $form_state['grid_line_count']--;
  }
  $form_state['rebuild'] = TRUE;
}

/**
 * This helper provides the common elements for generic chart display settings.
 *
 * Eg: Titles, legends, palette, etc.
 *
 * @param array $form
 *   Form array for the parent form.
 * @param array $form_state
 *   Current form state array from the parent form.
 * @param array $config
 *   The current configuration values.
 *
 * @return array
 *   Form structure for new elements.
 */
function govcms_ckan_media_visualisation_default_chart_config($form, &$form_state, $config = array()) {
  $element = array();

  $element['title_chart_settings'] = govcms_ckan_media_form_title_element(t('Chart settings'));

  $element['show_title'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable chart title'),
    '#description' => t('Render the title as part of the chart, this ensures it is present when exported.  The title used will be the file name above.'),
    '#default_value' => govcms_ckan_get_config_value($config, 'show_title'),
  );

  $element['disable_chart_interaction'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable chart interaction'),
    '#description' => t('Prevents values from showing on chart hover.'),
    '#default_value' => govcms_ckan_get_config_value($config, 'disable_chart_interaction'),
  );

  $element['disable_legend_interaction'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable legend interaction'),
    '#description' => t('This disables click to toggle and hover effects on the legend. Default legend hover will focus on that column/group, default click toggles visibility of that column/group.'),
    '#default_value' => govcms_ckan_get_config_value($config, 'disable_legend_interaction'),
  );

  $element['palette_override'] = array(
    '#title' => t('Palette override'),
    '#type' => 'textfield',
    '#default_value' => govcms_ckan_get_config_value($config, 'palette_override'),
    '#description' => t('Palette is a comma separated list of hex values. If not set, default palette is applied. If more columns exist than are defined in the palette it will loop back though the palette resulting in duplicate colours used.'),
  );

  // Chart padding.
  $element['chart_padding'] = array(
    '#type' => 'fieldset',
    '#title' => t('Chart padding'),
    '#description' => '<p>' . t('Add additional padding to the outer edges of the chart. This can potentially break charts so use with care.') . '</p>',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
  );

  $padding_edges = array('top', 'right', 'bottom', 'left');
  foreach ($padding_edges as $edge) {
    $element['chart_padding'][$edge] = array(
      '#title' => ucwords($edge),
      '#type' => 'textfield',
      '#default_value' => govcms_ckan_get_config_value($config, 'chart_padding/' . $edge),
    );
  }

  return $element;
}

/**
 * A helper to return a form title element (markup).
 *
 * @param string $title
 *   The title to use.
 *
 * @return array
 *   Markup form element.
 */
function govcms_ckan_media_form_title_element($title) {
  return array('#type' => 'markup', '#markup' => '<h3 class="govcms-ckan-media--settings-title">' . $title . '</h3>');
}
