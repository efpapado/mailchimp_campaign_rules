<?php

/**
 * @file
 * Contains a class that implements a rules input evaluator processing tokens.
 */

/**
 * A class implementing a rules input evaluator processing tokens.
 */
class MailchimpCampaignTokenEvaluator extends RulesDataInputEvaluator
{

  /**
   * Prepares the evaluation.
   */
  public function prepare($values, $variables) {
    $needs_scan = NULL;
    foreach ($values as $value) {
      if (token_scan($value['value'])) {
        $needs_scan = TRUE;
      }
    }
    // Skip this evaluator if there are no tokens.
    $this->setting = $needs_scan;
  }

  /**
   * Performs the token replacement.
   *
   * We replace the tokens on our own as we cannot use token_replace(), because
   * token usually assumes that $data['node'] is a of type node, which doesn't
   * hold in general in our case.
   * So we properly map variable names to variable data types and then run the
   * replacement ourself.
   */
  public function evaluate($values, $options, RulesState $state) {
    $variables = $state->varInfo();
    $options += array('sanitize' => FALSE);

    // Replace the tokens in each section.
    foreach ($values as $section => $value) {
      $replacements[$section] = array();
      $data = array();
      foreach (token_scan($value['value']) as $var_name => $tokens) {
        $var_name = str_replace('-', '_', $var_name);
        if (!function_exists('_rules_system_token_map_type')) {
          module_load_include('inc', 'rules', 'modules/system.eval');
        }
        if (isset($variables[$var_name]) && ($token_type = _rules_system_token_map_type($variables[$var_name]['type']))) {
          // We have to key $data with the type token uses for the variable.
          $data = rules_unwrap_data(array($token_type => $state->get($var_name)), array($token_type => $variables[$var_name]));
          $replacements[$section] += token_generate($token_type, $tokens, $data, $options);
        } else {
          $replacements[$section] += token_generate($var_name, $tokens, array(), $options);
        }
        // Remove tokens if no replacement value is found. As token_replace()
        // does if 'clear' is set.
        $replacements[$section] += array_fill_keys($tokens, '');
      }

      // Optionally clean the list of replacement values.
      if (!empty($options['callback']) && function_exists($options['callback'])) {
        $function = $options['callback'];
        $function($replacements[$section], $data, $options);
      }

      // Actually apply the replacements.
      $token_keys = array_keys($replacements[$section]);
      $token_values = array_values($replacements[$section]);
      $values[$section]['value'] = str_replace($token_keys, $token_values, $values[$section]['value']);
    }
    return $values;
  }

  /**
   * Create documentation about the available replacement patterns.
   *
   * @param array $variables
   *   Array with the available variables.
   *
   * @return array
   *   Renderable array with the replacement pattern documentation.
   */
  public static function help($variables) {
    $render = array(
      '#type' => 'fieldset',
      '#title' => t('Replacement patterns'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('Note that token replacements containing chained objects â€“ such as [node:author:uid] â€“ are not listed here, but are till available. The <em>data selection</em> input mode may help you find more complex replacement patterns. See <a href="@url">the online ocumentation</a> for more information about complex replacement patterns.',
        array('@url' => rules_external_help('chained-tokens'))),
    );

    $token_info = token_info();
    $token_types = array();
    foreach ($variables as $name => $info) {
      $token_types[$name] = _rules_system_token_map_type($info['type']);
    }

    foreach ($token_types as $name => $token_type) {
      if (isset($token_info['types'][$token_type])) {
        $render[$name] = array(
          '#theme' => 'table',
          '#header' => array(t('Token'), t('Label'), t('Description')),
          '#prefix' => '<h3>' . t('Replacement patterns for %label', array('%label' => $variables[$name]['label'])) . '</h3>',
        );
        foreach ($token_info['tokens'][$token_type] as $token => $info) {
          $token = '[' . str_replace('_', '-', $name) . ':' . $token . ']';
          $render[$name]['#rows'][$token] = array(
            check_plain($token),
            check_plain($info['name']),
            check_plain($info['description']),
          );
        }
      }
    }
    return $render;
  }
}