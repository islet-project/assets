#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 Google LLC
#
# This scripts takes one single commit and imports it into quilt.
# It uses git-format for extraction, but normalizes the output as much as
# possible to get consistent patch files.

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <commit>"
    exit 1
fi

commit=$1

name=$(git show -s --format=%f $commit)
printf -v patch_file "%s.patch" $name

# check for and work around collisions
index=1
while [[ -f "patches/$patch_file" ]]; do
  ((index++))
  printf -v patch_file "%s-%d.patch" $name $index
done

# write the actual patch file and update the series
git format-patch $commit -1 --no-signoff    \
                            --keep-subject  \
                            --zero-commit   \
                            --no-signature  \
                            --stdout > patches/$patch_file

echo $patch_file >> patches/series

