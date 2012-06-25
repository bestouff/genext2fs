#!/bin/sh

# This script generates a variety of filesystems and checks that they
# are identical to ones that are known to be mountable, and pass fsck
# and various other sanity checks.

# Passing these tests is preferable to passing test-mount.sh because
# this script doesn't require root, and because passing these tests
# guarantees byte-for-byte agreement with other builds, ports,
# architectures, times of day etc.

set -e

. ./test-gen.lib

# md5cmp - Calculate MD5 digest and compare it to an expected value.
# Usage: md5cmp expected-digest
md5cmp () {
	checksum=$1
	md5=`calc_digest`
	if [ x$md5 = x$checksum ] ; then
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
	size=$1; blocks=$2; blocksz=$3; checksum=$4
	echo Testing with file of size $size
	dgen $size $blocks $blocksz
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

dtest 0 4096 1024 4a25d1109fa03f8519e89a5fc365580f
dtest 0 2048 2048 a2c5c3b014510456320373e72a7d4b3f
dtest 0 1024 4096 4af8e2915523207f780e22ff1de29633
dtest 0 8193 1024 537400c0b8f868895ced27af64dc703b
dtest 0 8194 1024 f6f6a6124104c8ff5ca2e3cfd8b3f396
dtest 0 8193 4096 63f60f06d4a4858404a09d071b688a7d
dtest 0 8194 2048 851d7cac136b44e1bf461fef27180a1b
dtest 1 4096 1024 518b75cd6651ae864babf3b12a70866b
dtest 1 1024 4096 87b13fb5e0f31e2f13faea02f829730b
dtest 12288 4096 1024 548ce3a3bf7d57e350a11c0c274b7795
dtest 274432 4096 1024 d6041295c924de32af49ad35a3b37c0d
dtest 8388608 9000 1024 2dcd1c07084e616433b43043c1309cc6
dtest 8388608 4500 2048 227d38c05d5860dc039812eed603f20f
dtest 8388608 2250 4096 627c09439a5ed6194900f12f5591d28e
dtest 16777216 20000 1024 84dbb9949b3c1c9d7f3237d3cdaa86b5
dtest 16777216 10000 2048 a4ab80a62c0fd09a3be023c77e0307b1
ftest device_table.txt 4096 9108433a817035cb5306e8217d5e634b
