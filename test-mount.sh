#!/bin/sh
set -e

cleanup () {
	umount mnt 2>/dev/null || true
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
	if [ x$size == x0 ]; then
		> file.$1
	else
		dd if=/dev/zero of=file.$1 bs=$size count=1 2>/dev/null
	fi
	cd ..
	./genext2fs -b $blocks -d test ext2.img 
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
	echo PASSED
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
	# Exclude those devices that have interpolated
	# minor numbers, as being too hard to match.
	egrep -v "(hda|hdb|tty|loop|ram|ubda)" $fname | \
		grep '^[^	#]*	[bc]' | \
		awk '{print $1,$4,$5,$6","$7}'| \
		sort -d -k3.6 > fout
	ls -aln mnt/dev | \
		egrep -v "(hda|hdb|tty|loop|ram|ubda)" | \
		grep ^[bc] | \
		awk '{ print "/dev/"$10,$3,$4,$5$6}' | \
		sort -d -k3.6 > lsout
	if ! diff fout lsout ; then
		echo FAILED
		cleanup
		exit 1
	fi
	echo PASSED
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

ftest device_table.txt 4096 

exit 0
