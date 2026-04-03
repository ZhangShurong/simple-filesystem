#define _GNU_SOURCE

/*
 * Minimal unit test for mkfs.c: verifies on-disk layout for a small image.
 *
 * This test does NOT load the kernel module; it only validates mkfs output.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "constants.h"

struct HUST_fs_super_block {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes_count;
	uint64_t free_blocks;
	uint64_t blocks_count;

	uint64_t bmap_block;
	uint64_t imap_block;
	uint64_t inode_table_block;
	uint64_t data_block_number;
	char padding[4016];
};

struct HUST_inode {
	mode_t mode;
	uint64_t inode_no;
	uint64_t blocks;
	uint64_t block[HUST_N_BLOCKS];
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
	int32_t i_uid;
	int32_t i_gid;
	int32_t i_nlink;
	int64_t i_atime;
	int64_t i_mtime;
	int64_t i_ctime;
	char padding[112];
};

struct HUST_dir_record {
	char filename[HUST_FILENAME_MAX_LEN];
	uint64_t inode_no;
};

static void die(const char *msg)
{
	perror(msg);
	exit(1);
}

static void expect_int64_eq(const char *name, int64_t got, int64_t want)
{
	if (got != want) {
		fprintf(stderr, "EXPECT %s: got=%" PRId64 " want=%" PRId64 "\n", name, got, want);
		exit(2);
	}
}

static void expect_u64_eq(const char *name, uint64_t got, uint64_t want)
{
	if (got != want) {
		fprintf(stderr, "EXPECT %s: got=%" PRIu64 " want=%" PRIu64 "\n", name, got, want);
		exit(2);
	}
}

static void expect_str_eq(const char *name, const char *got, const char *want)
{
	if (strcmp(got, want) != 0) {
		fprintf(stderr, "EXPECT %s: got='%s' want='%s'\n", name, got, want);
		exit(2);
	}
}

static uint64_t div_round_up_u64(uint64_t a, uint64_t b)
{
	return (a + b - 1) / b;
}

int main(void)
{
	_Static_assert(sizeof(struct HUST_fs_super_block) == HUST_BLOCKSIZE, "superblock must be 4096 bytes");

	char path[] = "/tmp/hustfs-image-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0)
		die("mkstemp");

	/* Create a 100-block image. */
	if (ftruncate(fd, (off_t)HUST_BLOCKSIZE * 100) != 0)
		die("ftruncate");
	close(fd);

	char cmd[512];
	snprintf(cmd, sizeof(cmd), "./mkfs %s", path);
	int rc = system(cmd);
	if (rc != 0) {
		fprintf(stderr, "mkfs failed: rc=%d\n", rc);
		unlink(path);
		return 3;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0)
		die("open");

	/* Superblock is at block 1. */
	struct HUST_fs_super_block sb;
	ssize_t n = pread(fd, &sb, sizeof(sb), (off_t)HUST_BLOCKSIZE);
	if (n != (ssize_t)sizeof(sb))
		die("pread superblock");

	expect_u64_eq("sb.magic", sb.magic, MAGIC_NUM);
	expect_u64_eq("sb.block_size", sb.block_size, HUST_BLOCKSIZE);
	expect_u64_eq("sb.blocks_count", sb.blocks_count, 100);

	/* Values derived from mkfs.c's current layout formula (using actual struct sizes). */
	uint64_t blocks_count = sb.blocks_count;
	uint64_t inode_size = sizeof(struct HUST_inode);
	uint64_t bmap_size_blocks = div_round_up_u64(blocks_count, (uint64_t)8 * HUST_BLOCKSIZE);
	uint64_t imap_size_blocks = div_round_up_u64(sb.inodes_count, (uint64_t)8 * HUST_BLOCKSIZE);
	uint64_t inode_per_block = HUST_BLOCKSIZE / inode_size;
	uint64_t inode_table_size_blocks = (inode_per_block == 0) ? 0 : (sb.inodes_count / inode_per_block);
	uint64_t expect_bmap_block = RESERVE_BLOCKS;
	uint64_t expect_imap_block = expect_bmap_block + bmap_size_blocks;
	uint64_t expect_inode_table_block = expect_imap_block + imap_size_blocks;
	uint64_t expect_data_block_number = RESERVE_BLOCKS + bmap_size_blocks + imap_size_blocks + inode_table_size_blocks;

	expect_u64_eq("sb.bmap_block", sb.bmap_block, expect_bmap_block);
	expect_u64_eq("sb.imap_block", sb.imap_block, expect_imap_block);
	expect_u64_eq("sb.inode_table_block", sb.inode_table_block, expect_inode_table_block);
	expect_u64_eq("sb.data_block_number", sb.data_block_number, expect_data_block_number);

	/* Root inode is the first inode in inode table. */
	struct HUST_inode root;
	n = pread(fd, &root, sizeof(root), (off_t)sb.inode_table_block * HUST_BLOCKSIZE);
	if (n != (ssize_t)sizeof(root))
		die("pread root inode");
	expect_u64_eq("root.inode_no", root.inode_no, HUST_ROOT_INODE_NUM);
	expect_u64_eq("root.blocks", root.blocks, 1);
	expect_u64_eq("root.block[0]", root.block[0], sb.data_block_number);
	expect_u64_eq("root.dir_children_count", root.dir_children_count, 3);
	expect_int64_eq("root.i_nlink", root.i_nlink, 2);

	/* Root directory entries live at data_block_number. */
	struct HUST_dir_record ent[3];
	n = pread(fd, &ent, sizeof(ent), (off_t)sb.data_block_number * HUST_BLOCKSIZE);
	if (n != (ssize_t)sizeof(ent))
		die("pread root dir");
	expect_str_eq("ent[0].filename", ent[0].filename, ".");
	expect_u64_eq("ent[0].inode_no", ent[0].inode_no, 0);
	expect_str_eq("ent[1].filename", ent[1].filename, "..");
	expect_u64_eq("ent[1].inode_no", ent[1].inode_no, 0);
	expect_str_eq("ent[2].filename", ent[2].filename, "file");
	expect_u64_eq("ent[2].inode_no", ent[2].inode_no, 1);

	close(fd);
	unlink(path);
	printf("test_mkfs: OK\n");
	return 0;
}
