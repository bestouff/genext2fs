#!/bin/sh

# Use this script if you need to regenerate the digest values
# in test.sh, or if you don't care about digests and you just
# want to see some fsck results. Should be run as root.

# Each test here creates a test image, verifies it and prints
# the command line for use in test.sh for regression testing.

set -e

. ./test-gen.lib

test_mnt=t_mnt

test_common () {
	/sbin/e2fsck -fn $test_img || fail
	mkdir $test_mnt
	mount -t ext2 -o ro,loop $test_img $test_mnt || fail
}

test_cleanup () {
	umount $test_mnt
	rmdir $test_mnt
	rm -f fout lsout
}

fail () {
	echo FAIL
	test_cleanup
	gen_cleanup
	exit 1
}

pass () {
	cmd=$1
	shift
	md5=`calc_digest`
	echo PASS
	echo $cmd $md5 $@
	test_cleanup
	gen_cleanup
}

# dtest_mount - Exercise the -d option of genext2fs.
dtest_mount () {
	size=$3
	dgen $@
	test_common
	test -f $test_mnt/file.$size || fail
	test $size = "`ls -al $test_mnt | \
	               grep file.$size | \
	               awk '{print $5}'`" || fail
	pass dtest $@
}

# ftest_mount - Exercise the -D option of genext2fs.
ftest_mount () {
	fname=$3
	fgen $@
	test_common
	test -d $test_mnt/dev || fail
	# Exclude those devices that have interpolated
	# minor numbers, as being too hard to match.
	egrep -v "(hda|hdb|tty|loop|ram|ubda)" $fname | \
		grep '^[^	#]*	[bc]' | \
		awk '{print $1,$4,$5,$6","$7}'| \
		sort -d -k3.6 > fout
	ls -aln $test_mnt/dev | \
		egrep -v "(hda|hdb|tty|loop|ram|ubda)" | \
		grep ^[bc] | \
		awk '{ print "/dev/"$10,$3,$4,$5$6}' | \
		sort -d -k3.6 > lsout
	diff fout lsout || fail
	pass ftest $@
}

# ltest_mount - Exercise the -d option of genext2fs, with symlinks.
ltest_mount () {
	lgen $@
	test_common
	cd $test_mnt
	ls 1* > ../lsout
	cat symlink > ../fout
	cd ..
	test -s fout || fail
	diff fout lsout || fail
	pass ltest $@
}

dtest_mount 4096 1024 0
dtest_mount 2048 2048 0
dtest_mount 1024 4096 0
dtest_mount 8193 1024 0
dtest_mount 8194 1024 0
dtest_mount 8193 4096 0
dtest_mount 8194 2048 0
dtest_mount 4096 1024 1
dtest_mount 1024 4096 1
dtest_mount 4096 1024 12288
dtest_mount 4096 1024 274432
dtest_mount 9000 1024 8388608
dtest_mount 4500 2048 8388608
dtest_mount 2250 4096 8388608
dtest_mount 20000 1024 16777216
dtest_mount 10000 2048 16777216
ftest_mount 4096 default device_table.txt
ltest_mount 200 1024 123456789
ltest_mount 200 1024 1234567890
ltest_mount 200 4096 12345678901
