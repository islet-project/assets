#!/bin/bash

set -e
set -x

pushd $(dirname $0)/../android-mainline

for tag in NOUPSTREAM SUBMIT ONHOLD REVERT REVISIT; do
    for f in `git status --short | grep "?? $tag-" | sed 's/?? //'`; do
	orig=`echo ${f} | sed "s/$tag-//"`

	mv ${f} ${orig}
	git mv ${orig} ${f}
    done
done

popd
