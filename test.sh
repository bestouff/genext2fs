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

dtest 0 4096 baa6525d94a8e58b0bc38af7f6ca913b
dtest 0 8193 98a1696b085289ddec8ea3b28e15adbe
dtest 0 8194 464546452a531af4d46733edb902c5d2
dtest 1 4096 36dcf89dbc9ca415ed20746d81985289
dtest 12288 4096 1cb3cb993f559b11c644c8fe71110390
dtest 274432 4096 7745a68a08762f87223d1f6e97848d89
dtest 8388608 9000 333252327a182006372ba64a0ce7f8fb
dtest 16777216 20000 55a7e7878b1c43b0e051f2e418838698

ftest device_table.txt 4096 d04dbf1f872b99254876416da07e83c9

exit 0
