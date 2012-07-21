/* vi: set sw=8 ts=8: */
// genext2fs.c
//
// ext2 filesystem generator for embedded systems
// Copyright (C) 2000 Xavier Bestel <xavier.bestel@free.fr>
//
// Please direct support requests to genext2fs-devel@lists.sourceforge.net
//
// 'du' portions taken from coreutils/du.c in busybox:
//	Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
//	Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
//	Copyright (C) 2002  Edward Betts <edward@debian.org>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; version
// 2 of the License.
//
// Changes:
// 	 3 Jun 2000	Initial release
// 	 6 Jun 2000	Bugfix: fs size multiple of 8
// 			Bugfix: fill blocks with inodes
// 	14 Jun 2000	Bugfix: bad chdir() with -d option
// 			Bugfix: removed size=8n constraint
// 			Changed -d file to -f file
// 			Added -e option
// 	22 Jun 2000	Changed types for 64bits archs
// 	24 Jun 2000	Added endianness swap
// 			Bugfix: bad dir name lookup
// 	03 Aug 2000	Bugfix: ind. blocks endian swap
// 	09 Aug 2000	Bugfix: symlinks endian swap
// 	01 Sep 2000	Bugfix: getopt returns int, not char	proski@gnu.org
// 	10 Sep 2000	Bugfix: device nodes endianness		xavier.gueguen@col.bsf.alcatel.fr
// 			Bugfix: getcwd values for Solaris	xavier.gueguen@col.bsf.alcatel.fr
// 			Bugfix: ANSI scanf for non-GNU C	xavier.gueguen@col.bsf.alcatel.fr
// 	28 Jun 2001	Bugfix: getcwd differs for Solaris/GNU	mike@sowbug.com
// 	 8 Mar 2002	Bugfix: endianness swap of x-indirects
// 	23 Mar 2002	Bugfix: test for IFCHR or IFBLK was flawed
// 	10 Oct 2002	Added comments,makefile targets,	vsundar@ixiacom.com    
// 			endianess swap assert check.  
// 			Copyright (C) 2002 Ixia communications
// 	12 Oct 2002	Added support for triple indirection	vsundar@ixiacom.com
// 			Copyright (C) 2002 Ixia communications
// 	14 Oct 2002	Added support for groups		vsundar@ixiacom.com
// 			Copyright (C) 2002 Ixia communications
// 	 5 Jan 2003	Bugfixes: reserved inodes should be set vsundar@usc.edu
// 			only in the first group; directory names
// 			need to be null padded at the end; and 
// 			number of blocks per group should be a 
// 			multiple of 8. Updated md5 values. 
// 	 6 Jan 2003	Erik Andersen <andersee@debian.org> added
// 			mkfs.jffs2 compatible device table support,
// 			along with -q, -P, -U


/*
 * Allow fseeko/off_t to be 64-bit offsets to allow filesystems and
 * individual files >2GB.
 */
#define _FILE_OFFSET_BITS 64

#include <config.h>
#include <stdio.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if MAJOR_IN_MKDEV
# include <sys/mkdev.h>
#elif MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# if HAVE_STDLIB_H
#  include <stdlib.h>
# endif
# if HAVE_STDDEF_H
#  include <stddef.h>
# endif
#endif

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif

#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if HAVE_LIBGEN_H
# include <libgen.h>
#endif

#include <stdarg.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_GETOPT_H
# include <getopt.h>
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include "cache.h"

struct stats {
	unsigned long nblocks;
	unsigned long ninodes;
};

// block size

static int blocksize = 1024;

#define SUPERBLOCK_OFFSET	1024
#define SUPERBLOCK_SIZE		1024

#define BLOCKSIZE         blocksize
#define BLOCKS_PER_GROUP  8192
#define INODES_PER_GROUP  8192
/* Percentage of blocks that are reserved.*/
#define RESERVED_BLOCKS       5/100
#define MAX_RESERVED_BLOCKS  25/100

/* The default value for s_creator_os. */
#if defined(__linux__)    &&    defined(EXT2_OS_LINUX)
#define CREATOR_OS EXT2_OS_LINUX
#define CREATOR_OS_NAME "linux"
#else
#if defined(__GNU__)     &&     defined(EXT2_OS_HURD)
#define CREATOR_OS EXT2_OS_HURD
#define CREATOR_OS_NAME "hurd"
#else
#if defined(__FreeBSD__) &&     defined(EXT2_OS_FREEBSD)
#define CREATOR_OS EXT2_OS_FREEBSD
#define CREATOR_OS_NAME "freebsd"
#else
#if defined(LITES)         &&   defined(EXT2_OS_LITES)
#define CREATOR_OS EXT2_OS_LITES
#define CREATOR_OS_NAME "lites"
#else
#define CREATOR_OS EXT2_OS_LINUX /* by default */
#define CREATOR_OS_NAME "linux"
#endif /* defined(LITES) && defined(EXT2_OS_LITES) */
#endif /* defined(__FreeBSD__) && defined(EXT2_OS_FREEBSD) */
#endif /* defined(__GNU__)     && defined(EXT2_OS_HURD) */
#endif /* defined(__linux__)   && defined(EXT2_OS_LINUX) */


// inode block size (why is it != BLOCKSIZE ?!?)
/* The field i_blocks in the ext2 inode stores the number of data blocks
   but in terms of 512 bytes. That is what INODE_BLOCKSIZE represents.
   INOBLK is the number of such blocks in an actual disk block            */

#define INODE_BLOCKSIZE   512
#define INOBLK            (BLOCKSIZE / INODE_BLOCKSIZE)

// reserved inodes

#define EXT2_BAD_INO         1     // Bad blocks inode
#define EXT2_ROOT_INO        2     // Root inode
#define EXT2_ACL_IDX_INO     3     // ACL inode
#define EXT2_ACL_DATA_INO    4     // ACL inode
#define EXT2_BOOT_LOADER_INO 5     // Boot loader inode
#define EXT2_UNDEL_DIR_INO   6     // Undelete directory inode
#define EXT2_FIRST_INO       11    // First non reserved inode

// magic number for ext2

#define EXT2_MAGIC_NUMBER  0xEF53


// direct/indirect block addresses

#define EXT2_NDIR_BLOCKS   11                    // direct blocks
#define EXT2_IND_BLOCK     12                    // indirect block
#define EXT2_DIND_BLOCK    13                    // double indirect block
#define EXT2_TIND_BLOCK    14                    // triple indirect block
#define EXT2_INIT_BLOCK    0xFFFFFFFF            // just initialized (not really a block address)

// codes for operating systems

#define EXT2_OS_LINUX           0
#define EXT2_OS_HURD            1
#define EXT2_OS_MASIX           2
#define EXT2_OS_FREEBSD         3
#define EXT2_OS_LITES           4

// end of a block walk

#define WALK_END           0xFFFFFFFE

// file modes

#define FM_IFMT    0170000	// format mask
#define FM_IFSOCK  0140000	// socket
#define FM_IFLNK   0120000	// symbolic link
#define FM_IFREG   0100000	// regular file

#define FM_IFBLK   0060000	// block device
#define FM_IFDIR   0040000	// directory
#define FM_IFCHR   0020000	// character device
#define FM_IFIFO   0010000	// fifo

#define FM_IMASK   0007777	// *all* perms mask for everything below

#define FM_ISUID   0004000	// SUID
#define FM_ISGID   0002000	// SGID
#define FM_ISVTX   0001000	// sticky bit

#define FM_IRWXU   0000700	// entire "user" mask
#define FM_IRUSR   0000400	// read
#define FM_IWUSR   0000200	// write
#define FM_IXUSR   0000100	// execute

#define FM_IRWXG   0000070	// entire "group" mask
#define FM_IRGRP   0000040	// read
#define FM_IWGRP   0000020	// write
#define FM_IXGRP   0000010	// execute

#define FM_IRWXO   0000007	// entire "other" mask
#define FM_IROTH   0000004	// read
#define FM_IWOTH   0000002	// write
#define FM_IXOTH   0000001	// execute

/* Defines for accessing group details */

// Number of groups in the filesystem
#define GRP_NBGROUPS(fs) \
	(((fs)->sb->s_blocks_count - fs->sb->s_first_data_block + \
	  (fs)->sb->s_blocks_per_group - 1) / (fs)->sb->s_blocks_per_group)

// Get group block bitmap (bbm) given the group number
#define GRP_GET_GROUP_BBM(fs,grp,bi) (get_blk((fs),(grp)->bg_block_bitmap,(bi)))
#define GRP_PUT_GROUP_BBM(bi) ( put_blk((bi)) )

// Get group inode bitmap (ibm) given the group number
#define GRP_GET_GROUP_IBM(fs,grp,bi) (get_blk((fs), (grp)->bg_inode_bitmap,(bi)))
#define GRP_PUT_GROUP_IBM(bi) ( put_blk((bi)) )

// Given an inode number find the group it belongs to
#define GRP_GROUP_OF_INODE(fs,nod) ( ((nod)-1) / (fs)->sb->s_inodes_per_group)

//Given an inode number get the inode bitmap that covers it
#define GRP_GET_INODE_BITMAP(fs,nod,bi,gi)				\
	( GRP_GET_GROUP_IBM((fs),get_gd(fs,GRP_GROUP_OF_INODE((fs),(nod)),gi),bi) )
#define GRP_PUT_INODE_BITMAP(bi,gi)		\
	( GRP_PUT_GROUP_IBM((bi)),put_gd((gi)) )

//Given an inode number find its offset within the inode bitmap that covers it
#define GRP_IBM_OFFSET(fs,nod) \
	( (nod) - GRP_GROUP_OF_INODE((fs),(nod))*(fs)->sb->s_inodes_per_group )

// Given a block number find the group it belongs to
#define GRP_GROUP_OF_BLOCK(fs,blk) ( ((blk)-1) / (fs)->sb->s_blocks_per_group)
	
//Given a block number get/put the block bitmap that covers it
#define GRP_GET_BLOCK_BITMAP(fs,blk,bi,gi)				\
	( GRP_GET_GROUP_BBM((fs),get_gd(fs,GRP_GROUP_OF_BLOCK((fs),(blk)),(gi)),(bi)) )
#define GRP_PUT_BLOCK_BITMAP(bi,gi)		\
	( GRP_PUT_GROUP_BBM((bi)),put_gd((gi)) )

//Given a block number find its offset within the block bitmap that covers it
#define GRP_BBM_OFFSET(fs,blk) \
	( (blk) - GRP_GROUP_OF_BLOCK((fs),(blk))*(fs)->sb->s_blocks_per_group )


// used types

typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;


// the GNU C library has a wonderful scanf("%as", string) which will
// allocate the string with the right size, good to avoid buffer
// overruns. the following macros use it if available or use a
// hacky workaround
// moreover it will define a snprintf() like a sprintf(), i.e.
// without the buffer overrun checking, to work around bugs in
// older solaris. Note that this is still not very portable, in that
// the return value cannot be trusted.

#if 0 // SCANF_CAN_MALLOC
// C99 define "a" for floating point, so you can have runtime surprise
// according the library versions
# define SCANF_PREFIX "a"
# define SCANF_STRING(s) (&s)
#else
# define SCANF_PREFIX "511"
# define SCANF_STRING(s) (s = malloc(512))
#endif /* SCANF_CAN_MALLOC */

#if PREFER_PORTABLE_SNPRINTF
static inline int
portable_snprintf(char *str, size_t n, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsprintf(str, fmt, ap);
	va_end(ap);
	return ret;
}
# define SNPRINTF portable_snprintf
#else
# define SNPRINTF snprintf
#endif /* PREFER_PORTABLE_SNPRINTF */

#if !HAVE_GETLINE
// getline() replacement for Darwin and Solaris etc.
// This code uses backward seeks (unless rchunk is set to 1) which can't work
// on pipes etc. However, add2fs_from_file() only calls getline() for
// regular files, so a larger rchunk and backward seeks are okay.

ssize_t 
getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	char *p;                    // reads stored here
	size_t const rchunk = 512;  // number of bytes to read
	size_t const mchunk = 512;  // number of extra bytes to malloc
	size_t m = rchunk + 1;      // initial buffer size
	
	if (*lineptr) {
		if (*n < m) {
			*lineptr = (char*)realloc(*lineptr, m);
			if (!*lineptr) return -1;
			*n = m;
		}
	} else {
		*lineptr = (char*)malloc(m);
		if (!*lineptr) return -1;
		*n = m;
	}

	m = 0; // record length including seperator

	do {
		size_t i;     // number of bytes read etc
		size_t j = 0; // number of bytes searched

		p = *lineptr + m;

		i = fread(p, 1, rchunk, stream);
		if (i < rchunk && ferror(stream))
			return -1;
		while (j < i) {
			++j;
			if (*p++ == (char)delim) {
				*p = '\0';
				if (j != i) {
					if (fseek(stream, j - i, SEEK_CUR))
						return -1;
					if (feof(stream))
						clearerr(stream);
				}
				m += j;
				return m;
			}
		}

		m += j;
		if (feof(stream)) {
			if (m) return m;
			if (!i) return -1;
		}

		// allocate space for next read plus possible null terminator
		i = ((m + (rchunk + 1 > mchunk ? rchunk + 1 : mchunk) +
		      mchunk - 1) / mchunk) * mchunk;
		if (i != *n) {
			*lineptr = (char*)realloc(*lineptr, i);
			if (!*lineptr) return -1;
			*n = i;
		}
	} while (1);
}
#define getline(a,b,c) getdelim(a,b,'\n',c)
#endif /* HAVE_GETLINE */

// Convert a numerical string to a float, and multiply the result by an
// IEC or SI multiplier if provided; supported multipliers are Ki, Mi, Gi, k, M
// and G.

float
SI_atof(const char *nptr)
{
	float f = 0;
	float m = 1;
	char *suffixptr;

#if HAVE_STRTOF
	f = strtof(nptr, &suffixptr);
#else
	f = (float)strtod(nptr, &suffixptr);
#endif /* HAVE_STRTOF */

	if (*suffixptr) {
		if (!strcmp(suffixptr, "Ki"))
			m = 1 << 10;
		else if (!strcmp(suffixptr, "Mi"))
			m = 1 << 20;
		else if (!strcmp(suffixptr, "Gi"))
			m = 1 << 30;
		else if (!strcmp(suffixptr, "k"))
			m = 1000;
		else if (!strcmp(suffixptr, "M"))
			m = 1000 * 1000;
		else if (!strcmp(suffixptr, "G"))
			m = 1000 * 1000 * 1000;
	}
	return f * m;
}

// endianness swap

static inline uint16
swab16(uint16 val)
{
	return (val >> 8) | (val << 8);
}

static inline uint32
swab32(uint32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
			((val<<8)&0xFF0000) | (val<<24));
}

static inline int
is_blk_empty(uint8 *b)
{
	uint32 i;
	uint32 *v = (uint32 *) b;

	for(i = 0; i < BLOCKSIZE / 4; i++)
		if (*v++)
			return 0;
	return 1;
}

// on-disk structures
// this trick makes me declare things only once
// (once for the structures, once for the endianness swap)

#define superblock_decl \
	udecl32(s_inodes_count)        /* Count of inodes in the filesystem */ \
	udecl32(s_blocks_count)        /* Count of blocks in the filesystem */ \
	udecl32(s_r_blocks_count)      /* Count of the number of reserved blocks */ \
	udecl32(s_free_blocks_count)   /* Count of the number of free blocks */ \
	udecl32(s_free_inodes_count)   /* Count of the number of free inodes */ \
	udecl32(s_first_data_block)    /* The first block which contains data */ \
	udecl32(s_log_block_size)      /* Indicator of the block size */ \
	decl32(s_log_frag_size)        /* Indicator of the size of the fragments */ \
	udecl32(s_blocks_per_group)    /* Count of the number of blocks in each block group */ \
	udecl32(s_frags_per_group)     /* Count of the number of fragments in each block group */ \
	udecl32(s_inodes_per_group)    /* Count of the number of inodes in each block group */ \
	udecl32(s_mtime)               /* The time that the filesystem was last mounted */ \
	udecl32(s_wtime)               /* The time that the filesystem was last written to */ \
	udecl16(s_mnt_count)           /* The number of times the file system has been mounted */ \
	decl16(s_max_mnt_count)        /* The number of times the file system can be mounted */ \
	udecl16(s_magic)               /* Magic number indicating ex2fs */ \
	udecl16(s_state)               /* Flags indicating the current state of the filesystem */ \
	udecl16(s_errors)              /* Flags indicating the procedures for error reporting */ \
	udecl16(s_minor_rev_level)     /* The minor revision level of the filesystem */ \
	udecl32(s_lastcheck)           /* The time that the filesystem was last checked */ \
	udecl32(s_checkinterval)       /* The maximum time permissable between checks */ \
	udecl32(s_creator_os)          /* Indicator of which OS created the filesystem */ \
	udecl32(s_rev_level)           /* The revision level of the filesystem */ \
	udecl16(s_def_resuid)          /* The default uid for reserved blocks */ \
	udecl16(s_def_resgid)          /* The default gid for reserved blocks */ \
	/* rev 1 version fields start here */ \
	udecl32(s_first_ino) 		/* First non-reserved inode */	\
	udecl16(s_inode_size) 		/* size of inode structure */	\
	udecl16(s_block_group_nr) 	/* block group # of this superblock */ \
	udecl32(s_feature_compat) 	/* compatible feature set */	\
	udecl32(s_feature_incompat) 	/* incompatible feature set */	\
	udecl32(s_feature_ro_compat) 	/* readonly-compatible feature set */ \
	utdecl8(s_uuid,16)		/* 128-bit uuid for volume */	\
	utdecl8(s_volume_name,16) 	/* volume name */		\
	utdecl8(s_last_mounted,64) 	/* directory where last mounted */ \
	udecl32(s_algorithm_usage_bitmap) /* For compression */

#define EXT2_GOOD_OLD_FIRST_INO	11
#define EXT2_GOOD_OLD_INODE_SIZE 128
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002

#define groupdescriptor_decl \
	udecl32(bg_block_bitmap)       /* Block number of the block bitmap */ \
	udecl32(bg_inode_bitmap)       /* Block number of the inode bitmap */ \
	udecl32(bg_inode_table)        /* Block number of the inode table */ \
	udecl16(bg_free_blocks_count)  /* Free blocks in the group */ \
	udecl16(bg_free_inodes_count)  /* Free inodes in the group */ \
	udecl16(bg_used_dirs_count)    /* Number of directories in the group */ \
	udecl16(bg_pad)

#define inode_decl \
	udecl16(i_mode)                /* Entry type and file mode */ \
	udecl16(i_uid)                 /* User id */ \
	udecl32(i_size)                /* File/dir size in bytes */ \
	udecl32(i_atime)               /* Last access time */ \
	udecl32(i_ctime)               /* Creation time */ \
	udecl32(i_mtime)               /* Last modification time */ \
	udecl32(i_dtime)               /* Deletion time */ \
	udecl16(i_gid)                 /* Group id */ \
	udecl16(i_links_count)         /* Number of (hard) links to this inode */ \
	udecl32(i_blocks)              /* Number of blocks used (1 block = 512 bytes) */ \
	udecl32(i_flags)               /* ??? */ \
	udecl32(i_reserved1) \
	utdecl32(i_block,15)           /* Blocks table */ \
	udecl32(i_version)             /* ??? */ \
	udecl32(i_file_acl)            /* File access control list */ \
	udecl32(i_dir_acl)             /* Directory access control list */ \
	udecl32(i_faddr)               /* Fragment address */ \
	udecl8(i_frag)                 /* Fragments count*/ \
	udecl8(i_fsize)                /* Fragment size */ \
	udecl16(i_pad1)

#define directory_decl \
	udecl32(d_inode)               /* Inode entry */ \
	udecl16(d_rec_len)             /* Total size on record */ \
	udecl16(d_name_len)            /* Size of entry name */

#define decl8(x) int8 x;
#define udecl8(x) uint8 x;
#define utdecl8(x,n) uint8 x[n];
#define decl16(x) int16 x;
#define udecl16(x) uint16 x;
#define decl32(x) int32 x;
#define udecl32(x) uint32 x;
#define utdecl32(x,n) uint32 x[n];

typedef struct
{
	superblock_decl
	uint32 s_reserved[205];       // Reserved
} superblock;

typedef struct
{
	groupdescriptor_decl
	uint32 bg_reserved[3];
} groupdescriptor;

typedef struct
{
	inode_decl
	uint32 i_reserved2[2];
} inode;

typedef struct
{
	directory_decl
} directory;

typedef uint8 *block;

/* blockwalker fields:
   The blockwalker is used to access all the blocks of a file (including
   the indirection blocks) through repeated calls to walk_bw.  
   
   bpdir -> index into the inode->i_block[]. Indicates level of indirection.
   bnum -> total number of blocks so far accessed. including indirection
           blocks.
   bpind,bpdind,bptind -> index into indirection blocks.
   
   bpind, bpdind, bptind do *NOT* index into single, double and triple
   indirect blocks resp. as you might expect from their names. Instead 
   they are in order the 1st, 2nd & 3rd index to be used
   
   As an example..
   To access data block number 70000:
        bpdir: 15 (we are doing triple indirection)
        bpind: 0 ( index into the triple indirection block)
        bpdind: 16 ( index into the double indirection block)
        bptind: 99 ( index into the single indirection block)
	70000 = 12 + 256 + 256*256 + 16*256 + 100 (indexing starts from zero)

   So,for double indirection bpind will index into the double indirection 
   block and bpdind into the single indirection block. For single indirection
   only bpind will be used.
*/
   
typedef struct
{
	uint32 bnum;
	uint32 bpdir;
	uint32 bpind;
	uint32 bpdind;
	uint32 bptind;
} blockwalker;

#define HDLINK_CNT   16
struct hdlink_s
{
	uint32	src_inode;
	uint32	dst_nod;
};

struct hdlinks_s
{
	int32 count;
	struct hdlink_s *hdl;
};

/* Filesystem structure that support groups */
typedef struct
{
	FILE *f;
	superblock *sb;
	int swapit;
	int32 hdlink_cnt;
	struct hdlinks_s hdlinks;

	int holes;

	listcache blks;
	listcache gds;
	listcache inodes;
	listcache blkmaps;
} filesystem;

// now the endianness swap

#undef decl8
#undef udecl8
#undef utdecl8
#undef decl16
#undef udecl16
#undef decl32
#undef udecl32
#undef utdecl32

#define decl8(x)
#define udecl8(x)
#define utdecl8(x,n)
#define decl16(x) this->x = swab16(this->x);
#define udecl16(x) this->x = swab16(this->x);
#define decl32(x) this->x = swab32(this->x);
#define udecl32(x) this->x = swab32(this->x);
#define utdecl32(x,n) { int i; for(i=0; i<n; i++) this->x[i] = swab32(this->x[i]); }

static void
swap_sb(superblock *sb)
{
#define this sb
	superblock_decl
#undef this
}

static void
swap_gd(groupdescriptor *gd)
{
#define this gd
	groupdescriptor_decl
#undef this
}

static void
swap_nod(inode *nod)
{
	uint32 nblk;

#define this nod
	inode_decl
#undef this

	// block and character inodes store the major and minor in the
	// i_block, so we need to unswap to get those.  Also, if it's
	// zero iblocks, put the data back like it belongs.
	nblk = nod->i_blocks / INOBLK;
	if ((nod->i_size && !nblk)
	    || ((nod->i_mode & FM_IFBLK) == FM_IFBLK)
	    || ((nod->i_mode & FM_IFCHR) == FM_IFCHR))
	{
		int i;
		for(i = 0; i <= EXT2_TIND_BLOCK; i++)
			nod->i_block[i] = swab32(nod->i_block[i]);
	}
}

static void
swap_dir(directory *dir)
{
#define this dir
	directory_decl
#undef this
}

static void
swap_block(block b)
{
	int i;
	uint32 *blk = (uint32*)b;
	for(i = 0; i < BLOCKSIZE/4; i++)
		blk[i] = swab32(blk[i]);
}

#undef decl8
#undef udecl8
#undef utdecl8
#undef decl16
#undef udecl16
#undef decl32
#undef udecl32
#undef utdecl32

static char * app_name;
static const char *const memory_exhausted = "memory exhausted";

// error (un)handling
static void
verror_msg(const char *s, va_list p)
{
	fflush(stdout);
	fprintf(stderr, "%s: ", app_name);
	vfprintf(stderr, s, p);
}
static void
error_msg(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	verror_msg(s, p);
	va_end(p);
	putc('\n', stderr);
}

static void
error_msg_and_die(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	verror_msg(s, p);
	va_end(p);
	putc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void
vperror_msg(const char *s, va_list p)
{
	int err = errno;
	if (s == 0)
		s = "";
	verror_msg(s, p);
	if (*s)
		s = ": ";
	fprintf(stderr, "%s%s\n", s, strerror(err));
}

static void
perror_msg_and_die(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	vperror_msg(s, p);
	va_end(p);
	exit(EXIT_FAILURE);
}

static FILE *
xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		perror_msg_and_die("%s", path);
	return fp;
}

static char *
xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;
	t = strdup(s);
	if (t == NULL)
		error_msg_and_die(memory_exhausted);
	return t;
}

static void *
xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		error_msg_and_die(memory_exhausted);
	return ptr;
}

static char *
xreadlink(const char *path)
{
	static const int GROWBY = 80; /* how large we will grow strings by */

	char *buf = NULL;
	int bufsize = 0, readsize = 0;

	do {
		buf = xrealloc(buf, bufsize += GROWBY);
		readsize = readlink(path, buf, bufsize); /* 1st try */
		if (readsize == -1) {
			perror_msg_and_die("%s:%s", app_name, path);
		}
	}
	while (bufsize < readsize + 1);

	buf[readsize] = '\0';
	return buf;
}

int
is_hardlink(filesystem *fs, ino_t inode)
{
	int i;

	for(i = 0; i < fs->hdlinks.count; i++) {
		if(fs->hdlinks.hdl[i].src_inode == inode)
			return i;
	}
	return -1;
}

// printf helper macro
#define plural(a) (a), ((a) > 1) ? "s" : ""

// temporary working block
static inline uint8 *
get_workblk(void)
{
	unsigned char* b=calloc(1,BLOCKSIZE);
	if (!b)
		error_msg_and_die("get_workblk() failed, out of memory");
	return b;
}
static inline void
free_workblk(block b)
{
	free(b);
}

/* Rounds qty upto a multiple of siz. siz should be a power of 2 */
static inline uint32
rndup(uint32 qty, uint32 siz)
{
	return (qty + (siz - 1)) & ~(siz - 1);
}

// check if something is allocated in the bitmap
static inline uint32
allocated(block b, uint32 item)
{
	return b[(item-1) / 8] & (1 << ((item-1) % 8));
}

// Used by get_blk/put_blk to hold information about a block owned
// by the user.
typedef struct
{
	cache_link link;

	filesystem *fs;
	uint32 blk;
	uint8 *b;
	uint32 usecount;
} blk_info;

#define MAX_FREE_CACHE_BLOCKS 100

static uint32
blk_elem_val(cache_link *elem)
{
	blk_info *bi = container_of(elem, blk_info, link);
	return bi->blk;
}

static void
blk_freed(cache_link *elem)
{
	blk_info *bi = container_of(elem, blk_info, link);

	if (fseeko(bi->fs->f, ((off_t) bi->blk) * BLOCKSIZE, SEEK_SET))
		perror_msg_and_die("fseek");
	if (fwrite(bi->b, BLOCKSIZE, 1, bi->fs->f) != 1)
		perror_msg_and_die("get_blk: write");
	free(bi->b);
	free(bi);
}

// Return a given block from a filesystem.  Make sure to call
// put_blk when you are done with it.
static inline uint8 *
get_blk(filesystem *fs, uint32 blk, blk_info **rbi)
{
	cache_link *curr;
	blk_info *bi;

	if (blk >= fs->sb->s_blocks_count)
		error_msg_and_die("Internal error, block out of range");

	curr = cache_find(&fs->blks, blk);
	if (curr) {
		bi = container_of(curr, blk_info, link);
		bi->usecount++;
		goto out;
	}

	bi = malloc(sizeof(*bi));
	if (!bi)
		error_msg_and_die("get_blk: out of memory");
	bi->fs = fs;
	bi->blk = blk;
	bi->usecount = 1;
	bi->b = malloc(BLOCKSIZE);
	if (!bi->b)
		error_msg_and_die("get_blk: out of memory");
	cache_add(&fs->blks, &bi->link);
	if (fseeko(fs->f, ((off_t) blk) * BLOCKSIZE, SEEK_SET))
		perror_msg_and_die("fseek");
	if (fread(bi->b, BLOCKSIZE, 1, fs->f) != 1) {
		if (ferror(fs->f))
			perror_msg_and_die("fread");
		memset(bi->b, 0, BLOCKSIZE);
	}

out:
	*rbi = bi;
	return bi->b;
}

// return a given inode from a filesystem
static inline void
put_blk(blk_info *bi)
{
	if (bi->usecount == 0)
		error_msg_and_die("Internal error: put_blk usecount zero");
	bi->usecount--;
	if (bi->usecount == 0)
		/* Free happens in the cache code */
		cache_item_set_unused(&bi->fs->blks, &bi->link);
}

typedef struct
{
	cache_link link;

	filesystem *fs;
	int gds;
	blk_info *bi;
	groupdescriptor *gd;
	uint32 usecount;
} gd_info;

#define MAX_FREE_CACHE_GDS 100

static uint32
gd_elem_val(cache_link *elem)
{
	gd_info *gi = container_of(elem, gd_info, link);
	return gi->gds;
}

static void
gd_freed(cache_link *elem)
{
	gd_info *gi = container_of(elem, gd_info, link);

	if (gi->fs->swapit)
		swap_gd(gi->gd);
	put_blk(gi->bi);
	free(gi);
}

#define GDS_START ((SUPERBLOCK_OFFSET + SUPERBLOCK_SIZE + BLOCKSIZE - 1) / BLOCKSIZE)
#define GDS_PER_BLOCK (BLOCKSIZE / sizeof(groupdescriptor))
// the group descriptors are aligned on the block size
static inline groupdescriptor *
get_gd(filesystem *fs, uint32 no, gd_info **rgi)
{
	uint32 gdblk;
	uint32 offset;
	gd_info *gi;
	cache_link *curr;

	curr = cache_find(&fs->gds, no);
	if (curr) {
		gi = container_of(curr, gd_info, link);
		gi->usecount++;
		goto out;
	}

	gi = malloc(sizeof(*gi));
	if (!gi)
		error_msg_and_die("get_gd: out of memory");
	gi->fs = fs;
	gi->gds = no;
	gi->usecount = 1;
	gdblk = GDS_START + (no / GDS_PER_BLOCK);
	offset = no % GDS_PER_BLOCK;
	gi->gd = ((groupdescriptor *) get_blk(fs, gdblk, &gi->bi)) + offset;
	cache_add(&fs->gds, &gi->link);
	if (fs->swapit)
		swap_gd(gi->gd);
 out:
	*rgi = gi;

	return gi->gd;
}

static inline void
put_gd(gd_info *gi)
{
	if (gi->usecount == 0)
		error_msg_and_die("Internal error: put_gd usecount zero");

	gi->usecount--;
	if (gi->usecount == 0)
		/* Free happens in the cache code */
		cache_item_set_unused(&gi->fs->gds, &gi->link);
}

// Used by get_blkmap/put_blkmap to hold information about an block map
// owned by the user.
typedef struct
{
	cache_link link;

	filesystem *fs;
	uint32 blk;
	uint8 *b;
	blk_info *bi;
	uint32 usecount;
} blkmap_info;

#define MAX_FREE_CACHE_BLOCKMAPS 100

static uint32
blkmap_elem_val(cache_link *elem)
{
	blkmap_info *bmi = container_of(elem, blkmap_info, link);
	return bmi->blk;
}

static void
blkmap_freed(cache_link *elem)
{
	blkmap_info *bmi = container_of(elem, blkmap_info, link);

	if (bmi->fs->swapit)
		swap_block(bmi->b);
	put_blk(bmi->bi);
	free(bmi);
}

// Return a given block map from a filesystem.  Make sure to call
// put_blkmap when you are done with it.
static inline uint32 *
get_blkmap(filesystem *fs, uint32 blk, blkmap_info **rbmi)
{
	blkmap_info *bmi;
	cache_link *curr;

	curr = cache_find(&fs->blkmaps, blk);
	if (curr) {
		bmi = container_of(curr, blkmap_info, link);
		bmi->usecount++;
		goto out;
	}

	bmi = malloc(sizeof(*bmi));
	if (!bmi)
		error_msg_and_die("get_blkmap: out of memory");
	bmi->fs = fs;
	bmi->blk = blk;
	bmi->b = get_blk(fs, blk, &bmi->bi);
	bmi->usecount = 1;
	cache_add(&fs->blkmaps, &bmi->link);

	if (fs->swapit)
		swap_block(bmi->b);
 out:
	*rbmi = bmi;
	return (uint32 *) bmi->b;
}

static inline void
put_blkmap(blkmap_info *bmi)
{
	if (bmi->usecount == 0)
		error_msg_and_die("Internal error: put_blkmap usecount zero");

	bmi->usecount--;
	if (bmi->usecount == 0)
		/* Free happens in the cache code */
		cache_item_set_unused(&bmi->fs->blkmaps, &bmi->link);
}

// Used by get_nod/put_nod to hold information about an inode owned
// by the user.
typedef struct
{
	cache_link link;

	filesystem *fs;
	uint32 nod;
	uint8 *b;
	blk_info *bi;
	inode *itab;
	uint32 usecount;
} nod_info;

#define MAX_FREE_CACHE_INODES 100

static uint32
inode_elem_val(cache_link *elem)
{
	nod_info *ni = container_of(elem, nod_info, link);
	return ni->nod;
}

static void
inode_freed(cache_link *elem)
{
	nod_info *ni = container_of(elem, nod_info, link);

	if (ni->fs->swapit)
		swap_nod(ni->itab);
	put_blk(ni->bi);
	free(ni);
}

#define INODES_PER_BLOCK (BLOCKSIZE / sizeof(inode))

// return a given inode from a filesystem
static inline inode *
get_nod(filesystem *fs, uint32 nod, nod_info **rni)
{
	uint32 grp, boffset, offset;
	cache_link *curr;
	groupdescriptor *gd;
	gd_info *gi;
	nod_info *ni;

	curr = cache_find(&fs->inodes, nod);
	if (curr) {
		ni = container_of(curr, nod_info, link);
		ni->usecount++;
		goto out;
	}

	ni = malloc(sizeof(*ni));
	if (!ni)
		error_msg_and_die("get_nod: out of memory");
	ni->fs = fs;
	ni->nod = nod;
	ni->usecount = 1;
	cache_add(&fs->inodes, &ni->link);

	offset = GRP_IBM_OFFSET(fs,nod) - 1;
	boffset = offset / INODES_PER_BLOCK;
	offset %= INODES_PER_BLOCK;
	grp = GRP_GROUP_OF_INODE(fs,nod);
	gd = get_gd(fs, grp, &gi);
	ni->b = get_blk(fs, gd->bg_inode_table + boffset, &ni->bi);
	ni->itab = ((inode *) ni->b) + offset;
	if (fs->swapit)
		swap_nod(ni->itab);
	put_gd(gi);
 out:
	*rni = ni;
	return ni->itab;
}

static inline void
put_nod(nod_info *ni)
{
	if (ni->usecount == 0)
		error_msg_and_die("Internal error: put_nod usecount zero");

	ni->usecount--;
	if (ni->usecount == 0)
		/* Free happens in the cache code */
		cache_item_set_unused(&ni->fs->inodes, &ni->link);
}

// Used to hold state information while walking a directory inode.
typedef struct
{
	directory d;
	filesystem *fs;
	uint32 nod;
	directory *last_d;
	uint8 *b;
	blk_info *bi;
} dirwalker;

// Start a directory walk on the given inode.  You must pass in a
// dirwalker structure, then use that dirwalker for future operations.
// Call put_dir when you are done walking the directory.
static inline directory *
get_dir(filesystem *fs, uint32 nod, dirwalker *dw)
{
	dw->fs = fs;
	dw->b = get_blk(fs, nod, &dw->bi);
	dw->nod = nod;
	dw->last_d = (directory *) dw->b;

	memcpy(&dw->d, dw->last_d, sizeof(directory));
	if (fs->swapit)
		swap_dir(&dw->d);
	return &dw->d;
}

// Move to the next directory.
static inline directory *
next_dir(dirwalker *dw)
{
	directory *next_d = (directory *)((int8*)dw->last_d + dw->d.d_rec_len);

	if (dw->fs->swapit)
		swap_dir(&dw->d);
	memcpy(dw->last_d, &dw->d, sizeof(directory));

	if (((int8 *) next_d) >= ((int8 *) dw->b + BLOCKSIZE))
		return NULL;

	dw->last_d = next_d;
	memcpy(&dw->d, next_d, sizeof(directory));
	if (dw->fs->swapit)
		swap_dir(&dw->d);
	return &dw->d;
}

// Call then when you are done with the directory walk.
static inline void
put_dir(dirwalker *dw)
{
	if (dw->fs->swapit)
		swap_dir(&dw->d);
	memcpy(dw->last_d, &dw->d, sizeof(directory));

	if (dw->nod == 0)
		free_workblk(dw->b);
	else
		put_blk(dw->bi);
}

// Create a new directory block with the given inode as it's destination
// and append it to the current dirwalker.
static directory *
new_dir(filesystem *fs, uint32 dnod, const char *name, int nlen, dirwalker *dw)
{
	directory *d;

	dw->fs = fs;
	dw->b = get_workblk();
	dw->nod = 0;
	dw->last_d = (directory *) dw->b;
	d = &dw->d;
	d->d_inode = dnod;
	d->d_rec_len = BLOCKSIZE;
	d->d_name_len = nlen;
	strncpy(((char *) dw->last_d) + sizeof(directory), name, nlen);
	return d;
}

// Shrink the current directory entry, make a new one with the free
// space, and return the new directory entry (making it current).
static inline directory *
shrink_dir(dirwalker *dw, uint32 nod, const char *name, int nlen)
{
	int reclen, preclen;
	directory *d = &dw->d;

	reclen = d->d_rec_len;
	d->d_rec_len = sizeof(directory) + rndup(d->d_name_len, 4);
	preclen = d->d_rec_len;
	reclen -= preclen;
	if (dw->fs->swapit)
		swap_dir(&dw->d);
	memcpy(dw->last_d, &dw->d, sizeof(directory));

	dw->last_d = (directory *) (((int8 *) dw->last_d) + preclen);
	d->d_rec_len = reclen;
	d->d_inode = nod;
	d->d_name_len = nlen;
	strncpy(((char *) dw->last_d) + sizeof(directory), name, nlen);

	return d;
}

// Return the current block the directory is walking
static inline uint8 *
dir_data(dirwalker *dw)
{
	return dw->b;
}

// Return the pointer to the name for the current directory
static inline char *
dir_name(dirwalker *dw)
{
	return ((char *) dw->last_d) + sizeof(directory);
}

// Set the name for the current directory.  Note that this doesn't
// verify that there is space for the directory name, you must do
// that yourself.
static void
dir_set_name(dirwalker *dw, const char *name, int nlen)
{
	dw->d.d_name_len = nlen;
	strncpy(((char *) dw->last_d) + sizeof(directory), name, nlen);
}

// allocate a given block/inode in the bitmap
// allocate first free if item == 0
static uint32
allocate(block b, uint32 item)
{
	if(!item)
	{
		int i;
		uint8 bits;
		for(i = 0; i < BLOCKSIZE; i++)
			if((bits = b[i]) != (uint8)-1)
			{
				int j;
				for(j = 0; j < 8; j++)
					if(!(bits & (1 << j)))
						break;
				item = i * 8 + j + 1;
				break;
			}
		if(i == BLOCKSIZE)
			return 0;
	}
	b[(item-1) / 8] |= (1 << ((item-1) % 8));
	return item;
}

// deallocate a given block/inode
static void
deallocate(block b, uint32 item)
{
	b[(item-1) / 8] &= ~(1 << ((item-1) % 8));
}

// allocate a block
static uint32
alloc_blk(filesystem *fs, uint32 nod)
{
	uint32 bk=0;
	uint32 grp,nbgroups;
	blk_info *bi;
	groupdescriptor *gd;
	gd_info *gi;

	grp = GRP_GROUP_OF_INODE(fs,nod);
	nbgroups = GRP_NBGROUPS(fs);
	gd = get_gd(fs, grp, &gi);
	bk = allocate(GRP_GET_GROUP_BBM(fs, gd, &bi), 0);
	GRP_PUT_GROUP_BBM(bi);
	put_gd(gi);
	if (!bk) {
		for (grp=0; grp<nbgroups && !bk; grp++) {
			gd = get_gd(fs, grp, &gi);
			bk = allocate(GRP_GET_GROUP_BBM(fs, gd, &bi), 0);
			GRP_PUT_GROUP_BBM(bi);
			put_gd(gi);
		}
		grp--;
	}
	if (!bk)
		error_msg_and_die("couldn't allocate a block (no free space)");
	gd = get_gd(fs, grp, &gi);
	if(!(gd->bg_free_blocks_count--))
		error_msg_and_die("group descr %d. free blocks count == 0 (corrupted fs?)",grp);
	put_gd(gi);
	if(!(fs->sb->s_free_blocks_count--))
		error_msg_and_die("superblock free blocks count == 0 (corrupted fs?)");
	return fs->sb->s_first_data_block + fs->sb->s_blocks_per_group*grp + (bk-1);
}

// free a block
static void
free_blk(filesystem *fs, uint32 bk)
{
	uint32 grp;
	blk_info *bi;
	gd_info *gi;
	groupdescriptor *gd;

	grp = bk / fs->sb->s_blocks_per_group;
	bk %= fs->sb->s_blocks_per_group;
	gd = get_gd(fs, grp, &gi);
	deallocate(GRP_GET_GROUP_BBM(fs, gd, &bi), bk);
	GRP_PUT_GROUP_BBM(bi);
	gd->bg_free_blocks_count++;
	put_gd(gi);
	fs->sb->s_free_blocks_count++;
}

// allocate an inode
static uint32
alloc_nod(filesystem *fs)
{
	uint32 nod,best_group=0;
	uint32 grp,nbgroups,avefreei;
	blk_info *bi;
	gd_info *gi, *bestgi;
	groupdescriptor *gd, *bestgd;

	nbgroups = GRP_NBGROUPS(fs);

	/* Distribute inodes amongst all the blocks                           */
	/* For every block group with more than average number of free inodes */
	/* find the one with the most free blocks and allocate node there     */
	/* Idea from find_group_dir in fs/ext2/ialloc.c in 2.4.19 kernel      */
	/* We do it for all inodes.                                           */
	avefreei  =  fs->sb->s_free_inodes_count / nbgroups;
	bestgd = get_gd(fs, best_group, &bestgi);
	for(grp=0; grp<nbgroups; grp++) {
		gd = get_gd(fs, grp, &gi);
		if (gd->bg_free_inodes_count < avefreei ||
		    gd->bg_free_inodes_count == 0) {
			put_gd(gi);
			continue;
		}
		if (!best_group || gd->bg_free_blocks_count > bestgd->bg_free_blocks_count) {
			put_gd(bestgi);
			best_group = grp;
			bestgd = gd;
			bestgi = gi;
		} else
			put_gd(gi);
	}
	if (!(nod = allocate(GRP_GET_GROUP_IBM(fs, bestgd, &bi), 0)))
		error_msg_and_die("couldn't allocate an inode (no free inode)");
	GRP_PUT_GROUP_IBM(bi);
	if(!(bestgd->bg_free_inodes_count--))
		error_msg_and_die("group descr. free blocks count == 0 (corrupted fs?)");
	put_gd(bestgi);
	if(!(fs->sb->s_free_inodes_count--))
		error_msg_and_die("superblock free blocks count == 0 (corrupted fs?)");
	return fs->sb->s_inodes_per_group*best_group+nod;
}

// print a bitmap allocation
static void
print_bm(block b, uint32 max)
{
	uint32 i;
	printf("----+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0\n");
	for(i=1; i <= max; i++)
	{
		putchar(allocated(b, i) ? '*' : '.');
		if(!(i % 100))
			printf("\n");
	}
	if((i-1) % 100)
		printf("\n");
}

// initalize a blockwalker (iterator for blocks list)
static inline void
init_bw(blockwalker *bw)
{
	bw->bnum = 0;
	bw->bpdir = EXT2_INIT_BLOCK;
}

// return next block of inode (WALK_END for end)
// if *create>0, append a newly allocated block at the end
// if *create<0, free the block - warning, the metadata blocks contents is
//				  used after being freed, so once you start
//				  freeing blocks don't stop until the end of
//				  the file. moreover, i_blocks isn't updated.
// if hole!=0, create a hole in the file
static uint32
walk_bw(filesystem *fs, uint32 nod, blockwalker *bw, int32 *create, uint32 hole)
{
	uint32 *bkref = 0;
	uint32 bk = 0;
	blkmap_info *bmi1 = NULL, *bmi2 = NULL, *bmi3 = NULL;
	uint32 *b;
	int extend = 0, reduce = 0;
	inode *inod;
	nod_info *ni;
	uint32 *iblk;

	if(create && (*create) < 0)
		reduce = 1;
	inod = get_nod(fs, nod, &ni);
	if(bw->bnum >= inod->i_blocks / INOBLK)
	{
		if(create && (*create) > 0)
		{
			(*create)--;
			extend = 1;
		}
		else
		{
			put_nod(ni);
			return WALK_END;
		}
	}
	iblk = inod->i_block;
	// first direct block
	if(bw->bpdir == EXT2_INIT_BLOCK)
	{
		bkref = &iblk[bw->bpdir = 0];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	// direct block
	else if(bw->bpdir < EXT2_NDIR_BLOCKS)
	{
		bkref = &iblk[++bw->bpdir];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free block
			free_blk(fs, *bkref);
	}
	// first block in indirect block
	else if(bw->bpdir == EXT2_NDIR_BLOCKS)
	{
		bw->bnum++;
		bw->bpdir = EXT2_IND_BLOCK;
		bw->bpind = 0;
		if(extend) // allocate indirect block
			iblk[bw->bpdir] = alloc_blk(fs,nod);
		if(reduce) // free indirect block
			free_blk(fs, iblk[bw->bpdir]);
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		bkref = &b[bw->bpind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	// block in indirect block
	else if((bw->bpdir == EXT2_IND_BLOCK) && (bw->bpind < BLOCKSIZE/4 - 1))
	{
		bw->bpind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		bkref = &b[bw->bpind];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free block
			free_blk(fs, *bkref);
	}
	// first block in first indirect block in first double indirect block
	else if(bw->bpdir == EXT2_IND_BLOCK)
	{
		bw->bnum += 2;
		bw->bpdir = EXT2_DIND_BLOCK;
		bw->bpind = 0;
		bw->bpdind = 0;
		if(extend) // allocate double indirect block
			iblk[bw->bpdir] = alloc_blk(fs,nod);
		if(reduce) // free double indirect block
			free_blk(fs, iblk[bw->bpdir]);
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		if(extend) // allocate first indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		if(reduce) // free  firstindirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		bkref = &b[bw->bpdind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	// block in indirect block in double indirect block
	else if((bw->bpdir == EXT2_DIND_BLOCK) && (bw->bpdind < BLOCKSIZE/4 - 1))
	{
		bw->bpdind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		bkref = &b[bw->bpdind];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free block
			free_blk(fs, *bkref);
	}
	// first block in indirect block in double indirect block
	else if((bw->bpdir == EXT2_DIND_BLOCK) && (bw->bpind < BLOCKSIZE/4 - 1))
	{
		bw->bnum++;
		bw->bpdind = 0;
		bw->bpind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		if(extend) // allocate indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		if(reduce) // free indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		bkref = &b[bw->bpdind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}

	/* Adding support for triple indirection */
	/* Just starting triple indirection. Allocate the indirection
	   blocks and the first data block
	 */
	else if (bw->bpdir == EXT2_DIND_BLOCK) 
	{
	  	bw->bnum += 3;
		bw->bpdir = EXT2_TIND_BLOCK;
		bw->bpind = 0;
		bw->bpdind = 0;
		bw->bptind = 0;
		if(extend) // allocate triple indirect block
			iblk[bw->bpdir] = alloc_blk(fs,nod);
		if(reduce) // free triple indirect block
			free_blk(fs, iblk[bw->bpdir]);
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		if(extend) // allocate first double indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		if(reduce) // free first double indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		if(extend) // allocate first indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		if(reduce) // free first indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpdind], &bmi3);
		bkref = &b[bw->bptind];
		if(extend) // allocate first data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	/* Still processing a single indirect block down the indirection
	   chain.Allocate a data block for it
	 */
	else if ( (bw->bpdir == EXT2_TIND_BLOCK) && 
		  (bw->bptind < BLOCKSIZE/4 -1) )
	{
		bw->bptind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		b = get_blkmap(fs, b[bw->bpdind], &bmi3);
		bkref = &b[bw->bptind];
		if(extend) // allocate data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free block
			free_blk(fs, *bkref);
	}
	/* Finished processing a single indirect block. But still in the 
	   same double indirect block. Allocate new single indirect block
	   for it and a data block
	 */
	else if ( (bw->bpdir == EXT2_TIND_BLOCK) &&
		  (bw->bpdind < BLOCKSIZE/4 -1) )
	{
		bw->bnum++;
		bw->bptind = 0;
		bw->bpdind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		if(extend) // allocate single indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		if(reduce) // free indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpdind], &bmi3);
		bkref = &b[bw->bptind];
		if(extend) // allocate first data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	/* Finished processing a double indirect block. Allocate the next
	   double indirect block and the single,data blocks for it
	 */
	else if ( (bw->bpdir == EXT2_TIND_BLOCK) && 
		  (bw->bpind < BLOCKSIZE/4 - 1) )
	{
		bw->bnum += 2;
		bw->bpdind = 0;
		bw->bptind = 0;
		bw->bpind++;
		b = get_blkmap(fs, iblk[bw->bpdir], &bmi1);
		if(extend) // allocate double indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		if(reduce) // free double indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpind], &bmi2);
		if(extend) // allocate single indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		if(reduce) // free indirect block
			free_blk(fs, b[bw->bpind]);
		b = get_blkmap(fs, b[bw->bpdind], &bmi3);
		bkref = &b[bw->bptind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
		if(reduce) // free first block
			free_blk(fs, *bkref);
	}
	else
		error_msg_and_die("file too big !"); 
	/* End change for walking triple indirection */

	bk = *bkref;
	if (bmi3)
		put_blkmap(bmi3);
	if (bmi2)
		put_blkmap(bmi2);
	if (bmi1)
		put_blkmap(bmi1);

	if(bk)
	{
		blk_info *bi;
		gd_info *gi;
		uint8 *block;
		bw->bnum++;
		block = GRP_GET_BLOCK_BITMAP(fs,bk,&bi,&gi);
		if(!reduce && !allocated(block, GRP_BBM_OFFSET(fs,bk)))
			error_msg_and_die("[block %d of inode %d is unallocated !]", bk, nod);
		GRP_PUT_BLOCK_BITMAP(bi, gi);
	}
	if(extend)
		inod->i_blocks = bw->bnum * INOBLK;
	put_nod(ni);
	return bk;
}

typedef struct
{
	blockwalker bw;
	uint32 nod;
	nod_info *ni;
	inode *inod;
} inode_pos;
#define INODE_POS_TRUNCATE 0
#define INODE_POS_EXTEND 1

// Call this to set up an ipos structure for future use with
// extend_inode_blk to append blocks to the given inode.  If
// op is INODE_POS_TRUNCATE, the inode is truncated to zero size.
// If op is INODE_POS_EXTEND, the position is moved to the end
// of the inode's data blocks.
// Call inode_pos_finish when done with the inode_pos structure.
static void
inode_pos_init(filesystem *fs, inode_pos *ipos, uint32 nod, int op,
	       blockwalker *endbw)
{
	blockwalker lbw;

	init_bw(&ipos->bw);
	ipos->nod = nod;
	ipos->inod = get_nod(fs, nod, &ipos->ni);
	if (op == INODE_POS_TRUNCATE) {
		int32 create = -1;
		while(walk_bw(fs, nod, &ipos->bw, &create, 0) != WALK_END)
			/*nop*/;
		ipos->inod->i_blocks = 0;
	}

	if (endbw)
		ipos->bw = *endbw;
	else {
		/* Seek to the end */
		init_bw(&ipos->bw);
		lbw = ipos->bw;
		while(walk_bw(fs, nod, &ipos->bw, 0, 0) != WALK_END)
			lbw = ipos->bw;
		ipos->bw = lbw;
	}
}

// Clean up the inode_pos structure.
static void
inode_pos_finish(filesystem *fs, inode_pos *ipos)
{
	put_nod(ipos->ni);
}

// add blocks to an inode (file/dir/etc...) at the given position.
// This will only work when appending to the end of an inode.
static void
extend_inode_blk(filesystem *fs, inode_pos *ipos, block b, int amount)
{
	uint32 bk;
	uint32 pos;

	if (amount < 0)
		error_msg_and_die("extend_inode_blk: Got negative amount");

	for (pos = 0; amount; pos += BLOCKSIZE)
	{
		int hole = (fs->holes && is_blk_empty(b + pos));

		bk = walk_bw(fs, ipos->nod, &ipos->bw, &amount, hole);
		if (bk == WALK_END)
			error_msg_and_die("extend_inode_blk: extend failed");
		if (!hole) {
			blk_info *bi;
			uint8 *block = get_blk(fs, bk, &bi);
			memcpy(block, b + pos, BLOCKSIZE);
			put_blk(bi);
		}
	}
}

// link an entry (inode #) to a directory
static void
add2dir(filesystem *fs, uint32 dnod, uint32 nod, const char* name)
{
	blockwalker bw, lbw;
	uint32 bk;
	directory *d;
	dirwalker dw;
	int reclen, nlen;
	inode *node;
	inode *pnode;
	nod_info *dni, *ni;
	inode_pos ipos;

	pnode = get_nod(fs, dnod, &dni);
	if((pnode->i_mode & FM_IFMT) != FM_IFDIR)
		error_msg_and_die("can't add '%s' to a non-directory", name);
	if(!*name)
		error_msg_and_die("can't create an inode with an empty name");
	if(strchr(name, '/'))
		error_msg_and_die("bad name '%s' (contains a slash)", name);
	nlen = strlen(name);
	reclen = sizeof(directory) + rndup(nlen, 4);
	if(reclen > BLOCKSIZE)
		error_msg_and_die("bad name '%s' (too long)", name);
	init_bw(&bw);
	lbw = bw;
	while((bk = walk_bw(fs, dnod, &bw, 0, 0)) != WALK_END) // for all blocks in dir
	{
		// for all dir entries in block
		for(d = get_dir(fs, bk, &dw); d; d = next_dir(&dw))
		{
			// if empty dir entry, large enough, use it
			if((!d->d_inode) && (d->d_rec_len >= reclen))
			{
				d->d_inode = nod;
				node = get_nod(fs, nod, &ni);
				dir_set_name(&dw, name, nlen);
				put_dir(&dw);
				node->i_links_count++;
				put_nod(ni);
				goto out;
			}
			// if entry with enough room (last one?), shrink it & use it
			if(d->d_rec_len >= (sizeof(directory) + rndup(d->d_name_len, 4) + reclen))
			{
				d = shrink_dir(&dw, nod, name, nlen);
				put_dir(&dw);
				node = get_nod(fs, nod, &ni);
				node->i_links_count++;
				put_nod(ni);
				goto out;
			}
		}
		put_dir(&dw);
		lbw = bw;
	}
	// we found no free entry in the directory, so we add a block
	node = get_nod(fs, nod, &ni);
	d = new_dir(fs, nod, name, nlen, &dw);
	node->i_links_count++;
	put_nod(ni);
	next_dir(&dw); // Force the data into the buffer

	inode_pos_init(fs, &ipos, dnod, INODE_POS_EXTEND, &lbw);
	extend_inode_blk(fs, &ipos, dir_data(&dw), 1);
	inode_pos_finish(fs, &ipos);

	put_dir(&dw);
	pnode->i_size += BLOCKSIZE;
out:
	put_nod(dni);
}

// find an entry in a directory
static uint32
find_dir(filesystem *fs, uint32 nod, const char * name)
{
	blockwalker bw;
	uint32 bk;
	int nlen = strlen(name);
	init_bw(&bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		directory *d;
		dirwalker dw;
		for (d = get_dir(fs, bk, &dw); d; d=next_dir(&dw))
			if(d->d_inode && (nlen == d->d_name_len) && !strncmp(dir_name(&dw), name, nlen)) {
				put_dir(&dw);
				return d->d_inode;
			}
		put_dir(&dw);
	}
	return 0;
}

// find the inode of a full path
static uint32
find_path(filesystem *fs, uint32 nod, const char * name)
{
	char *p, *n, *n2 = xstrdup(name);
	n = n2;
	while(*n == '/')
	{
		nod = EXT2_ROOT_INO;
		n++;
	}
	while(*n)
	{
		if((p = strchr(n, '/')))
			(*p) = 0;
		if(!(nod = find_dir(fs, nod, n)))
			break;
		if(p)
			n = p + 1;
		else
			break;
	}
	free(n2);
	return nod;
}

// chmod an inode
void
chmod_fs(filesystem *fs, uint32 nod, uint16 mode, uint16 uid, uint16 gid)
{
	inode *node;
	nod_info *ni;
	node = get_nod(fs, nod, &ni);
	node->i_mode = (node->i_mode & ~FM_IMASK) | (mode & FM_IMASK);
	node->i_uid = uid;
	node->i_gid = gid;
	put_nod(ni);
}

// create a simple inode
static uint32
mknod_fs(filesystem *fs, uint32 parent_nod, const char *name, uint16 mode, uint16 uid, uint16 gid, uint8 major, uint8 minor, uint32 ctime, uint32 mtime)
{
	uint32 nod;
	inode *node;
	nod_info *ni;
	gd_info *gi;

	nod = alloc_nod(fs);
	node = get_nod(fs, nod, &ni);
	node->i_mode = mode;
	add2dir(fs, parent_nod, nod, name);
	switch(mode & FM_IFMT)
	{
	case FM_IFLNK:
		mode = FM_IFLNK | FM_IRWXU | FM_IRWXG | FM_IRWXO;
		break;
	case FM_IFBLK:
	case FM_IFCHR:
		((uint8*)node->i_block)[0] = minor;
		((uint8*)node->i_block)[1] = major;
		break;
	case FM_IFDIR:
		add2dir(fs, nod, nod, ".");
		add2dir(fs, nod, parent_nod, "..");
		get_gd(fs,GRP_GROUP_OF_INODE(fs,nod),&gi)->bg_used_dirs_count++;
		put_gd(gi);
		break;
	}
	node->i_uid = uid;
	node->i_gid = gid;
	node->i_atime = mtime;
	node->i_ctime = ctime;
	node->i_mtime = mtime;
	put_nod(ni);
	return nod;
}

// make a full-fledged directory (i.e. with "." & "..")
static inline uint32
mkdir_fs(filesystem *fs, uint32 parent_nod, const char *name, uint32 mode,
	uid_t uid, gid_t gid, uint32 ctime, uint32 mtime)
{
	return mknod_fs(fs, parent_nod, name, mode|FM_IFDIR, uid, gid, 0, 0, ctime, mtime);
}

// make a symlink
static uint32
mklink_fs(filesystem *fs, uint32 parent_nod, const char *name, size_t size, uint8 *b, uid_t uid, gid_t gid, uint32 ctime, uint32 mtime)
{
	uint32 nod = mknod_fs(fs, parent_nod, name, FM_IFLNK | FM_IRWXU | FM_IRWXG | FM_IRWXO, uid, gid, 0, 0, ctime, mtime);
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);
	inode_pos ipos;

	inode_pos_init(fs, &ipos, nod, INODE_POS_TRUNCATE, NULL);
	node->i_size = size;
	if(size < 4 * (EXT2_TIND_BLOCK+1))
	{
		strncpy((char*)node->i_block, (char*)b, size);
		((char*)node->i_block)[size+1] = '\0';
		inode_pos_finish(fs, &ipos);
		put_nod(ni);
		return nod;
	}
	extend_inode_blk(fs, &ipos, b, rndup(size, BLOCKSIZE) / BLOCKSIZE);
	inode_pos_finish(fs, &ipos);
	put_nod(ni);
	return nod;
}

static void
fs_upgrade_rev1_largefile(filesystem *fs)
{
	fs->sb->s_rev_level = 1;
	fs->sb->s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	fs->sb->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
}

#define COPY_BLOCKS 16
#define CB_SIZE (COPY_BLOCKS * BLOCKSIZE)

// make a file from a FILE*
static uint32
mkfile_fs(filesystem *fs, uint32 parent_nod, const char *name, uint32 mode, FILE *f, uid_t uid, gid_t gid, uint32 ctime, uint32 mtime)
{
	uint8 * b;
	uint32 nod = mknod_fs(fs, parent_nod, name, mode|FM_IFREG, uid, gid, 0, 0, ctime, mtime);
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);
	off_t size = 0;
	size_t readbytes;
	inode_pos ipos;
	int fullsize;

	b = malloc(CB_SIZE);
	if (!b)
		error_msg_and_die("mkfile_fs: out of memory");
	inode_pos_init(fs, &ipos, nod, INODE_POS_TRUNCATE, NULL);
	readbytes = fread(b, 1, CB_SIZE, f);
	while (readbytes) {
		fullsize = rndup(readbytes, BLOCKSIZE);
		// Fill to end of block with zeros.
		memset(b + readbytes, 0, fullsize - readbytes);
		extend_inode_blk(fs, &ipos, b, fullsize / BLOCKSIZE);
		size += readbytes;
		readbytes = fread(b, 1, CB_SIZE, f);
	}
	if (size > 0x7fffffff) {
		if (fs->sb->s_rev_level < 1)
			fs_upgrade_rev1_largefile(fs);
		fs->sb->s_feature_ro_compat |= EXT2_FEATURE_RO_COMPAT_LARGE_FILE;
	}
	node->i_dir_acl = size >> 32;
	node->i_size = size;
	inode_pos_finish(fs, &ipos);
	put_nod(ni);
	free(b);
	return nod;
}

// retrieves a mode info from a struct stat
static uint32
get_mode(struct stat *st)
{
	uint32 mode = 0;

	if(st->st_mode & S_IRUSR)
		mode |= FM_IRUSR;
	if(st->st_mode & S_IWUSR)
		mode |= FM_IWUSR;
	if(st->st_mode & S_IXUSR)
		mode |= FM_IXUSR;
	if(st->st_mode & S_IRGRP)
		mode |= FM_IRGRP;
	if(st->st_mode & S_IWGRP)
		mode |= FM_IWGRP;
	if(st->st_mode & S_IXGRP)
		mode |= FM_IXGRP;
	if(st->st_mode & S_IROTH)
		mode |= FM_IROTH;
	if(st->st_mode & S_IWOTH)
		mode |= FM_IWOTH;
	if(st->st_mode & S_IXOTH)
		mode |= FM_IXOTH;
	if(st->st_mode & S_ISUID)
		mode |= FM_ISUID;
	if(st->st_mode & S_ISGID)
		mode |= FM_ISGID;
	if(st->st_mode & S_ISVTX)
		mode |= FM_ISVTX;
	return mode;
}

// add or fixup entries to the filesystem from a text file
/*  device table entries take the form of:
    <path>	<type> <mode>	<uid>	<gid>	<major>	<minor>	<start>	<inc>	<count>
    /dev/mem     c    640       0       0         1       1       0     0         -

    type can be one of: 
	f	A regular file
	d	Directory
	c	Character special device file
	b	Block special device file
	p	Fifo (named pipe)

    I don't bother with symlinks (permissions are irrelevant), hard
    links (special cases of regular files), or sockets (why bother).

    Regular files must exist in the target root directory.  If a char,
    block, fifo, or directory does not exist, it will be created.
*/

static void
add2fs_from_file(filesystem *fs, uint32 this_nod, FILE * fh, uint32 fs_timestamp, struct stats *stats)
{
	unsigned long mode, uid, gid, major, minor;
	unsigned long start, increment, count;
	uint32 nod, ctime, mtime;
	char *c, type, *path = NULL, *path2 = NULL, *dir, *name, *line = NULL;
	size_t len;
	struct stat st;
	int nbargs, lineno = 0;

	fstat(fileno(fh), &st);
	ctime = fs_timestamp;
	mtime = st.st_mtime;
	while(getline(&line, &len, fh) >= 0)
	{
		mode = uid = gid = major = minor = 0;
		start = 0; increment = 1; count = 0;
		lineno++;
		if((c = strchr(line, '#')))
			*c = 0;
		if (path) {
			free(path);
			path = NULL;
		}
		if (path2) {
			free(path2);
			path2 = NULL;
		}
		nbargs = sscanf (line, "%" SCANF_PREFIX "s %c %lo %lu %lu %lu %lu %lu %lu %lu",
					SCANF_STRING(path), &type, &mode, &uid, &gid, &major, &minor,
					&start, &increment, &count);
		if(nbargs < 3)
		{
			if(nbargs > 0)
				error_msg("device table line %d skipped: bad format for entry '%s'", lineno, path);
			continue;
		}
		mode &= FM_IMASK;
		path2 = strdup(path);
		name = basename(path);
		dir = dirname(path2);
		if((!strcmp(name, ".")) || (!strcmp(name, "..")))
		{
			error_msg("device table line %d skipped", lineno);
			continue;
		}
		if(fs)
		{
			if(!(nod = find_path(fs, this_nod, dir)))
			{
				error_msg("device table line %d skipped: can't find directory '%s' to create '%s''", lineno, dir, name);
				continue;
			}
		}
		else
			nod = 0;
		switch (type)
		{
			case 'd':
				mode |= FM_IFDIR;
				break;
			case 'f':
				mode |= FM_IFREG;
				break;
			case 'p':
				mode |= FM_IFIFO;
				break;
			case 's':
				mode |= FM_IFSOCK;
				break;
			case 'c':
				mode |= FM_IFCHR;
				break;
			case 'b':
				mode |= FM_IFBLK;
				break;
			default:
				error_msg("device table line %d skipped: bad type '%c' for entry '%s'", lineno, type, name);
				continue;
		}
		if(stats) {
			if(count > 0)
				stats->ninodes += count - start;
			else
				stats->ninodes++;
		} else {
			if(count > 0)
			{
				char *dname;
				unsigned long i;
				unsigned len;
				len = strlen(name) + 10;
				dname = malloc(len + 1);
				for(i = start; i < count; i++)
				{
					uint32 oldnod;
					SNPRINTF(dname, len, "%s%lu", name, i);
					oldnod = find_dir(fs, nod, dname);
					if(oldnod)
						chmod_fs(fs, oldnod, mode, uid, gid);
					else
						mknod_fs(fs, nod, dname, mode, uid, gid, major, minor + (i * increment - start), ctime, mtime);
				}
				free(dname);
			}
			else
			{
				uint32 oldnod = find_dir(fs, nod, name);
				if(oldnod)
					chmod_fs(fs, oldnod, mode, uid, gid);
				else
					mknod_fs(fs, nod, name, mode, uid, gid, major, minor, ctime, mtime);
			}
		}
	}
	if (line)
		free(line);
	if (path) 
		free(path);
	if (path2)
		free(path2);
}

// adds a tree of entries to the filesystem from current dir
static void
add2fs_from_dir(filesystem *fs, uint32 this_nod, int squash_uids, int squash_perms, uint32 fs_timestamp, struct stats *stats)
{
	uint32 nod;
	uint32 uid, gid, mode, ctime, mtime;
	const char *name;
	FILE *fh;
	DIR *dh;
	struct dirent *dent;
	struct stat st;
	char *lnk;
	uint32 save_nod;

	if(!(dh = opendir(".")))
		perror_msg_and_die(".");
	while((dent = readdir(dh)))
	{
		if((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, "..")))
			continue;
		lstat(dent->d_name, &st);
		uid = st.st_uid;
		gid = st.st_gid;
		ctime = fs_timestamp;
		mtime = st.st_mtime;
		name = dent->d_name;
		mode = get_mode(&st);
		if(squash_uids)
			uid = gid = 0;
		if(squash_perms)
			mode &= ~(FM_IRWXG | FM_IRWXO);
		if(stats)
			switch(st.st_mode & S_IFMT)
			{
				case S_IFLNK:
					if((st.st_mode & S_IFMT) == S_IFREG || st.st_size >= 4 * (EXT2_TIND_BLOCK+1))
						stats->nblocks += (st.st_size + BLOCKSIZE - 1) / BLOCKSIZE;
					stats->ninodes++;
					break;
				case S_IFREG:
					if((st.st_mode & S_IFMT) == S_IFREG || st.st_size > 4 * (EXT2_TIND_BLOCK+1))
						stats->nblocks += (st.st_size + BLOCKSIZE - 1) / BLOCKSIZE;
				case S_IFCHR:
				case S_IFBLK:
				case S_IFIFO:
				case S_IFSOCK:
					stats->ninodes++;
					break;
				case S_IFDIR:
					stats->ninodes++;
					if(chdir(dent->d_name) < 0)
						perror_msg_and_die(dent->d_name);
					add2fs_from_dir(fs, this_nod, squash_uids, squash_perms, fs_timestamp, stats);
					if (chdir("..") == -1)
						perror_msg_and_die("..");

					break;
				default:
					break;
			}
		else
		{
			if((nod = find_dir(fs, this_nod, name)))
			{
				error_msg("ignoring duplicate entry %s", name);
				if(S_ISDIR(st.st_mode)) {
					if(chdir(dent->d_name) < 0)
						perror_msg_and_die(name);
					add2fs_from_dir(fs, nod, squash_uids, squash_perms, fs_timestamp, stats);
					if (chdir("..") == -1)
						perror_msg_and_die("..");
				}
				continue;
			}
			save_nod = 0;
			/* Check for hardlinks */
			if (!S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode) && st.st_nlink > 1) {
				int32 hdlink = is_hardlink(fs, st.st_ino);
				if (hdlink >= 0) {
					add2dir(fs, this_nod, fs->hdlinks.hdl[hdlink].dst_nod, name);
					continue;
				} else {
					save_nod = 1;
				}
			}
			switch(st.st_mode & S_IFMT)
			{
#if HAVE_STRUCT_STAT_ST_RDEV
				case S_IFCHR:
					nod = mknod_fs(fs, this_nod, name, mode|FM_IFCHR, uid, gid, major(st.st_rdev), minor(st.st_rdev), ctime, mtime);
					break;
				case S_IFBLK:
					nod = mknod_fs(fs, this_nod, name, mode|FM_IFBLK, uid, gid, major(st.st_rdev), minor(st.st_rdev), ctime, mtime);
					break;
#endif
				case S_IFIFO:
					nod = mknod_fs(fs, this_nod, name, mode|FM_IFIFO, uid, gid, 0, 0, ctime, mtime);
					break;
				case S_IFSOCK:
					nod = mknod_fs(fs, this_nod, name, mode|FM_IFSOCK, uid, gid, 0, 0, ctime, mtime);
					break;
				case S_IFLNK:
					lnk = xreadlink(dent->d_name);
					mklink_fs(fs, this_nod, name, st.st_size, (uint8*)lnk, uid, gid, ctime, mtime);
					free(lnk);
					break;
				case S_IFREG:
					fh = fopen(dent->d_name, "rb");
					if (!fh) {
						error_msg("Unable to open file %s", dent->d_name);
						break;
					}
					nod = mkfile_fs(fs, this_nod, name, mode, fh, uid, gid, ctime, mtime);
					fclose(fh);
					break;
				case S_IFDIR:
					nod = mkdir_fs(fs, this_nod, name, mode, uid, gid, ctime, mtime);
					if(chdir(dent->d_name) < 0)
						perror_msg_and_die(name);
					add2fs_from_dir(fs, nod, squash_uids, squash_perms, fs_timestamp, stats);
					if (chdir("..") == -1)
						perror_msg_and_die("..");
					break;
				default:
					error_msg("ignoring entry %s", name);
			}
			if (save_nod) {
				if (fs->hdlinks.count == fs->hdlink_cnt) {
					if ((fs->hdlinks.hdl =
						 realloc (fs->hdlinks.hdl, (fs->hdlink_cnt + HDLINK_CNT) *
								  sizeof (struct hdlink_s))) == NULL) {
						error_msg_and_die("Not enough memory");
					}
					fs->hdlink_cnt += HDLINK_CNT;
				}
				fs->hdlinks.hdl[fs->hdlinks.count].src_inode = st.st_ino;
				fs->hdlinks.hdl[fs->hdlinks.count].dst_nod = nod;
				fs->hdlinks.count++;
			}
		}
	}
	closedir(dh);
}

// Copy size blocks from src to dst, putting holes in the output
// file (if possible) if the input block is all zeros.
// Copy size blocks from src to dst, putting holes in the output
// file (if possible) if the input block is all zeros.
static void
copy_file(filesystem *fs, FILE *dst, FILE *src, size_t size)
{
	uint8 *b;

	b = malloc(BLOCKSIZE);
	if (!b)
		error_msg_and_die("copy_file: out of memory");
	if (fseek(src, 0, SEEK_SET))
		perror_msg_and_die("fseek");
	if (ftruncate(fileno(dst), 0))
		perror_msg_and_die("copy_file: ftruncate");
	while (size > 0) {
		if (fread(b, BLOCKSIZE, 1, src) != 1)
			perror_msg_and_die("copy failed on read");
		if ((dst != stdout) && fs->holes && is_blk_empty(b)) {
			/* Empty block, just skip it */
			if (fseek(dst, BLOCKSIZE, SEEK_CUR))
				perror_msg_and_die("fseek");
		} else {
			if (fwrite(b, BLOCKSIZE, 1, dst) != 1)
				perror_msg_and_die("copy failed on write");
		}
		size--;
	}
	free(b);
}

// Allocate a new filesystem structure, allocate internal memory,
// and initialize the contents.
static filesystem *
alloc_fs(int swapit, char *fname, uint32 nbblocks, FILE *srcfile)
{
	filesystem *fs;
	struct stat srcstat, dststat;

	fs = malloc(sizeof(*fs));
	if (!fs)
		error_msg_and_die("not enough memory for filesystem");
	memset(fs, 0, sizeof(*fs));
	fs->swapit = swapit;
	cache_init(&fs->blks, MAX_FREE_CACHE_BLOCKS, blk_elem_val, blk_freed);
	cache_init(&fs->gds, MAX_FREE_CACHE_GDS, gd_elem_val, gd_freed);
	cache_init(&fs->blkmaps, MAX_FREE_CACHE_BLOCKMAPS,
		   blkmap_elem_val, blkmap_freed);
	cache_init(&fs->inodes, MAX_FREE_CACHE_INODES,
		   inode_elem_val, inode_freed);
	fs->hdlink_cnt = HDLINK_CNT;
	fs->hdlinks.hdl = calloc(sizeof(struct hdlink_s), fs->hdlink_cnt);
	if (!fs->hdlinks.hdl)
		error_msg_and_die("Not enough memory");
	fs->hdlinks.count = 0 ;

	if (strcmp(fname, "-") == 0)
		fs->f = tmpfile();
	else if (srcfile) {
		if (fstat(fileno(srcfile), &srcstat))
			perror_msg_and_die("fstat srcfile");
		if (stat(fname, &dststat) == 0
		    && srcstat.st_ino == dststat.st_ino
		    && srcstat.st_dev == dststat.st_dev)
		  {
			// source and destination are the same file, don't
			// truncate or copy, just use the file.
			fs->f = fopen(fname, "r+b");
		} else {
			fs->f = fopen(fname, "w+b");
			if (fs->f)
				copy_file(fs, fs->f, srcfile, nbblocks);
		}
	} else
		fs->f = fopen(fname, "w+b");
	if (!fs->f)
		perror_msg_and_die("opening %s", fname);
	return fs;
}

/* Make sure the output file is the right size */
static void
set_file_size(filesystem *fs)
{
	if (ftruncate(fileno(fs->f),
		      ((off_t) fs->sb->s_blocks_count) * BLOCKSIZE))
		perror_msg_and_die("set_file_size: ftruncate");
}

// initialize an empty filesystem
static filesystem *
init_fs(int nbblocks, int nbinodes, int nbresrvd, int holes,
	uint32 fs_timestamp, uint32 creator_os, int swapit, char *fname)
{
	uint32 i;
	filesystem *fs;
	dirwalker dw;
	uint32 nod, first_block;
	uint32 nbgroups,nbinodes_per_group,overhead_per_group,free_blocks,
		free_blocks_per_group,nbblocks_per_group,min_nbgroups;
	uint32 gdsz,itblsz,bbmpos,ibmpos,itblpos;
	uint32 j;
	uint8 *bbm,*ibm;
	inode *itab0;
	blk_info *bi;
	nod_info *ni;
	groupdescriptor *gd;
	gd_info *gi;
	inode_pos ipos;
	
	if(nbresrvd < 0)
		error_msg_and_die("reserved blocks value is invalid. Note: options have changed, see --help or the man page.");
	if(nbinodes < EXT2_FIRST_INO - 1 + (nbresrvd ? 1 : 0))
		error_msg_and_die("too few inodes. Note: options have changed, see --help or the man page.");
	if(nbblocks < 8)
		error_msg_and_die("too few blocks. Note: options have changed, see --help or the man page.");

	/* nbinodes is the total number of inodes in the system.
	 * a block group can have no more than 8192 inodes.
	 */
	min_nbgroups = (nbinodes + INODES_PER_GROUP - 1) / INODES_PER_GROUP;

	/* On filesystems with 1k block size, the bootloader area uses a full
	 * block. For 2048 and up, the superblock can be fitted into block 0.
	 */
	first_block = (BLOCKSIZE == 1024);

	/* nbblocks is the total number of blocks in the filesystem.
	 * a block group can have no more than 8192 blocks.
	 */
	nbgroups = (nbblocks - first_block + BLOCKS_PER_GROUP - 1) / BLOCKS_PER_GROUP;
	if(nbgroups < min_nbgroups) nbgroups = min_nbgroups;
	nbblocks_per_group = rndup((nbblocks - first_block + nbgroups - 1)/nbgroups, 8);
	nbinodes_per_group = rndup((nbinodes + nbgroups - 1)/nbgroups,
						(BLOCKSIZE/sizeof(inode)));
	if (nbinodes_per_group < 16)
		nbinodes_per_group = 16; //minimum number b'cos the first 10 are reserved

	gdsz = rndup(nbgroups*sizeof(groupdescriptor),BLOCKSIZE)/BLOCKSIZE;
	itblsz = nbinodes_per_group * sizeof(inode)/BLOCKSIZE;
	overhead_per_group = 3 /*sb,bbm,ibm*/ + gdsz + itblsz;
	free_blocks = nbblocks - overhead_per_group*nbgroups - first_block;
	free_blocks_per_group = nbblocks_per_group - overhead_per_group;
	if(free_blocks < 0)
		error_msg_and_die("too much overhead, try fewer inodes or more blocks. Note: options have changed, see --help or the man page.");

	fs = alloc_fs(swapit, fname, nbblocks, NULL);
	fs->sb = calloc(1, SUPERBLOCK_SIZE);
	if (!fs->sb)
		error_msg_and_die("error allocating header memory");

	// create the superblock for an empty filesystem
	fs->sb->s_inodes_count = nbinodes_per_group * nbgroups;
	fs->sb->s_blocks_count = nbblocks;
	fs->sb->s_r_blocks_count = nbresrvd;
	fs->sb->s_free_blocks_count = free_blocks;
	fs->sb->s_free_inodes_count = fs->sb->s_inodes_count - EXT2_FIRST_INO + 1;
	fs->sb->s_first_data_block = first_block;
	fs->sb->s_log_block_size = BLOCKSIZE >> 11;
	fs->sb->s_log_frag_size = BLOCKSIZE >> 11;
	fs->sb->s_blocks_per_group = nbblocks_per_group;
	fs->sb->s_frags_per_group = nbblocks_per_group;
	fs->sb->s_inodes_per_group = nbinodes_per_group;
	fs->sb->s_wtime = fs_timestamp;
	fs->sb->s_magic = EXT2_MAGIC_NUMBER;
	fs->sb->s_lastcheck = fs_timestamp;
	fs->sb->s_creator_os = creator_os;

	set_file_size(fs);

	// set up groupdescriptors
	for(i=0, bbmpos=first_block+1+gdsz, ibmpos=bbmpos+1, itblpos=ibmpos+1;
		i<nbgroups;
		i++, bbmpos+=nbblocks_per_group, ibmpos+=nbblocks_per_group, itblpos+=nbblocks_per_group)
	{
		gd = get_gd(fs, i, &gi);

		if(free_blocks > free_blocks_per_group) {
			gd->bg_free_blocks_count = free_blocks_per_group;
			free_blocks -= free_blocks_per_group;
		} else {
			gd->bg_free_blocks_count = free_blocks;
			free_blocks = 0; // this is the last block group
		}
		if(i)
			gd->bg_free_inodes_count = nbinodes_per_group;
		else
			gd->bg_free_inodes_count = nbinodes_per_group -
							EXT2_FIRST_INO + 2;
		gd->bg_used_dirs_count = 0;
		gd->bg_block_bitmap = bbmpos;
		gd->bg_inode_bitmap = ibmpos;
		gd->bg_inode_table = itblpos;
		put_gd(gi);
	}

	/* Mark non-filesystem blocks and inodes as allocated */
	/* Mark system blocks and inodes as allocated         */
	for(i = 0; i<nbgroups;i++) {
		/* Block bitmap */
		gd = get_gd(fs, i, &gi);
		bbm = GRP_GET_GROUP_BBM(fs, gd, &bi);
		//non-filesystem blocks
		for(j = gd->bg_free_blocks_count
		        + overhead_per_group + 1; j <= BLOCKSIZE * 8; j++)
			allocate(bbm, j); 
		//system blocks
		for(j = 1; j <= overhead_per_group; j++)
			allocate(bbm, j); 
		GRP_PUT_GROUP_BBM(bi);

		/* Inode bitmap */
		ibm = GRP_GET_GROUP_IBM(fs, gd, &bi);
		//non-filesystem inodes
		for(j = fs->sb->s_inodes_per_group+1; j <= BLOCKSIZE * 8; j++)
			allocate(ibm, j);

		//system inodes
		if(i == 0)
			for(j = 1; j < EXT2_FIRST_INO; j++)
				allocate(ibm, j);
		GRP_PUT_GROUP_IBM(bi);
		put_gd(gi);
	}

	// make root inode and directory
	/* We have groups now. Add the root filesystem in group 0 */
	/* Also increment the directory count for group 0 */
	gd = get_gd(fs, 0, &gi);
	gd->bg_free_inodes_count--;
	gd->bg_used_dirs_count = 1;
	put_gd(gi);
	itab0 = get_nod(fs, EXT2_ROOT_INO, &ni);
	itab0->i_mode = FM_IFDIR | FM_IRWXU | FM_IRGRP | FM_IROTH | FM_IXGRP | FM_IXOTH;
	itab0->i_ctime = fs_timestamp;
	itab0->i_mtime = fs_timestamp;
	itab0->i_atime = fs_timestamp;
	itab0->i_size = BLOCKSIZE;
	itab0->i_links_count = 2;
	put_nod(ni);

	new_dir(fs, EXT2_ROOT_INO, ".", 1, &dw);
	shrink_dir(&dw, EXT2_ROOT_INO, "..", 2);
	next_dir(&dw); // Force the data into the buffer
	inode_pos_init(fs, &ipos, EXT2_ROOT_INO, INODE_POS_EXTEND, NULL);
	extend_inode_blk(fs, &ipos, dir_data(&dw), 1);
	inode_pos_finish(fs, &ipos);
	put_dir(&dw);

	// make lost+found directory
	if(fs->sb->s_r_blocks_count)
	{
		inode *node;
		uint8 *b;

		nod = mkdir_fs(fs, EXT2_ROOT_INO, "lost+found", FM_IRWXU,
			       0, 0, fs_timestamp, fs_timestamp);
		b = get_workblk();
		memset(b, 0, BLOCKSIZE);
		((directory*)b)->d_rec_len = BLOCKSIZE;
		inode_pos_init(fs, &ipos, nod, INODE_POS_EXTEND, NULL);
		// It is always 16 blocks to start out with
		for(i = 1; i < 16; i++)
			extend_inode_blk(fs, &ipos, b, 1);
		inode_pos_finish(fs, &ipos);
		free_workblk(b);
		node = get_nod(fs, nod, &ni);
		node->i_size = 16 * BLOCKSIZE;
		put_nod(ni);
	}

	// administrative info
	fs->sb->s_state = 1;
	fs->sb->s_max_mnt_count = 20;

	// options for me
	fs->holes = holes;
	
	return fs;
}

// loads a filesystem from disk
static filesystem *
load_fs(FILE *fh, int swapit, char *fname)
{
	off_t fssize;
	filesystem *fs;

	if((fseek(fh, 0, SEEK_END) < 0) || ((fssize = ftello(fh)) == -1))
		perror_msg_and_die("input filesystem image");
	rewind(fh);
	if ((fssize % BLOCKSIZE) != 0)
		error_msg_and_die("Input file not a multiple of block size");
	fssize /= BLOCKSIZE;
	if(fssize < 16) // totally arbitrary
		error_msg_and_die("too small filesystem");
	fs = alloc_fs(swapit, fname, fssize, fh);

	/* Read and check the superblock, then read the superblock
	 * and all the group descriptors */
	fs->sb = malloc(SUPERBLOCK_SIZE);
	if (!fs->sb)
		error_msg_and_die("error allocating header memory");
	if (fseek(fs->f, SUPERBLOCK_OFFSET, SEEK_SET))
		perror_msg_and_die("fseek");
	if (fread(fs->sb, SUPERBLOCK_SIZE, 1, fs->f) != 1)
		perror_msg_and_die("fread filesystem image superblock");
	if(swapit)
		swap_sb(fs->sb);

	if((fs->sb->s_rev_level > 1) || (fs->sb->s_magic != EXT2_MAGIC_NUMBER))
		error_msg_and_die("not a suitable ext2 filesystem");
	if (fs->sb->s_rev_level > 0) {
		if (fs->sb->s_first_ino != EXT2_GOOD_OLD_FIRST_INO)
			error_msg_and_die("First inode incompatible");
		if (fs->sb->s_inode_size != EXT2_GOOD_OLD_INODE_SIZE)
			error_msg_and_die("inode size incompatible");
		if (fs->sb->s_feature_compat)
			error_msg_and_die("Unsupported compat features");
		if (fs->sb->s_feature_incompat)
			error_msg_and_die("Unsupported incompat features");
		if (fs->sb->s_feature_ro_compat
		    & ~EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
			error_msg_and_die("Unsupported ro compat features");
	}

	set_file_size(fs);
	return fs;
}

static void
free_fs(filesystem *fs)
{
	free(fs->hdlinks.hdl);
	fclose(fs->f);
	free(fs->sb);
	free(fs);
}

// just walk through blocks list
static void
flist_blocks(filesystem *fs, uint32 nod, FILE *fh)
{
	blockwalker bw;
	uint32 bk;
	init_bw(&bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
		fprintf(fh, " %d", bk);
	fprintf(fh, "\n");
}

// walk through blocks list
static void
list_blocks(filesystem *fs, uint32 nod)
{
	int bn = 0;
	blockwalker bw;
	uint32 bk;
	init_bw(&bw);
	printf("blocks in inode %d:", nod);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
		printf(" %d", bk), bn++;
	printf("\n%d blocks (%d bytes)\n", bn, bn * BLOCKSIZE);
}

// saves blocks to FILE*
static void
write_blocks(filesystem *fs, uint32 nod, FILE* f)
{
	blockwalker bw;
	uint32 bk;
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);
	int32 fsize = node->i_size;
	blk_info *bi;

	init_bw(&bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		if(fsize <= 0)
			error_msg_and_die("wrong size while saving inode %d", nod);
		if(fwrite(get_blk(fs, bk, &bi),
			  (fsize > BLOCKSIZE) ? BLOCKSIZE : fsize, 1, f) != 1)
			error_msg_and_die("error while saving inode %d", nod);
		put_blk(bi);
		fsize -= BLOCKSIZE;
	}
	put_nod(ni);
}


// print block/char device minor and major
static void
print_dev(filesystem *fs, uint32 nod)
{
	int minor, major;
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);
	minor = ((uint8*)node->i_block)[0];
	major = ((uint8*)node->i_block)[1];
	put_nod(ni);
	printf("major: %d, minor: %d\n", major, minor);
}

// print an inode as a directory
static void
print_dir(filesystem *fs, uint32 nod)
{
	blockwalker bw;
	uint32 bk;
	init_bw(&bw);
	printf("directory for inode %d:\n", nod);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		directory *d;
		dirwalker dw;
		for (d = get_dir(fs, bk, &dw); d; d = next_dir(&dw))
			if(d->d_inode)
			{
				printf("entry '");
				fwrite(dir_name(&dw), 1, d->d_name_len, stdout);
				printf("' (inode %d): rec_len: %d (name_len: %d)\n", d->d_inode, d->d_rec_len, d->d_name_len);
			}
		put_dir(&dw);
	}
}

// print a symbolic link
static void
print_link(filesystem *fs, uint32 nod)
{
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);

	if(!node->i_blocks)
		printf("links to '%s'\n", (char*)node->i_block);
	else
	{
		printf("links to '");
		write_blocks(fs, nod, stdout);
		printf("'\n");
	}
	put_nod(ni);
}

// make a ls-like printout of permissions
static void
make_perms(uint32 mode, char perms[11])
{
	strcpy(perms, "----------");
	if(mode & FM_IRUSR)
		perms[1] = 'r';
	if(mode & FM_IWUSR)
		perms[2] = 'w';
	if(mode & FM_IXUSR)
		perms[3] = 'x';
	if(mode & FM_IRGRP)
		perms[4] = 'r';
	if(mode & FM_IWGRP)
		perms[5] = 'w';
	if(mode & FM_IXGRP)
		perms[6] = 'x';
	if(mode & FM_IROTH)
		perms[7] = 'r';
	if(mode & FM_IWOTH)
		perms[8] = 'w';
	if(mode & FM_IXOTH)
		perms[9] = 'x';
	if(mode & FM_ISUID)
		perms[3] = 's';
	if(mode & FM_ISGID)
		perms[6] = 's';
	if(mode & FM_ISVTX)
		perms[9] = 't';
	switch(mode & FM_IFMT)
	{
		case 0:
			*perms = '0';
			break;
		case FM_IFSOCK:
			*perms = 's';
			break;
		case FM_IFLNK:
			*perms = 'l';
			break;
		case FM_IFREG:
			*perms = '-';
			break;
		case FM_IFBLK:
			*perms = 'b';
			break;
		case FM_IFDIR:
			*perms = 'd';
			break;
		case FM_IFCHR:
			*perms = 'c';
			break;
		case FM_IFIFO:
			*perms = 'p';
			break;
		default:
			*perms = '?';
	}
}

// print an inode
static void
print_inode(filesystem *fs, uint32 nod)
{
	char *s;
	char perms[11];
	nod_info *ni;
	inode *node = get_nod(fs, nod, &ni);
	blk_info *bi;
	gd_info *gi;

	if(!node->i_mode)
		goto out;
	switch(nod)
	{
		case EXT2_BAD_INO:
			s = "bad blocks";
			break;
		case EXT2_ROOT_INO:
			s = "root";
			break;
		case EXT2_ACL_IDX_INO:
		case EXT2_ACL_DATA_INO:
			s = "ACL";
			break;
		case EXT2_BOOT_LOADER_INO:
			s = "boot loader";
			break;
		case EXT2_UNDEL_DIR_INO:
			s = "undelete directory";
			break;
		default:
			s = (nod >= EXT2_FIRST_INO) ? "normal" : "unknown reserved"; 
	}
	printf("inode %d (%s, %d links): ", nod, s, node->i_links_count);
	if(!allocated(GRP_GET_INODE_BITMAP(fs,nod,&bi,&gi), GRP_IBM_OFFSET(fs,nod)))
	{
		GRP_PUT_INODE_BITMAP(bi,gi);
		printf("unallocated\n");
		goto out;
	}
	GRP_PUT_INODE_BITMAP(bi,gi);
	make_perms(node->i_mode, perms);
	printf("%s,  size: %d byte%s (%d block%s)\n", perms,
	       plural(node->i_size), plural(node->i_blocks / INOBLK));
	switch(node->i_mode & FM_IFMT)
	{
		case FM_IFSOCK:
			list_blocks(fs, nod);
			break;
		case FM_IFLNK:
			print_link(fs, nod);
			break;
		case FM_IFREG:
			list_blocks(fs, nod);
			break;
		case FM_IFBLK:
			print_dev(fs, nod);
			break;
		case FM_IFDIR:
			list_blocks(fs, nod);
			print_dir(fs, nod);
			break;
		case FM_IFCHR:
			print_dev(fs, nod);
			break;
		case FM_IFIFO:
			list_blocks(fs, nod);
			break;
		default:
			list_blocks(fs, nod);
	}
	printf("Done with inode %d\n",nod);
out:
	put_nod(ni);
}

// describes various fields in a filesystem
static void
print_fs(filesystem *fs)
{
	uint32 i;
	blk_info *bi;
	groupdescriptor *gd;
	gd_info *gi;
	uint8 *ibm;

	printf("%d blocks (%d free, %d reserved), first data block: %d\n",
	       fs->sb->s_blocks_count, fs->sb->s_free_blocks_count,
	       fs->sb->s_r_blocks_count, fs->sb->s_first_data_block);
	printf("%d inodes (%d free)\n", fs->sb->s_inodes_count,
	       fs->sb->s_free_inodes_count);
	printf("block size = %d, frag size = %d\n",
	       fs->sb->s_log_block_size ? (fs->sb->s_log_block_size << 11) : 1024,
	       fs->sb->s_log_frag_size ? (fs->sb->s_log_frag_size << 11) : 1024);
	printf("number of groups: %d\n",GRP_NBGROUPS(fs));
	printf("%d blocks per group,%d frags per group,%d inodes per group\n",
	     fs->sb->s_blocks_per_group, fs->sb->s_frags_per_group,
	     fs->sb->s_inodes_per_group);
	printf("Size of inode table: %d blocks\n",
		(int)(fs->sb->s_inodes_per_group * sizeof(inode) / BLOCKSIZE));
	for (i = 0; i < GRP_NBGROUPS(fs); i++) {
		printf("Group No: %d\n", i+1);
		gd = get_gd(fs, i, &gi);
		printf("block bitmap: block %d,inode bitmap: block %d, inode table: block %d\n",
		     gd->bg_block_bitmap,
		     gd->bg_inode_bitmap,
		     gd->bg_inode_table);
		printf("block bitmap allocation:\n");
		print_bm(GRP_GET_GROUP_BBM(fs, gd, &bi),fs->sb->s_blocks_per_group);
		GRP_PUT_GROUP_BBM(bi);
		printf("inode bitmap allocation:\n");
		ibm = GRP_GET_GROUP_IBM(fs, gd, &bi);
		print_bm(ibm, fs->sb->s_inodes_per_group);
		for (i = 1; i <= fs->sb->s_inodes_per_group; i++)
			if (allocated(ibm, i))
				print_inode(fs, i);
		GRP_PUT_GROUP_IBM(bi);
		put_gd(gi);
	}
}

static void
finish_fs(filesystem *fs)
{
	if (cache_flush(&fs->inodes))
		error_msg_and_die("entry mismatch on inode cache flush");
	if (cache_flush(&fs->blkmaps))
		error_msg_and_die("entry mismatch on blockmap cache flush");
	if (cache_flush(&fs->gds))
		error_msg_and_die("entry mismatch on gd cache flush");
	if (cache_flush(&fs->blks))
		error_msg_and_die("entry mismatch on block cache flush");
	if(fs->swapit)
		swap_sb(fs->sb);
	if (fseek(fs->f, SUPERBLOCK_OFFSET, SEEK_SET))
		perror_msg_and_die("fseek");
	if(fwrite(fs->sb, SUPERBLOCK_SIZE, 1, fs->f) != 1)
		perror_msg_and_die("output filesystem superblock");
	if(fs->swapit)
		swap_sb(fs->sb);
}

static void
populate_fs(filesystem *fs, char **dopt, int didx, int squash_uids, int squash_perms, uint32 fs_timestamp, struct stats *stats)
{
	int i;
	for(i = 0; i < didx; i++)
	{
		struct stat st;
		FILE *fh;
		int pdir;
		char *pdest;
		uint32 nod = EXT2_ROOT_INO;
		if(fs)
			if((pdest = strchr(dopt[i], ':')))
			{
				*(pdest++) = 0;
				if(!(nod = find_path(fs, EXT2_ROOT_INO, pdest)))
					error_msg_and_die("path %s not found in filesystem", pdest);
			}
		stat(dopt[i], &st);
		switch(st.st_mode & S_IFMT)
		{
			case S_IFREG:
				fh = xfopen(dopt[i], "rb");
				add2fs_from_file(fs, nod, fh, fs_timestamp, stats);
				fclose(fh);
				break;
			case S_IFDIR:
				if((pdir = open(".", O_RDONLY)) < 0)
					perror_msg_and_die(".");
				if(chdir(dopt[i]) < 0)
					perror_msg_and_die(dopt[i]);
				add2fs_from_dir(fs, nod, squash_uids, squash_perms, fs_timestamp, stats);
				if(fchdir(pdir) < 0)
					perror_msg_and_die("fchdir");
				if(close(pdir) < 0)
					perror_msg_and_die("close");
				break;
			default:
				error_msg_and_die("%s is neither a file nor a directory", dopt[i]);
		}
	}
}

static void
showversion(void)
{
	printf("genext2fs " VERSION "\n");
}

static void
showhelp(void)
{
	fprintf(stderr, "Usage: %s [options] image\n"
	"Create an ext2 filesystem image from directories/files\n\n"
	"  -x, --starting-image <image>\n"
	"  -d, --root <directory>\n"
	"  -D, --devtable <file>\n"
	"  -B, --block-size <bytes>\n"
	"  -b, --size-in-blocks <blocks>\n"
	"  -i, --bytes-per-inode <bytes per inode>\n"
	"  -N, --number-of-inodes <number of inodes>\n"
	"  -m, --reserved-percentage <percentage of blocks to reserve>\n"
	"  -o, --creator-os <os>      'linux' (default), 'hurd', 'freebsd' or number.\n"
	"  -g, --block-map <path>     Generate a block map file for this path.\n"
	"  -e, --fill-value <value>   Fill unallocated blocks with value.\n"
	"  -z, --allow-holes          Allow files with holes.\n"
	"  -f, --faketime             Set filesystem timestamps to 0 (for testing).\n"
	"  -q, --squash               Same as \"-U -P\".\n"
	"  -U, --squash-uids          Squash owners making all files be owned by root.\n"
	"  -P, --squash-perms         Squash permissions on all files.\n"
	"  -h, --help\n"
	"  -V, --version\n"
	"  -v, --verbose\n\n"
	"Report bugs to genext2fs-devel@lists.sourceforge.net\n", app_name);
}

#define MAX_DOPT 128
#define MAX_GOPT 128

#define MAX_FILENAME 255

extern char* optarg;
extern int optind, opterr, optopt;

// parse the value for -o <os>
int
lookup_creator_os(const char *name)
{
        if (isdigit (*name))
                return atoi(name);
        else if (strcasecmp(name, "linux") == 0)
                return EXT2_OS_LINUX;
        else if (strcasecmp(name, "GNU") == 0 || strcasecmp(name, "hurd") == 0)
                return EXT2_OS_HURD;
        else if (strcasecmp(name, "freebsd") == 0)
                return EXT2_OS_FREEBSD;
        else if (strcasecmp(name, "lites") == 0)
                return EXT2_OS_LITES;
        else
                return EXT2_OS_LINUX;
}

int
main(int argc, char **argv)
{
	long long nbblocks = -1;
	int nbinodes = -1;
	int nbresrvd = -1;
	float bytes_per_inode = -1;
	float reserved_frac = -1;
	int fs_timestamp = -1;
	int creator_os = CREATOR_OS;
	char * fsout = "-";
	char * fsin = 0;
	char * dopt[MAX_DOPT];
	int didx = 0;
	char * gopt[MAX_GOPT];
	int gidx = 0;
	int verbose = 0;
	int holes = 0;
	int emptyval = 0;
	int squash_uids = 0;
	int squash_perms = 0;
	uint16 endian = 1;
	int bigendian = !*(char*)&endian;
	char *volumelabel = NULL;
	filesystem *fs;
	int i;
	int c;
	struct stats stats;

#if HAVE_GETOPT_LONG
	struct option longopts[] = {
	  { "starting-image",	required_argument,	NULL, 'x' },
	  { "root",		required_argument,	NULL, 'd' },
	  { "devtable",		required_argument,	NULL, 'D' },
	  { "block-size",	required_argument,	NULL, 'B' },
	  { "size-in-blocks",	required_argument,	NULL, 'b' },
	  { "bytes-per-inode",	required_argument,	NULL, 'i' },
	  { "number-of-inodes",	required_argument,	NULL, 'N' },
	  { "volume-label",     required_argument,      NULL, 'L' },
	  { "reserved-percentage", required_argument,	NULL, 'm' },
	  { "creator-os",	required_argument,	NULL, 'o' },
	  { "block-map",	required_argument,	NULL, 'g' },
	  { "fill-value",	required_argument,	NULL, 'e' },
	  { "allow-holes",	no_argument,		NULL, 'z' },
	  { "faketime",		no_argument,		NULL, 'f' },
	  { "squash",		no_argument,		NULL, 'q' },
	  { "squash-uids",	no_argument,		NULL, 'U' },
	  { "squash-perms",	no_argument,		NULL, 'P' },
	  { "help",		no_argument,		NULL, 'h' },
	  { "version",		no_argument,		NULL, 'V' },
	  { "verbose",		no_argument,		NULL, 'v' },
	  { 0, 0, 0, 0}
	} ;

	app_name = argv[0];

	while((c = getopt_long(argc, argv, "x:d:D:B:b:i:N:L:m:o:g:e:zfqUPhVv", longopts, NULL)) != EOF) {
#else
	app_name = argv[0];

	while((c = getopt(argc, argv,      "x:d:D:B:b:i:N:L:m:o:g:e:zfqUPhVv")) != EOF) {
#endif /* HAVE_GETOPT_LONG */
		switch(c)
		{
			case 'x':
				fsin = optarg;
				break;
			case 'd':
			case 'D':
				dopt[didx++] = optarg;
				break;
			case 'B':
				blocksize = SI_atof(optarg);
				break;
			case 'b':
				nbblocks = SI_atof(optarg);
				break;
			case 'i':
				bytes_per_inode = SI_atof(optarg);
				break;
			case 'N':
				nbinodes = SI_atof(optarg);
				break;
			case 'L':
				volumelabel = optarg;
				break;
			case 'm':
				reserved_frac = SI_atof(optarg) / 100;
				break;
			case 'o':
				creator_os = lookup_creator_os(optarg);
				break;
			case 'g':
				gopt[gidx++] = optarg;
				break;
			case 'e':
				emptyval = atoi(optarg);
				break;
			case 'z':
				holes = 1;
				break;
			case 'f':
				fs_timestamp = 0;
				break;
			case 'q':
				squash_uids = 1;
				squash_perms = 1;
				break;
			case 'U':
				squash_uids = 1;
				break;
			case 'P':
				squash_perms = 1;
				break;
			case 'h':
				showhelp();
				exit(0);
			case 'V':
				showversion();
				exit(0);
			case 'v':
				verbose = 1;
				showversion();
				break;
			default:
				error_msg_and_die("Note: options have changed, see --help or the man page.");
		}
	}

	if(optind < (argc - 1))
		error_msg_and_die("Too many arguments. Try --help or else see the man page.");
	if(optind > (argc - 1))
		error_msg_and_die("Not enough arguments. Try --help or else see the man page.");
	fsout = argv[optind];

	if(blocksize != 1024 && blocksize != 2048 && blocksize != 4096)
		error_msg_and_die("Valid block sizes: 1024, 2048 or 4096.");
	if(creator_os < 0)
		error_msg_and_die("Creator OS unknown.");

	if(fsin)
	{
		if(strcmp(fsin, "-"))
		{
			FILE * fh = xfopen(fsin, "rb");
			fs = load_fs(fh, bigendian, fsout);
			fclose(fh);
		}
		else
			fs = load_fs(stdin, bigendian, fsout);
	}
	else
	{
		if(reserved_frac == -1)
			nbresrvd = nbblocks * RESERVED_BLOCKS;
		else 
			nbresrvd = nbblocks * reserved_frac;

		stats.ninodes = EXT2_FIRST_INO - 1 + (nbresrvd ? 1 : 0);
		stats.nblocks = 0;

		populate_fs(NULL, dopt, didx, squash_uids, squash_perms, fs_timestamp, &stats);

		if(nbinodes == -1)
			nbinodes = stats.ninodes;
		else
			if(stats.ninodes > (unsigned long)nbinodes)
			{
				fprintf(stderr, "number of inodes too low, increasing to %ld\n", stats.ninodes);
				nbinodes = stats.ninodes;
			}

		if(bytes_per_inode != -1) {
			int tmp_nbinodes = nbblocks * BLOCKSIZE / bytes_per_inode;
			if(tmp_nbinodes > nbinodes)
				nbinodes = tmp_nbinodes;
		}
		if(fs_timestamp == -1)
			fs_timestamp = time(NULL);
		fs = init_fs(nbblocks, nbinodes, nbresrvd, holes,
			     fs_timestamp, creator_os, bigendian, fsout);
	}
	if (volumelabel != NULL)
		strncpy((char *)fs->sb->s_volume_name, volumelabel,
			sizeof(fs->sb->s_volume_name));
	
	populate_fs(fs, dopt, didx, squash_uids, squash_perms, fs_timestamp, NULL);

	if(emptyval) {
		uint32 b;
		for(b = 1; b < fs->sb->s_blocks_count; b++) {
			blk_info *bi;
			gd_info *gi;
			if(!allocated(GRP_GET_BLOCK_BITMAP(fs,b,&bi,&gi),
				      GRP_BBM_OFFSET(fs,b))) {
				blk_info *bi2;
				memset(get_blk(fs, b, &bi2), emptyval,
				       BLOCKSIZE);
				put_blk(bi2);
			}
			GRP_PUT_BLOCK_BITMAP(bi,gi);
		}
	}
	if(verbose)
		print_fs(fs);
	for(i = 0; i < gidx; i++)
	{
		uint32 nod;
		char fname[MAX_FILENAME];
		char *p;
		FILE *fh;
		nod_info *ni;
		if(!(nod = find_path(fs, EXT2_ROOT_INO, gopt[i])))
			error_msg_and_die("path %s not found in filesystem", gopt[i]);
		while((p = strchr(gopt[i], '/')))
			*p = '_';
		SNPRINTF(fname, MAX_FILENAME-1, "%s.blk", gopt[i]);
		fh = xfopen(fname, "wb");
		fprintf(fh, "%d:", get_nod(fs, nod, &ni)->i_size);
		put_nod(ni);
		flist_blocks(fs, nod, fh);
		fclose(fh);
	}
	finish_fs(fs);
	if(strcmp(fsout, "-") == 0)
		copy_file(fs, stdout, fs->f, fs->sb->s_blocks_count);

	free_fs(fs);
	return 0;
}
