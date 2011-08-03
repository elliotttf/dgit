<?php

/**
 * @file
 * This drush definition integrates common git commands with drush.
 */

/**
 * Implements hook_drush_command().
 */
function dgit_drush_command() {
  $items = array();

  $items['dgit-dl'] = array(
    'description' => 'Downloads a Drupal project as a git submodule.',
    'arguments' => array(
      'projects' => 'The Drupal project(s) to download.',
    ),
    'options' => array(
      'destination' => "Path to which the project will be copied. If you're providing a relative path, note it is relative to the drupal     root (if bootstrapped).",
      'select' => "Select the version to download interactively from a list of available releases.",
      'drupal-project-rename' => 'Alternate name for "drupal-x.y" directory when downloading Drupal project. Defaults to "drupal".',
    ),
    'examples' => array(
      'drush dgit-dl views',
    ),
  );

  return $itmes;
}

/**
 * Implements hook_drush_help().
 */
function dgit_drush_help($section) {
  switch ($section) {
    case 'drush:dgit-dl':
      return dt('This command downloads a Drupal module as a git submodule.');
      break;
  }
}

/**
 * Implementats drush_COMMAND_validate().
 */
function drush_dgit_dl_validate($projects) {
  // Validate the project request.
  if (empty($projects)) {
    return drush_set_error('DGIT_NO_PROJECTS', dt('Missing argument: projects'));
  }

  // Validate the user specified destination directory.
  $destination = drush_get_option('destination');
  if (!empty($destination)) {
    $destination = rtrim($destination, DIRECTORY_SEPARATOR);
    if (!is_dir($destination)) {
      drush_print(dt("The directory !destination does not exist.", array('!destination' => $destination)));
      if (!drush_get_context('DRUSH_SIMULATE')) {
        if (drush_confirm(dt('Would you like to create it?'))) {
          drush_mkdir($destination);
        }
        if (!is_dir($destination)) {
          return drush_set_error('DRUSH_PM_NO_DESTINATION', dt('Unable to create destination directory !destination.', array('!destination'   => $destination)));
        }
      }
    }
    if (!is_writable($destination)) {
      return drush_set_error('DRUSH_PM_NO_DESTINATION', dt('Destination directory !destination is not writable.', array('!destination' =>     $destination)));
    }
  }
}

/**
 * Command callback - download Drupal projects as git submodules.
 */
function drush_dgit_dl() {
  
}
