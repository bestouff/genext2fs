bin_PROGRAMS = genext2fs
genext2fs_SOURCES = genext2fs.c
genext2fs_LDADD = $(ARCHIVE_LIBS)
man_MANS = genext2fs.8
EXTRA_DIST = $(man_MANS) test-gen.lib test-mount.sh test.sh device_table.txt device_table_link.txt cache.h list.h m4/ac_func_scanf_can_malloc.m4 m4/ac_func_snprintf.m4
TESTS = test.sh
