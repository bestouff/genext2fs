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
	md5=`md5sum ext2.img | cut -d" " -f1`
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

dtest 0 4096 63900fd66725bbe66bfa926acee760fe
dtest 0 8193 bc71dd5ac7ccf497d7ffb0f60f4be92b
dtest 0 8194 739fabefde5873ea5ae531fc76377a5e
dtest 1 4096 224a3b8468b7978dc82032445a5629d2
dtest 12288 4096 79f19ace9683a0b7192d95a17bf95f34
dtest 274432 4096 a873dbcd6efa6837e1767e495f98d1f5
dtest 8388608 9000 f1ae7f8c37418a8e902fbcfc24595fec
dtest 16777216 20000 367e6f6de52c6c73ad487ab0588c9064
ftest device_table.txt 4096 d04dbf1f872b99254876416da07e83c9
