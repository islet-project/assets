#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2021 Google LLC

#set -x # Uncomment to enable debugging
set -e

trap "tput smam; exit 1" SIGINT SIGTERM

REFRESH=""
ONLYREFRESH=""

function usage()
{
    echo "Usage: $(basename $0): [--refresh|--refresh-patches|--only-refresh|--only-refresh-patches]"
    exit 1
}

function print_red()
{
    echo -e "\e[01;31m$@\e[0m"
}

function print_blue()
{
    echo -e "\e[01;34m$@\e[0m"
}

function is_merge_commit()
{
    local commit=${1}

    if [ $(git show --no-patch --format="%p" ${commit} | wc -w) -gt 1 ]; then
        return 0
    else
        return 1
    fi
}

function read_series_file()
{
    base_commit=$(cat patches/series | grep "# Applies onto" | awk '{print $5}')  # Last Mainline commit we processed
    last_commit=$(cat patches/series | grep "# Matches " | awk '{print $4}')      # Last ${target} commit we processed
    target=aosp/$(cat patches/series | grep "# Matches " | awk '{print $3}')      # Remote branch (aosp/android-mainline)
    status=$(cat patches/series | grep "# Status: " | awk '{print $3}')      # Common patches branch status

    UNTESTABLE=""
    if [ "${status}" == "Untested" ]; then
	UNTESTABLE=true
    fi

    if [ ! -z ${LOCAL} ]; then
        target=$(echo ${target} | sed 's!aosp/!!')

        if ! git branch | grep -q " ${target}$"; then
            print_red "Local specified, but branch '${target}' does not exist locally"
            exit 1
        fi
    fi
}

function sanity_check()
{
    if [[ ! -L patches || ! -d patches || ! -e patches/series ]]; then
        print_red "'patches' symlink to 'common-patches' repo is missing"
        exit 1
    fi
}

function check_and_update_remotes()
{
    # Check public 'stable' / 'mainline' remotes are present

    if ! git remote | grep -q "^stable$"; then
        print_blue "Couldn't find repo 'stable' - adding it now"
        git remote add stable git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
    fi

    if ! git remote | grep -q "^mainline$"; then
        print_blue "Couldn't find repo 'mainline' - adding it now"
        git remote add mainline git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
    fi

    # ... and up-to-date

    print_blue "Fetching from Mainline"
    git fetch mainline
    print_blue "Fetching from Stable"
    git fetch stable

    # For `repo checkout` managed repositories (see: https://source.android.com/setup/build/building-kernels)

    if which repo > /dev/null && repo > /dev/null 2>&1; then
        repo sync -n && git fetch --all
        return 0
    fi

    # For non-`repo`/manually managed repositories

    if ! git remote | grep -q "^aosp$"; then
        print_blue "Couldn't find repo 'aosp' - adding it now"
        git remote add aosp https://android.googlesource.com/kernel/common.git
    fi

    print_blue "Fetching from AOSP"
    git fetch aosp
    echo
}

function preamble()
{
    check_and_update_remotes

    if [ -d .git ]; then
        DOTGIT=.git
    elif [ -f .git ]; then
        DOTGIT=$(cat .git | sed 's/gitdir: //')
    else
        print_red "Internal error: .git appears to be neither a regular file or a directory"
    fi

    if [ -z "${UNTESTABLE}" ]; then
	# Ensure repo is in expected state
	if ! git diff ${last_commit} --exit-code > /dev/null; then
            print_blue "Tree is out of sync with 'common-patches' - resetting\n"
            git reset --hard ${base_commit}
            git quiltimport
	fi
    fi

    LOCAL_LAST_COMMIT=$(git log -n1 --format=%H)    # HEAD

    # Obtain list of patches to be processed
    pick_list=$(git rev-list --first-parent ${last_commit}..${target} --reverse)

    # Exit successfully if there's nothing to be done
    if [ -z "$pick_list" ] ; then
        print_blue "Great news - we're up-to-date\n"
        exit 0
    fi

    print_blue "Patches to process from ${target}:"
    tput rmam    # Don't wrap
    git --no-pager log --first-parent --format="%C(auto)  %h %s" ${last_commit}..${target} --reverse
    tput smam    # Reset wrapping
    echo
}

function rebase_no_fail
{
    local args=${@}

    git rebase ${args} && true
    while [[ -f ${DOTGIT}/REBASE_HEAD ]]; do
        print_red "\nRebase needs attention\n"
        print_blue "Either use another shell or Ctrl+z this one to fix, then \`fg\` and hit return"
        read
        if git rebase --show-current-patch 2>&1 | grep -q "No rebase in progress"; then
            print_blue "Rebase no longer in progress - assuming the issue was rectified"
        else
            git rebase --continue && true
        fi
    done
    echo
}

function commit_to_common_patches()
{
    local commit=${1}

    print_blue "Committing all changes into 'common-patches' repo\n"
    git -C patches/ add .
    git -C patches commit -s -F- <<EOF
$target: update series${commit_additional_text}

up to $(git --no-pager show -s --pretty='format:%h ("%s")' ${commit})
EOF
    echo
}

function create_series_and_patch_files()
{
    up_to=${1}
    branch=${target#*/}

    if [ ! -z "${UNTESTABLE}" ]; then
	status="Untested"
    else
	status="Tested"
    fi

    rm patches/*.patch

    echo "# Android Series" > patches/series
    cat <<EOT > patches/series
#
# $branch patches
#
# Applies onto upstream $(git log -1 --format=%h $base_commit) Linux $(git describe $base_commit)
# Matches $branch $(git show -s --pretty='format:%h ("%s")' $up_to)
# Status: ${status}
#
EOT

    files=()  # Keep track of used *.patch file names to deal with collisions

    print_blue "Updating 'series' file\n"
    for sha1 in $(git rev-list ${base_commit}.. --reverse); do

        # Create the *.patch filename
        patchfilename=$(git show -s --format=%f ${sha1})
        printf -v patch_file "%s.patch" ${patchfilename}

        # Identify and work around collisions (<name>-<index>.patch)
        index=1
        while [[ " ${files[@]} " =~ " ${patch_file} " ]]; do
            ((index++))
            printf -v patch_file "%s-%d.patch" ${patchfilename} ${index}
        done
        files+=(${patch_file})

        # Write the actual patch file and update the series file
        git format-patch ${sha1} -1 \
            --no-signoff    \
            --keep-subject  \
            --zero-commit   \
            --no-signature  \
            --stdout > patches/${patch_file}

        # Remove 'index' changes - they are not required and clog up the diff
        sed -i '/index [0-9a-f]\{12,\}\.\.[0-9a-f]\{12,\} [0-9]\{6\}/d' patches/$patch_file
        sed -i '/index [0-9a-f]\{12,\}\.\.[0-9a-f]\{12,\}$/d' patches/$patch_file

        print_blue "Adding ${patch_file}"
        echo ${patch_file} >> patches/series
    done
    echo
}

commit_patches () {
    local commit=${1}

    if [ -z "${UNTESTABLE}" ]; then
	while ! git --no-pager diff ${commit} --exit-code > /dev/null; do
            print_red "Failed to commit patches: Unexpected diff found:\n"
            git --no-pager diff ${commit}
            echo

            print_blue "Either use another shell or Ctrl+z this one to fix, then \`fg\` and hit return"
            read
	done
    else
	print_red "WARNING: Committing to Common Patches without testing\n"
	sleep 2
    fi

    if ! is_merge_commit ${commit}; then
        print_blue "Entering interactive rebase to rearrange (press return to continue or Ctrl+c to exit)"
        read

        rebase_no_fail -i ${base_commit}
    fi

    create_series_and_patch_files ${commit}

    commit_to_common_patches ${commit}

    # Read new (base, last and target) variables
    read_series_file

    LOCAL_LAST_COMMIT=$(git --no-pager log -n1 --format=%H)    # HEAD
}

function process_merge_commit()
{
    local commit=${1}

    # Here ${commit} is the Android merge commit and ${parent_commit} is the upstream one
    parent_commit=$(git show --no-patch --format="%p" ${commit} | awk '{print $2}')
    parent_commit_desc=$(git describe $parent_commit)

    UNTESTABLE=""
    if ! git show ${parent_commit} --no-patch --format=%an | grep -q "Linus Torvalds"; then
	print_red "WARNING: Skipping non-Torvalds merge\n"
	UNTESTABLE=true
	return
    fi
    base_commit=${parent_commit}

    print_blue "Found merge (new base) commit - rebasing onto upstream commit ${parent_commit_desc}\n"

    rebase_no_fail ${base_commit}

    commit_additional_text=" (rebase onto ${parent_commit_desc})"
    commit_patches ${commit}
}

function process_normal_commit()
{
    local commit=${1}

    print_blue "Applying: "

    if ! git cherry-pick ${commit} --exit-code; then
        print_red "Failed to check-pick commit: $(git log --oneline -n1 ${commit})\n"

        print_blue "Either use another shell or Ctrl+z this one to fix, then \`fg\` and hit return"
        read
    fi
    echo
}

function process_pick_list()
{
    local commit=${1}

    # Handle merge commit - ${target} has a new merge-base
    if is_merge_commit ${commit}; then

        # Apply any regular patches already processed before this merge
        if [ "$(git log ${LOCAL_LAST_COMMIT}..HEAD)" != "" ]; then
            commit_additional_text=""
            commit_patches ${commit}~1    # This specifies the last 'normal commit' before the 'merge commit'
        fi

        process_merge_commit ${commit}
        return
    fi

    process_normal_commit ${commit}
}

function sync_with_target()
{
    preamble    # Start-up checks and initialisation

    if git diff ${last_commit}..${target} --exit-code > /dev/null; then
        oneline=$(git show -s --pretty='format:%h ("%s")' ${target})

        print_red "No meaningful work to be done - was all the work reverted?\n"

        print_blue "Processing anyway - don't forget to remove them (press return to continue)"
        read
    fi

    for commit in ${pick_list}; do
        process_pick_list ${commit}
    done

    # Apply any regular patches already processed
    if [ "$(git log ${LOCAL_LAST_COMMIT}..HEAD)" != "" ]; then
        commit_additional_text=""
        commit_patches ${commit}    # Commit up to last 'normal commit' processed
    fi

    print_blue "That's it, all done!\n"
    exit 0
}

function start()
{
    sanity_check

    read_series_file

    if [ ! -z ${REFRESH} ]; then
        local commit=$(git --no-pager log -n1 --format=%H ${last_commit})    # HEAD
        print_blue "Refreshing Quilt patches\n"
        create_series_and_patch_files ${commit}

        print_red "Quilt patches refreshed - please check and commit the result\n"

        if [ ! -z ${ONLYREFRESH} ]; then
            exit 0
        else
            print_blue "Once result has been committed, press return to continue syncing or Ctrl+x to exit\n"
            read
        fi
    fi

    sync_with_target
}

while [ $# -gt 0 ]; do
    case "$1" in
    --local)
        LOCAL=true
        ;;
    --refresh|--refresh-patches)
        REFRESH=true
        ;;
    --only-refresh|--only-refresh-patches)
        REFRESH=true
        ONLYREFRESH=true
        ;;
    *)
        echo "Unknown argument: ${1}"
        usage
    esac
    shift
done

start
