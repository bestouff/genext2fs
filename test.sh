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

md5cmp () {
	md5=`calc_digest`
	if [ x$md5 = x$1 ] ; then
		echo PASS
	else
		echo FAIL
		exit 1
	fi
}

dtest () {
	expected_digest=$1
	shift
	dgen $@
	md5cmp $expected_digest
	gen_cleanup
}

ftest () {
	expected_digest=$1
	shift
	fgen $@
	md5cmp $expected_digest
	gen_cleanup
}

ltest () {
	expected_digest=$1
	shift
	lgen $@
	md5cmp $expected_digest
	gen_cleanup
}

# NB: always use test-mount.sh to regenerate these digests, that is,
# replace the following lines with the output of
# sudo sh test-mount.sh|grep test

dtest 4a25d1109fa03f8519e89a5fc365580f 4096 1024 0
dtest a2c5c3b014510456320373e72a7d4b3f 2048 2048 0
dtest 4af8e2915523207f780e22ff1de29633 1024 4096 0
dtest 537400c0b8f868895ced27af64dc703b 8193 1024 0
dtest f6f6a6124104c8ff5ca2e3cfd8b3f396 8194 1024 0
dtest 63f60f06d4a4858404a09d071b688a7d 8193 4096 0
dtest 851d7cac136b44e1bf461fef27180a1b 8194 2048 0
dtest 518b75cd6651ae864babf3b12a70866b 4096 1024 1
dtest 87b13fb5e0f31e2f13faea02f829730b 1024 4096 1
dtest 548ce3a3bf7d57e350a11c0c274b7795 4096 1024 12288
dtest d6041295c924de32af49ad35a3b37c0d 4096 1024 274432
dtest 2dcd1c07084e616433b43043c1309cc6 9000 1024 8388608
dtest 227d38c05d5860dc039812eed603f20f 4500 2048 8388608
dtest 627c09439a5ed6194900f12f5591d28e 2250 4096 8388608
dtest 84dbb9949b3c1c9d7f3237d3cdaa86b5 20000 1024 16777216
dtest a4ab80a62c0fd09a3be023c77e0307b1 10000 2048 16777216
ftest 9108433a817035cb5306e8217d5e634b 4096 default device_table.txt
ltest dd1a554d9253c84f9d3fff1f4669b4c2 200 1024 123456789
ltest 8c9f6f0c9c15b42e32bef60af88ac6ff 200 1024 1234567890
ltest 9d8be8d71a7743b8c675c83f50d6eff0 200 4096 12345678901
