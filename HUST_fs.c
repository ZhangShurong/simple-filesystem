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

#include "constants.h"
#include "HUST_fs.h"

int HUST_fs_get_inode(struct super_block *sb,
		      uint64_t inode_no, struct HUST_inode *inode)
{
	if (!inode) {
		printk(KERN_ERR "inode is null");
		return -1;
	}
	struct HUST_fs_super_block *H_sb = sb->s_fs_info;
	struct HUST_inode *H_inode_array = NULL;

	int i;
	struct buffer_head *bh;
	bh = sb_bread(sb,
		      H_sb->inode_table_block
		      + inode_no * sizeof(struct HUST_inode) / HUST_BLOCKSIZE);
	printk(KERN_INFO "H_sb->inode_table_block is %lld",
	       H_sb->inode_table_block);
	BUG_ON(!bh);
	//TODO 
	H_inode_array = (struct HUST_inode *)bh->b_data;
	int idx = inode_no % (HUST_BLOCKSIZE / sizeof(struct HUST_inode));
	ssize_t inode_array_size = HUST_BLOCKSIZE / sizeof(struct HUST_inode);
	if (idx > inode_array_size) {
		printk(KERN_ERR "in get_inode: out of index");
		return -1;
	}
	memcpy(inode, H_inode_array + idx, sizeof(struct HUST_inode));
	if (inode->inode_no != inode_no) {
		printk(KERN_ERR "inode not init");
	}
	return 0;
}

ssize_t
HUST_fs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos)
{
	struct buffer_head *bh;
	sector_t at_block;
	loff_t into_block;
	unsigned long c2u_ret, c2u_len;

	if (*ppos >= filp->f_inode->i_size)
		return 0;

	at_block = (*ppos >> 12) + 1;
	into_block = *ppos - ((*ppos >> 12) << 12);

	c2u_len = min((loff_t) len, 4096 - into_block);
	c2u_len = min((loff_t) c2u_len, filp->f_inode->i_size - *ppos);

	printk(KERN_INFO
	       "read block %lu, offset in block %llu, returning %lu bytes\n",
	       at_block, into_block, c2u_len);

	bh = sb_bread(filp->f_inode->i_sb, at_block);
	if (!bh) {
		printk(KERN_ERR "Error reading block %lu\n", at_block);
		return -EFAULT;
	}
	c2u_ret = copy_to_user(buf, bh->b_data + into_block, c2u_len);
	brelse(bh);
	if (c2u_ret) {
		printk(KERN_ERR "Error in copy_to_user()\n");
		return -EFAULT;
	}

	*ppos += c2u_len;
	return c2u_len;
}

//  const struct address_space_operations HUST_fs_aops = {
//     .readpage = HUST_fs_readpage,
//     .writepage = HUST_fs_writepage,
//     .write_begin = HUST_fs_write_begin,
//     .write_end = HUST_fs_write_end,
// };

const struct file_operations HUST_fs_file_ops = {
	.owner = THIS_MODULE,
	.llseek = generic_file_llseek,
	.mmap = generic_file_mmap,
	.fsync = generic_file_fsync,
	.read_iter = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
};

int HUST_fs_iterate(struct file *filp, struct dir_context *ctx)
{
	/* Emit the standard entries "." and ".." and quit. */
	dir_emit_dots(filp, ctx);
	if (ctx->pos == 2) {
		dir_emit(ctx, "file", 4, 2, DT_REG);
		ctx->pos++;
	}
	return 0;
}

const struct file_operations HUST_fs_dir_operations = {
	.owner = THIS_MODULE,
	.iterate = HUST_fs_iterate,
};

struct dentry *HUST_fs_lookup(struct inode *parent_inode,
			      struct dentry *child_dentry, unsigned int flags)
{
	struct inode *inode;
	struct buffer_head *bh;
	uint64_t filesize;

	printk(KERN_INFO "lookup:[%s]\n", child_dentry->d_name.name);

	if (strcmp(child_dentry->d_name.name, "file") != 0) {
		printk(KERN_ERR "lookup:no inode for [%s] found\n",
		       child_dentry->d_name.name);
		d_add(child_dentry, NULL);
		return NULL;
	}

	inode = iget_locked(parent_inode->i_sb, "file");
	if (!inode) {
		printk(KERN_ERR "lookup: iget_locked() return NULL\n");
		return ERR_PTR(-ENOMEM);
	}

	if (inode->i_state & I_NEW) {
		bh = sb_bread(parent_inode->i_sb, 0);
		if (!bh) {
			printk(KERN_ERR
			       "lookup: sb_bread for block 0 failed\n");
			return ERR_PTR(-EFAULT);
		}
		filesize = ((uint64_t *) bh->b_data)[0];
		brelse(bh);

		inode_init_owner(inode, parent_inode, S_IFREG |
				 S_IRUSR | S_IXUSR |
				 S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		inode->i_ino = 2;
		inode->i_atime = inode->i_mtime = inode->i_ctime =
		    current_time(inode);
		inode->i_size = (loff_t) filesize;
		inode->i_fop = &HUST_fs_file_ops;

		unlock_new_inode(inode);
	}

	d_add(child_dentry, inode);
	return NULL;
}

const struct inode_operations HUST_fs_inode_ops = {
	.lookup = HUST_fs_lookup,
};

int HUST_fs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret = -EPERM;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);
	BUG_ON(!bh);
	struct HUST_fs_super_block *sb_disk;
	sb_disk = (struct HUST_fs_super_block *)bh->b_data;

	printk(KERN_INFO "HUST_fs: version num is %lu\n", sb_disk->version);
	printk(KERN_INFO "HUST_fs: magic num is %lu\n", sb_disk->magic);
	printk(KERN_INFO "HUST_fs: block_size num is %lu\n",
	       sb_disk->block_size);
	printk(KERN_INFO "HUST_fs: inodes_count num is %lu\n",
	       sb_disk->inodes_count);
	printk(KERN_INFO "HUST_fs: free_blocks num is %lu\n",
	       sb_disk->free_blocks);
	printk(KERN_INFO "HUST_fs: blocks_count num is %lu\n",
	       sb_disk->blocks_count);

	if (sb_disk->magic != MAGIC_NUM) {
		printk(KERN_ERR "Magic number not match!\n");
		goto release;
	}

	struct inode *root_inode;

	if (sb_disk->block_size != 4096) {
		printk(KERN_ERR "HUST_fs expects a blocksize of %d\n", 4096);
		ret = -EFAULT;
		goto release;
	}
	//fill vfs super block
	sb->s_magic = sb_disk->magic;
	sb->s_fs_info = sb_disk;
	sb->s_maxbytes = HUST_BLOCKSIZE * HUST_N_BLOCKS;	/* Max file size */
	//TODO sd->s_op = HUST_sops;

	//-----------test get inode-----
	struct HUST_inode root_node;
	if (HUST_fs_get_inode(sb, HUST_ROOT_INODE_NUM, &root_node) != -1) {
		printk(KERN_INFO "Get inode sucessfully!\n");
		printk(KERN_INFO "root blocks is %llu and block[0] is %llu\n",
		       root_node.blocks, root_node.block[0]);
	}
	//-----------end test-----------

	root_inode = new_inode(sb);
	if (!root_inode)
		return -ENOMEM;

	/* Our root inode. It doesn't contain useful information for now.
	 * Note that i_ino must not be 0, since valid inode numbers start at
	 * 1. */
	inode_init_owner(root_inode, NULL, S_IFDIR |
			 S_IRUSR | S_IXUSR |
			 S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	root_inode->i_sb = sb;
	root_inode->i_ino = HUST_ROOT_INODE_NUM;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
	    current_time(root_inode);

	//root_inode->i_private = HUST_fs_get_inode(sb, HUST_ROOT_INODE_NUM);
	/* Doesn't really matter. Since this is a directory, it "should"
	 * have a link count of 2. See btrfs, though, where directories
	 * always have a link count of 1. That appears to work, even though
	 * it created a number of bug reports in other tools. :-) Just
	 * search the web for that topic. */
	inc_nlink(root_inode);

	/* What can you do with our inode? */
	root_inode->i_op = &HUST_fs_inode_ops;
	root_inode->i_fop = &HUST_fs_dir_operations;

	/* Make a struct dentry from our inode and store it in our
	 * superblock. */
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
		return -ENOMEM;
 release:
	brelse(bh);

	return 0;
}

struct dentry *HUST_fs_mount(struct file_system_type *fs_type, int flags,
			     const char *dev_name, void *data)
{
	struct dentry *ret;

	/* This is a generic mount function that accepts a callback. */
	ret = mount_bdev(fs_type, flags, dev_name, data, HUST_fs_fill_super);

	/* Use IS_ERR to find out if this pointer is a valid pointer to data
	 * or if it indicates an error condition. */
	if (IS_ERR(ret))
		printk(KERN_ERR "Error mounting HUST_fs\n");
	else
		printk(KERN_INFO "HUST_fs is succesfully mounted on [%s]\n",
		       dev_name);

	return ret;
}

void HUST_fs_kill_superblock(struct super_block *s)
{
	/* We don't do anything here, but it's important to call
	 * kill_block_super because it frees some internal resources. */
	kill_block_super(s);
	printk(KERN_INFO
	       "HUST_fs superblock is destroyed. Unmount succesful.\n");
}

/* Information about our filesystem, available operations, ...
 *
 * Note that this does *not* specify available operations on filesystem
 * objects such as files. */
struct file_system_type HUST_fs_type = {
	.owner = THIS_MODULE,
	.name = "HUST_fs",
	.mount = HUST_fs_mount,
	.kill_sb = HUST_fs_kill_superblock,	/* unmount */
};

/* Called when the module is loaded. */
int HUST_fs_init(void)
{
	int ret;

	ret = register_filesystem(&HUST_fs_type);
	if (ret == 0)
		printk(KERN_INFO "Sucessfully registered HUST_fs\n");
	else
		printk(KERN_ERR "Failed to register HUST_fs. Error: [%d]\n",
		       ret);

	return ret;
}

/* Called when the module is unloaded. */
void HUST_fs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&HUST_fs_type);

	if (ret == 0)
		printk(KERN_INFO "Sucessfully unregistered HUST_fs\n");
	else
		printk(KERN_ERR "Failed to unregister HUST_fs. Error: [%d]\n",
		       ret);
}

module_init(HUST_fs_init);
module_exit(HUST_fs_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("cv");
