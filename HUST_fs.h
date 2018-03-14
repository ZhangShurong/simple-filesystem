
#ifndef HUST_FS_H
#define HUST_FS_H
/*
 * Based on psankar's simplefs:
 * https://github.com/psankar/simplefs */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include "constants.h"

#define setbit(number,x) number |= 1UL << x
#define clearbit(number, x) number &= ~(1UL << x)

int checkbit(uint8_t number, int x);

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
	mode_t mode;//sizeof(mode_t) is 4
	uint64_t inode_no;
	uint64_t blocks;
	uint64_t block[HUST_N_BLOCKS];
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};
#define HUST_INODE_SIZE sizeof(struct HUST_inode)
struct HUST_dir_record {
	char filename[HUST_FILENAME_MAX_LEN];
	uint64_t inode_no;
};

//Utils
ssize_t HUST_read_inode_data(struct inode* inode,void* buf, size_t size);
ssize_t HUST_write_inode_data(struct inode* inode, const void *buf, size_t count);
int HUST_find_first_zero_bit(const void *vaddr, unsigned size);
int get_bmap(struct super_block* sb, uint8_t* bmap, ssize_t bmap_size);
int get_imap(struct super_block* sb, uint8_t* imap, ssize_t imap_size);
uint64_t HUST_fs_get_empty_block(struct super_block* sb);
uint64_t HUST_fs_get_empty_inode(struct super_block* sb);
int save_inode(struct super_block* sb, struct HUST_inode H_inode);
int save_block(struct super_block* sb, uint64_t block_num, void* buf, ssize_t size);
int save_bmap(struct super_block* sb, uint8_t* bmap, ssize_t bmap_size);
int set_and_save_imap(struct super_block* sb, uint64_t inode_num, uint8_t value);
int set_and_save_bmap(struct super_block* sb, uint64_t block_num, uint8_t value);
int save_super(struct super_block* sb);

int HUST_fs_get_block(struct inode *inode, sector_t block,
                       struct buffer_head *bh, int create);
int alloc_block_for_inode(struct super_block* sb, struct HUST_inode* p_H_inode, ssize_t size);
int HUST_fs_readpage(struct file *file, struct page *page);
int HUST_fs_get_inode(struct super_block *sb,
		uint64_t inode_no, struct HUST_inode* inode);

int HUST_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int HUST_fs_create(struct inode *dir, struct dentry *dentry, umode_t mode,bool excl);

int HUST_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode);
int HUST_fs_unlink(struct inode *dir, struct dentry *dentry);
int HUST_fs_iterate(struct file *filp, struct dir_context *ctx);

struct dentry *HUST_fs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
    unsigned int flags);

int HUST_fs_fill_super(struct super_block *sb, void *data, int silent);

struct dentry *
HUST_fs_mount(struct file_system_type *fs_type, int flags,
                  const char *dev_name, void *data);
void
HUST_fs_kill_superblock(struct super_block *s);

#endif /* HUST_FS_H */
