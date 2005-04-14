#!/bin/sh

# This script generates a variety of filesystems and checks that they
# are identical to ones that are known to be mountable, and pass fsck
# and various other sanity checks.

# Passing these tests is preferable to passing test-mount.sh because
# this script doesn't require root, and because passing these tests
# guarantees byte-for-byte agreement with other builds, ports,
# architectures, times of day etc.

set -e

. test-gen.lib

# md5cmp - Calculate MD5 digest and compare it to an expected value.
# Usage: md5cmp expected-digest
md5cmp () {
	checksum=$1
	md5=`calc_digest`
	if [ $md5 == $checksum ] ; then
		echo PASSED
	else
		echo FAILED
		exit 1
	fi
}

# dtest - Exercises the -d directory option of genext2fs
# Creates an image with a file of given size and verifies it
# Usage: dtest file-size number-of-blocks correct-checksum
dtest () {
	size=$1; blocks=$2; checksum=$3
	echo Testing with file of size $size
	dgen $size $blocks
	md5cmp $checksum
	gen_cleanup
}

# ftest - Exercises the -f spec-file option of genext2fs
# Creates an image with the devices mentioned in the given spec 
# file and verifies it
# Usage: ftest spec-file number-of-blocks correct-checksum
ftest () {
	fname=$1; blocks=$2; checksum=$3
	echo Testing with devices file $fname
	fgen $fname $blocks
	md5cmp $checksum
	gen_cleanup
}

# NB: to regenerate these values, always use test-mount.sh, that is,
# replace the following lines with the output of
# sudo sh test-mount.sh|grep test

dtest 0 4096 1072592267c1bd31462ed6c614fcbcd7
dtest 0 8193 16380ac3b418ec809cb2c072ba4a41ba
dtest 0 8194 5656e8f429b88d3dfda90b3b9b4fa05d
dtest 1 4096 01d8115d8fe38e92953a66af22f7c4fe
dtest 12288 4096 17baa803f86767922c7f3409c11c73d5
dtest 274432 4096 f5df115a66778d2ffe47917a7e6404e4
dtest 8388608 9000 55739b414c858f19e7db72f227c0dbb4
dtest 16777216 20000 4cebadd02a2f10c9df4db807c8edaadf
ftest device_table.txt 4096 3524efcf5d005a6f0b1fa5c12675f5c3
