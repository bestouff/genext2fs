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

# NB: to regenerate these values, always use test-mount.sh

dtest 0 4096 f956d2bc3b31bcd8291e160858295f29
dtest 0 8193 d1e5ab6b04f0680a0978185c12c70255
dtest 0 8194 4855a55d0cbdc44584634df49ebd5711
dtest 1 4096 be5e36047e1c873295e9dc1af01ec563
dtest 12288 4096 809909d899395e1793368edf2a2f41bc
dtest 274432 4096 32b103730267d445ff497b18bd359826
dtest 8388608 9000 e0e5ea15bced10ab486d8135584b5d8e
dtest 16777216 20000 fdf636eb905ab4dc1bf76dce5ac5d209
ftest device_table.txt 4096 b621bb4217cc047a6d7df75234e8f66a
