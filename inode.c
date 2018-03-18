#include "constants.h"
#include "HUST_fs.h"
#include <linux/time.h>

extern struct file_operations HUST_fs_file_ops ;

extern struct file_operations HUST_fs_dir_ops;

extern struct inode_operations HUST_fs_inode_ops;

extern struct address_space_operations HUST_fs_aops;

int HUST_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct buffer_head * bh;
	struct HUST_inode * raw_inode = NULL;
    HUST_fs_get_inode(inode->i_sb, inode->i_ino, raw_inode);
	if (!raw_inode)
		return -EFAULT;
	raw_inode->mode = inode->i_mode;
	raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
	raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
	raw_inode->i_nlink = inode->i_nlink;
	raw_inode->file_size = inode->i_size;
    
    raw_inode->i_atime = (inode->i_atime.tv_sec);
    raw_inode->i_mtime = (inode->i_mtime.tv_sec);
    raw_inode->i_ctime = (inode->i_ctime.tv_sec);
    
	//raw_inode->i_time = inode->i_mtime.tv_sec;
	mark_buffer_dirty(bh);
	brelse(bh);
    return 0;
}

void HUST_evict_inode(struct inode *vfs_inode)
{
    struct super_block *sb = vfs_inode->i_sb;
    printk(KERN_INFO "HUST evict: Clearing inode [%lu]\n", vfs_inode->i_ino);
    truncate_inode_pages_final(&vfs_inode->i_data);
    clear_inode(vfs_inode);
    if (vfs_inode->i_nlink)
    {
        printk(KERN_INFO "HUST evict: Inode [%lu] still has links\n", vfs_inode->i_ino);
        return;
    }
    printk(KERN_INFO "HUST evict: Inode [%lu] has no links!\n", vfs_inode->i_ino);
    set_and_save_imap(sb, vfs_inode->i_ino, 0);
    return;
}

int HUST_fs_create(struct inode *dir, struct dentry *dentry, umode_t mode,bool excl)
{
    return HUST_fs_create_obj(dir, dentry, mode);
}

int HUST_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return HUST_fs_create_obj(dir, dentry, S_IFDIR | mode);
}

ssize_t HUST_write_inode_data(struct inode* inode, const void *buf, size_t count)
{
    struct super_block *sb;
    struct HUST_inode H_inode;
    
    sb = inode->i_sb;
    
    if(!buf) {
        printk(KERN_ERR "HUST: buf is null\n");
        return -EFAULT;
    }
    if(count > HUST_BLOCKSIZE*HUST_N_BLOCKS) {
        return -ENOSPC;
    }
    
    if (-1 == HUST_fs_get_inode(sb, inode->i_ino, &H_inode)){
        printk(KERN_ERR "HUST: cannot read inode\n");
		return -EFAULT;
    }
    if(count > HUST_BLOCKSIZE*H_inode.blocks) {
        int ret;
        ret = alloc_block_for_inode(sb, &H_inode, 
                              (count - HUST_BLOCKSIZE*H_inode.blocks) /HUST_BLOCKSIZE);
        if(ret) {
            return -EFAULT;
        }
        mark_inode_dirty(inode);
    }
    size_t count_res = count;
    int i;
    i = 0;
    while(count_res && i < HUST_N_BLOCKS) {
        struct buffer_head* bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        size_t cpy_size;
        if(count_res >= HUST_BLOCKSIZE) {
            count_res -= HUST_BLOCKSIZE;
            cpy_size = HUST_BLOCKSIZE;
        }
        else {
            count_res = 0;
            cpy_size = count_res;
        }
        memcpy(bh->b_data, buf+i*HUST_BLOCKSIZE, cpy_size);
        map_bh(bh, sb,H_inode.block[i]);
        i++;
        brelse(bh);
    }
    while(i < H_inode.blocks) {
        struct buffer_head* bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        memset(bh->b_data, 0, HUST_BLOCKSIZE);
        map_bh(bh, sb, H_inode.block[i]);
        brelse(bh);
        i++;
    }
    return count;
}

ssize_t HUST_read_inode_data(struct inode* inode,void* buf, size_t size)
{
    if(!buf) {
        printk(KERN_ERR "HUST: buf is null\n");
        return 0;
    }
    memset(buf, 0, size);
    struct super_block *sb = inode->i_sb;
	printk(KERN_INFO "HUST: read inode [%llu]\n", inode->i_ino);
	struct HUST_inode H_inode;
	if (-1 == HUST_fs_get_inode(sb, inode->i_ino, &H_inode)){
        printk(KERN_ERR "HUST: cannot read inode\n");
		return -EFAULT;
    }
    int i;
    for(i = 0; i < H_inode.blocks; ++i) {
        struct buffer_head* bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        if((i+1)*HUST_BLOCKSIZE > size){
            brelse(bh);
            return i*HUST_BLOCKSIZE;
        }
        memcpy(buf + i*(HUST_BLOCKSIZE), bh->b_data, HUST_BLOCKSIZE);
        brelse(bh);
    }
	return i*(HUST_BLOCKSIZE);
}

int HUST_fs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct super_block* sb = dir->i_sb;
    printk(KERN_INFO "HUST: unlink [%s] from dir inode [%lu]\n",
           dentry->d_name.name, dir->i_ino);
    struct HUST_inode H_dir_inode;
    if(HUST_fs_get_inode(sb, dir->i_ino,&H_dir_inode)) {
       return -EFAULT; 
    }
    ssize_t buf_size = H_dir_inode.blocks*HUST_BLOCKSIZE;
    void* buf = kmalloc(buf_size, GFP_KERNEL);
    if(HUST_read_inode_data(dir, buf, buf_size) != buf_size) {
        printk(KERN_ERR "HUST_read_inode_data failed\n");
        kfree(buf);
        return -EFAULT;
    }
    struct inode *inode = d_inode(dentry);
    int i;
    struct HUST_dir_record* p_dir;
    p_dir = (struct HUST_dir_record*) buf;
    for(i = 0; i < H_dir_inode.dir_children_count; ++i) {
        if(strncmp(dentry->d_name.name, p_dir[i].filename, HUST_FILENAME_MAX_LEN)) {
            /* We have found our directory entry. We can now clear i
             * and then decrease the inode's link count. 
             */
            H_dir_inode.dir_children_count -= 1;
            //remove it from buf
            struct HUST_dir_record* new_buf = kmalloc(buf_size - sizeof(struct HUST_dir_record), GFP_KERNEL);
            memcpy(new_buf, p_dir, (i)*sizeof(struct HUST_dir_record));
            memcpy(new_buf + i, p_dir + i + 1, 
                       (H_dir_inode.dir_children_count -i- 1)*sizeof(struct HUST_dir_record));
            HUST_write_inode_data(dir, new_buf, buf_size - sizeof(struct HUST_dir_record));
            kfree(new_buf);
            break;
        }
    }
    inode_dec_link_count(inode);
    mark_inode_dirty(inode);
    kfree(buf);
    save_inode(sb, H_dir_inode);
    return 0;
}

int HUST_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct super_block* sb = dir->i_sb;
    struct HUST_fs_super_block* disk_sb = sb->s_fs_info;
	printk(KERN_ERR "In create obj and dir is %llu\n", (uint64_t)dir);
    const unsigned char *name = dentry->d_name.name;
    
    struct HUST_inode H_dir_inode;
    HUST_fs_get_inode(sb, dir->i_ino, &H_dir_inode);
        
    if(H_dir_inode.dir_children_count >= HUST_BLOCKSIZE/sizeof(struct HUST_dir_record)) {
        return -ENOSPC;
    }
    //1. write inode
    uint64_t first_empty_inode_num = HUST_fs_get_empty_inode(dir->i_sb);
    BUG_ON(!first_empty_inode_num);
    struct inode* inode;
    struct HUST_inode raw_inode;
    inode = new_inode(sb);
    if(!inode) {
        return -ENOSPC;
    }
    inode->i_ino = first_empty_inode_num;
    raw_inode.inode_no = first_empty_inode_num;
    inode_init_owner(inode, dir, mode);
    inode->i_op = &HUST_fs_inode_ops;
    raw_inode.i_uid = i_uid_read(inode);
    raw_inode.i_gid = i_gid_read(inode);
    raw_inode.i_nlink = inode->i_nlink;
    struct timespec current_time;
    getnstimeofday(&current_time);
    inode->i_mtime = inode->i_atime = inode->i_ctime = current_time;
    raw_inode.i_atime = (inode->i_atime.tv_sec);
	raw_inode.i_ctime = (inode->i_ctime.tv_sec);
	raw_inode.i_mtime = (inode->i_mtime.tv_sec);
    
    raw_inode.mode = mode;
    if(S_ISDIR(mode)) {
        inode->i_size = 1;
        inode->i_blocks = 1;    
        inode->i_fop = &HUST_fs_dir_ops;
        
        raw_inode.blocks = 1;
        raw_inode.dir_children_count = 2;
        
        //2. write block
        if(disk_sb->free_blocks <= 0){
            return -ENOSPC;
        }        
        struct HUST_dir_record dir_arr[2];
        uint64_t first_empty_block_num = HUST_fs_get_empty_block(sb);
        raw_inode.block[0] = first_empty_block_num;
        const char* cur_dir = ".";
        const char* parent_dir = "..";
        memcpy(dir_arr[0].filename, cur_dir, strlen(cur_dir) + 1);
        dir_arr[0].inode_no = first_empty_inode_num;
        memcpy(dir_arr[1].filename, parent_dir, strlen(parent_dir) + 1);
        dir_arr[2].inode_no = dir->i_ino;    
        save_inode(sb, raw_inode);
        save_block(sb, first_empty_block_num, dir_arr, sizeof(struct HUST_dir_record)*2);
        set_and_save_bmap(sb, first_empty_block_num, 1);
        
        //update dir
        disk_sb->free_blocks-=1;
    }
    else if(S_ISREG(mode)) {
        inode->i_size = 0;
        inode->i_blocks = 0;
        inode->i_fop = &HUST_fs_file_ops;
        inode->i_mapping->a_ops = &HUST_fs_aops;
        raw_inode.blocks = 0;
        raw_inode.file_size = 0;
        
        //write inode
        save_inode(sb, raw_inode);
    }
    struct HUST_dir_record new_dir;
    memcpy(new_dir.filename, name, strlen(name)+1);
    new_dir.inode_no = first_empty_inode_num;
    struct buffer_head* bh;
    bh = sb_bread(sb, H_dir_inode.block[0]);
    memcpy(bh->b_data + H_dir_inode.dir_children_count*sizeof(struct HUST_dir_record), &new_dir, sizeof(new_dir));
    map_bh(bh, sb, H_dir_inode.block[0]);
    brelse(bh);
        
    //updata dir inode
    H_dir_inode.dir_children_count += 1;
    save_inode(sb, H_dir_inode);
        
    set_and_save_imap(sb, first_empty_inode_num, 1);
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    d_instantiate(dentry, inode);
    printk(KERN_ERR "first_empty_inode_num is %llu\n", first_empty_inode_num);
    return 0;
}

int HUST_fs_get_inode(struct super_block *sb,
		      uint64_t inode_no, struct HUST_inode *raw_inode)
{
	if (!raw_inode) {
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
	memcpy(raw_inode, H_inode_array + idx, sizeof(struct HUST_inode));
	if (raw_inode->inode_no != inode_no) {
		printk(KERN_ERR "inode not init");
	}
	return 0;
}

void HUST_fs_convert_inode(struct HUST_inode *H_inode, struct inode *vfs_inode)
{
	vfs_inode->i_ino = H_inode->inode_no;
	vfs_inode->i_mode = H_inode->mode;
	vfs_inode->i_size = H_inode->file_size;
    set_nlink(vfs_inode, H_inode->i_nlink);
    i_uid_write(vfs_inode, H_inode->i_uid);
    i_gid_write(vfs_inode, H_inode->i_gid);
    vfs_inode->i_atime.tv_sec = H_inode->i_atime;
    vfs_inode->i_ctime.tv_sec = H_inode->i_ctime;
    vfs_inode->i_mtime.tv_sec = H_inode->i_mtime;
    
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
                printk(KERN_ERR "uid is %lu and gid is %lu", inode->i_uid, inode->i_gid);
				inode->i_op = &HUST_fs_inode_ops;

				if (S_ISDIR(H_child_inode.mode)) {
					inode->i_fop = &HUST_fs_dir_ops;
				} else if (S_ISREG(H_child_inode.mode)) {
					inode->i_fop = &HUST_fs_file_ops;;
					inode->i_mapping->a_ops = &HUST_fs_aops;
				}
				inode->i_mode = H_child_inode.mode;
                inode->i_size = H_child_inode.file_size;
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
    printk(KERN_ERR "In save inode and inode_no is %llu and block_idx is %llu and bh is %llu\n", 
           inode_num, block_idx, sb);
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
