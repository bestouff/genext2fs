bin_PROGRAMS = genext2fs
genext2fs_SOURCES = genext2fs.c
man_MANS = genext2fs.8
EXTRA_DIST = $(man_MANS)

# test genext2fs by creating the image and comparing checksums
test: all
	sh ./test.sh

# test genext2fs by actually mounting the created image.
test-mount: all
	sudo sh ./test-mount.sh
