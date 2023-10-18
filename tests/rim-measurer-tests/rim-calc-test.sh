#!/usr/bin/bash

for test in `ls realm[0-9]*.sh | sort -g`
do
	expected=`awk '/RIM/ {print $3}' $test`
	result=`./$test 2>/dev/null | awk '/RIM/ {print $2}'`
	if [ "$result" != "$expected" ]
	then
		echo -e "Test $test [\e[31mFAILED\e[0m], expected RIM $expected got $result"
	else
		echo -e "Test $test [\e[32mPASSED\e[0m]"
	fi
done

