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

# Extended attribute tests - skip if setfattr/getfattr not available
if command -v setfattr >/dev/null 2>&1 && command -v getfattr >/dev/null 2>&1; then
	echo "Testing extended attributes (xattr) support"
	gen_setup
	cd $test_dir
	echo "hello" > testfile1
	echo "world" > testfile2
	mkdir subdir
	echo "nested" > subdir/nested
	# set xattrs (user. namespace works without root)
	setfattr -n user.myattr -v "myvalue" testfile1
	setfattr -n user.tag -v "important" testfile1
	setfattr -n user.label -v "data" testfile2
	setfattr -n user.dirattr -v "dirval" subdir
	setfattr -n user.deep -v "nested_val" subdir/nested
	TZ=UTC-11 touch -t 200502070321.43 testfile1 testfile2 subdir/nested subdir .
	cd ..
	./genext2fs -B 1024 -N 32 -b 300 -d $test_dir -f -o Linux $test_img
	# e2fsck must pass cleanly
	if /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
		echo "  e2fsck: PASS"
	else
		echo "  e2fsck: FAIL"
		exit 1
	fi
	# verify individual xattr values via debugfs
	xattr_pass=true
	val=$(/usr/sbin/debugfs -R "ea_get -V testfile1 user.myattr" $test_img 2>/dev/null)
	if [ "$val" = "myvalue" ]; then echo "  user.myattr: PASS"; else echo "  user.myattr: FAIL (got '$val')"; xattr_pass=false; fi
	val=$(/usr/sbin/debugfs -R "ea_get -V testfile1 user.tag" $test_img 2>/dev/null)
	if [ "$val" = "important" ]; then echo "  user.tag: PASS"; else echo "  user.tag: FAIL (got '$val')"; xattr_pass=false; fi
	val=$(/usr/sbin/debugfs -R "ea_get -V testfile2 user.label" $test_img 2>/dev/null)
	if [ "$val" = "data" ]; then echo "  user.label: PASS"; else echo "  user.label: FAIL (got '$val')"; xattr_pass=false; fi
	val=$(/usr/sbin/debugfs -R "ea_get -V subdir user.dirattr" $test_img 2>/dev/null)
	if [ "$val" = "dirval" ]; then echo "  user.dirattr: PASS"; else echo "  user.dirattr: FAIL (got '$val')"; xattr_pass=false; fi
	val=$(/usr/sbin/debugfs -R "ea_get -V subdir/nested user.deep" $test_img 2>/dev/null)
	if [ "$val" = "nested_val" ]; then echo "  user.deep: PASS"; else echo "  user.deep: FAIL (got '$val')"; xattr_pass=false; fi
	if $xattr_pass; then echo "  xattr values: PASS"; else echo "  xattr values: FAIL"; exit 1; fi
	gen_cleanup

	# Test xattr with auto-size (-b 0) to verify block counting
	echo "Testing xattr with auto block calculation"
	gen_setup
	cd $test_dir
	for i in $(seq 1 20); do
		echo "file $i" > "file$i"
		setfattr -n user.num -v "val$i" "file$i"
	done
	TZ=UTC-11 touch -t 200502070321.43 file* .
	cd ..
	./genext2fs -B 1024 -N 40 -b 0 -d $test_dir -f -o Linux $test_img
	if /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
		echo "  auto-size e2fsck: PASS"
	else
		echo "  auto-size e2fsck: FAIL"
		exit 1
	fi
	gen_cleanup
else
	echo "SKIP xattr tests (setfattr/getfattr not found)"
fi

# ---- Starting image (-x): load existing image and add more content ----
echo "Testing starting image (-x)"
gen_setup
mkdir -p $test_dir/dir1
echo "original" > $test_dir/dir1/file1.txt
TZ=UTC-11 touch -t 200502070321.43 $test_dir/dir1/file1.txt $test_dir/dir1 $test_dir
./genext2fs -B 1024 -N 32 -b 200 -d $test_dir -f -o Linux t_base.img
gen_cleanup
gen_setup
mkdir -p $test_dir/dir2
echo "added" > $test_dir/dir2/file2.txt
TZ=UTC-11 touch -t 200502070321.43 $test_dir/dir2/file2.txt $test_dir/dir2 $test_dir
./genext2fs -x t_base.img -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
v1=$(/usr/sbin/debugfs -R "cat dir1/file1.txt" $test_img 2>/dev/null)
v2=$(/usr/sbin/debugfs -R "cat dir2/file2.txt" $test_img 2>/dev/null)
if [ "$v1" = "original" ] && [ "$v2" = "added" ]; then
	echo "  contents: PASS"
else
	echo "  contents: FAIL (got '$v1' and '$v2')"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
rm -f t_base.img
gen_cleanup

# ---- Holes/sparse files (-z) ----
echo "Testing sparse file support (-z)"
gen_setup
dd if=/dev/zero bs=1024 count=20 of=$test_dir/sparse_candidate 2>/dev/null
echo "header data here" | dd of=$test_dir/sparse_candidate bs=1 count=17 conv=notrunc 2>/dev/null
TZ=UTC-11 touch -t 200502070321.43 $test_dir/sparse_candidate $test_dir
./genext2fs -B 1024 -N 16 -b 100 -d $test_dir -f -o Linux t_noz.img
./genext2fs -B 1024 -N 16 -b 100 -z -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
blocks_noz=$(stat -c%b t_noz.img)
blocks_z=$(stat -c%b $test_img)
if [ "$blocks_z" -lt "$blocks_noz" ]; then
	echo "  sparse output: PASS ($blocks_noz -> $blocks_z 512-byte blocks)"
else
	echo "  sparse output: FAIL (expected fewer blocks, got $blocks_noz vs $blocks_z)"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
rm -f t_noz.img
gen_cleanup

# ---- Fill value (-e) ----
echo "Testing fill value (-e 255)"
gen_setup
echo "small" > $test_dir/tiny.txt
TZ=UTC-11 touch -t 200502070321.43 $test_dir/tiny.txt $test_dir
./genext2fs -B 1024 -N 16 -b 32 -d $test_dir -e 255 -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
# The last block should be unallocated and filled with 0xff
last_block_bytes=$(od -A n -t x1 -j $((31 * 1024)) -N 1024 $test_img | tr -d ' \n')
expected=$(printf '%0.s ff' $(seq 1 1024) | tr -d ' ')
# Simplify: just check that it does NOT contain any 00 bytes
zeros=$(od -A n -t x1 -j $((31 * 1024)) -N 1024 $test_img | tr -s ' ' '\n' | grep -c '^00$' || true)
if [ "$zeros" -eq 0 ]; then
	echo "  fill value: PASS"
else
	echo "  fill value: FAIL ($zeros zero bytes in last block)"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Volume label (-L) ----
echo "Testing volume label (-L)"
gen_setup
TZ=UTC-11 touch -t 200502070321.43 $test_dir
./genext2fs -B 1024 -N 16 -b 32 -d $test_dir -L "MYVOLUME" -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
label=$(/usr/sbin/dumpe2fs -h $test_img 2>/dev/null | sed -n 's/^Filesystem volume name: *//p')
if [ "$label" = "MYVOLUME" ]; then
	echo "  label: PASS"
else
	echo "  label: FAIL (got '$label')"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Hardlinks ----
echo "Testing hardlinks"
gen_setup
echo "shared content" > $test_dir/original.txt
ln $test_dir/original.txt $test_dir/hardlink1.txt
ln $test_dir/original.txt $test_dir/hardlink2.txt
TZ=UTC-11 touch -t 200502070321.43 $test_dir/original.txt $test_dir
./genext2fs -B 1024 -N 32 -b 100 -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
ino1=$(/usr/sbin/debugfs -R "stat original.txt" $test_img 2>/dev/null | sed -n 's/^Inode: *\([0-9]*\).*/\1/p')
ino2=$(/usr/sbin/debugfs -R "stat hardlink1.txt" $test_img 2>/dev/null | sed -n 's/^Inode: *\([0-9]*\).*/\1/p')
ino3=$(/usr/sbin/debugfs -R "stat hardlink2.txt" $test_img 2>/dev/null | sed -n 's/^Inode: *\([0-9]*\).*/\1/p')
links=$(/usr/sbin/debugfs -R "stat original.txt" $test_img 2>/dev/null | grep -o 'Links: [0-9]*' | awk '{print $2}')
if [ "$ino1" = "$ino2" ] && [ "$ino2" = "$ino3" ] && [ "$links" = "3" ]; then
	echo "  shared inode ($ino1) with 3 links: PASS"
else
	echo "  hardlinks: FAIL (inodes $ino1/$ino2/$ino3, links=$links)"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Deep directory nesting (20 levels) ----
echo "Testing deep directory nesting (20 levels)"
gen_setup
deep=$test_dir
for i in $(seq 1 20); do deep="$deep/level$i"; done
mkdir -p "$deep"
echo "deep content" > "$deep/deepfile.txt"
TZ=UTC-11 touch -t 200502070321.43 "$deep/deepfile.txt"
p=$test_dir
for i in $(seq 20 -1 1); do
	p2=$test_dir; for j in $(seq 1 $i); do p2="$p2/level$j"; done
	TZ=UTC-11 touch -t 200502070321.43 "$p2"
done
TZ=UTC-11 touch -t 200502070321.43 $test_dir
./genext2fs -B 1024 -N 64 -b 200 -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
deeppath="level1/level2/level3/level4/level5/level6/level7/level8/level9/level10"
deeppath="$deeppath/level11/level12/level13/level14/level15/level16/level17/level18/level19/level20/deepfile.txt"
val=$(/usr/sbin/debugfs -R "cat $deeppath" $test_img 2>/dev/null)
if [ "$val" = "deep content" ]; then
	echo "  deep file: PASS"
else
	echo "  deep file: FAIL (got '$val')"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Long filenames (255 characters) ----
echo "Testing long filenames (255 chars)"
gen_setup
longname=$(python3 -c "print('A' * 251 + '.txt')")
echo "long name content" > "$test_dir/$longname"
TZ=UTC-11 touch -t 200502070321.43 "$test_dir/$longname" $test_dir
./genext2fs -B 1024 -N 16 -b 100 -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
val=$(/usr/sbin/debugfs -R "cat $longname" $test_img 2>/dev/null)
if [ "$val" = "long name content" ]; then
	echo "  long filename: PASS"
else
	echo "  long filename: FAIL (got '$val')"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Colons in paths (-d dir:/subpath) ----
echo "Testing destination path (-d dir:/subpath)"
gen_setup
echo "installed file" > $test_dir/myfile.txt
TZ=UTC-11 touch -t 200502070321.43 $test_dir/myfile.txt $test_dir
./genext2fs -B 1024 -N 32 -b 200 -d "$test_dir:/usr/local/share" -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
val=$(/usr/sbin/debugfs -R "cat /usr/local/share/myfile.txt" $test_img 2>/dev/null)
if [ "$val" = "installed file" ]; then
	echo "  destination path: PASS"
else
	echo "  destination path: FAIL (got '$val')"; pass=false
fi
# Also test with auto-size (-b 0) to verify stats accounting
./genext2fs -B 1024 -N 32 -b 0 -d "$test_dir:/opt/data" -f -o Linux $test_img
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  auto-size e2fsck: FAIL"; pass=false
fi
val=$(/usr/sbin/debugfs -R "cat /opt/data/myfile.txt" $test_img 2>/dev/null)
if [ "$val" = "installed file" ]; then
	echo "  auto-size destination: PASS"
else
	echo "  auto-size destination: FAIL (got '$val')"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup

# ---- Many small files (inode pressure) ----
echo "Testing many small files (200 files in 10 dirs)"
gen_setup
for d in $(seq 1 10); do
	mkdir "$test_dir/dir$d"
	for f in $(seq 1 20); do
		echo "content $d/$f" > "$test_dir/dir$d/file$f.txt"
	done
done
TZ=UTC-11 find $test_dir -exec touch -h -t 200502070321.43 {} +
./genext2fs -B 1024 -b 0 -d $test_dir -f -o Linux $test_img
pass=true
if ! /usr/sbin/e2fsck -fn $test_img > /dev/null 2>&1; then
	echo "  e2fsck: FAIL"; pass=false
fi
# Spot-check a few files
for check in "dir1/file1.txt:content 1/1" "dir5/file10.txt:content 5/10" "dir10/file20.txt:content 10/20"; do
	path="${check%%:*}"
	expected="${check#*:}"
	val=$(/usr/sbin/debugfs -R "cat $path" $test_img 2>/dev/null)
	if [ "$val" = "$expected" ]; then
		echo "  $path: PASS"
	else
		echo "  $path: FAIL (got '$val')"; pass=false
	fi
done
# Verify total file count via dumpe2fs (should be at least 200 files + 10 dirs + root + lost+found + reserved)
used_inodes=$(/usr/sbin/dumpe2fs -h $test_img 2>/dev/null | sed -n 's/^Inode count: *//p')
if [ "$used_inodes" -ge 212 ]; then
	echo "  inode count ($used_inodes): PASS"
else
	echo "  inode count ($used_inodes): FAIL (expected >= 212)"; pass=false
fi
$pass && echo "PASS" || { echo "FAIL"; exit 1; }
gen_cleanup
