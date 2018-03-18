#ifndef HUST_FS_H
#define HUST_FS_H

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
	mode_t mode; //sizeof(mode_t) is 4
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

#define HUST_INODE_SIZE sizeof(struct HUST_inode)
struct HUST_dir_record {
	char filename[HUST_FILENAME_MAX_LEN];
	uint64_t inode_no;
};

//inode_map anf block_map
int checkbit(uint8_t number, int x);
int HUST_find_first_zero_bit(const void *vaddr, unsigned size);
int get_bmap(struct super_block* sb, uint8_t* bmap, ssize_t bmap_size);
int get_imap(struct super_block* sb, uint8_t* imap, ssize_t imap_size);
uint64_t HUST_fs_get_empty_block(struct super_block* sb);
uint64_t HUST_fs_get_empty_inode(struct super_block* sb);
int save_bmap(struct super_block* sb, uint8_t* bmap, ssize_t bmap_size);
int set_and_save_imap(struct super_block* sb, uint64_t inode_num, uint8_t value);
int set_and_save_bmap(struct super_block* sb, uint64_t block_num, uint8_t value);

//block oprations
int save_block(struct super_block* sb, uint64_t block_num, void* buf, ssize_t size);
int HUST_fs_get_block(struct inode *inode, sector_t block,
                       struct buffer_head *bh, int create);
int alloc_block_for_inode(struct super_block* sb, struct HUST_inode* p_H_inode, ssize_t size);

//inode oprerations
ssize_t HUST_read_inode_data(struct inode* inode,void* buf, size_t size);
ssize_t HUST_write_inode_data(struct inode* inode, const void *buf, size_t count);
int save_inode(struct super_block* sb, struct HUST_inode H_inode);
int HUST_fs_get_inode(struct super_block* sb, uint64_t inode_no, struct HUST_inode* raw_inode);

//file operations
int HUST_fs_readpage(struct file *file, struct page *page);
int HUST_fs_writepage(struct page* page, struct writeback_control* wbc);
int HUST_fs_write_begin(struct file* file, struct address_space* mapping, 
		loff_t pos, unsigned len, unsigned flags, 
		struct page** pagep, void** fsdata);


//dir operations
int HUST_fs_iterate(struct file *filp, struct dir_context *ctx);
int HUST_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int HUST_fs_create(struct inode *dir, struct dentry *dentry, umode_t mode,bool excl);

int HUST_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode);
int HUST_fs_unlink(struct inode *dir, struct dentry *dentry);


struct dentry* HUST_fs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
    unsigned int flags);

//super_block operations
int save_super(struct super_block* sb);
int HUST_fs_fill_super(struct super_block *sb, void *data, int silent);
int HUST_write_inode(struct inode *inode, struct writeback_control *wbc);
void HUST_evict_inode(struct inode *vfs_inode);

//file-system operations
struct dentry* HUST_fs_mount(struct file_system_type *fs_type, int flags,
                  const char *dev_name, void *data);
void HUST_fs_kill_superblock(struct super_block *s);
#endif /* HUST_FS_H */
