#!/bin/sh
set -e

cleanup () {
	set +e
	umount mnt 2>/dev/null
	rm -rf mnt ext2.img lsout fout test 2>/dev/null
}

# dtest - Uses the -d directory option of genext2fs
# Creates an image with a file of given size and verifies it
# Usage: dtest file-size number-of-blocks 
dtest () {
	size=$1; blocks=$2;fname=$size 
	echo "Testing with file of size $size "
	mkdir -p test
	cd test
	dd if=/dev/zero of=file.$1 bs=1 count=$size 
	cd ..
	./genext2fs -b $blocks -d test ext2.img 
	md5=`md5sum ext2.img | cut -f1 -d " "`
	if ! /sbin/e2fsck -fn ext2.img ; then
		echo "fsck failed"
		echo FAILED
		cleanup
		exit 1
	fi
	mkdir -p mnt
	if ! mount -t ext2 -o loop ext2.img mnt; then
		echo FAILED
		cleanup
		exit 1
	fi
	if (! [ -f mnt/file.$fname ]) || \
			[ $fname != "`ls -al mnt | grep file.$fname |awk '{print $5}'`" ] ; then
		echo FAILED
		cleanup
		exit 1
	fi
	echo PASSED "(md5 checksum for the image: $md5)"
	cleanup
}

# ftest - Uses the -f spec-file option of genext2fs
# Creates an image with the devices mentioned in the given spec 
# file and verifies it
# Usage: ftest spec-file number-of-blocks 
ftest () {
	fname=$1; blocks=$2; 
	echo "Testing with devices file $fname"
	./genext2fs -b $blocks -f $fname ext2.img
	md5=`md5sum ext2.img | cut -f 1 -d " "`
	if ! /sbin/e2fsck -fn ext2.img ; then
		echo "fsck failed"
		echo FAILED
		cleanup
		exit 1
	fi
	mkdir -p mnt
	if ! mount -t ext2 -o loop ext2.img mnt; then
		echo FAILED
		cleanup
		exit 1
	fi
	if ! [ -d mnt/dev ] ; then
		echo FAILED
		cleanup
		exit 1
	fi
	cat dev.txt | grep ^[bc] | \
		awk '{print $1substr($1,2)substr($1,2),$2,$3}'| \
			sort -d -k3.6 > fout
	ls -al mnt/dev | grep ^[bc] | \
		awk '{ print $1,$5$6,"/dev/"$10}' | \
			sort -d -k3.6 > lsout
	if ! diff fout lsout ; then
		echo FAILED
		cleanup
		exit 1
	fi
	echo PASSED "(md5 checksum for the image: $md5)"
	cleanup
}

dtest 0 4096 
dtest 0 8193
dtest 0 8194
dtest 1 4096 
dtest 12288 4096 
dtest 274432 4096 
dtest 8388608 9000 
dtest 16777216 20000

ftest dev.txt 4096 

exit 0
