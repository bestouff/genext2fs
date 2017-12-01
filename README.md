GENEXT2FS
=========

genext2fs - ext2 filesystem generator for embedded systems

SYNOPSIS
--------

`genext2fs [ options ] [ output-image ]`

DESCRIPTION
-----------

**genext2fs** generates an ext2 filesystem as a normal (non-root) user.
It does not require you to mount the image file to copy files on it, nor
does it require that you become the superuser to make device nodes.

The filesystem image is created in the file *output-image*. If not
specified, it is sent to stdout.

By default, the maximum number of inodes in the filesystem is the
minimum number required to accommodate the initial contents. In this
way, a minimal filesystem (typically read-only) can be created with
minimal free inodes. If required, free inodes can be added by passing
the relevant options. The filesystem image size in blocks can be
minimised by trial and error.

OPTIONS
-------

**-x, --starting-image image**

Use this image as a starting point.

**-d, --root directory[:path]**

Add the given directory and contents at a particular path (by default
the root).

**-D, --devtable spec-file[:path]**

Use **spec-file** to specify inodes to be added, at the given path (by
default the root), including files, directories and special files like
devices. If the specified files are already present in the image, their
ownership and permission modes will be adjusted accordingly (this can
only occur when the -D option appears after the options that create the
specified files). Furthermore, you can use a single table entry to
create many devices with a range of minor numbers (see examples below).
All specified inodes receive the mtime of **spec-file** itself.

**-b, --size-in-blocks blocks**

Size of the image in blocks.

**-B, --block-size bytes**

Size of a filesystem block in bytes.

**-N, --number-of-inodes inodes**

Maximum number of inodes.

**-L, --volume-label name**

Set the volume label for the filesystem.

**-i, --bytes-per-inode ratio**

Used to calculate the maximum number of inodes from the available
blocks.

**-m, --reserved-percentage N**

Number of reserved blocks as a percentage of size. Reserving 0 blocks
will prevent creation of the `lost+found` directory.

**-o, --creator-os name**

Value for creator OS field in superblock.

**-g, --block-map path**

Generate a block map file for this path.

**-e, --fill-value value**

Fill unallocated blocks with value.

**-z, --allow-holes**

Make files with holes.

**-f, --faketime**

Use a timestamp of 0 for inode and filesystem creation, instead of the
present. Useful for testing.

**-q, --squash**

Squash permissions and owners (same as -P -U).

**-U, --squash-uids**

Squash ownership of inodes added using the -d option, making them all
owned by root:root.

**-P, --squash-perms**

Squash permissions of inodes added using the -d option. Analogous to
`umask 077`.

**-v, --verbose**

Print resulting filesystem structure.

**-V, --version**

Print genext2fs version.

**-h, --help**

Display help.

EXAMPLES
--------

**genext2fs -b 1440 -d src /dev/fd0**

All files in the *src* directory will be written to **/dev/fd0** as a
new ext2 filesystem image. You can then mount the floppy as usual.

**genext2fs -b 1024 -d src -D devicetable.txt flashdisk.img**

This example builds a filesystem from all the files in *src*, then
device nodes are created based on the contents of the file
*devicetable.txt*. Entries in the device table take the form of:

<name> <type> <mode> <uid> <gid> <major> <minor> <start> <inc> <count>

where name is the file name and type can be one of:

              f    A regular file
              d    Directory
              c    Character special device file
              b    Block special device file
              p    Fifo (named pipe)

uid is the user id for the target file, gid is the group id for the
target file. The rest of the entries (major, minor, etc) apply only to
device special files.

An example device file follows:

              # name    type mode uid gid major minor start inc count

              /dev      d    755  0    0    -    -    -    -    -
              /dev/mem  c    640  0    0    1    1    0    0    -
              /dev/tty  c    666  0    0    5    0    0    0    -
              /dev/tty  c    666  0    0    4    0    0    1    6
              /dev/loop b    640  0    0    7    0    0    1    2
              /dev/hda  b    640  0    0    3    0    0    0    -
              /dev/hda  b    640  0    0    3    1    1    1    16
              /dev/log  s    666  0    0    -    -    -    -    -

This device table creates the /dev directory, a character device node
`/dev/mem` (major 1, minor 1), and also creates `/dev/tty`, `/dev/tty[0-5]`,
`/dev/loop[0-1]`, `/dev/hda`, `/dev/hda1` to `/dev/hda15` and `/dev/log` socket.
