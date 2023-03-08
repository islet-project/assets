# Updating the Android Mainline Patch Series

## Initial Setup

1. Create a Git controlled repo containing Linux Kernel source

   `$ git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git`

2. Create a symbolic link to the patches subdirectory of android-mainline

   `$ ln -s <path to common-patches repo>/android-mainline/ patches`

## Realign with `aosp/android-mainline`

1. Run the helper script with no arguments and follow the prompts

   `$ <path to common-patches repo>/scripts/update_android_mainline.sh`

   On first run, the script will reset to an upstream base and reapply the Quilt series

   Once aligned additional/missing patches/merges (if any) will be applied

   The script should complete in a single invocation

   In the case of merge conflicts or other issues that may arise, an opportunity will be
   given to either use another terminal or to temporarily background the script (Ctrl+z)
   in order to rectify them - once complete the application can be brought to the
   foreground (`fg`) and continued by pressing Return

## Refresh the Quilt series with local changes before realigning with `aosp/android-mainline`

1. Run the helper script with `--refresh`

   `$ <path to common-patches repo>/scripts/update_android_mainline.sh --refresh`

## Only refresh the Quilt series with local changes

1. Run the helper script with `--only-refresh`

   `$ <path to common-patches repo>/scripts/update_android_mainline.sh --only-refresh`

   This will take the current history and create a Quilt file from each commit above the base
