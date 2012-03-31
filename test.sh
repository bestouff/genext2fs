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

dtest 0 4096 1024 3bc6424b8fcd51a0de34ee59d91d5f16
dtest 0 2048 2048 230afa16496df019878cc2370c661cdc
dtest 0 1024 4096 ebff5eeb38b70f3f1cd081e60eb44561
dtest 0 8193 1024 f174804f6b433b552706cbbfc60c416d
dtest 0 8194 1024 4855a55d0cbdc44584634df49ebd5711
dtest 0 8193 4096 c493679698418ec7e6552005e2d2a6d8
dtest 0 8194 2048 ec13f328fa7543563f35f494bddc059c
dtest 1 4096 1024 09c569b6bfb45222c729c42d04d5451f
dtest 1 1024 4096 d318a326fdc907810ae9e6b0a20e9b06
dtest 12288 4096 1024 61febcbfbf32024ef99103fcdc282c39
dtest 274432 4096 1024 0c517803552c55c1806e4220b0a0164f
dtest 8388608 9000 1024 e0e5ea15bced10ab486d8135584b5d8e
dtest 8388608 4500 2048 39f4d537a72f5053fd6891721c59680d
dtest 8388608 2250 4096 1d697fa4bc2cfffe02ac91edfadc40bf
dtest 16777216 20000 1024 fdf636eb905ab4dc1bf76dce5ac5d209
dtest 16777216 10000 2048 f9824a81ea5e74fdf469c097927c292b
ftest device_table.txt 4096 a0af06d944b11d2902dfd705484c64cc
