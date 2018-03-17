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
    bh = sb_bread(sb, block_num+disk_sb->data_block_number);
    
    BUG_ON(!bh);
    memset(bh->b_data, 0, HUST_BLOCKSIZE);
    memcpy(bh->b_data, buf, size);
    brelse(bh);
    return 0;
}
int HUST_fs_get_block(struct inode *inode, sector_t block,
		      struct buffer_head *bh, int create)
{
	struct super_block *sb = inode->i_sb;
	
	printk(KERN_INFO "HUST: get block [%lu] of inode [%llu]\n", block,
	       inode->i_ino);
	if (block > HUST_N_BLOCKS) {
		return -ENOSPC;
	}
	struct HUST_inode H_inode;
	if (-1 == HUST_fs_get_inode(sb, inode->i_ino, &H_inode))
		return -EFAULT;
	if (H_inode.blocks == 0){
        if(alloc_block_for_inode(sb, &H_inode, 1)) {
            return -EFAULT;
        }
    }
    mark_inode_dirty(inode);
	map_bh(bh, sb, H_inode.block[0]);
	return 0;
}

int alloc_block_for_inode(struct super_block* sb, struct HUST_inode* p_H_inode, ssize_t size)
{
    struct HUST_fs_super_block* disk_sb;
    ssize_t bmap_size;
    uint8_t* bmap;
    unsigned int i;
    
    ssize_t alloc_blocks = size - p_H_inode->blocks;
    if(size + p_H_inode->blocks > HUST_N_BLOCKS){
        return -ENOSPC;
    }
    //read bmap
    disk_sb = sb->s_fs_info;
    bmap_size = disk_sb->blocks_count/8;
    bmap = kmalloc(bmap_size, GFP_KERNEL);
    
    if(get_bmap(sb, bmap, bmap_size))
    {
        kfree(bmap);
        return -EFAULT;
    }
    
    for(i = 0; i < alloc_blocks; ++i) {
        uint64_t empty_blk_num = HUST_find_first_zero_bit(bmap, disk_sb->blocks_count / 8);
        p_H_inode->block[p_H_inode->blocks] = empty_blk_num;
        p_H_inode->blocks++;
        uint64_t bit_off = empty_blk_num % (HUST_BLOCKSIZE*8);
        setbit(bmap[bit_off/8], bit_off%8);
    }
    save_bmap(sb,bmap,bmap_size);
    save_inode(sb,*p_H_inode);
    disk_sb->free_blocks -= size;
    kfree(bmap);
    return 0;
}

