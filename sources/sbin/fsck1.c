/*
 * fsck.c - a file system consistency checker for Linux.
 *
 * (C) 1991 Linus Torvalds. This file may be redistributed as per
 * the Linux copyright.
 */

/*
 * 09.11.91  -  made the first rudimetary functions
 *
 * 10.11.91  -  updated, does checking, no repairs yet.
 *		Sent out to the mailing-list for testing.
 *
 * I've had no time to add comments - hopefully the function names
 * are comments enough. As with all file system checkers, this assumes
 * the file system is quiescent - don't use it on a mounted device
 * unless you can be sure nobody is writing to it (and remember that the
 * kernel can write to it when it searches for files).
 *
 * Usuage: fsck [-larvsm] device
 *	-l for a listing of all the filenames
 *	-a for automatic repairs (not implemented)
 *	-r for repairs (interactive) (not implemented)
 *	-v for verbose (tells how many files)
 *	-s for super-block info
 *	-m for minix-like "mode not cleared" warnings
 *
 * The device may be a block device or a image of one, but this isn't
 * enforced (but it's not much fun on a character device :-). 
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <linux/fs.h>

#define ROOT_INO 1

#define UPPER(size,n) ((size+(n-1))/n)
#define INODE_SIZE (sizeof(struct d_inode))
#define INODE_BLOCKS UPPER(INODES,INODES_PER_BLOCK)
#define INODE_BUFFER_SIZE (INODE_BLOCKS * BLOCK_SIZE)

#define BITS_PER_BLOCK (BLOCK_SIZE<<3)

static char * program_name = "fsck";
static char * device_name = NULL;
static int IN;
static int repair=0, automatic=0, verbose=0, list=0, show=0, warn_mode=0;
static int directory=0, regular=0, blockdev=0, chardev=0, links=0, total=0;

#define MAX_DEPTH 50
static int name_depth = 0;
static char name_list[MAX_DEPTH][NAME_LEN+1];

static char * inode_buffer = NULL;
#define Inode (((struct d_inode *) inode_buffer)-1)
static char super_block_buffer[BLOCK_SIZE];
#define Super (*(struct super_block *)super_block_buffer)
#define INODES ((unsigned long)Super.s_ninodes)
#define ZONES ((unsigned long)Super.s_nzones)
#define IMAPS ((unsigned long)Super.s_imap_blocks)
#define ZMAPS ((unsigned long)Super.s_zmap_blocks)
#define FIRSTZONE ((unsigned long)Super.s_firstdatazone)
#define ZONESIZE ((unsigned long)Super.s_log_zone_size)
#define MAXSIZE ((unsigned long)Super.s_max_size)
#define MAGIC (Super.s_magic)
#define NORM_FIRSTZONE (2+IMAPS+ZMAPS+INODE_BLOCKS)

static char inode_map[BLOCK_SIZE * I_MAP_SLOTS];
static char zone_map[BLOCK_SIZE * Z_MAP_SLOTS];

static unsigned char * inode_count = NULL;
static unsigned char * zone_count = NULL;

void recursive_check(unsigned int ino);

#define bitop(name,op) \
static inline int name(char * addr,unsigned int nr) \
{ \
int __res; \
__asm__("bt" op " %1,%2; adcl $0,%0" \
:"=g" (__res) \
:"r" (nr),"m" (*(addr)),"0" (0)); \
return __res; \
}

bitop(bit,"")
bitop(setbit,"s")
bitop(clrbit,"c")

inline int inode_in_use(unsigned int nr)
{
	return bit(inode_map,nr);
}

inline int zone_in_use(unsigned int nr)
{
	return bit(zone_map,nr-FIRSTZONE+1);
}

volatile void usage(void)
{
	fprintf(stderr,"Usage: %s [-larvsm] /dev/name\n",program_name);
	exit(1);
}

volatile void die(char * string)
{
	fprintf(stderr,"%s: ",program_name);
	fprintf(stderr,string,device_name);
	fprintf(stderr,"\n");
	exit(1);
}

inline void read_block(unsigned int nr, char * addr)
{
	if (!nr || nr >= ZONES) {
		memset(addr,0,BLOCK_SIZE);
		return;
	}
	if (BLOCK_SIZE*nr != lseek(IN, BLOCK_SIZE*nr, SEEK_SET))
		die("seek failed");
	if (BLOCK_SIZE != read(IN, addr, BLOCK_SIZE))
		die("unable to read block");
}

inline void mapped_read_block(struct d_inode * inode,
	unsigned int blknr, char * addr)
{
	char blk[BLOCK_SIZE];

	if (blknr<7) {
		read_block(inode->i_zone[blknr],addr);
		return;
	}
	blknr -= 7;
	if (blknr<512) {
		read_block(inode->i_zone[7],blk);
		read_block(((unsigned short *)blk)[blknr],addr);
		return;
	}
	blknr -= 512;
	read_block(inode->i_zone[8],blk);
	read_block(((unsigned short *)blk)[blknr/512],blk);
	read_block(((unsigned short *)blk)[blknr%512],addr);
}

void read_tables(void)
{
	memset(inode_map,0,sizeof(inode_map));
	memset(zone_map,0,sizeof(zone_map));
	if (BLOCK_SIZE != lseek(IN, BLOCK_SIZE, SEEK_SET))
		die("seek failed");
	if (BLOCK_SIZE != read(IN, super_block_buffer, BLOCK_SIZE))
		die("unable to read super block");
	if (MAGIC != SUPER_MAGIC)
		die("bad magic number in super-block");
	if (ZONESIZE != 0 || BLOCK_SIZE != 1024)
		die("Only 1k blocks/zones supported");
	if (!IMAPS || IMAPS > I_MAP_SLOTS)
		die("bad s_imap_blocks field in super-block");
	if (!ZMAPS || ZMAPS > Z_MAP_SLOTS)
		die("bad s_zmap_blocks field in super-block");
	inode_buffer = malloc(INODE_BUFFER_SIZE);
	if (!inode_buffer)
		die("Unable to allocate buffer for inodes");
	inode_count = malloc(INODES);
	if (!inode_count)
		die("Unable to allocate buffer for inode count");
	zone_count = malloc(ZONES);
	if (!zone_count)
		die("Unable to allocate buffer for zone count");
	if (IMAPS*BLOCK_SIZE != read(IN,inode_map,IMAPS*BLOCK_SIZE))
		die("Unable to read inode map");
	if (ZMAPS*BLOCK_SIZE != read(IN,zone_map,ZMAPS*BLOCK_SIZE))
		die("Unable to read zone map");
	if (INODE_BUFFER_SIZE != read(IN,inode_buffer,INODE_BUFFER_SIZE))
		die("Unable to read inodes");
	if (NORM_FIRSTZONE != FIRSTZONE)
		printf("Warning: Firstzone != Norm_firstzone");
	if (show) {
		printf("%d inodes\n",INODES);
		printf("%d blocks\n",ZONES);
		printf("Firstdatazone=%d (%d)\n",FIRSTZONE,NORM_FIRSTZONE);
		printf("Zonesize=%d\n",BLOCK_SIZE<<ZONESIZE);
		printf("Maxsize=%d\n",MAXSIZE);
	}
}

struct d_inode * get_inode(unsigned int nr)
{
	struct d_inode * inode;

	if (!nr || nr > INODES)
		return NULL;
	total++;
	inode = Inode + nr;
	if (!inode_count[nr]) {
		if (S_ISDIR(inode->i_mode))
			directory++;
		else if (S_ISREG(inode->i_mode))
			regular++;
		else if (S_ISCHR(inode->i_mode))
			chardev++;
		else if (S_ISBLK(inode->i_mode))
			blockdev++;
	} else
		links++;
	if (!++inode_count[nr])
		inode_count[nr]--;
	return inode;
}

void check_root(void)
{
	struct d_inode * inode = Inode + ROOT_INO;

	if (!inode || !S_ISDIR(inode->i_mode))
		die("root inode isn't a directory");
}

static int add_zone(unsigned int znr)
{
	if (!znr || znr >= ZONES)
		return 0;
	if (!++zone_count[znr])
		zone_count[znr]--;
	return 1;
}

static int add_zone_ind(unsigned int znr)
{
	static char blk[BLOCK_SIZE];
	int i;

	if (!add_zone(znr))
		return 0;
	read_block(znr,blk);
	for (i=0 ; i < (BLOCK_SIZE>>1) ; i++)
		add_zone(((unsigned short *)blk)[i]);
	return 1;
}

static int add_zone_dind(unsigned int znr)
{
	static char blk[BLOCK_SIZE];
	int i;

	if (!add_zone(znr))
		return 0;
	read_block(znr,blk);
	for (i=0 ; i < (BLOCK_SIZE>>1) ; i++)
		add_zone_ind(((unsigned short *)blk)[i]);
	return 1;
}

void check_zones(unsigned int i)
{
	struct d_inode * inode;

	if (!i || i >= INODES)
		return;
	if (inode_count[i] > 1)	/* have we counted this file already? */
		return;
	inode = Inode + i;
	if (!S_ISDIR(inode->i_mode) && !S_ISREG(inode->i_mode))
		return;
	for (i=0 ; i<7 ; i++)
		add_zone(inode->i_zone[i]);
	add_zone_ind(inode->i_zone[7]);
	add_zone_dind(inode->i_zone[8]);
}

void print_path(void)
{
	int i=0;

	while (i<name_depth)
		printf("/%.14s",name_list[i++]);
	printf("/");
}

void check_file(struct d_inode * dir, unsigned int offset)
{
	static char blk[BLOCK_SIZE];
	struct d_inode * inode;
	int ino;
	char * name;

	mapped_read_block(dir,offset/BLOCK_SIZE,blk);
	name = blk + (offset % BLOCK_SIZE) + 2;
	ino = * (unsigned short *) (name-2);
	inode = get_inode(ino);
	if (!offset) {
		if (!ino || strcmp(".",name))
			printf("Bad directory: '.' isn't first\n");
		else return;
	}
	if (offset == 16) {
		if (!ino || strcmp("..",name))
			printf("Bad directory: '..' isn't second\n");
		else return;
	}
	if (!inode)
		return;
	if (list) {
		if (verbose)
			printf("%6d %07o ",ino,inode->i_mode);
		print_path();
		printf("%.14s%s\n",name,S_ISDIR(inode->i_mode)?":":"");
	}
	check_zones(ino);
	if (inode && S_ISDIR(inode->i_mode)) {
		if (name_depth < MAX_DEPTH)
			strncpy(name_list[name_depth],name,14);
		name_depth++;
		recursive_check(ino);
		name_depth--;
	}
}

void recursive_check(unsigned int ino)
{
	struct d_inode * dir;
	unsigned int offset;

	dir = Inode + ino;
	if (!S_ISDIR(dir->i_mode))
		die("internal error");
	for (offset = 0 ; offset < dir->i_size ; offset += 16)
		check_file(dir,offset);
}

void check_counts(void)
{
	int i;

	for (i=1 ; i < INODES ; i++) {
		if (!inode_in_use(i) && Inode[i].i_mode && warn_mode)
			printf("Inode %d mode not cleared\n",i);
		if (inode_in_use(i)*Inode[i].i_nlinks == inode_count[i])
			continue;
		printf("Inode %d: %s in use, nlinks=%d, counted=%d\n",
		i,inode_in_use(i)?"":"not",Inode[i].i_nlinks,
		inode_count[i]);
	}
	for (i=FIRSTZONE ; i < ZONES ; i++) {
		if (zone_in_use(i) == zone_count[i])
			continue;
		printf("Zone %d: %s in use, counted=%d\n",
		i,zone_in_use(i)?"":"not",zone_count[i]);
	}
}

void check(void)
{
	memset(inode_count,0,INODES*sizeof(*inode_count));
	memset(zone_count,0,ZONES*sizeof(*zone_count));
	check_zones(ROOT_INO);
	recursive_check(ROOT_INO);
	check_counts();
}

int main(int argc, char ** argv)
{
	if (argc && *argv)
		program_name = *argv;
	if (INODE_SIZE * INODES_PER_BLOCK != BLOCK_SIZE)
		die("bad inode size");
	while (argc-- > 1) {
		argv++;
		if (argv[0][0] != '-')
			if (device_name)
				usage();
			else
				device_name = argv[0];
		else while (*++argv[0])
			switch (argv[0][0]) {
				case 'l': list=1; break;
				case 'a': automatic=1; repair=1; break;
				case 'r': automatic=0; repair=1; break;
				case 'v': verbose=1; break;
				case 's': show=1; break;
				case 'm': warn_mode=1; break;
				default: usage();
			}
	}
	if (!device_name)
		usage();
	IN = open(device_name,repair?O_RDWR:O_RDONLY);
	if (IN < 0)
		die("unable to open '%s'");
	read_tables();
	check_root();
	check();
	if (verbose) {
		printf("\n%6d regular files\n"
		"%6d directories\n"
		"%6d character device files\n"
		"%6d block device files\n"
		"%6d links\n"
		"------\n"
		"%6d files\n",
		regular,directory,chardev,blockdev,
		links-2*directory+1,total-2*directory+1);
	}
	return (0);
}
