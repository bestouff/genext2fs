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
	chmod 777 file.$1
	touch -t 200502070321.43 file.$1 .
	cd ..
	./genext2fs -b $blocks -d test -t 1107706903 -q ext2.img 
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

dtest 0 4096 63900fd66725bbe66bfa926acee760fe
dtest 0 8193 bc71dd5ac7ccf497d7ffb0f60f4be92b
dtest 0 8194 739fabefde5873ea5ae531fc76377a5e
dtest 1 4096 224a3b8468b7978dc82032445a5629d2
dtest 12288 4096 79f19ace9683a0b7192d95a17bf95f34
dtest 274432 4096 a873dbcd6efa6837e1767e495f98d1f5
dtest 8388608 9000 f1ae7f8c37418a8e902fbcfc24595fec
dtest 16777216 20000 367e6f6de52c6c73ad487ab0588c9064

ftest device_table.txt 4096 d04dbf1f872b99254876416da07e83c9
