#!/bin/sh
set -e

# dtest - Uses the -d directory option of genext2fs
# Creates an image with a file of given size and verifies it
# Usage: dtest file-size number-of-blocks correct-checksum
dtest () {
	size=$1; blocks=$2; checksum=$3
	echo "Testing with file of size $size "
	rm -fr test
	mkdir -p test
	cd test
	if [ x$size == x0 ]; then
		> file.$1
	else
		dd if=/dev/zero of=file.$1 bs=$size count=1 2>/dev/null
	fi
	touch -t 200502070321.43 file.$1 .
	cd ..
	./genext2fs -b $blocks -d test -t 1107706903 ext2.img 
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
	./genext2fs -b $blocks -f $fname -t 0 ext2.img
	md5=`md5sum ext2.img | cut -d" " -f1`
	rm -rf ext2.img
	if [ $md5 == $checksum ] ; then
		echo PASSED
	else
		echo FAILED
		exit 1
	fi
}

dtest 0 4096 799c5b5747a81d547af083bb904c268a
dtest 0 8193 19dfadad2271da9c22509941616f6648
dtest 0 8194 551716fcddeb8547e6b42dfa9ad338d5
dtest 1 4096 596ae7c65ea2e76dcd6ce47d31858396
dtest 12288 4096 b6e4a1fc13141ac642d03d5f71595ab5
dtest 274432 4096 e5d74b0a2547b328b81ed61267f88827
dtest 8388608 9000 8043fdf8f1888d2547e49c2de6d6bb10
dtest 16777216 20000 d0f9492a8eccf1aa7119c7b5f854cc6c

ftest device_table.txt 4096 d04dbf1f872b99254876416da07e83c9

exit 0
