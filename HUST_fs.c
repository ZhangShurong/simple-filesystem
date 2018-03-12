/*
 * Based on psankar's simplefs:
 * https://github.com/psankar/simplefs */

#include "constants.h"
#include "HUST_fs.h"

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
//for test
ssize_t HUST_file_read_iter(struct file* filp, char* buf, size_t len, loff_t* ppos){
	printk(KERN_ERR "filp->f_inode is %llu\n", filp->f_inode->i_ino);
	return 0;
}

const struct file_operations HUST_fs_file_ops = {
	.owner = THIS_MODULE,
	.llseek = generic_file_llseek,
	.mmap = generic_file_mmap,
	.fsync = generic_file_fsync,
	.read_iter = generic_file_read_iter,
//	.read = HUST_file_read_iter,

	.write_iter = generic_file_write_iter,
};

const struct file_operations HUST_fs_dir_ops = {
	.owner = THIS_MODULE,
	.iterate = HUST_fs_iterate,
};

const struct inode_operations HUST_fs_inode_ops = {
	.lookup = HUST_fs_lookup,
	.mkdir = HUST_fs_mkdir,
};

/*
const struct super_operations oneblockfs_super_ops = {
    .evict_inode = HUST_evict_inode,
    .write_inode = HUST_write_inode,
};
*/
int HUST_fs_writepage(struct page* page, struct writeback_control* wbc) {
	printk(KERN_ERR "HUST: in write page\n");
       return block_write_full_page(page, HUST_fs_get_block, wbc);
}
int HUST_fs_write_begin(struct file* file, struct address_space* mapping, 
		loff_t pos, unsigned len, unsigned flags, 
		struct page** pagep, void** fsdata) {
	printk("HUST: in write_begin\n");
	int ret;
    ret = block_write_begin(mapping, pos, len, flags, pagep, HUST_fs_get_block);
    if (unlikely(ret))
        printk(KERN_ERR "HUST: Write failed for pos [%llu], len [%u]\n", pos, len);

    return ret;
}
const struct address_space_operations HUST_fs_aops = {
	.readpage = HUST_fs_readpage,
	 .writepage = HUST_fs_writepage,
	.write_begin = HUST_fs_write_begin,
	.write_end = generic_write_end,
};

int HUST_fs_get_block(struct inode *inode, sector_t block,
		      struct buffer_head *bh, int create)
{
	struct super_block *sb = inode->i_sb;
	uint64_t data_block;
	printk(KERN_ERR "HUST: get block [%llu] of inode [%llu]\n", block,
	       inode->i_ino);
	if (block > 0) {
		return -ENOSPC;
	}
	struct HUST_inode H_inode;
	if (-1 == HUST_fs_get_inode(sb, inode->i_ino, &H_inode))
		return -EFAULT;
	if (H_inode.blocks == 0)
		return -EFAULT;
	map_bh(bh, sb, H_inode.block[0]);
	return 0;
}

int HUST_fs_readpage(struct file *file, struct page *page)
{
	printk(KERN_ERR "HUST: readpage");
	return block_read_full_page(page, HUST_fs_get_block);
}


int HUST_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return HUST_fs_create_obj(dir, dentry, S_IFDIR | mode);
}

int HUST_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct super_block* sb = dir->i_sb;
    struct HUST_fs_super_block* disk_sb = sb->s_fs_info;
	printk(KERN_ERR "In create obj and dir is %llu\n", (uint64_t)dir);
    const unsigned char *name = dentry->d_name.name;
    
    //1. write inode
    uint64_t new_inode_no = HUST_fs_get_empty_inode(dir->i_sb);
    BUG_ON(!new_inode_no);
    struct inode* inode;
    struct HUST_inode obj_inode;
    inode = new_inode(sb);
    if(!inode) {
        return -ENOSPC;
    }
    inode->i_ino = new_inode_no;
    obj_inode.inode_no = new_inode_no;
    inode_init_owner(inode, dir, mode);
    inode->i_op = &HUST_fs_inode_ops;
    obj_inode.mode = mode;
    if(S_ISDIR(mode)) {
        inode->i_size = 1;
        inode->i_blocks = 1;    
        inode->i_fop = &HUST_fs_dir_ops;
        
        obj_inode.blocks = 1;
        obj_inode.dir_children_count = 2;
        
        //2. write block
        if(disk_sb->free_blocks <= 0){
            return -ENOSPC;
        }        
        struct HUST_dir_record dir_arr[2];
        uint64_t first_empty_block_num = HUST_fs_get_empty_block(sb);
        const char* cur_dir = ".";
        const char* parent_dir = "..";
        memcpy(dir_arr[0].filename, cur_dir, strlen(cur_dir) + 1);
        dir_arr[0].inode_no = new_inode_no;
        memcpy(dir_arr[1].filename, parent_dir, strlen(parent_dir) + 1);
        dir_arr[2].inode_no = dir->i_ino;    
        save_inode(sb, obj_inode);
        save_block(sb, first_empty_block_num, dir_arr, sizeof(dir_arr));
        save_bmap(sb, first_memory_node, 1);
        
        //update dir
        
        struct HUST_inode H_dir_inode;
        HUST_fs_get_inode(sb, dir->i_ino, &H_dir_inode);
        
        if(H_dir_inode.dir_children_count >= HUST_BLOCKSIZE/sizeof(struct HUST_dir_record))
        {
            return -ENOSPC;
        }
        
        struct HUST_dir_record new_dir;
        memcpy(new_dir.filename, name, strlen(name)+1);
        new_dir.inode_no = new_inode_no;
        struct buffer_head* bh;
        bh = sb_bread(sb, H_dir_inode.block[0]);
        memcpy(bh->b_data + H_dir_inode.dir_children_count*sizeof(struct HUST_dir_record), &new_dir, sizeof(new_dir));
        map_bh(bh, sb, H_dir_inode.block[0]);
        brelse(bh);
        
        //updata dir inode
        H_dir_inode.dir_children_count += 1;
        save_inode(sb, H_dir_inode);
    }
    else if(S_ISREG(mode)) {
        inode->i_size = 0;
        inode->i_blocks = 0;        
        obj_inode.blocks = 0;
        obj_inode.file_size = 0;
    }
        
    
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    d_instantiate(dentry, inode);
    return 0;
}

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

int HUST_fs_iterate(struct file *filp, struct dir_context *ctx)
{
	struct HUST_inode H_inode;
	struct super_block *sb = filp->f_inode->i_sb;

	printk(KERN_INFO "HUST_fs: Iterate on inode [%llu]\n",
	       filp->f_inode->i_ino);

	if (-1 == HUST_fs_get_inode(sb, filp->f_inode->i_ino, &H_inode))
		return -EFAULT;

	printk(KERN_INFO "H_inode.dir_children_count is %llu\n",
	       H_inode.dir_children_count);
	if (ctx->pos >= H_inode.dir_children_count) {
		return 0;
	}

	if (H_inode.blocks == 0) {
		printk(KERN_INFO
		       "HUST_fs: inode [%lu] has no data!\n",
		       filp->f_inode->i_ino);
		return 0;
	}

	uint64_t i, dir_unread;
	dir_unread = H_inode.dir_children_count;
	printk(KERN_INFO "HUST_fs: dir_unread [%llu]\n", dir_unread);
	if (dir_unread == 0) {
		return 0;
	}

	struct HUST_dir_record *dir_arr =
	    kmalloc(sizeof(struct HUST_dir_record) * dir_unread, GFP_KERNEL);

	struct buffer_head *bh;
	for (i = 0; (i < H_inode.blocks) && (dir_unread > 0); ++i) {
		bh = sb_bread(sb, H_inode.block[i]);
		uint64_t len = dir_unread * sizeof(struct HUST_dir_record);
		uint64_t off = H_inode.dir_children_count - dir_unread;
		if (len < HUST_BLOCKSIZE) {	//read over
			memcpy(dir_arr + (off * sizeof(struct HUST_dir_record)),
			       bh->b_data, len);
			dir_unread = 0;
		} else {
			memcpy(dir_arr + (off * sizeof(struct HUST_dir_record)),
			       bh->b_data, HUST_BLOCKSIZE);
			dir_unread -=
			    HUST_BLOCKSIZE / sizeof(struct HUST_dir_record);
		}
		brelse(bh);
	}
	for (i = 0; i < H_inode.dir_children_count; ++i) {
		printk(KERN_INFO " dir_arr[i].filename is %s\n",
		       dir_arr[i].filename);
		dir_emit(ctx, dir_arr[i].filename, strlen(dir_arr[i].filename),
			 dir_arr[i].inode_no, DT_REG);
		ctx->pos++;
	}
	kfree(dir_arr);
	printk(KERN_INFO "ctx->pos is %llu\n", ctx->pos);
	return 0;
}

void HUST_fs_convert_inode(struct HUST_inode *H_inode, struct inode *vfs_inode)
{
	vfs_inode->i_ino = H_inode->inode_no;
	vfs_inode->i_mode = H_inode->mode;
	vfs_inode->i_size = H_inode->file_size;
	//vfs_inode->i_private = *H_inode;
}

struct dentry *HUST_fs_lookup(struct inode *parent_inode,
			      struct dentry *child_dentry, unsigned int flags)
{
	struct super_block *sb = parent_inode->i_sb;
	struct HUST_inode H_inode;
	struct inode *inode = NULL;
	uint64_t data_block = 0, i;
	struct HUST_dir_record *dtptr;
	struct buffer_head *bh;

	printk(KERN_ERR "HUST_fs: lookup [%s] in inode [%lu]\n",
	       child_dentry->d_name.name, parent_inode->i_ino);

	if (-1 == HUST_fs_get_inode(sb, parent_inode->i_ino, &H_inode)){
		printk(KERN_ERR "HUST: cannot get inode\n");
		return ERR_PTR(-EFAULT);
	}

	data_block = H_inode.block[0];
	bh = sb_bread(sb, data_block);
	if (!bh) {
		printk(KERN_ERR
		       "1bfs lookup: Could not read data block [%llu]\n",
		       data_block);
		return ERR_PTR(-EFAULT);
	}

	dtptr = (struct HUST_dir_record *)bh->b_data;

	for (i = 0; i < H_inode.dir_children_count; i++) {
		printk(KERN_ERR "child_dentry is %s and file name is %s\n", child_dentry->d_name.name, dtptr[i].filename);
		if (strncmp
		    (child_dentry->d_name.name, dtptr[i].filename,
		     HUST_FILENAME_MAX_LEN) == 0) {

					printk(KERN_ERR "in case 1");
			inode = iget_locked(sb, dtptr[i].inode_no);
			if (!inode) {
				printk(KERN_ERR
				       "HUST_fs lookup: iget_locked() returned NULL\n");
				brelse(bh);
				return ERR_PTR(-EFAULT);
			}

			if (inode->i_state & I_NEW) {
				inode_init_owner(inode, parent_inode, 0);

				struct HUST_inode H_child_inode;
				if (-1 == HUST_fs_get_inode(sb, dtptr[i].inode_no, &H_child_inode))
				{
					return ERR_PTR(-EFAULT);
				}

				HUST_fs_convert_inode(&H_child_inode, inode);

				inode->i_op = &HUST_fs_inode_ops;

				if (S_ISDIR(H_child_inode.mode)) {
					printk(KERN_ERR "in case a");
					inode->i_fop = &HUST_fs_dir_ops;
				} else if (S_ISREG(H_child_inode.mode)) {

					printk(KERN_ERR "in case b");
					inode->i_fop = &HUST_fs_file_ops;;
					inode->i_mapping->a_ops = &HUST_fs_aops;
				}

					printk(KERN_ERR "in case c");
				/* XXX Clarify meaning of this function. */
				insert_inode_hash(inode);
				printk(KERN_ERR "inode->i_sb is %llu and sb is %llu\n", (uint64_t) inode->i_sb, (uint64_t)sb);
				unlock_new_inode(inode);
			}

			d_add(child_dentry, inode);
			brelse(bh);

	printk(KERN_ERR "lookup over at a\n");
			return NULL;
		}
	}

	d_add(child_dentry, NULL);
	brelse(bh);
	printk(KERN_ERR "lookup over\n");
	return NULL;
}

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
	root_inode->i_fop = &HUST_fs_dir_ops;

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


int checkbit(uint8_t number, int x)
{
    return (number >> x) & 1U;
}
int HUST_find_first_zero_bit(const void *vaddr, unsigned size)
{
	const unsigned short *p = vaddr, *addr = vaddr;
	unsigned short num;

	if (!size)
		return 0;

	size >>= 4;
	while (*p++ == 0xffff) {
		if (--size == 0)
			return (p - addr) << 4;
	}

	num = *--p;
	return ((p - addr) << 4) + ffz(num);
}
uint64_t HUST_fs_get_empty_inode(struct super_block* sb)
{
    struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
    //read imap
    
    uint64_t imap_size = disk_sb->blocks_count / 8;
	printk(KERN_INFO "imap_size is %llu\n", imap_size);
	uint8_t *imap = kmalloc(imap_size, GFP_KERNEL);
	uint64_t i;
	for (i = disk_sb->imap_block;
	     i < disk_sb->data_block_number && imap_size != 0; ++i) {
        
		struct buffer_head *bh;
		bh = sb_bread(sb, i);

		if (!bh) {
			printk(KERN_ERR "bh empty\n");
		}
		uint8_t *imap_t = (uint8_t *) bh->b_data;
		printk(KERN_INFO "imap is %x\n", imap_t[0]);
		if (imap_size >= HUST_BLOCKSIZE) {
			memcpy(imap, imap_t, HUST_BLOCKSIZE);
			imap_size -= HUST_BLOCKSIZE;
		} else {
			memcpy(imap, imap_t, imap_size);
			imap_size = 0;
		}
		brelse(bh);
	}
	uint64_t empty_ilock_num = HUST_find_first_zero_bit(imap, disk_sb->blocks_count / 8);
	printk(KERN_INFO "HUST_find_first_zero_bit at %llu\n",
	       empty_ilock_num);
    kfree(imap);
    return empty_ilock_num;
}
uint64_t HUST_fs_get_empty_block(struct super_block* sb)
{
	struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
	
	//read imap and bmap
	uint64_t bmap_empty = disk_sb->blocks_count / 8;
	printk(KERN_INFO "bmap_empty is %llu\n", bmap_empty);
	uint8_t *bmap = kmalloc(bmap_empty, GFP_KERNEL);
	uint64_t i;
	for (i = disk_sb->bmap_block;
	     i < disk_sb->imap_block && bmap_empty != 0; ++i) {
		struct buffer_head *bh;
		bh = sb_bread(sb, i);

		if (!bh) {
			printk(KERN_ERR "bh empty\n");
		}
		uint8_t *bmap_t = (uint8_t *) bh->b_data;
		printk(KERN_INFO "bmap is %x\n", bmap_t[0]);
		if (bmap_empty >= HUST_BLOCKSIZE) {
			memcpy(bmap, bmap_t, HUST_BLOCKSIZE);
			bmap_empty -= HUST_BLOCKSIZE;
		} else {
			memcpy(bmap, bmap_t, bmap_empty);
			bmap_empty = 0;
		}
		brelse(bh);
	}
	uint64_t empty_block_num = HUST_find_first_zero_bit(bmap, disk_sb->blocks_count / 8);
	printk(KERN_INFO "HUST_find_first_zero_bit at %llu\n",
	       empty_block_num);
    kfree(bmap);
    return empty_block_num;
}

int save_imap(struct super_block* sb, uint64_t inode_num, uint8_t value)
{
    /*
     * 1. find one block we want to change;
     * 2. write the block
     */
	
    struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
    uint64_t block_idx = inode_num / (HUST_BLOCKSIZE*8) + disk_sb->bmap_block;
    uint64_t bit_off = inode_num % (HUST_BLOCKSIZE*8);
    
    struct buffer_head* bh;
    bh = sb_bread(sb, block_idx);
    
    printk(KERN_ERR "In save imap\n");
    
    BUG_ON(!bh);
    if(value == 1){
        setbit(bh->b_data[bit_off/8], bit_off%8);
    }
    else if(value == 0){
        clearbit(bh->b_data[bit_off/8], bit_off%8);
    }
    else{
        printk(KERN_ERR "value error\n");
    }
    map_bh(bh, sb, block_idx);
    brelse(bh);
    return 0;
}

int save_inode(struct super_block* sb, struct HUST_inode H_inode)
{
    uint64_t inode_num = H_inode.inode_no;
    struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
    uint64_t block_idx = inode_num*sizeof(struct HUST_inode) / HUST_BLOCKSIZE 
        + disk_sb->inode_table_block ;
    uint64_t arr_off = inode_num % (HUST_BLOCKSIZE / sizeof(struct HUST_inode));
    
    //1. read disk inode
    struct buffer_head* bh;
    bh = sb_bread(sb, block_idx);
    printk(KERN_ERR "In save inode and inode_no is %llu\n", inode_num);
    BUG_ON(!bh);
    
    //2. change disk inode, TODO:verify inode
    struct HUST_inode* p_disk_inode;
    p_disk_inode = (struct HUST_inode*)bh->b_data;
    memcpy(p_disk_inode + arr_off, &H_inode, sizeof(H_inode));
    
    //3. save disk inode
    map_bh(bh, sb, block_idx);
    brelse(bh);
    return 0;
}
int save_block(struct super_block* sb, uint64_t block_num, void* buf, ssize_t size)
{
    /*
     * 1. read disk block
     * 2. change block
     * 2.1 TODO verify block
     * 3. save block
     */
    struct HUST_fs_super_block *disk_sb;
    disk_sb = sb->s_fs_info;
    struct buffer_head* bh;
    bh = sb_bread(sb, block_num+disk_sb->data_block_number);
    
    BUG_ON(!bh);
    memset(bh->b_data, 0, HUST_BLOCKSIZE);
    memcpy(bh->b_data, buf, size);
    brelse(bh);
    return 0;
}
int save_bmap(struct super_block* sb, uint64_t block_num, uint8_t value)
{
        /*
     * 1. find one block we want to change;
     * 2. write the block
     */
    struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
    uint64_t block_idx = block_num / (HUST_BLOCKSIZE*8) + disk_sb->imap_block;
    uint64_t bit_off = block_num % (HUST_BLOCKSIZE*8);
    
    struct buffer_head* bh;
    bh = sb_bread(sb, block_idx);
    
    printk(KERN_ERR "In save bmap\n");
    
    BUG_ON(!bh);
    if(value == 1){
        setbit(bh->b_data[bit_off/8], bit_off%8);
    }
    else if(value == 0){
        clearbit(bh->b_data[bit_off/8], bit_off%8);
    }
    else{
        printk(KERN_ERR "value error\n");
    }
    map_bh(bh, sb, block_idx);
    brelse(bh);
    return 0;
}
int save_super(struct super_block* sb)
{
    struct HUST_fs_super_block *disk_sb = sb->s_fs_info;
    
	return 0;
}


