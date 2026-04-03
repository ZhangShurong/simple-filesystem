/* Block I/O helpers and VFS -> disk block mapping. */

#include "constants.h"
#include "HUST_fs.h"

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
    /* block_num stored in inode is an on-disk absolute block number. */
    bh = sb_bread(sb, block_num);
    
    BUG_ON(!bh);
    memset(bh->b_data, 0, HUST_BLOCKSIZE);
    memcpy(bh->b_data, buf, size);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}
int HUST_fs_get_block(struct inode *inode, sector_t block,
		      struct buffer_head *bh, int create)
{
	struct super_block *sb = inode->i_sb;
	
	printk(KERN_INFO "HUST: get block [%lu] of inode [%llu]\n", block,
	       inode->i_ino);
	if (block >= HUST_N_BLOCKS) {
		return -ENOSPC;
	}
	struct HUST_inode H_inode;
	if (-1 == HUST_fs_get_inode(sb, inode->i_ino, &H_inode))
		return -EFAULT;
	if ((uint64_t)block >= H_inode.blocks) {
		if (!create)
			return 0;
		/* Allocate up to block+1 blocks. */
		if (alloc_block_for_inode(sb, &H_inode, (ssize_t)block + 1))
			return -EFAULT;
		mark_inode_dirty(inode);
	}
	map_bh(bh, sb, H_inode.block[block]);
	return 0;
}

int alloc_block_for_inode(struct super_block* sb, struct HUST_inode* p_H_inode, ssize_t target_blocks)
{
    struct HUST_fs_super_block* disk_sb;
    unsigned int i;

	if (target_blocks < 0)
		return -EINVAL;
	if (target_blocks > HUST_N_BLOCKS)
        return -ENOSPC;

    if (p_H_inode->blocks >= (uint64_t)target_blocks)
        return 0;

    /* Allocate new blocks. */
    disk_sb = sb->s_fs_info;

    for(i = (unsigned int)p_H_inode->blocks; i < (unsigned int)target_blocks; ++i) {
        uint64_t empty_blk_num = HUST_fs_get_empty_block(sb);
        if ((int64_t)empty_blk_num < 0)
            return -EFAULT;
        p_H_inode->block[p_H_inode->blocks] = empty_blk_num;
        p_H_inode->blocks++;
        set_and_save_bmap(sb, empty_blk_num, 1);
        if (disk_sb && disk_sb->free_blocks > 0)
            disk_sb->free_blocks -= 1;
    }

    save_inode(sb, *p_H_inode);
    save_super(sb);
    return 0;
}
