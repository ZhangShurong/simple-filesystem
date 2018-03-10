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
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>


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

int HUST_fs_readpage(struct file *file, struct page *page);
int HUST_fs_get_inode(struct super_block *sb,
		uint64_t inode_no, struct HUST_inode* inode);

int HUST_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

int HUST_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode);

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
