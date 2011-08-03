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

    // Identify the most appropriate release.
    $release = pm_parse_release($project, $xml);

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
    $dl_dir = $project['project_install_location'] . DIRECTORY_SEPARATOR . $project['project_dir'];
    if (is_dir($dl_dir)) {
      if (!drush_confirm(dt('Install location !location already exists. Do you want to overwrite it?', array('!location' =>                   $dl_dir)))) {
        drush_log(dt("Skip installation of !project to !dest.", array('!project' => $project['name'], '!dest' =>                              $dl_dir)), 'warning');
        continue;
      }
      else {
        rrmdir($dl_dir);
      }
    }

    // Add the submodule.
    // HACK - this is an UBER MEGA hack, will try to revisit this later when I'm
    // not up against a time limit.
    chdir(drush_locate_root());
    $mod_path = str_replace(drush_locate_root() . '/', '',    $dl_dir);
    shell_exec('git submodule add git://git.drupal.org/project/' . $project['name'] . '.git ' . $mod_path);

    // If the release tag isn't a tag, checkout the branch.
    chdir($dl_dir);
    if (!preg_match('/^\d+\.x-\d+\.x/', $release['tag'])) {
      shell_exec('git checkout tags/' . $release['tag']);
    }
    else {
      shell_exec('git checkout ' . $release['tag']);
    }
  }
}

/**
 * Helper function to recursively delete directories.
 */
function rrmdir($dir) {
  if (is_dir($dir)) {
    $objects = scandir($dir);
    foreach ($objects as $object) {
      if ($object != "." && $object != "..") {
        if (filetype($dir."/".$object) == "dir") {
          rrmdir($dir."/".$object);
        }
        else {
          unlink($dir."/".$object);
        }
      }
    }
    reset($objects);
    rmdir($dir);
  }
}
