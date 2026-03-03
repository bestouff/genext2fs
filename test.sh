#!/bin/sh

# This script generates a variety of filesystems and checks that they
# are identical to ones that are known to be mountable, and pass fsck
# and various other sanity checks.

# Passing these tests is preferable to passing test-mount.sh because
# this script doesn't require root, and because passing these tests
# guarantees byte-for-byte agreement with other builds, ports,
# architectures, times of day etc.

set -e

origin_dir="$(dirname "$(realpath "$0")")"
. $origin_dir/test-gen.lib

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

dtest_s () {
	expected_digest=$1
	shift
	reversed=$1;
	shift

	gen_setup
	ROOTDIR=$(mktemp -d)
	TZ=UTC-11 touch -t 200502070321.43 $ROOTDIR/a $ROOTDIR/b $ROOTDIR/c $ROOTDIR

	disorderfs --sort-dirents=yes --reverse-dirents="$reversed" "$ROOTDIR" "$test_dir"
	dgen_raw $@
	fusermount -u $test_dir
	md5cmp $expected_digest
	gen_cleanup
}

ftest () {
	expected_digest=$1
	shift
	fgen y $@
	md5cmp $expected_digest
	fgen n $@
	md5cmp $expected_digest
	gen_cleanup
}

ltest () {
	expected_digest=$1
	shift
	lgen y $@
	md5cmp $expected_digest
	lgen n $@
	md5cmp $expected_digest
	gen_cleanup
}

atest() {
	expected_digest=$1
	shift
	agen y $@
	md5cmp $expected_digest
	agen n $@
	md5cmp $expected_digest
	gen_cleanup
}

# NB: always use test-mount.sh to regenerate these digests, that is,
# replace the following lines with the output of
# sudo sh test-mount.sh|grep test

dtest d28c461a408de69eef908045a11207ec 4096 1024 0
dtest 8501cc97857245d16aaf4b8a06345fc2 2048 2048 0
dtest 1fc39a40fa808aa974aa3065f1862067 1024 4096 0
dtest a6bc1b4937db12944a9ae496da1ba65c 8193 1024 0
dtest 21d8675cb8b8873540fa3d911dddca7e 8194 1024 0
dtest 346ecb90c1ca7a1d6e5fbe57383e1055 8193 4096 0
dtest b2672c5deb9cdf598db2c11118b5c91a 8194 2048 0
dtest 1e950ef4f2719cd2d4f70de504c88244 4096 1024 1
dtest 4a47abd795e5282a1762e1fa8b22a432 1024 4096 1
dtest 3e229c70850d2388c172124b05e93ddc 4096 1024 12288
dtest 495cda3636eb0f42aceb9d63bc34690b 4096 1024 274432
dtest fe792a70e336ed1e7a29aee7ecdeb0ab 9000 1024 8388608
dtest 2375e7344dfa1550583ea25d30bc02bf 4500 2048 8388608
dtest 4dedea56398027fe3267ebc602b9a6b6 2250 4096 8388608
dtest c41835904c45320731aab685436ba8f6 20000 1024 16777216
dtest 662529e81e6106288aec9516bcefe109 10000 2048 16777216
ftest 3db16dd57bd15c1c80dd6bc900411c58 4096 default device_table.txt
ltest c21b5a3cad405197e821ba7143b0ea9b 200 1024 123456789 device_table_link.txt
ltest 18b04b4bea2f7ebf315a116be5b92589 200 1024 1234567890 device_table_link.txt
ltest 8aaed693b75dbdf77607f376d661027a 200 4096 12345678901 device_table_link.txt
atest 994ca42d3179c88263af777bedec0c55 200 1024 H4sIAAAAAAAAA+3WTW6DMBAF4Fn3FD6B8fj3PKAqahQSSwSk9vY1uKssGiJliFretzECJAYeY1s3JM4UKYRlLG7H5ZhdTIHZGevK+ZTYkgrypRFN17EdlKIh5/G3++5d/6N004qbA47er8/fWVduV2aLD7D7/A85C88Ba/ufA/sQIhk25VdA/2+h5t+1gx4/pd7vfv+Hm/ytmfNH/8vr+ql7e3UR8DK6uUx9L/uMtev/3P8p+KX/oyHlZMuqntX/9T34Z9yk9Gco8//xkGWf8Uj+Mbpl/Y+JVJQtq9r5/K+bj3Z474+Xk9wG4JH86/rvyzxAirfYnOw+/+vXWTb+uv9PaV3+JfiSv/WOlJVPf/f5AwAAAAAAAAAAAMD/9A0cPbO/ACgAAA==
dtest_s c2745eb185e738821acfcc4c9c92e355 no 200 1024 0
dtest_s c2745eb185e738821acfcc4c9c92e355 yes 200 1024 0
