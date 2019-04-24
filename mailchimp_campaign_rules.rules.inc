<?php

/**
 * @file
 * Contains rules to create and send campaigns.
 */

/**
 * Implements hook_rules_data_info().
 */
function mailchimp_campaign_rules_rules_data_info() {
  return array(
    'mailchimp_campaign_content' => array(
      'label' => t('Mailchimp Content'),
      'ui class' => 'RulesDataUIMailchimpCampaignContent',
      'token type' => 'rules_text',
    ),
  );
}

/**
 * Implements hook_rules_evaluator_info().
 */
function mailchimp_campaign_rules_rules_evaluator_info() {
  return array(
    'mailchimp_campaign_token' => array(
      'class' => 'MailchimpCampaignTokenEvaluator',
      'type' => 'mailchimp_campaign_content',
      'weight' => 0,
    ),
  );
}

/**
 * Implements hook_rules_action_info().
 */
function mailchimp_campaign_rules_rules_action_info() {
  return array(
    'mailchimp_campaign_action_create_campaign' => array(
      'label' => t('Create mailchimp campaign'),
      'named parameter' => TRUE,
      'group' => t('Mailchimp Campaign'),
      'parameter' => array(
        'title' => array(
          'type' => 'text',
          'label' => t('Title'),
          'description' => t('An internal name to use for this campaign. By default, the campaign subject will be used.'),
          'optional' => TRUE,
        ),
        'subject' => array(
          'type' => 'text',
          'label' => t('Subject'),
        ),
        'list_id' => array(
          'type' => 'text',
          'label' => t('List'),
          'description' => t('Select the list this campaign should be sent to.'),
          'restriction' => 'input',
          'ui class' => 'RulesDataUIMailchimpCampaignListId',
        ),
        'list_segment_id' => array(
          'type' => 'text',
          'label' => t('List Segment'),
          'description' => t('Optionally select the list segment this campaign should be sent to.'),
          'restriction' => 'input',
          'ui class' => 'RulesDataUIMailchimpCampaignSegmentId',
          'optional' => TRUE,
        ),
        'from_email' => array(
          'type' => 'text',
          'label' => t('From Email'),
          'description' => t('the From: email address for your campaign message.'),
          'default value' => variable_get('site_mail'),
        ),
        'from_name' => array(
          'type' => 'text',
          'label' => t('From Name'),
          'description' => t('the From: name for your campaign message (not an email address)'),
          'default value' => variable_get('site_name'),
          'optional' => TRUE,
        ),
        'template_id' => array(
          'type' => 'integer',
          'label' => t('Template'),
          'description' => t('Select a MailChimp user template to use. Due to a limitation in the API, only templates that do not contain repeating ections are available. If empty, the default template will be applied.'),
          'restriction' => 'input',
          'ui class' => 'RulesDataUIMailchimpCampaignTemplateId',
        ),
        'content' => array(
          'type' => 'mailchimp_campaign_content',
          'label' => t('Content sections'),
          'description' => t('The HTML content or, if a template is selected, the content for each section.'),
          'restriction' => 'input',
        ),
      ),
      'provides' => array(
        'campaign_id' => array(
          'label' => t('Campaign ID'),
          'type' => 'integer',
          'save' => TRUE,
        ),
      ),
      'callbacks' => array(
        'execute' => 'mailchimp_campaign_action_create_campaign',
      ),
    ),
    'mailchimp_campaign_action_send_campaign' => array(
      'label' => t('Send mailchimp campaign'),
      'group' => t('Mailchimp Campaign'),
      'parameter' => array(
        'campaign_id' => array(
          'type' => 'integer',
          'label' => t('Campaign id'),
          'description' => t('The campaign id for the campaign that should be sent.'),
        ),
      ),
      'callbacks' => array(
        'execute' => 'mailchimp_campaign_action_send_campaign',
      ),
    ),
  );
}

/**
 * Ajax callback to return the content sections form.
 */
function mailchimp_campaign_ajax_template_sections_form($form, $form_state) {
  return $form['parameter']['content']['settings']['content'];
}

/**
 * AJAX callback when changing list ID.
 */
function mailchimp_campaign_ajax_list_segment_callback($form, $form_state) {
  $commands = array();
  $list_id = $form_state['values']['parameter']['list_id']['settings']['list_id'];
  $list_segments = array();
  if (isset($list_id) && !empty($list_id)) {
    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $list_segments = mailchimp_campaign_get_list_segments($list_id, 'saved');
  }

  $form['list_segment_id'] = array(
    '#type' => 'select',
    '#title' => t('Select'),
    '#options' => _mailchimp_campaign_build_option_list($list_segments, '-- Entire list --'),
    '#value' => '',
    '#required' => FALSE,
    '#id' => 'edit-parameter-list-segment-id-settings-list-segment-id',
    '#name' => 'parameter[list_segment_id][settings][list_segment_id]',
  );
  $form['list_segment_id']['#prefix'] = '<div id="list-segments-wrapper">';
  $form['list_segment_id']['#suffix'] = '</div>';

  $list_segment_html = drupal_render($form['list_segment_id']);

  $commands[] = ajax_command_replace('#list-segments-wrapper',
    $list_segment_html);

  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * The action function to create a mailchimp campaign.
 */
function mailchimp_campaign_action_create_campaign($mailchimp_campaign_settings) {
  $recipients = new stdClass();
  $recipients->list_id = $mailchimp_campaign_settings['list_id'];
  if (isset($mailchimp_campaign_settings['list_segment_id']) && !empty($mailchimp_campaign_settings['list_segment_id'])) {
    $recipients->segment_opts = new stdClass();
    $recipients->segment_opts->saved_segment_id = (int)$mailchimp_campaign_settings['list_segment_id'];
  }

  $options = new stdClass();
  $options->title = !empty($mailchimp_campaign_settings['title']) ? $mailchimp_campaign_settings['title'] : $mailchimp_campaign_settings['subject'];
  $options->subject_line = $mailchimp_campaign_settings['subject'];
  $options->from_email = $mailchimp_campaign_settings['from_email'];
  $options->from_name = check_plain($mailchimp_campaign_settings['from_name']);
  $options->reply_to = check_plain($mailchimp_campaign_settings['from_email']);

  // Save the mailchimp campaign.
  $campaign_id = mailchimp_campaign_save_campaign($mailchimp_campaign_settings['content'], $recipients, $options, mailchimp_campaign_settings['template_id']);

  // Clear campaign cache.
  mailchimp_campaign_get_campaigns(array($campaign_id), TRUE);

  return array('campaign_id' => $campaign_id);
}

/**
 * The action function to send a mailchimp campaign.
 */
function mailchimp_campaign_action_send_campaign($campaign_id) {
  if (!empty($campaign_id)) {
    // Load the campaign from the id.
    $campaign = mailchimp_campaign_load_by_mc_campaign_id($campaign_id);
  }

  if (!empty($campaign)) {
    // Send the campaign.
    mailchimp_campaign_send_campaign($campaign);
    drupal_set_message(t('Campaign %name has been sent.', array('%name' => $campaign->label())));

    // Clear campaign cache.
    mailchimp_campaign_get_campaigns(array($campaign_id), TRUE);
  } else {
    drupal_set_message(t('The campaign could not be sent because the campaign or campaign id is invalid.'), array(), 'error');
  }
}

/**
 * load a campaign by mailchimp campaign id.
 */
function mailchimp_campaign_load_by_mc_campaign_id($mc_campaign_id, $reset = false) {
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'mailchimp_campaign')
    ->propertyCondition('mc_campaign_id', $mc_campaign_id);

  $result = $query->execute();
  if (isset($result['mailchimp_campaign'])) {
    $mcids = array_keys($result['mailchimp_campaign']);
    $campaign = mailchimp_campaign_load_multiple($mcids, array(), $reset);
    return $campaign ? $campaign[$mcids[0]] : false;
  }
  return false;
}

/**
 * Defines an input form for the List id.
 */
class RulesDataUIMailchimpCampaignListId extends RulesDataUI implements RulesDataDirectInputFormInterface
{
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('list_id' => NULL));
    $value = $settings[$name];

    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $form['list_id'] = array(
      '#type' => 'select',
      '#title' => t('Select'),
      '#options' => _mailchimp_campaign_build_option_list(mailchimp_get_lists()),
      '#default_value' => $value,
      '#required' => TRUE,
      '#ajax' => array(
        'callback' => 'mailchimp_campaign_ajax_list_segment_callback',
        'method' => 'replace',
        'wrapper' => 'list-segments-wrapper',
      ),
    );

    return $form;
  }

  /**
   * Build a render array for the campaign list selector.
   */
  public static function render($value) {
    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $lists = _mailchimp_campaign_build_option_list(mailchimp_get_lists());
    if (!empty($lists[$value])) {
      return array(
        'content' => array(
          '#markup' => $lists[$value],
        ),
      );
    }
  }
}

/**
 * Defines an input form for the Segment id.
 */
class RulesDataUIMailchimpCampaignSegmentId extends RulesDataUI implements RulesDataDirectInputFormInterface
{
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('list_segment_id' => NULL));
    $value = $settings[$name];

    $list_segments = array();
    if (isset($settings['list_id']) && !empty($settings['list_id'])) {
      module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
      $list_segments = mailchimp_campaign_get_list_segments($settings['list_id'], 'saved');
    }

    $form['list_segment_id'] = array(
      '#type' => 'select',
      '#title' => t('Select'),
      '#options' => _mailchimp_campaign_build_option_list($list_segments, '-- Entire list --'),
      '#default_value' => $value ? $value : '',
      '#required' => FALSE,
    );
    $form['list_segment_id']['#prefix'] = '<div id="list-segments-wrapper">';
    $form['list_segment_id']['#suffix'] = '</div>';

    return $form;
  }

  /**
   * Build a render array for the campaign list selector.
   */
  public static function render($value) {
    return array(
      'content' => array(
        '#markup' => $value,
      ),
    );
  }
}

/**
 * Defines an input form for the Template id.
 */
class RulesDataUIMailchimpCampaignTemplateId extends RulesDataUI implements RulesDataDirectInputFormInterface
{
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('template_id' => NULL));
    $value = $settings[$name];

    module_load_include('inc', 'mailchimp_campaign', 'includes/mailchimp_campaign.admin');
    $form['template_id'] = array(
      '#type' => 'select',
      '#title' => t('Select'),
      '#options' => _mailchimp_campaign_build_option_list(mailchimp_campaign_list_templates(FALSE, 0, 500)),
      '#default_value' => $value,
      '#required' => FALSE,
      '#ajax' => array(
        'wrapper' => 'edit-parameter-content-settings-content',
        'callback' => 'mailchimp_campaign_ajax_template_sections_form',
      ),
    );

    return $form;
  }

  /**
   * Render the configured value.
   */
  public static function render($value) {
    return array(
      'content' => array(
        '#markup' => !empty($value) ? $value : 'default',
      ),
    );
  }
}

/**
 * Defines an input form for the Content.
 */
class RulesDataUIMailchimpCampaignContent extends RulesDataUI implements RulesDataDirectInputFormInterface
{
  /**
   * Specifies the default input mode per data type.
   */
  public static function getDefaultMode() {
    return 'input';
  }

  /**
   * Constructs the direct input form.
   */
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    $settings += array($name => isset($info['default value']) ? $info['default value'] : array('content' => NULL));
    $value = $settings[$name];

    $form['content'] = array(
      '#type' => 'container',
      '#id' => 'edit-parameter-content-settings-content',
      '#tree' => TRUE,
    );

    if (isset($settings['template_id']) && !empty($settings['template_id'])) {
      $mc_template = mailchimp_campaign_get_template($settings['template_id']);
      if (!empty($mc_template->info->sections)) {
        foreach ($mc_template->info->sections as $section => $content) {
          // Set the default value and text format to either saved campaign values
          // or defaults coming from the MC template.
          $default_value = isset($value[$section]['value']) ? $value[$section]['value'] : $content;
          $format = 'mailchimp_campaign';
          $form['content'][$section] = array(
            '#type' => 'text_format',
            '#format' => $format,
            '#title' => drupal_ucfirst($section),
            '#default_value' => $default_value,
          );
        }
      }

      // If template was not available or there are no sections, add a default section.
      if (empty($mc_template->info->sections) || empty($section)) {
        $form['content']['html'] = array(
          '#type' => 'text_format',
          '#format' => 'mailchimp_campaign',
          '#title' => t('Content'),
          '#default_value' => isset($value['html']['value']) ? $value['html']['value'] : '',
          '#description' => t('The HTML content of the campaign.'),
        );
      }
      RulesDataInputEvaluator::attachForm($form, $settings, $info, $element->availableVariables());
    }
    return $form;
  }

  /**
   * Render the configured value.
   */
  public static function render($value) {
    $sections = array_keys($value);
    return array(
      'content' => array(
        '#markup' => implode(', ', $sections),
      ),
    );
  }
}