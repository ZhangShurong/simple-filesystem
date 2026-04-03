/* File + directory operations. */

#include "HUST_fs.h"
#include "constants.h"

int HUST_fs_readpage(struct file *file, struct page *page)
{
	printk(KERN_ERR "HUST: readpage");
	return block_read_full_page(page, HUST_fs_get_block);
}

int HUST_fs_writepage(struct page* page, struct writeback_control* wbc) {
	printk(KERN_ERR "HUST: in write page\n");
       return block_write_full_page(page, HUST_fs_get_block, wbc);
}

int HUST_fs_write_begin(struct file* file, struct address_space* mapping, 
		loff_t pos, unsigned len, unsigned flags, 
		struct page** pagep, void** fsdata) {
    int ret;
	printk(KERN_INFO "HUST: in write_begin\n");
    ret = block_write_begin(mapping, pos, len, flags, pagep, HUST_fs_get_block);
    if (unlikely(ret))
        printk(KERN_INFO "HUST: Write failed for pos [%llu], len [%u]\n", pos, len);
    return ret;
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

	uint64_t total = H_inode.dir_children_count;
	uint64_t idx;

	printk(KERN_INFO "HUST_fs: dir entries total [%llu]\n", total);
	if (total == 0) {
		return 0;
	}

	for (idx = ctx->pos; idx < total; ++idx) {
		uint64_t byte_off = idx * sizeof(struct HUST_dir_record);
		uint64_t blk_idx = byte_off / HUST_BLOCKSIZE;
		uint64_t in_blk_off = byte_off % HUST_BLOCKSIZE;
		struct buffer_head *bh;
		struct HUST_dir_record rec;

		if (blk_idx >= H_inode.blocks)
			break;
		bh = sb_bread(sb, H_inode.block[blk_idx]);
		if (!bh)
			return -EIO;
		memcpy(&rec, bh->b_data + in_blk_off, sizeof(rec));
		brelse(bh);

		if (!dir_emit(ctx, rec.filename, strnlen(rec.filename, HUST_FILENAME_MAX_LEN),
			     rec.inode_no, DT_UNKNOWN))
			break;
		ctx->pos++;
	}
	printk(KERN_INFO "ctx->pos is %llu\n", ctx->pos);
	return 0;
}
