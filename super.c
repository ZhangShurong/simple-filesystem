/*
 * HUST_fs: a simple block-based filesystem (teaching project).
 *
 * Notes:
 * - On-disk superblock lives at block 1.
 * - Never store pointers into buffer_head->b_data in sb->s_fs_info.
 */

#include "HUST_fs.h"
#include "constants.h"


struct file_system_type HUST_fs_type = {
	.owner = THIS_MODULE,
	.name = "HUST_fs",
	.mount = HUST_fs_mount,
	.kill_sb = HUST_fs_kill_superblock,	/* unmount */
};

const struct file_operations HUST_fs_file_ops = {
	.owner = THIS_MODULE,
	.llseek = generic_file_llseek,
	.mmap = generic_file_mmap,
	.fsync = generic_file_fsync,
	.read_iter = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
};

const struct file_operations HUST_fs_dir_ops = {
	.owner = THIS_MODULE,
	.iterate = HUST_fs_iterate,
};

const struct inode_operations HUST_fs_inode_ops = {
	.lookup = HUST_fs_lookup,
	.mkdir = HUST_fs_mkdir,
    .create = HUST_fs_create,
    .unlink = HUST_fs_unlink,
};

const struct super_operations HUST_fs_super_ops = {
    .evict_inode = HUST_evict_inode,
    .write_inode = HUST_write_inode,
};

const struct address_space_operations HUST_fs_aops = {
	.readpage = HUST_fs_readpage,
    .writepage = HUST_fs_writepage,
	.write_begin = HUST_fs_write_begin,
	.write_end = generic_write_end,
};

int save_super(struct super_block* sb)
{
	struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
	struct buffer_head* bh;

	if (!disk_sb)
		return -EFAULT;

	bh = sb_bread(sb, 1);
	if (!bh)
		return -EIO;

	memcpy(bh->b_data, disk_sb, sizeof(*disk_sb));
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}

int HUST_fs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret = -EPERM;
	struct buffer_head *bh;
	struct HUST_fs_super_block *sb_disk = NULL;

	bh = sb_bread(sb, 1);
	if (!bh)
		return -EIO;

	sb_disk = kzalloc(sizeof(*sb_disk), GFP_KERNEL);
	if (!sb_disk) {
		ret = -ENOMEM;
		goto release;
	}
	memcpy(sb_disk, bh->b_data, sizeof(*sb_disk));

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
		ret = -EINVAL;
		goto release;
	}

	struct inode *root_inode;

	if (sb_disk->block_size != HUST_BLOCKSIZE) {
		printk(KERN_ERR "HUST_fs expects a blocksize of %d\n", 4096);
		ret = -EFAULT;
		goto release;
	}
	if (!sb_set_blocksize(sb, HUST_BLOCKSIZE)) {
		ret = -EFAULT;
		goto release;
	}
	/* Fill VFS super block. */
	sb->s_magic = sb_disk->magic;
	sb->s_fs_info = sb_disk;
	sb->s_maxbytes = HUST_BLOCKSIZE * HUST_N_BLOCKS;	/* Max file size */
	sb->s_op = &HUST_fs_super_ops;

	//-----------test get inode-----
	struct HUST_inode raw_root_node;
	if (HUST_fs_get_inode(sb, HUST_ROOT_INODE_NUM, &raw_root_node) != -1) {
		printk(KERN_INFO "Get inode sucessfully!\n");
		printk(KERN_INFO "root blocks is %llu and block[0] is %llu\n",
		       raw_root_node.blocks, raw_root_node.block[0]);
	}
	//-----------end test-----------

	root_inode = new_inode(sb);
	if (!root_inode) {
		ret = -ENOMEM;
		goto release;
	}

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
    
    root_inode->i_mode = raw_root_node.mode;
    root_inode->i_size = raw_root_node.dir_children_count;
	//root_inode->i_private = HUST_fs_get_inode(sb, HUST_ROOT_INODE_NUM);
	/* Doesn't really matter. Since this is a directory, it "should"
	 * have a link count of 2. See btrfs, though, where directories
	 * always have a link count of 1. That appears to work, even though
	 * it created a number of bug reports in other tools. :-) Just
	 * search the web for that topic. */
	inc_nlink(root_inode);

	/* What can you do with our inode? */
	root_inode->i_op = &HUST_fs_inode_ops;
	root_inode->i_fop = &HUST_fs_dir_ops;

	/* Make a struct dentry from our inode and store it in our
	 * superblock. */
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		ret = -ENOMEM;
		goto release;
	}
 release:
	if (bh)
		brelse(bh);
	if (ret) {
		kfree(sb_disk);
		sb->s_fs_info = NULL;
	}
	return ret;
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
	/* Free private superblock copy. */
	kfree(s->s_fs_info);
	s->s_fs_info = NULL;
	/* We don't do anything here, but it's important to call
	 * kill_block_super because it frees some internal resources. */
	kill_block_super(s);
	printk(KERN_INFO
	       "HUST_fs superblock is destroyed. Unmount succesful.\n");
}


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
