#!/bin/sh
set -e

# dtest - Uses the -d directory option of genext2fs
# Creates an image with a file of given size and verifies it
# Usage: dtest file-size number-of-blocks correct-checksum
dtest () {
	size=$1; blocks=$2; checksum=$3
	echo "Testing with file of size $size "
	mkdir -p test
	cd test
	dd if=/dev/zero of=file.$1 bs=1 count=$size 
	cd ..
	./genext2fs -b $blocks -d test ext2.img 
	md5=`md5sum ext2.img | cut -d" " -f1`
	rm -rf ext2.img test
	if [ $md5 == $checksum ] ; then
		echo PASSED
	else
		echo FAILED
		exit 1
	fi
}

# ftest - Uses the -f spec-file option of genext2fs
# Creates an image with the devices mentioned in the given spec 
# file and verifies it
# Usage: ftest spec-file number-of-blocks correct-checksum
ftest () {
	fname=$1; blocks=$2; checksum=$3
	echo "Testing with devices file $fname"
	./genext2fs -b $blocks -f $fname ext2.img
	md5=`md5sum ext2.img | cut -d" " -f1`
	rm -rf ext2.img
	if [ $md5 == $checksum ] ; then
		echo PASSED
	else
		echo FAILED
		exit 1
	fi
}

dtest 0 4096 491a43ab93c2e5c186c9f1f72d88e5c5
dtest 0 8193 6289224f0b7f151994479ba156c43505
dtest 0 8194 3272c43c25e8d0c3768935861a643a65
dtest 1 4096 5ee24486d33af88c63080b09d8cadfb5
dtest 12288 4096 494498364defdc27b2770d1f9c1e3387
dtest 274432 4096 65c4bd8d30bf563fa5434119a12abff1
dtest 8388608 9000 9a49b0461ee236b7fd7c452fb6a1f2dc
dtest 16777216 20000 91e16429c901b68d30f783263f0611b7

ftest dev.txt 4096 921ee9343b0759e16ad8d979d7dd16ec
exit 0
