#!/usr/bin/env perl

#
# Copyright (c) 2018, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

#
# Arg0: Name of the C file to generate.
# Arg1: Name of the header file to generate.
# Arg2: XML file that contains the list of test suites.
# Arg3: Text file listing the files to skip. Takes precedence over Arg2 file.
#

my $TESTLIST_SRC_FILENAME = $ARGV[0];
my $TESTLIST_HDR_FILENAME = $ARGV[1];
my $XML_TEST_FILENAME     = $ARGV[2];
my $SKIPPED_LIST_FILENAME = $ARGV[3];

use strict;
use warnings;
use File::Temp;
use XML::LibXML;

# Create the source & header files
open FILE_SRC, ">", $TESTLIST_SRC_FILENAME or die $!;
open FILE_HDR, ">", $TESTLIST_HDR_FILENAME or die $!;

#
# Open the test list
#
my $doc;
my $testsuite_elem;
my $failure_elem;

if (-e $XML_TEST_FILENAME) {
  my $parser = XML::LibXML->new();
  $doc = $parser->parse_file($XML_TEST_FILENAME);
} else {
  exit 1
}

# We assume if there is a root then it is a 'testsuites' element
my $root = $doc->documentElement();
my @all_testcases   = $root->findnodes("//testcase");
my @all_testsuites  = $root->findnodes("//testsuite");


# Check the validity of the XML file:
# - A testsuite name must be unique.
# - A testsuite name must not contain a '/' character.
# - All test cases belonging to a given testsuite must have unique names.
for my $testsuite (@all_testsuites) {
  my $testsuite_name = $testsuite->getAttribute('name');
  if ($testsuite_name =~ /\//) {
      print "ERROR: $XML_TEST_FILENAME: Invalid test suite name '$testsuite_name'.\n";
      print "ERROR: $XML_TEST_FILENAME: Test suite names can't include a '/' character.\n";
      exit 1;
  }
  my @testsuites = $root->findnodes("//testsuite[\@name='$testsuite_name']");
  if (@testsuites != 1) {
    print "ERROR: $XML_TEST_FILENAME: Can't have 2 test suites named '$testsuite_name'.\n";
    exit 1;
  }

  my @testcases_of_testsuite = $testsuite->findnodes("testcase");
  for my $testcase (@testcases_of_testsuite) {
    my $testcase_name = $testcase->getAttribute('name');
    my @testcases = $testsuite->findnodes("testcase[\@name='$testcase_name']");
    if (@testcases != 1) {
      print "ERROR: $XML_TEST_FILENAME: Can't have 2 tests named '$testsuite_name/$testcase_name'.\n";
      exit 1;
    }
  }
}

#
# Get the list of tests to skip.
# For each test to skip, find it in the XML tree and remove its node.
#
if (($SKIPPED_LIST_FILENAME) && (open SKIPPED_FILE, "<", $SKIPPED_LIST_FILENAME)) {
  my @lines = <SKIPPED_FILE>;
  close $SKIPPED_LIST_FILENAME;

  # Remove the newlines from the end of each line.
  chomp @lines;

  my $line_no = 0;
  my $testsuite_name;
  my $testcase_name;
  my $index = 0;

  for my $line (@lines) {
    ++$line_no;

    # Skip empty lines.
    if ($line =~ /^ *$/) { next; }
    # Skip comments.
    if ($line =~ /^#/) { next; }

    ($testsuite_name, $testcase_name) = split('/', $line);

    my @testsuites = $root->findnodes("//testsuite[\@name=\"$testsuite_name\"]");
    if (!@testsuites) {
      print "WARNING: $SKIPPED_LIST_FILENAME:$line_no: Test suite '$testsuite_name' doesn't exist or has already been deleted.\n";
      next;
    }

    if (!defined $testcase_name) {
      print "INFO: Testsuite '$testsuite_name' will be skipped.\n";
      $testsuites[0]->unbindNode();
      next;
    }

    my @testcases = $testsuites[0]->findnodes("testcase[\@name=\"$testcase_name\"]");
    if (!@testcases) {
      print "WARNING: $SKIPPED_LIST_FILENAME:$line_no: Test case '$testsuite_name/$testcase_name' doesn't exist or has already been deleted.\n";
      next;
    }

    print "INFO: Testcase '$testsuite_name/$testcase_name' will be skipped.\n";
    $testcases[0]->unbindNode();
  }
}


@all_testcases = $root->findnodes("//testcase");

#
# Generate the test function prototypes
#
my $testcase_count = 0;

print FILE_SRC "#include \"tftf.h\"\n\n";

for my $testcase (@all_testcases) {
  my $testcase_function = $testcase->getAttribute('function');
  $testcase_count++;
  print FILE_SRC "test_result_t $testcase_function(void);\n";
}

#
# Generate the header file.
#
print FILE_HDR "#ifndef __TEST_LIST_H__\n";
print FILE_HDR "#define __TEST_LIST_H__\n\n";
print FILE_HDR "#define TESTCASE_RESULT_COUNT $testcase_count\n\n";
print FILE_HDR "#endif\n";

#
# Generate the lists of testcases
#
my $testsuite_index = 0;
my $testcase_index = 0;
@all_testsuites  = $root->findnodes("//testsuite");
for my $testsuite (@all_testsuites) {
  my $testsuite_name = $testsuite->getAttribute('name');
  my @testcases = $testsuite->findnodes("//testsuite[\@name='$testsuite_name']//testcase");

  print FILE_SRC "\nconst test_case_t testcases_${testsuite_index}[] = {\n";

  for my $testcase (@testcases) {
    my $testcase_name = $testcase->getAttribute('name');
    my $testcase_description = $testcase->getAttribute('description');
    my $testcase_function = $testcase->getAttribute('function');

    if (!defined($testcase_description)) { $testcase_description = ""; }

    print FILE_SRC "  { $testcase_index, \"$testcase_name\", \"$testcase_description\", $testcase_function },\n";

    $testcase_index++;
  }
  print FILE_SRC "  { 0, NULL, NULL, NULL }\n";
  print FILE_SRC "};\n\n";
  $testsuite_index++;
}

#
# Generate the lists of testsuites
#
$testsuite_index = 0;
print FILE_SRC "const test_suite_t testsuites[] = {\n";
for my $testsuite (@all_testsuites) {
  my $testsuite_name = $testsuite->getAttribute('name');
  my $testsuite_description = $testsuite->getAttribute('description');
  print FILE_SRC "  { \"$testsuite_name\", \"$testsuite_description\", testcases_${testsuite_index} },\n";
  $testsuite_index++;
}
print FILE_SRC "  { NULL, NULL, NULL }\n";
print FILE_SRC "};\n";

