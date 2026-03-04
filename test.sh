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
	if ! command -v disorderfs >/dev/null 2>&1; then
		echo "SKIP (disorderfs not found)"
		return
	fi
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
	rm -rf $ROOTDIR
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

dtest 06bd978bfbc5dc10bec8f20405569131 4096 1024 0
dtest 5916ef8b54803e793ed239514c94f4e5 2048 2048 0
dtest 8b335fe50767f404a661470f83def85c 1024 4096 0
dtest fb63ff0aabb08eb8046e9465ed351aa7 8193 1024 0
dtest ed28e26628bd03f9bc3b118023f53ee7 8194 1024 0
dtest 2787260fe879db1062b23d4e4dceb3a7 8193 4096 0
dtest 868328932d2e10f67b223dc5e1889f02 8194 2048 0
dtest 9d0574181f7dbb22a7c30a5d8d7532ca 4096 1024 1
dtest a1d30accb00acd75b671785c09ec9c9b 1024 4096 1
dtest 0fd7245c9cdfe950ccecac8cc1d2419b 4096 1024 12288
dtest 900a2d44239af4a90bb68f3e1b79ecb6 4096 1024 274432
dtest afa33d58144a62e2600220d737037829 9000 1024 8388608
dtest 2437152585b6accfcaf50789a0876ab1 4500 2048 8388608
dtest b250f478f03e70d66435052909374e5f 2250 4096 8388608
dtest 8b455e8bc41ba222a09797fe7da69a20 20000 1024 16777216
dtest 4f1396c857daaa6771aaf7379da6c362 10000 2048 16777216
ftest 7f6505911e756781f55fda6a4a4e1e0e 4096 default device_table.txt
ltest 1ea1aea6f0741a2d0b3b188246e48f7e 200 1024 123456789 device_table_link.txt
ltest 846ad72dcc8dfcac7161cc43c9b66bc8 200 1024 1234567890 device_table_link.txt
ltest ff25a9a4d997f309125f1f042100701b 200 4096 12345678901 device_table_link.txt
atest 7c4a6bc8b3b74804d5788a133f9cc862 200 1024 H4sIAAAAAAAAA+3WTW6DMBAF4Fn3FD6B8fj3PKAqahQSSwSk9vY1uKssGiJliFretzECJAYeY1s3JM4UKYRlLG7H5ZhdTIHZGevK+ZTYkgrypRFN17EdlKIh5/G3++5d/6N004qbA47er8/fWVduV2aLD7D7/A85C88Ba/ufA/sQIhk25VdA/2+h5t+1gx4/pd7vfv+Hm/ytmfNH/8vr+ql7e3UR8DK6uUx9L/uMtev/3P8p+KX/oyHlZMuqntX/9T34Z9yk9Gco8//xkGWf8Uj+Mbpl/Y+JVJQtq9r5/K+bj3Z474+Xk9wG4JH86/rvyzxAirfYnOw+/+vXWTb+uv9PaV3+JfiSv/WOlJVPf/f5AwAAAAAAAAAAAMD/9A0cPbO/ACgAAA==
dtest_s 500e2e280ed87569e4ee4df4af617fa7 no 200 1024 0
dtest_s 500e2e280ed87569e4ee4df4af617fa7 yes 200 1024 0
