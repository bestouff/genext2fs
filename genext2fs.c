/* vi: set sw=8 ts=8: */
// genext2fs.c
//
// ext2 filesystem generator for embedded systems
// Copyright (C) 2000 Xavier Bestel <xavier.bestel@free.fr>
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
//     5 Jan 2003  Bugfixes: reserved inodes should be set vsundar@usc.edu
//             only in the first group; directory names
//             need to be null padded at the end; and 
//             number of blocks per group should be a 
//             multiple of 8. Updated md5 values. 
//     6 Jan 2003  Erik Andersen <andersee@debian.org> added
//                         mkfs.jffs2 compatible device table support,
//                         along with -q, -P, -U


// `genext2fs' is a mean to generate an ext2 filesystem
// as a normal (non-root) user. It doesn't require you to mount
// the image file to copy files on it. It doesn't even require
// you to be the superuser to make device nodes.
//
// Example usage:
//
// # genext2fs -b 1440 -d srcdir /dev/fd0
//
// All files in the srcdir directory will be written to /dev/fd0 as
// a new ext2 filesystem image. You can then mount the floppy as
// usual.
//
// # genext2fs -b 1024 -d builddir -D device_table.txt flashdisk.img
//
// This one would build a filesystem from all the files in builddir,
// then would read the device_table.txt file and make apropriate nodes.
// The format for the device table file is covered in detail in the sample
// device_table.txt file provided with the genext2fs source.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <getopt.h>

#define HASH_SIZE	311		/* Should be prime */
#define hash_inode(i)	((i) % HASH_SIZE)

typedef struct ino_dev_hash_bucket_struct {
	struct ino_dev_hash_bucket_struct *next;
	ino_t ino;
	dev_t dev;
	char name[1];
} ino_dev_hashtable_bucket_t;

static ino_dev_hashtable_bucket_t *ino_dev_hashtable[HASH_SIZE];

struct stats {
	unsigned long nblocks;
	unsigned long ninodes;
};

int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name)
{
	ino_dev_hashtable_bucket_t *bucket;

	bucket = ino_dev_hashtable[hash_inode(statbuf->st_ino)];
	while (bucket != NULL) {
		if ((bucket->ino == statbuf->st_ino) &&
		    (bucket->dev == statbuf->st_dev))
		{
			if (name) *name = bucket->name;
			return 1;
		}
		bucket = bucket->next;
	}
	return 0;
}

/* Add statbuf to statbuf hash table */
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name)
{
	int i;
	size_t s;
	ino_dev_hashtable_bucket_t *bucket;
    
	i = hash_inode(statbuf->st_ino);
	s = name ? strlen(name) : 0;
	bucket = malloc(sizeof(ino_dev_hashtable_bucket_t) + s);
	bucket->ino = statbuf->st_ino;
	bucket->dev = statbuf->st_dev;
	if (name)
		strcpy(bucket->name, name);
	else
		bucket->name[0] = '\0';
	bucket->next = ino_dev_hashtable[i];
	ino_dev_hashtable[i] = bucket;
}

/* Clear statbuf hash table */
void reset_ino_dev_hashtable(void)
{
	int i;
	ino_dev_hashtable_bucket_t *bucket;

	for (i = 0; i < HASH_SIZE; i++) {
		while (ino_dev_hashtable[i] != NULL) {
			bucket = ino_dev_hashtable[i]->next;
			free(ino_dev_hashtable[i]);
			ino_dev_hashtable[i] = bucket;
		}
	}
}

static int count_ino_in_hashtable(void)
{
	long count = 0;
	int i;

	for (i = 0; i < HASH_SIZE; i++) {
		ino_dev_hashtable_bucket_t *bucket = ino_dev_hashtable[i];
		while (bucket != NULL) {
			count++;
			bucket = bucket->next;
		}
	}

	return count;
}

// block size

#define BLOCKSIZE         1024
#define BLOCKS_PER_GROUP  8192
#define BYTES_PER_INODE   (8*BLOCKSIZE)
/* Percentage of blocks that are reserved.*/
#define RESERVED_INODES   5/100


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

// end of a block walk

#define WALK_END           0xFFFFFFFE


// file modes

#define FM_IFMT    0xF000	// format mask
#define FM_IFLNK   0xA000	// socket
#define FM_IFSOCK  0xC000	// symbolic link
#define FM_IFREG   0x8000	// regular file
#define FM_IFBLK   0x6000	// block device
#define FM_IFDIR   0x4000	// directory
#define FM_IFCHR   0x2000	// character device
#define FM_IFIFO   0x1000	// fifo

#define FM_IMASK   0x0FFF
#define FM_ISUID   0x0800	// SUID
#define FM_ISGID   0x0400	// SGID
#define FM_ISVTX   0x0200	// sticky bit

#define FM_IRWXU   0x01C0	// user mask
#define FM_IRUSR   0x0100	// read
#define FM_IWUSR   0x0080	// write
#define FM_IXUSR   0x0040	// execute

#define FM_IRWXG   0x0038	// group mask
#define FM_IRGRP   0x0020	// read
#define FM_IWGRP   0x0010	// write
#define FM_IXGRP   0x0008	// execute

#define FM_IRWXO   0x0007	// other mask
#define FM_IROTH   0x0004	// read
#define FM_IWOTH   0x0002	// write
#define FM_IXOTH   0x0001	// execute


// options

#define OP_HOLES     0x01       // make files with holes

/* Defines for accessing group details */

// Number of groups in the filesystem
#define GRP_NBGROUPS(fs) (((fs)->sb.s_blocks_count-1)/(fs)->sb.s_blocks_per_group )

// Get group block bitmap (bbm) given the group number
#define GRP_GET_GROUP_BBM(fs,grp) ( get_blk((fs),(fs)->gd[(grp)].bg_block_bitmap) )

// Get group inode bitmap (ibm) given the group number
#define GRP_GET_GROUP_IBM(fs,grp) ( get_blk((fs),(fs)->gd[(grp)].bg_inode_bitmap) )
		
// Given an inode number find the group it belongs to
#define GRP_GROUP_OF_INODE(fs,nod) ( ((nod)-1) / (fs)->sb.s_inodes_per_group)

//Given an inode number get the inode bitmap that covers it
#define GRP_GET_INODE_BITMAP(fs,nod) \
	( GRP_GET_GROUP_IBM((fs),GRP_GROUP_OF_INODE((fs),(nod))) )

//Given an inode number find its offset within the inode bitmap that covers it
#define GRP_IBM_OFFSET(fs,nod) \
	( (nod) - GRP_GROUP_OF_INODE((fs),(nod))*(fs)->sb.s_inodes_per_group )

// Given a block number find the group it belongs to
#define GRP_GROUP_OF_BLOCK(fs,blk) ( ((blk)-1) / (fs)->sb.s_blocks_per_group)
	
//Given a block number get the block bitmap that covers it
#define GRP_GET_BLOCK_BITMAP(fs,blk) \
	( GRP_GET_GROUP_BBM((fs),GRP_GROUP_OF_BLOCK((fs),(blk))) )

//Given a block number find its offset within the block bitmap that covers it
#define GRP_BBM_OFFSET(fs,blk) \
	( (blk) - GRP_GROUP_OF_BLOCK((fs),(blk))*(fs)->sb.s_blocks_per_group )


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
// without the buffer overrun checking,
// and the correct getcwd() size argument for automatic allocation,
// which of course is not the same on Solaris, old glibc and new
// glibc ...

#ifdef __GNUC__
#define SCANF_PREFIX "a"
#define SCANF_STRING(s) (&s)
#define GETCWD_SIZE 0
#else
#define SCANF_PREFIX "511"
#define SCANF_STRING(s) (s = malloc(512))
#define GETCWD_SIZE -1
inline int snprintf(char *str, size_t n, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsprintf(str, fmt, ap);
	va_end(ap);
	return ret;
}
#endif


// endianness swap

inline uint16 swab16(uint16 val)
{
	return (val >> 8) | (val << 8);
}

inline uint32 swab32(uint32 val)
{
	return ((val>>24) | ((val>>8)&0xFF00) |
			((val<<8)&0xFF0000) | (val<<24));
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
	udecl16(s_def_resgid)          /* The default gid for reserved blocks */

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
#define decl16(x) int16 x;
#define udecl16(x) uint16 x;
#define decl32(x) int32 x;
#define udecl32(x) uint32 x;
#define utdecl32(x,n) uint32 x[n];

typedef struct
{
	superblock_decl
	uint32 s_reserved[235];       // Reserved
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
	char d_name[0];
} directory;

typedef uint8 block[BLOCKSIZE];

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


/* Filesystem structure that support groups */
#if BLOCKSIZE == 1024
typedef struct
{
	block zero;          // The famous block 0
	superblock sb;       // The superblock
	groupdescriptor gd[0]; // The group descriptors
} filesystem;
#else
#error UNHANDLED BLOCKSIZE
#endif

// now the endianness swap

#undef decl8
#undef udecl8
#undef decl16
#undef udecl16
#undef decl32
#undef udecl32
#undef utdecl32

#define decl8(x)
#define udecl8(x)
#define decl16(x) this->x = swab16(this->x);
#define udecl16(x) this->x = swab16(this->x);
#define decl32(x) this->x = swab32(this->x);
#define udecl32(x) this->x = swab32(this->x);
#define utdecl32(x,n) { int i; for(i=0; i<n; i++) this->x[i] = swab32(this->x[i]); }

void swap_sb(superblock *sb)
{
#define this sb
	superblock_decl
#undef this
}

void swap_gd(groupdescriptor *gd)
{
#define this gd
	groupdescriptor_decl
#undef this
}

void swap_nod(inode *nod)
{
#define this nod
	inode_decl
#undef this
}

void swap_dir(directory *dir)
{
#define this dir
	directory_decl
#undef this
}

void swap_block(block b)
{
	int i;
	uint32 *blk = (uint32*)b;
	for(i = 0; i < BLOCKSIZE/4; i++)
		blk[i] = swab32(blk[i]);
}

#undef decl8
#undef udecl8
#undef decl16
#undef udecl16
#undef decl32
#undef udecl32
#undef utdecl32

char * argv0;
static char * app_name;
static int squash_uids = 0;
static int squash_perms = 0;
static const char *const memory_exhausted = "memory exhausted";

// error (un)handling
static void verror_msg(const char *s, va_list p)
{
	fflush(stdout);
	fprintf(stderr, "%s: ", app_name);
	vfprintf(stderr, s, p);
}
static void error_msg(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	verror_msg(s, p);
	va_end(p);
	putc('\n', stderr);
}

static void error_msg_and_die(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	verror_msg(s, p);
	va_end(p);
	putc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void vperror_msg(const char *s, va_list p)
{
	int err = errno;
	if (s == 0)
		s = "";
	verror_msg(s, p);
	if (*s)
		s = ": ";
	fprintf(stderr, "%s%s\n", s, strerror(err));
}
#if 0
static void perror_msg(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	vperror_msg(s, p);
	va_end(p);
}
#endif
static void perror_msg_and_die(const char *s, ...)
{
	va_list p;
	va_start(p, s);
	vperror_msg(s, p);
	va_end(p);
	exit(EXIT_FAILURE);
}

static FILE *xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		perror_msg_and_die("%s", path);
	return fp;
}

static char *xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;
	t = strdup(s);
	if (t == NULL)
		error_msg_and_die(memory_exhausted);
	return t;
}

extern void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		error_msg_and_die(memory_exhausted);
	return ptr;
}

static char *xreadlink(const char *path)
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

// printf helper macro
#define plural(a) (a), ((a) > 1) ? "s" : ""

// temporary working block
inline uint8 * get_workblk(void)
{
	unsigned char* b=calloc(1,BLOCKSIZE);
	return b;
}
inline void free_workblk(block b)
{
	free(b);
}

/* Rounds qty upto a multiple of siz. siz should be a power of 2 */
uint32 rndup(uint32 qty, uint32 siz)
{
	return (qty + (siz - 1)) & ~(siz - 1);
}

// check if something is allocated in the bitmap
inline uint32 allocated(block b, uint32 item)
{
	return b[(item-1) / 8] & (1 << ((item-1) % 8));
}

// return a given block from a filesystem
inline uint8 * get_blk(filesystem *fs, uint32 blk)
{
	return (uint8*)fs + blk*BLOCKSIZE;
}

// return a given inode from a filesystem
inline inode * get_nod(filesystem *fs, uint32 nod)
{
	int grp,offset;
	inode *itab;

	offset = GRP_IBM_OFFSET(fs,nod);
	grp = GRP_GROUP_OF_INODE(fs,nod);
	itab = (inode *)get_blk(fs, fs->gd[grp].bg_inode_table);
	return itab+offset-1;
}

// allocate a given block/inode in the bitmap
// allocate first free if item == 0
uint32 allocate(block b, uint32 item)
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
void deallocate(block b, uint32 item)
{
	b[(item-1) / 8] &= ~(1 << ((item-1) % 8));
}

// allocate a block
uint32 alloc_blk(filesystem *fs, uint32  nod)
{
	uint32 bk=0;
	uint32 grp,nbgroups;

	grp = nod/fs->sb.s_inodes_per_group;
	nbgroups = ( fs->sb.s_blocks_count - fs->sb.s_first_data_block + fs->sb.s_blocks_per_group -1 ) / 
					fs->sb.s_blocks_per_group;
	if(!(bk = allocate(get_blk(fs,fs->gd[grp].bg_block_bitmap), 0))) {
		for(grp=0;grp<nbgroups && !bk;grp++)
			bk=allocate(get_blk(fs,fs->gd[grp].bg_block_bitmap),0);
		grp--;
	}
	if (!bk)
		error_msg_and_die("couldn't allocate a block (no free space)");
	if(!(fs->gd[grp].bg_free_blocks_count--))
		error_msg_and_die("group descr %d. free blocks count == 0 (corrupted fs?)",grp);
	if(!(fs->sb.s_free_blocks_count--))
		error_msg_and_die("superblock free blocks count == 0 (corrupted fs?)");
	return fs->sb.s_blocks_per_group*grp + bk;
}

// allocate an inode
uint32 alloc_nod(filesystem *fs)
{
	uint32 nod=0,best_group=0;
	uint32 grp,nbgroups,avefreei;

	nbgroups = ( fs->sb.s_blocks_count - fs->sb.s_first_data_block + fs->sb.s_blocks_per_group -1 ) / 
					fs->sb.s_blocks_per_group;

	/* Distribute inodes amongst all the blocks                           */
	/* For every block group with more than average number of free inodes */
	/* find the one with the most free blocks and allocate node there     */
	/* Idea from find_group_dir in fs/ext2/ialloc.c in 2.4.19 kernel      */
	/* We do it for all inodes.                                           */
	avefreei  =  fs->sb.s_free_inodes_count / nbgroups;
	for(grp=0;grp<nbgroups && !nod;grp++) {
		if (fs->gd[grp].bg_free_inodes_count < avefreei)
			continue;
		if (!best_group || 
			fs->gd[grp].bg_free_blocks_count > fs->gd[best_group].bg_free_blocks_count)
			best_group = grp;
	}
	if (!(nod = allocate(get_blk(fs,fs->gd[best_group].bg_inode_bitmap),0)))
		error_msg_and_die("couldn't allocate an inode (no free inode)");
	if(!(fs->gd[best_group].bg_free_inodes_count--))
		error_msg_and_die("group descr. free blocks count == 0 (corrupted fs?)");
	if(!(fs->sb.s_free_inodes_count--))
		error_msg_and_die("superblock free blocks count == 0 (corrupted fs?)");
	return fs->sb.s_inodes_per_group*best_group+nod;
}

// print a bitmap allocation
void print_bm(block b, uint32 max)
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
void init_bw(filesystem *fs, uint32 nod, blockwalker *bw)
{
	bw->bnum = 0;
	bw->bpdir = EXT2_INIT_BLOCK;
}

// return next block of inode (WALK_END for end)
// if create>0, append a newly allocated block at the end
// if hole!=0, create a hole in the file
uint32 walk_bw(filesystem *fs, uint32 nod, blockwalker *bw, uint32 *create, uint32 hole)
{
	uint32 *bkref = 0;
	uint32 *b;
	int extend = 0;
	if(bw->bnum >= get_nod(fs, nod)->i_blocks / INOBLK)
	{
		if(create && (*create)--)
			extend = 1;
		else	
			return WALK_END;
	}
	// first direct block
	if(bw->bpdir == EXT2_INIT_BLOCK)
	{
		bkref = &get_nod(fs, nod)->i_block[bw->bpdir = 0];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// direct block
	else if(bw->bpdir < EXT2_NDIR_BLOCKS)
	{
		bkref = &get_nod(fs, nod)->i_block[++bw->bpdir];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// first block in indirect block
	else if(bw->bpdir == EXT2_NDIR_BLOCKS)
	{
		bw->bnum++;
		bw->bpdir = EXT2_IND_BLOCK;
		bw->bpind = 0;
		if(extend) // allocate indirect block
			get_nod(fs, nod)->i_block[bw->bpdir] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		bkref = &b[bw->bpind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// block in indirect block
	else if((bw->bpdir == EXT2_IND_BLOCK) && (bw->bpind < BLOCKSIZE/4 - 1))
	{
		bw->bpind++;
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		bkref = &b[bw->bpind];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// first block in first indirect block in first double indirect block
	else if(bw->bpdir == EXT2_IND_BLOCK)
	{
		bw->bnum += 2;
		bw->bpdir = EXT2_DIND_BLOCK;
		bw->bpind = 0;
		bw->bpdind = 0;
		if(extend) // allocate double indirect block
			get_nod(fs, nod)->i_block[bw->bpdir] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		if(extend) // allocate first indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		bkref = &b[bw->bpdind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// block in indirect block in double indirect block
	else if((bw->bpdir == EXT2_DIND_BLOCK) && (bw->bpdind < BLOCKSIZE/4 - 1))
	{
		bw->bpdind++;
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		bkref = &b[bw->bpdind];
		if(extend) // allocate block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	// first block in indirect block in double indirect block
	else if((bw->bpdir == EXT2_DIND_BLOCK) && (bw->bpind < BLOCKSIZE/4 - 1))
	{
		bw->bnum++;
		bw->bpdind = 0;
		bw->bpind++;
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		if(extend) // allocate indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		bkref = &b[bw->bpdind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
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
			get_nod(fs, nod)->i_block[bw->bpdir] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		if(extend) // allocate first double indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		if(extend) // allocate first indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpdind]);
		bkref = &b[bw->bptind];
		if(extend) // allocate first data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	/* Still processing a single indirect block down the indirection
	   chain.Allocate a data block for it
	 */
	else if ( (bw->bpdir == EXT2_TIND_BLOCK) && 
		  (bw->bptind < BLOCKSIZE/4 -1) )
	{
		bw->bptind++;
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		b = (uint32*)get_blk(fs, b[bw->bpdind]);
		bkref = &b[bw->bptind];
		if(extend) // allocate data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
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
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		if (extend) // allocate single indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpdind]);
		bkref = &b[bw->bptind];
		if(extend) // allocate first data block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
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
		b = (uint32*)get_blk(fs, get_nod(fs, nod)->i_block[bw->bpdir]);
		if(extend) // allocate double indirect block
			b[bw->bpind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpind]);
		if(extend) // allocate single indirect block
			b[bw->bpdind] = alloc_blk(fs,nod);
		b = (uint32*)get_blk(fs, b[bw->bpdind]);
		bkref = &b[bw->bptind];
		if(extend) // allocate first block
			*bkref = hole ? 0 : alloc_blk(fs,nod);
	}
	else
		error_msg_and_die("file too big !"); 
	/* End change for walking triple indirection */

	if(*bkref)
	{
		bw->bnum++;
		if(!allocated(GRP_GET_BLOCK_BITMAP(fs,*bkref), GRP_BBM_OFFSET(fs,*bkref)))
			error_msg_and_die("[block %d of inode %d is unallocated !]", *bkref, nod);
	}
	if(extend)
		get_nod(fs, nod)->i_blocks = bw->bnum * INOBLK;
	return *bkref;
}

// add blocks to an inode (file/dir/etc...)
void extend_blk(filesystem *fs, uint32 nod, block b, int amount)
{
	int create = amount;
	blockwalker bw, lbw;
	uint32 bk;
	init_bw(fs, nod, &bw);
	lbw = bw;
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
		lbw = bw;
	bw = lbw;
	while(create)
	{
		int i, copyb = 0;
		if(!(fs->sb.s_reserved[200] & OP_HOLES))
			copyb = 1;
		else
			for(i = 0; i < BLOCKSIZE / 4; i++)
				if(((int32*)(b + BLOCKSIZE * (amount - create)))[i])
				{
					copyb = 1;
					break;
				}
		if((bk = walk_bw(fs, nod, &bw, &create, !copyb)) == WALK_END)
			break;
		if(copyb)
			memcpy(get_blk(fs, bk), b + BLOCKSIZE * (amount - create - 1), BLOCKSIZE);
	}
}

// link an entry (inode #) to a directory
void add2dir(filesystem *fs, uint32 dnod, uint32 nod, const char* name, uint32 mode, uid_t uid, gid_t gid, time_t ctime)
{
	blockwalker bw;
	uint32 bk;
	uint8 *b;
	directory *d;
	int reclen, nlen;
	inode *node;
	inode *pnode;

	/* Squash all permissions so files are owned by root 
	 * and file permissions have group/other perms removed */
	if (squash_uids) {
		uid = gid = 0;
	}
	if (squash_perms) {
		if (!S_ISLNK(mode)) {
			mode &= ~(S_IWGRP | S_IWOTH);
			mode &= ~(S_ISUID | S_ISGID);
		}
	}

	pnode = get_nod(fs, dnod);
	if(!S_ISDIR(pnode->i_mode))
		error_msg_and_die("can't add '%s' to a non-directory", name);
	if(!*name)
		error_msg_and_die("bad name '%s' (not meaningful)", name);
	if(strchr(name, '/'))
		error_msg_and_die("bad name '%s' (contains a slash)", name);
	nlen = strlen(name);
	reclen = sizeof(directory) + rndup(nlen, 4);
	if(reclen > BLOCKSIZE)
		error_msg_and_die("bad name '%s' (too long)", name);
	init_bw(fs, dnod, &bw);
	while((bk = walk_bw(fs, dnod, &bw, 0, 0)) != WALK_END) // for all blocks in dir
	{
		b = get_blk(fs, bk);
		// for all dir entries in block
		for(d = (directory*)b; (int8*)d + sizeof(*d) < (int8*)b + BLOCKSIZE; d = (directory*)((int8*)d + d->d_rec_len))
		{
			// if empty dir entry, large enough, use it
			if((!d->d_inode) && (d->d_rec_len >= reclen))
			{
				d->d_inode = nod;
				node = get_nod(fs, nod);
				node->i_links_count++;
				d->d_name_len = nlen;
				strncpy(d->d_name, name, rndup(nlen,4));
				node->i_mode = mode;
				node->i_uid = uid;
				node->i_gid = gid;
				node->i_atime = ctime;
				node->i_ctime = ctime;
				node->i_mtime = ctime;
				return;
			}
			// if entry with enough room (last one?), shrink it & use it
			if(d->d_rec_len >= (sizeof(directory) + rndup(d->d_name_len, 4) + reclen))
			{
				reclen = d->d_rec_len;
				d->d_rec_len = sizeof(directory) + rndup(d->d_name_len, 4);
				reclen -= d->d_rec_len;
				d = (directory*) (((int8*)d) + d->d_rec_len);
				d->d_rec_len = reclen;
				d->d_inode = nod;
				node = get_nod(fs, nod);
				node->i_links_count++;
				d->d_name_len = nlen;
				strncpy(d->d_name, name, rndup(nlen,4));
				node->i_mode = mode;
				node->i_uid = uid;
				node->i_gid = gid;
				node->i_atime = ctime;
				node->i_ctime = ctime;
				node->i_mtime = ctime;
				return;
			}
		}
	}
	// we found no free entry in the directory, so we add a block
	b = get_workblk();
	d = (directory*)b;
	d->d_inode = nod;
	node = get_nod(fs, nod);
	node->i_links_count++;
	d->d_rec_len = BLOCKSIZE;
	d->d_name_len = nlen;
	strncpy(d->d_name, name, rndup(nlen,4));
	node->i_mode = mode;
	node->i_uid = uid;
	node->i_gid = gid;
	node->i_atime = ctime;
	node->i_ctime = ctime;
	node->i_mtime = ctime;
	extend_blk(fs, dnod, b, 1);
	get_nod(fs, dnod)->i_size += BLOCKSIZE;
	free_workblk(b);
}

// find an entry in a directory
uint32 find_dir(filesystem *fs, uint32 nod, const char * name)
{
	blockwalker bw;
	uint32 bk;
	int nlen = strlen(name);
	init_bw(fs, nod, &bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		directory *d;
		uint8 *b;
		b = get_blk(fs, bk);
		for(d = (directory*)b; (int8*)d + sizeof(*d) < (int8*)b + BLOCKSIZE; d = (directory*)((int8*)d + d->d_rec_len))
			if(d->d_inode && (nlen == d->d_name_len) && !strncmp(d->d_name, name, nlen))
				return d->d_inode;
	}
	return 0;
}

// find the inode of a full path
uint32 find_path(filesystem *fs, uint32 nod, const char * name)
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

// make a full-fledged directory (i.e. with "." & "..")
uint32 mkdir_fs(filesystem *fs, uint32 parent_nod, const char *name, uint32 mode,
	uid_t uid, gid_t gid, time_t ctime)
{
	uint32 nod;
        inode *parent_node;
	if((nod = find_dir(fs, parent_nod, name)))
		return nod;
	nod = alloc_nod(fs);
	parent_node = get_nod(fs, parent_nod);
	mode |= FM_IFDIR;
	add2dir(fs, parent_nod, nod, name, mode, uid, gid, ctime);
	add2dir(fs, nod, nod, ".", mode, uid, gid, ctime);
	add2dir(fs, nod, parent_nod, "..", parent_node->i_mode,
	        parent_node->i_uid, parent_node->i_gid, parent_node->i_ctime);
	fs->gd[GRP_GROUP_OF_INODE(fs,nod)].bg_used_dirs_count++;
	return nod;
}

// make a symlink
uint32 mklink_fs(filesystem *fs, uint32 parent_nod, const char *name, size_t size,
	uint8 * b, uid_t uid, gid_t gid, time_t ctime)
{
	uint32 mode;
	uint32 nod = alloc_nod(fs);
	mode = FM_IFLNK | FM_IRWXU | FM_IRWXG | FM_IRWXO; 
	get_nod(fs, nod)->i_mode = FM_IFLNK | FM_IRWXU | FM_IRWXG | FM_IRWXO;
	get_nod(fs, nod)->i_size = size;
	add2dir(fs, parent_nod, nod, name, mode, uid, gid, ctime);
	if(size <= 4 * (EXT2_TIND_BLOCK+1))
	{
		strncpy((char*)get_nod(fs, nod)->i_block, (char*)b, size);
		return nod;
	}
	extend_blk(fs, nod, b, rndup(size, BLOCKSIZE) / BLOCKSIZE);
	return nod;
}

// make a file from a FILE*
uint32 mkfile_fs(filesystem *fs, uint32 parent_nod, const char *name, uint32 mode, size_t size, FILE *f, uid_t uid, gid_t gid, time_t ctime)
{
	uint8 * b;
	uint32 nod = alloc_nod(fs);
	mode |= FM_IFREG;
	get_nod(fs, nod)->i_size = size;
	add2dir(fs, parent_nod, nod, name, mode, uid, gid, ctime);
	if(!(b = (uint8*)malloc(rndup(size, BLOCKSIZE))))
		error_msg_and_die("not enough mem to read file '%s'", name);
	memset(b, 0,rndup(size, BLOCKSIZE));
	if(f)
		fread(b, size, 1, f);
	else
		memset(b, 0, size);
	extend_blk(fs, nod, b, rndup(size, BLOCKSIZE) / BLOCKSIZE);
	free(b);
	return nod;
}

// retrieves a mode info from a struct stat
uint32 get_mode(struct stat *st)
{
	uint32 mode = 0;

	/* Squash file permissions as needed */
	if (squash_perms) {
		if (!S_ISLNK(mode)) {
			st->st_mode &= ~(S_IWGRP | S_IWOTH);
			st->st_mode &= ~(S_ISUID | S_ISGID);
		}
	}

	if(st->st_mode & S_IRUSR)
		mode |= FM_IRUSR | FM_IRGRP | FM_IROTH;
	if(st->st_mode & S_IWUSR)
		mode |= FM_IWUSR | FM_IWGRP | FM_IWOTH;
	if(st->st_mode & S_IXUSR)
		mode |= FM_IXUSR | FM_IXGRP | FM_IXOTH;
	return mode;
}

// basename of a path - free me
char * basename(const char * fullpath)
{
	char * p = strrchr(fullpath, '/');
	return xstrdup(p ? p + 1 : fullpath);
}

// dirname of a path - free me
char * dirname(const char * fullpath)
{
	char * p, * n = xstrdup(fullpath);
	if((p = strrchr(n, '/')))
		*(p+1) = 0;
	else
		*n = 0;
	return n;
}

void stats_from_dir(struct stats *stats)
{
	DIR *dh;
	struct dirent *dent;
	struct stat st;
	if(!(dh = opendir(".")))
		perror_msg_and_die(".");
	while((dent = readdir(dh)))
	{
		if((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, "..")))
			continue;
		lstat(dent->d_name, &st);
		if (S_ISLNK(st.st_mode)) {
			stats->ninodes++;
		} else if (S_ISDIR(st.st_mode)) {
			if(chdir(dent->d_name) < 0)
				perror_msg_and_die(dent->d_name);
			stats->ninodes++;
			stats_from_dir(stats);
			chdir("..");
		} else {
			if (!is_in_ino_dev_hashtable(&st, NULL)) {
				add_to_ino_dev_hashtable(&st, NULL);
				stats->nblocks += (st.st_blocks >> 1);
				stats->ninodes++;
			}
		}
	}
	closedir(dh);
	reset_ino_dev_hashtable();
}

// adds a tree of entries to the filesystem from current dir
void add2fs_from_dir(filesystem *fs, uint32 this_nod)
{
	uint32 nod;
	FILE *fh;
	DIR *dh;
	struct dirent *dent;
	struct stat st;
	uint8 *b;
	if(!(dh = opendir(".")))
		perror_msg_and_die(".");
	while((dent = readdir(dh)))
	{
		if((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, "..")))
			continue;
		lstat(dent->d_name, &st);
		switch(st.st_mode & S_IFMT)
		{
			case S_IFCHR:
			case S_IFBLK:
				nod = alloc_nod(fs);
				get_nod(fs, nod)->i_mode = (((st.st_mode & S_IFMT) == S_IFCHR) ? FM_IFCHR : FM_IFBLK) | get_mode(&st);
				((uint8*)get_nod(fs, nod)->i_block)[0] = (st.st_rdev & 0xff);
				((uint8*)get_nod(fs, nod)->i_block)[1] = (st.st_rdev >> 8);
				add2dir(fs, this_nod, nod, dent->d_name, st.st_mode, st.st_uid, st.st_gid, st.st_ctime);
				break;
			case S_IFIFO:
				nod = alloc_nod(fs);
				get_nod(fs, nod)->i_mode = FM_IFIFO | get_mode(&st);
				add2dir(fs, this_nod, nod, dent->d_name, st.st_mode, st.st_uid, st.st_gid, st.st_ctime);
				break;
			case S_IFLNK:
				b = xreadlink(dent->d_name);
				mklink_fs(fs, this_nod, dent->d_name, st.st_size, b, st.st_uid, st.st_gid, st.st_ctime);
				free(b);
				break;
			case S_IFREG:
				fh = xfopen(dent->d_name, "r");
				mkfile_fs(fs, this_nod, dent->d_name, st.st_mode, st.st_size, fh, st.st_uid, st.st_gid, st.st_ctime);
				fclose(fh);
				break;
			case S_IFDIR:
				nod = mkdir_fs(fs, this_nod, dent->d_name, st.st_mode, st.st_uid, st.st_gid, st.st_ctime);
				if(chdir(dent->d_name) < 0)
					perror_msg_and_die(dent->d_name);
				add2fs_from_dir(fs, nod);
				chdir("..");
				break;
			default:
				error_msg("ignoring entry %s", dent->d_name);
		}
	}
	closedir(dh);
}

// endianness swap of x-indirect blocks
void swap_goodblocks(filesystem *fs, inode *nod)
{
	int i,j,done=0;
	uint32 *b,*b2;

	int nblk = nod->i_blocks / INOBLK;
	if((nod->i_size && !nblk) || ((nod->i_mode & FM_IFBLK) == FM_IFBLK) || ((nod->i_mode & FM_IFCHR) == FM_IFCHR))
		for(i = 0; i <= EXT2_TIND_BLOCK; i++)
			nod->i_block[i] = swab32(nod->i_block[i]);
	if(nblk <= EXT2_IND_BLOCK)
		return;
	swap_block(get_blk(fs, nod->i_block[EXT2_IND_BLOCK]));
	if(nblk <= EXT2_DIND_BLOCK + BLOCKSIZE/4)
		return;
	/* Currently this will fail b'cos the number of blocks as stored
	   in i_blocks also includes the indirection blocks (see
	   walk_bw). But this function assumes that i_blocks only
	   stores the count of data blocks ( Actually according to
	   "Understanding the Linux Kernel" (Table 17-3 p502 1st Ed)
	   i_blocks IS supposed to store the count of data blocks). so
	   with a file of size 268K nblk would be 269.The above check
	   will be false even though double indirection hasn't been
	   started.This is benign as 0 means block 0 which has been
	   zeroed out and therefore points back to itself from any offset
	 */
	// FIXME: I have fixed that, but I have the feeling the rest of
	// ths function needs to be fixed for the same reasons - Xav
	assert(nod->i_block[EXT2_DIND_BLOCK] != 0);
	for(i = 0; i < BLOCKSIZE/4; i++)
		if(nblk > EXT2_IND_BLOCK + BLOCKSIZE/4 + (BLOCKSIZE/4)*i )
			swap_block(get_blk(fs, ((uint32*)get_blk(fs, nod->i_block[EXT2_DIND_BLOCK]))[i]));
	swap_block(get_blk(fs, nod->i_block[EXT2_DIND_BLOCK]));
	if(nblk <= EXT2_IND_BLOCK + BLOCKSIZE/4 + BLOCKSIZE/4 * BLOCKSIZE/4)
		return;
	/* Adding support for triple indirection */
	b = (uint32*)get_blk(fs,nod->i_block[EXT2_TIND_BLOCK]);
	for(i=0;i < BLOCKSIZE/4 && !done ; i++) {
		b2 = (uint32*)get_blk(fs,b[i]); 
		for(j=0; j<BLOCKSIZE/4;j++) {
			if (nblk > ( EXT2_IND_BLOCK + BLOCKSIZE/4 + 
				     (BLOCKSIZE/4)*(BLOCKSIZE/4) + 
				     i*(BLOCKSIZE/4)*(BLOCKSIZE/4) + 
				     j*(BLOCKSIZE/4)) ) 
			  swap_block(get_blk(fs,b2[j]));
			else {
			  done = 1;
			  break;
			}
		}
		swap_block((uint8 *)b2);
	}
	swap_block((uint8 *)b);
	return;
}

void swap_badblocks(filesystem *fs, inode *nod)
{
	int i,j,done=0;
	uint32 *b,*b2;

	int nblk = nod->i_blocks / INOBLK;
	if((nod->i_size && !nblk) || ((nod->i_mode & FM_IFBLK) == FM_IFBLK) || ((nod->i_mode & FM_IFCHR) == FM_IFCHR))
		for(i = 0; i <= EXT2_TIND_BLOCK; i++)
			nod->i_block[i] = swab32(nod->i_block[i]);
	if(nblk <= EXT2_IND_BLOCK)
		return;
	swap_block(get_blk(fs, nod->i_block[EXT2_IND_BLOCK]));
	if(nblk <= EXT2_DIND_BLOCK + BLOCKSIZE/4)
		return;
	/* See comment in swap_goodblocks */
	assert(nod->i_block[EXT2_DIND_BLOCK] != 0);
	swap_block(get_blk(fs, nod->i_block[EXT2_DIND_BLOCK]));
	for(i = 0; i < BLOCKSIZE/4; i++)
		if(nblk > EXT2_IND_BLOCK + BLOCKSIZE/4 + (BLOCKSIZE/4)*i )
			swap_block(get_blk(fs, ((uint32*)get_blk(fs, nod->i_block[EXT2_DIND_BLOCK]))[i]));
	if(nblk <= EXT2_IND_BLOCK + BLOCKSIZE/4 + BLOCKSIZE/4 * BLOCKSIZE/4)
		return;
	/* Adding support for triple indirection */
	b = (uint32*)get_blk(fs,nod->i_block[EXT2_TIND_BLOCK]);
	swap_block((uint8 *)b);
	for(i=0;i < BLOCKSIZE/4 && !done ; i++) {
		b2 = (uint32*)get_blk(fs,b[i]); 
		swap_block((uint8 *)b2);
		for(j=0; j<BLOCKSIZE/4;j++) {
			if (nblk > ( EXT2_IND_BLOCK + BLOCKSIZE/4 + 
				     (BLOCKSIZE/4)*(BLOCKSIZE/4) + 
				     i*(BLOCKSIZE/4)*(BLOCKSIZE/4) + 
				     j*(BLOCKSIZE/4)) ) 
			  swap_block(get_blk(fs,b2[j]));
			else {
			  done = 1;
			  break;
			}
		}
	}
	return;
}

// endianness swap of the whole filesystem
void swap_goodfs(filesystem *fs)
{
	int i;
	for(i = 1; i < fs->sb.s_inodes_count; i++)
	{
		inode *nod = get_nod(fs, i);
		if(nod->i_mode & FM_IFDIR)
		{
			blockwalker bw;
			uint32 bk;
			init_bw(fs, i, &bw);
			while((bk = walk_bw(fs, i, &bw, 0, 0)) != WALK_END)
			{
				directory *d;
				uint8 *b;
				b = get_blk(fs, bk);
				for(d = (directory*)b; (int8*)d + sizeof(*d) < (int8*)b + BLOCKSIZE; d = (directory*)((int8*)d + swab16(d->d_rec_len)))
					swap_dir(d);
			}
		}
		swap_goodblocks(fs, nod);
		swap_nod(nod);
	}
	for(i=0;i<GRP_NBGROUPS(fs);i++)
		swap_gd(&(fs->gd[i]));
	swap_sb(&fs->sb);
}

void swap_badfs(filesystem *fs)
{
	int i;
	swap_sb(&fs->sb);
	for(i=0;i<GRP_NBGROUPS(fs);i++)
		swap_gd(&(fs->gd[i]));
	for(i = 1; i < fs->sb.s_inodes_count; i++)
	{
		inode *nod = get_nod(fs, i);
		swap_nod(nod);
		swap_badblocks(fs, nod);
		if(nod->i_mode & FM_IFDIR)
		{
			blockwalker bw;
			uint32 bk;
			init_bw(fs, i, &bw);
			while((bk = walk_bw(fs, i, &bw, 0, 0)) != WALK_END)
			{
				directory *d;
				uint8 *b;
				b = get_blk(fs, bk);
				for(d = (directory*)b; (int8*)d + sizeof(*d) < (int8*)b + BLOCKSIZE; d = (directory*)((int8*)d + d->d_rec_len))
					swap_dir(d);
			}
		}
	}
}

// initialize an empty filesystem
filesystem * init_fs(int nbblocks, int nbinodes, int nbresrvd, int holes)
{
	int i;
	filesystem *fs;
	directory *d;
	uint8 * b;
	uint32 nod;
	uint32 nbgroups,nbinodes_per_group,overhead_per_group,free_blocks,
		free_blocks_per_group,nbblocks_per_group;
	uint32 gd,itbl,ibmpos,bbmpos,itblpos;
	int j;
	uint8 *bbm,*ibm;
	inode *itab0;
	
	if(nbblocks < 16) // totally arbitrary
		error_msg_and_die("too small filesystem");

	/* nbblocks is the total number of blocks in the system. First 
	 * calculate how much overhead blocks - inode table blocks,bitmap 
	 * blocks,group descriptor blocks etc. - are needed assuming each 
	 * group has BLOCKS_PER_GROUP blocks.Then recalculate nbblocks with 
	 * this figure. Each group has the same number of blocks. So the fs 
	 * has a size atleast the given value but usually rounded off to a i
	 * higher number.
	 */
	nbgroups = rndup(nbblocks,BLOCKS_PER_GROUP)/ BLOCKS_PER_GROUP;
	nbinodes_per_group = nbinodes/nbgroups +1;
	nbinodes_per_group = rndup(nbinodes_per_group, BLOCKSIZE/sizeof(inode));
	if (nbinodes_per_group < 16)
		nbinodes_per_group = 16; //minimum number b'cos the first 10 are reserved
	overhead_per_group = 3 /*sb,ibm,bbm*/
			     + nbinodes_per_group *sizeof(inode)/BLOCKSIZE 
			     + rndup(nbgroups*sizeof(groupdescriptor),BLOCKSIZE)/BLOCKSIZE;
	free_blocks = nbblocks - overhead_per_group * nbgroups - 1 /*boot block */;
	free_blocks_per_group = free_blocks/nbgroups;
	if (free_blocks > free_blocks_per_group * nbgroups)
		free_blocks_per_group++;
	nbblocks_per_group = free_blocks_per_group + overhead_per_group;
	if (nbblocks_per_group > BLOCKS_PER_GROUP) {
		/* Can this happen ? */
		nbblocks_per_group = BLOCKS_PER_GROUP;
		free_blocks_per_group = nbblocks_per_group - overhead_per_group;
	}
	nbblocks = nbblocks_per_group * nbgroups + 1;

	if(!(fs = (filesystem*)calloc(nbblocks, BLOCKSIZE)))
		error_msg_and_die("not enough memory for filesystem");

	// create the superblock for an empty filesystem
	fs->sb.s_inodes_count = nbinodes_per_group * nbgroups;
	fs->sb.s_blocks_count = nbblocks;
	fs->sb.s_r_blocks_count = nbresrvd;
	fs->sb.s_free_blocks_count = free_blocks_per_group*nbgroups;
	fs->sb.s_free_inodes_count = fs->sb.s_inodes_count - EXT2_FIRST_INO + 1;
	fs->sb.s_first_data_block = (BLOCKSIZE == 1024);
	fs->sb.s_log_block_size = BLOCKSIZE >> 11;
	fs->sb.s_log_frag_size = BLOCKSIZE >> 11;
	fs->sb.s_blocks_per_group = nbblocks_per_group;
	fs->sb.s_frags_per_group = nbblocks_per_group;
	fs->sb.s_inodes_per_group = nbinodes_per_group;
	fs->sb.s_magic = EXT2_MAGIC_NUMBER;

	// set up groupdescriptors
	gd = rndup(nbgroups*sizeof(groupdescriptor),BLOCKSIZE)/BLOCKSIZE;
	itbl = nbinodes_per_group*sizeof(inode)/BLOCKSIZE;
	for(i = 0,bbmpos=2+gd,ibmpos=3+gd,itblpos =4+gd;
		i<nbgroups;
		i++, bbmpos += nbblocks_per_group,ibmpos += nbblocks_per_group, 
		itblpos += nbblocks_per_group)  {
		
		fs->gd[i].bg_free_blocks_count = free_blocks_per_group;
		fs->gd[i].bg_free_inodes_count = nbinodes_per_group;
		fs->gd[i].bg_used_dirs_count = 0;
		fs->gd[i].bg_block_bitmap = bbmpos;
		fs->gd[i].bg_inode_bitmap = ibmpos;
		fs->gd[i].bg_inode_table = itblpos;
	}

	/* Mark non-filesystem blocks and inodes as allocated */
	/* Mark system blocks and inodes as allocated         */
	for(i = 0; i<nbgroups;i++) {

		/* Block bitmap */
		j=fs->sb.s_inodes_per_group;
		//printf("j: %d\n",j);
		bbm = get_blk(fs,fs->gd[i].bg_block_bitmap);	
		//non-filesystem blocks.*TODO* check the +1
		for(j=fs->sb.s_blocks_per_group + 1; j <= BLOCKSIZE * 8; j++)
			allocate(bbm, j); 
		//system blocks
		for(j = 1; j <= 3+gd+itbl; j++)
			allocate(bbm, j); 
		
		/* Inode bitmap */
		ibm = get_blk(fs,fs->gd[i].bg_inode_bitmap);	
		//non-filesystem inodes
		for(j = fs->sb.s_inodes_per_group+1; j <= BLOCKSIZE * 8; j++)
			allocate(ibm, j);
		//system inodes
		for(j = 1; j < EXT2_FIRST_INO; j++)
			allocate(ibm, j);
	}

	/* We have groups now. Add the root filesystem in group 0  */
	/* Also allocate the system inodes in group 0 and update   */
	/* directory count and inode count for group 0             */
	ibm = get_blk(fs,fs->gd[0].bg_inode_bitmap);
	for(j = 1; j < EXT2_FIRST_INO; j++) {
		allocate(ibm, j);
		fs->gd[0].bg_free_inodes_count--;
	}
	fs->gd[0].bg_used_dirs_count = 1;
	itab0 = (inode *)get_blk(fs,fs->gd[0].bg_inode_table);
	itab0[EXT2_ROOT_INO-1].i_mode = FM_IFDIR | FM_IRWXU | FM_IRWXG | FM_IRWXO; 
	itab0[EXT2_ROOT_INO-1].i_size = BLOCKSIZE;
	itab0[EXT2_ROOT_INO-1].i_links_count = 2;

	b = get_workblk();
	d = (directory*)b;
	d->d_inode = EXT2_ROOT_INO;
	d->d_rec_len = sizeof(directory)+4;
	d->d_name_len = 1;
	strcpy(d->d_name, ".");
	d = (directory*)(b + d->d_rec_len);
	d->d_inode = EXT2_ROOT_INO;
	d->d_rec_len = BLOCKSIZE - (sizeof(directory)+4);
	d->d_name_len = 2;
	strcpy(d->d_name, "..");
	extend_blk(fs, EXT2_ROOT_INO, b, 1);

	// make lost+found directory and reserve blocks
	if(fs->sb.s_r_blocks_count)
	{
		nod = mkdir_fs(fs, EXT2_ROOT_INO, "lost+found", S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, 0, 0, time(NULL));
		memset(b, 0, BLOCKSIZE);
		((directory*)b)->d_rec_len = BLOCKSIZE;
		/* We run into problems with e2fsck if directory lost+found grows
		 * bigger than this. Need to find out why this happens - sundar
		 */
		if (fs->sb.s_r_blocks_count > 2049 ) 
			fs->sb.s_r_blocks_count=2049;
		for(i = 1; i < fs->sb.s_r_blocks_count; i++)
			extend_blk(fs, nod, b, 1);
		get_nod(fs, nod)->i_size = fs->sb.s_r_blocks_count * BLOCKSIZE;
	}
	free_workblk(b);

	// administrative info
	fs->sb.s_state = 1;
	fs->sb.s_max_mnt_count = 20;

	// options for me
	if(holes)
		fs->sb.s_reserved[200] |= OP_HOLES;
	
	return fs;
}

// loads a filesystem from disk
filesystem * load_fs(FILE * fh, int swapit)
{
	size_t fssize;
	filesystem *fs;
	if((fseek(fh, 0, SEEK_END) < 0) || ((fssize = ftell(fh)) < 0))
		perror_msg_and_die("input filesystem image");
	rewind(fh);
	fssize = (fssize + BLOCKSIZE - 1) / BLOCKSIZE;
	if(fssize < 16) // totally arbitrary
		error_msg_and_die("too small filesystem");
	if(!(fs = (filesystem*)calloc(fssize, BLOCKSIZE)))
		error_msg_and_die("not enough memory for filesystem");
	if(fread(fs, BLOCKSIZE, fssize, fh) != fssize)
		perror_msg_and_die("input filesystem image");
	if(swapit)
		swap_badfs(fs);
	if(fs->sb.s_rev_level || (fs->sb.s_magic != EXT2_MAGIC_NUMBER))
		error_msg_and_die("not a suitable ext2 filesystem");
	return fs;
}

void free_fs(filesystem *fs)
{
	free(fs);
}

// just walk through blocks list
void flist_blocks(filesystem *fs, uint32 nod, FILE *fh)
{
	blockwalker bw;
	uint32 bk;
	init_bw(fs, nod, &bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
		fprintf(fh, " %d", bk);
	fprintf(fh, "\n");
}

// walk through blocks list
void list_blocks(filesystem *fs, uint32 nod)
{
	int bn = 0;
	blockwalker bw;
	uint32 bk;
	init_bw(fs, nod, &bw);
	printf("blocks in inode %d:", nod);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
		printf(" %d", bk), bn++;
	printf("\n%d blocks (%d bytes)\n", bn, bn * BLOCKSIZE);
}

// saves blocks to FILE*
void write_blocks(filesystem *fs, uint32 nod, FILE* f)
{
	blockwalker bw;
	uint32 bk;
	int32 fsize = get_nod(fs, nod)->i_size;
	init_bw(fs, nod, &bw);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		if(fsize <= 0)
			error_msg_and_die("wrong size while saving inode %d", nod);
		if(fwrite(get_blk(fs, bk), (fsize > BLOCKSIZE) ? BLOCKSIZE : fsize, 1, f) != 1)
			error_msg_and_die("error while saving inode %d", nod);
		fsize -= BLOCKSIZE;
	}
}

// hexdumps blocks to a FILE*
void hexdump_blocks(filesystem *fs, uint32 nod, FILE* f)
{
	blockwalker bw;
	uint32 bk;
	uint8 *b;
	int32 fsize = get_nod(fs, nod)->i_size;
	init_bw(fs, nod, &bw);
	printf("block: offset: data:                                ascii:\n");
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		int i, j;
		if(fsize <= 0)
			error_msg_and_die("wrong size while saving inode %d", nod);
		b = get_blk(fs, bk);
		for(i = 0; i < 64; i++)
		{
			int dmp = 0;
			for(j = 0; j < 4; j++)
				if(*(int32*)&b[i * 16 + j * 4])
					dmp = 1;
			if(!dmp)
				continue;
			printf("%5d:    %03X:", bk, i * 16);
			for(j = 0; j < 4; j++)
				printf(" %08x", *(int32*)&b[i * 16 + j * 4]);
			printf("  ");
			for(j = 0; j < 16; j++)
				printf("%c", (b[i * 16 + j] >= ' ' && b[i * 16 + j] < 127) ? b[i * 16 + j] : ' ');
			printf("\n");
		}
		fsize -= BLOCKSIZE;
	}
}

// print block/char device minor and major
void print_dev(filesystem *fs, uint32 nod)
{
	int minor, major;
	minor = ((uint8*)get_nod(fs, nod)->i_block)[0];
	major = ((uint8*)get_nod(fs, nod)->i_block)[1];
	printf("major: %d, minor: %d\n", major, minor);
}

// print an inode as a directory
void print_dir(filesystem *fs, uint32 nod)
{
	blockwalker bw;
	uint32 bk;
	init_bw(fs, nod, &bw);
	printf("directory for inode %d:\n", nod);
	while((bk = walk_bw(fs, nod, &bw, 0, 0)) != WALK_END)
	{
		directory *d;
		uint8 *b;
		b = get_blk(fs, bk);
		for(d = (directory*)b; (int8*)d + sizeof(*d) < (int8*)b + BLOCKSIZE; d = (directory*)((int8*)d + d->d_rec_len))
			if(d->d_inode)
			{
				int i;
				printf("entry '");
				for(i = 0; i < d->d_name_len; i++)
					putchar(d->d_name[i]);
				printf("' (inode %d): rec_len: %d (name_len: %d)\n", d->d_inode, d->d_rec_len, d->d_name_len);
			}
	}
}

// print a symbolic link
void print_link(filesystem *fs, uint32 nod)
{
	if(!get_nod(fs, nod)->i_blocks)
		printf("links to '%s'\n", (char*)get_nod(fs, nod)->i_block);
	else
	{
		printf("links to '");
		write_blocks(fs, nod, stdout);
		printf("'\n");
	}
}

// make a ls-like printout of permissions
void make_perms(uint32 mode, char perms[11])
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
void print_inode(filesystem *fs, uint32 nod)
{
	char *s;
	char perms[11];
	if(!get_nod(fs, nod)->i_mode)
		return;
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
	printf("inode %d (%s, %d links): ", nod, s, get_nod(fs, nod)->i_links_count);
	if(!allocated(GRP_GET_INODE_BITMAP(fs,nod), GRP_IBM_OFFSET(fs,nod)))
	{
		printf("unallocated\n");
		return;
	}
	make_perms(get_nod(fs, nod)->i_mode, perms);
	printf("%s,  size: %d byte%s (%d block%s)\n", perms, plural(get_nod(fs, nod)->i_size), plural(get_nod(fs, nod)->i_blocks / INOBLK));
	switch(get_nod(fs, nod)->i_mode & FM_IFMT)
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
}

// describes various fields in a filesystem
void print_fs(filesystem *fs)
{
	int i;
	uint8 *ibm;

	printf("%d blocks (%d free, %d reserved), first data block: %d\n",
	       fs->sb.s_blocks_count, fs->sb.s_free_blocks_count,
	       fs->sb.s_r_blocks_count, fs->sb.s_first_data_block);
	printf("%d inodes (%d free)\n", fs->sb.s_inodes_count,
	       fs->sb.s_free_inodes_count);
	printf("block size = %d, frag size = %d\n",
	       fs->sb.s_log_block_size ? (fs->sb.s_log_block_size << 11) : 1024,
	       fs->sb.s_log_frag_size ? (fs->sb.s_log_frag_size << 11) : 1024);
	printf("Number of groups: %d\n",GRP_NBGROUPS(fs));
	printf("%d blocks per group,%d frags per group,%d inodes per group\n",
	     fs->sb.s_blocks_per_group, fs->sb.s_frags_per_group,
	     fs->sb.s_inodes_per_group);
	printf("Size of inode table: %d blocks\n",
			fs->sb.s_inodes_per_group * sizeof(inode)/BLOCKSIZE);
	for (i = 0; i < GRP_NBGROUPS(fs); i++) {
		printf("Group No: %d\n", i+1);
		printf("block bitmap: block %d,inode bitmap: block %d, inode table: block %d\n",
		     fs->gd[i].bg_block_bitmap, fs->gd[i].bg_inode_bitmap,
		     fs->gd[i].bg_inode_table);
		printf("block bitmap allocation:\n");
		print_bm(GRP_GET_GROUP_BBM(fs, i),fs->sb.s_blocks_per_group);
		printf("inode bitmap allocation:\n");
		ibm = GRP_GET_GROUP_IBM(fs, i);
		print_bm(ibm, fs->sb.s_inodes_per_group);
		for (i = 1; i <= fs->sb.s_inodes_per_group; i++)
			if (allocated(ibm, i))
				print_inode(fs, i);
	}
}

void dump_fs(filesystem *fs, FILE * fh, int swapit)
{
	int nbblocks = fs->sb.s_blocks_count;
	fs->sb.s_reserved[200] = 0;
	if(swapit)
		swap_goodfs(fs);
	if(fwrite(fs, BLOCKSIZE, nbblocks, fh) < nbblocks)
		perror_msg_and_die("output filesystem image");
	if(swapit)
		swap_badfs(fs);
}

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

static int interpret_table_entry(filesystem *fs, char *line)
{
// FIXME: this function is weird, it probably needs some love - Xav
	char type, *name = NULL, *dir, *bname;
	unsigned long mode = 0, uid = 0, gid = 0, major = 0, minor = 0;
	unsigned long start = 0, increment = 1, count = 0;
	inode *entry;
	uint32 nod, parent;

	if (sscanf (line, "%" SCANF_PREFIX "s %c %lo %lu %lu %lu %lu %lu %lu %lu",
				SCANF_STRING(name), &type, &mode, &uid, &gid, &major, &minor,
				&start, &increment, &count) < 0) 
	{
		free(name);
		return 1;
	}
	mode &= FM_IMASK;

	if (*name != '/') {
		error_msg_and_die("Device table entries require absolute paths");
	}

	/* Check if this file already exists... */
	switch (type) {
		case 'd':
			mode |= S_IFDIR;
			break;
		case 'f':
			mode |= S_IFREG;
			break;
		case 'p':
			mode |= S_IFIFO;
			break;
		case 'c':
			mode |= S_IFCHR;
			break;
		case 'b':
			mode |= S_IFBLK;
			break;
		default:
			error_msg_and_die("Unsupported file type");
	}
	nod = 0;
	if (count==0)
		nod = find_path(fs, EXT2_ROOT_INO, name);
	if (nod) {
		/* Ok, we just need to fixup an existing entry 
		 * and we will be all done... */
		entry = get_nod(fs, nod);
		entry->i_uid = uid;
		entry->i_gid = gid;
		entry->i_mode = mode;
		if (major) {
			dev_t rdev = makedev(major, minor);
			((uint8*)entry->i_block)[0] = (rdev & 0xff);
			((uint8*)entry->i_block)[1] = (rdev >> 8);
		}
	} else {
		/* Try and find our parent now */
		dir = dirname(name);
		parent = find_path(fs, EXT2_ROOT_INO, dir);
		free(dir);
		if (!parent) {
			error_msg ("skipping device_table entry '%s': no parent directory!", name);
			free(name);
			return 1;
		}

		bname = basename(name);
		switch (type) {
			case 'd':
				mkdir_fs(fs, parent, bname, mode|FM_IFDIR, uid, gid, time(NULL));
				break;
			case 'f':
#if 0
				{
					// This is a bit odd.. This will try to include
					// the file of the same name from your _build_
					// system...  Probably a very bad idea....
					struct stat st;
					FILE *fh = xfopen(name, "r");
					lstat(name, &st);
					mkfile_fs(fs, parent, bname, mode|FM_IFREG, st.st_size, fh, uid, gid, st.st_ctime);
					fclose(fh);
				}
#else
				error_msg("ignoring entry %s", name);
#endif
				break;
			case 'p':
				error_msg("ignoring entry %s", name);
				break;
			case 'c':
			case 'b':
				if (count > 0) {
					dev_t rdev;
					char *dname;
					unsigned long i;
					for (i = start; i < count; i++) {
						asprintf(&dname, "%s%lu", bname, i);
						nod = find_path(fs, EXT2_ROOT_INO, dname);
						if (nod) {
							/* We just need to fixup an existing entry */ 
							entry = get_nod(fs, nod);
						} else {
							nod = alloc_nod(fs);
							add2dir(fs, parent, nod, dname, mode, uid, gid, time(NULL));
							entry = get_nod(fs, nod);
						}
						entry->i_uid = uid;
						entry->i_gid = gid;
						entry->i_mode = mode;
						rdev = makedev(major, minor + (i * increment - start));
						((uint8*)entry->i_block)[0] = (rdev & 0xff);
						((uint8*)entry->i_block)[1] = (rdev >> 8);
						free(dname);
					}
				} else {
					dev_t rdev = makedev(major, minor);
					nod = alloc_nod(fs);
					add2dir(fs, parent, nod, bname, mode, uid, gid, time(NULL));
					entry = get_nod(fs, nod);
					((uint8*)entry->i_block)[0] = (rdev & 0xff);
					((uint8*)entry->i_block)[1] = (rdev >> 8);
				}
				break;
			default:
				error_msg_and_die("Unsupported file type");
		}
		free(bname);
	}
	free(name);
	return 0;
}

static int stats_from_table_entry(char *line, struct stats *stats)
{
	char type, *name = NULL, *tmp, *dir, *bname;
	unsigned long mode = 0755, uid = 0, gid = 0, major = 0, minor = 0;
	unsigned long start = 0, increment = 1, count = 0;
	inode *entry;

	if (sscanf (line, "%" SCANF_PREFIX "s %c %lo %lu %lu %lu %lu %lu %lu %lu",
				SCANF_STRING(name), &type, &mode, &uid, &gid, &major, &minor,
				&start, &increment, &count) < 0) 
	{
		return 1;
	}

	if (!strcmp(name, "/")) {
		error_msg_and_die("Device table entries require absolute paths");
	}

	tmp = xstrdup(name);
	bname = xstrdup(basename(tmp));
	free(tmp);
	switch (type) {
		case 'd':
			stats->ninodes++;
			break;
		case 'c':
		case 'b':
			if (count > 0) {
				dev_t rdev;
				char *dname;
				unsigned long i;
				for (i = start; i < count; i++) {
					asprintf(&dname, "%s%lu", bname, i);
					stats->ninodes++;
					free(dname);
				}
			} else {
				stats->ninodes++;
			}
			break;
	}
	free(bname);
	free(name);
	return 0;
}

static int parse_device_table(filesystem *root, FILE * file)
{
	char *line;
	int status = 0;
	size_t length = 0;

	/* Turn off squash, since we must ensure that values
	 * entered via the device table are not squashed */
	squash_uids = 0;
	squash_perms = 0;

	/* Looks ok so far.  The general plan now is to read in one
	 * line at a time, check for leading comment delimiters ('#'),
	 * then try and parse the line as a device table.  If we fail
	 * to parse things, try and help the poor fool to fix their
	 * device table with a useful error msg... */
	line = NULL;
	while (getline(&line, &length, file) != -1) {
		/* First trim off any whitespace */
		int len = strlen(line);

		/* trim trailing whitespace */
		while (len > 0 && isspace(line[len - 1]))
			line[--len] = '\0';
		/* trim leading whitespace */
		memmove(line, &line[strspn(line, " \n\r\t\v")], len);

		/* How long are we after trimming? */
		len = strlen(line);

		/* If this is NOT a comment line, try to interpret it */
		if (len && *line != '#') {
			if (interpret_table_entry(root, line))
				status = 1;
		}

		free(line);
		line = NULL;
	}
	fclose(file);

	return status;
}

static int stats_from_dev_table(FILE *file, struct stats *stats)
{
	char *line;
	int status = 0;
	size_t length = 0;

	/* Looks ok so far.  The general plan now is to read in one
	 * line at a time, check for leading comment delimiters ('#'),
	 * then try and parse the line as a device table.  If we fail
	 * to parse things, try and help the poor fool to fix their
	 * device table with a useful error msg... */
	line = NULL;
	while (getline(&line, &length, file) != -1) {
		/* First trim off any whitespace */
		int len = strlen(line);

		/* trim trailing whitespace */
		while (len > 0 && isspace(line[len - 1]))
			line[--len] = '\0';
		/* trim leading whitespace */
		memmove(line, &line[strspn(line, " \n\r\t\v")], len);

		/* How long are we after trimming? */
		len = strlen(line);

		/* If this is NOT a comment line, try to interpret it */
		if (len && *line != '#') {
			if (stats_from_table_entry(line, stats))
				status = 1;
		}

		free(line);
		line = NULL;
	}
	fclose(file);

	return status;
}

void showhelp(void)
{
	fprintf(stderr, "Usage: %s [options] image\n"
	"Create an ext2 filesystem image from directories/files\n\n"
	"  -x image     Use this image as a starting point\n"
	"  -d directory     Add this directory as source\n"
	"  -b blocks        Size in blocks\n"
	"  -i inodes        Number of inodes\n"
	"  -r reserved      Number of reserved blocks\n"
	"  -g path          Generate a block map file for this path\n"
	"  -e value         Fill unallocated blocks with value\n"
	"  -z               Make files with holes\n"
	"  -D,-f            Use the named FILE as a device table file\n"
	"  -q               Squash permissions and owners making all files be owned by root\n"
	"  -U               Squash owners making all files be owned by root\n"
	"  -P               Squash permissions on all files\n"
	"  -v               Print resulting filesystem structure\n"
	"  -h               Show this help\n\n"
	"Report bugs to xavier.bestel@free.fr\n", app_name);
}

#define MAX_DOPT 128
#define MAX_GOPT 128

#define MAX_FILENAME 255

extern char* optarg;
extern int optind, opterr, optopt;

int main(int argc, char **argv)
{
	int nbblocks = -1;
	int nbinodes = -1;
	int nbresrvd = -1;
	int tmp_nbblocks = -1;
	int tmp_nbinodes = -1;
	char * fsout = "-";
	char * fsin = 0;
	char * dopt[MAX_DOPT];
	int didx = 0;
	char * gopt[MAX_GOPT];
	int gidx = 0;
	int verbose = 0;
	int holes = 0;
	int emptyval = 0;
	uint16 endian = 1;
	int bigendian = !*(char*)&endian;
	filesystem *fs;
	int i;
	int c;
	struct stat sb;
	FILE *devtable = NULL;
	struct stats stats;

	app_name = argv[0];
	while((c = getopt(argc, argv, "x:d:b:i:r:g:e:zvhD:f:qUP")) != EOF)
		switch(c)
		{
			case 'x':
				fsin = optarg;
				break;
			case 'd':
				dopt[didx++] = optarg;
				break;
			case 'b':
				nbblocks = atoi(optarg);
				break;
			case 'i':
				nbinodes = atoi(optarg);
				break;
			case 'r':
				nbresrvd = atoi(optarg);
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
			case 'D':
				devtable = xfopen(optarg, "r");
				if (fstat(fileno(devtable), &sb) < 0)
					perror_msg_and_die(optarg);
				if (sb.st_size < 10)
					error_msg_and_die("%s: not a proper device table file", optarg);
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
			case 'v':
				verbose = 1;
				break;
			case 'h':
				showhelp();
				exit(0);
			default:
				exit(1);
		}

	if(optind < (argc - 1))
		error_msg_and_die("too many arguments");
	if(optind == (argc - 1))
		fsout = argv[optind];
	if(fsin)
	{
		if(strcmp(fsin, "-"))
		{
			FILE * fh = xfopen(fsin, "r");
			fs = load_fs(fh, bigendian);
			fclose(fh);
		}
		else
			fs = load_fs(stdin, bigendian);
	}
	else
	{
		stats.ninodes = 0;
		stats.nblocks = 0;
		for(i = 0; i < didx; i++)
		{
			struct stat st;
			char *pdir;
			stat(dopt[i], &st);
			switch(st.st_mode & S_IFMT)
			{
				case S_IFDIR:
					if(!(pdir = getcwd(0, GETCWD_SIZE)))
						perror_msg_and_die(dopt[i]);
					if(chdir(dopt[i]) < 0)
						perror_msg_and_die(dopt[i]);
					stats_from_dir(&stats);
					if(chdir(pdir) < 0)
						perror_msg_and_die(pdir);
					free(pdir);
					break;
				default:
					error_msg_and_die("%s is neither a file nor a directory", dopt[i]);
			}
		}
	
		if(devtable)
			stats_from_dev_table(devtable, &stats);
	
		tmp_nbinodes = stats.ninodes + EXT2_FIRST_INO + 1;
		tmp_nbblocks = stats.nblocks;
	
		if(tmp_nbblocks > nbblocks)
		{
			printf("Number of blocks too low, increasing to %d\n",tmp_nbblocks);
			nbblocks = tmp_nbblocks;
		}
		if(tmp_nbinodes > nbinodes)
		{
			printf("Number of inodes too low, increasing to %d\n",tmp_nbinodes);
			nbinodes = tmp_nbinodes;
		}
		if(nbblocks == -1)
			error_msg_and_die("filesystem size unspecified");
		if(nbinodes == -1)
			nbinodes = nbblocks * BLOCKSIZE / rndup(BYTES_PER_INODE, BLOCKSIZE);
		if(nbresrvd == -1)
			nbresrvd = nbblocks * RESERVED_INODES;
		fs = init_fs(nbblocks, nbinodes, nbresrvd, holes);
	}
	for(i = 0; i < didx; i++)
	{
		struct stat st;
		FILE *fh;
		char *pdir;
		stat(dopt[i], &st);
		switch(st.st_mode & S_IFMT)
		{
			case S_IFDIR:
				if(!(pdir = getcwd(0, GETCWD_SIZE)))
					perror_msg_and_die(dopt[i]);
				if(chdir(dopt[i]) < 0)
					perror_msg_and_die(dopt[i]);
				add2fs_from_dir(fs, EXT2_ROOT_INO);
				if(chdir(pdir) < 0)
					perror_msg_and_die(pdir);
				free(pdir);
				break;
			default:
				error_msg_and_die("%s in neither a file nor a directory", dopt[i]);
		}
	}
	if(emptyval)
		for(i = 1; i < fs->sb.s_blocks_count; i++)
			if(!allocated(GRP_GET_BLOCK_BITMAP(fs,i),GRP_BBM_OFFSET(fs,i)))
				memset(get_blk(fs, i), emptyval, BLOCKSIZE);
	if(devtable)
		parse_device_table(fs, devtable);
	if(verbose)
		print_fs(fs);
	for(i = 0; i < gidx; i++)
	{
		uint32 nod;
		char fname[MAX_FILENAME];
		char *p;
		FILE *fh;
		if(!(nod = find_path(fs, EXT2_ROOT_INO, gopt[i])))
			error_msg_and_die("path %s not found in filesystem", gopt[i]);
		while((p = strchr(gopt[i], '/')))
			*p = '_';
		snprintf(fname, MAX_FILENAME-1, "%s.blk", gopt[i]);
		fh = xfopen(fname, "w");
		fprintf(fh, "%d:", get_nod(fs, nod)->i_size);
		flist_blocks(fs, nod, fh);
		fclose(fh);
	}
	if(strcmp(fsout, "-"))
	{
		FILE * fh = xfopen(fsout, "w");
		dump_fs(fs, fh, bigendian);
		fclose(fh);
	}
	else
		dump_fs(fs, stdout, bigendian);
	return 0;
}
