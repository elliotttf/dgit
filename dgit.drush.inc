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

  return $items;
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
function drush_dgit_dl_validate() {
  // Validate the project request.
  $projects = func_get_args();
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
  $projects = _convert_csv_to_array(func_get_args());
  $projects = pm_parse_project_version($projects);

  // Get release history for each request and download the project.
  $project_types = pm_project_types();
  $project_types_xpath = '(value="' . implode('" or value="', $project_types) . '")';
  foreach ($projects as $name => $project) {
    $xml = _drush_pm_get_release_history_xml($project);
    if (!$xml) {
      continue;
    }
    print_r($xml);

    // Identify the most appropriate release.
    $release = pm_parse_release($project, $xml);
    print_r($release);

    if (!$release) {
      continue;
    }

    // Determine what type of project we are to download.
    $project['project_type'] = 'module';
    if ($types = $xml->xpath('/project/terms/term[name="Projects" and ' . $project_types_xpath . ']')) {
      $project['project_type'] = array_search($types[0]->value, $project_types);
    }

    if ($project['project_type'] == 'translation') {
      drush_set_error('DRUSH_PM_DOWNLOAD_TRANSLATIONS_FORBIDDEN', dt('Drush has dropped support for downloading translation projects. See     l10n_update and l10n_install projects.'));
      continue;
    }

    // Determine the name of the directory that will contain the project.
    // We face here all the asymetries to make it smooth for package handlers.
    // For Drupal core: --drupal-project-rename or drupal-x.y
    if ($project['project_type'] == 'core') {
      if ($rename = drush_get_option('drupal-project-rename', FALSE)) {
        if ($rename === TRUE) {
          $project['project_dir'] = 'drupal';
        }
        else {
          $project['project_dir'] = $rename;
        }
      }
      else {
        // Set to drupal-x.y, the expected name for .tar.gz contents.
        // Explicitly needed for cvs package handler.
        $project['project_dir'] = strtolower(strtr($release['name'], ' ', '-'));
      }
    }
    // For the other project types we want the project name. Including core
    // variant for profiles.  Note those come with drupal-x.y in the .tar.gz.
    else {
      $project['project_dir'] = $project['name'];
    }

    // Determine the install location for the project.  User provided
    // --destination has preference.
    $destination = drush_get_option('destination');
    if (!empty($destination)) {
      $project['project_install_location'] = $destination;
    }
    else {
      $project['project_install_location'] = pm_dl_destination($project['project_type']);
    }

    // If user did not provide --destination, then call the
    // download-destination-alter hook to give the chance to any commandfiles
    // to adjust the install location or abort it.
    if (empty($destination)) {
      $result = drush_command_invoke_all_ref('drush_pm_download_destination_alter', $project, $release);
      if (array_search(FALSE, $result, TRUE) !== FALSE) {
        return FALSE;
      }
    }

    // Check if install location already exists.
    if (is_dir($project['project_install_location'])) {
      if (!drush_confirm(dt('Install location !location already exists. Do you want to overwrite it?', array('!location' =>                   $project['project_install_location'])))) {
        drush_log(dt("Skip installation of !project to !dest.", array('!project' => $project['name'], '!dest' =>                              $project['project_install_location'])), 'warning');
        continue;
      }
      else {
        rmdir($project['project_install_location']);
      }
    }

    // Post download actions.
    package_handler_post_download($project);
    drush_command_invoke_all('drush_pm_post_download', $project, $release);
  }
}
