This drush command aids in downloading Drupal modules to source
trees versioned by git.

When the command is executed:

  drush dgit-dl MODULE

the following git commands will be executed:

  git submodule add GIT-REPO DOWNLOAD-LOCATION
  cd DOWNLOAD-LOCATION
  git checkout tags/RELEASE-TAG

You can now commit your changes in git and will have the new
module under version control.
