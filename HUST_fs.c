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

static ssize_t
HUST_fs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct buffer_head *bh;
    sector_t at_block;
    loff_t into_block;
    unsigned long c2u_ret, c2u_len;

    if(*ppos >= filp->f_inode->i_size)
        return 0;
    at_block = (*ppos >> 12) +1;
    into_block = *ppos - ((*ppos >> 12) << 12);
    c2u_len = min((loff_t)len, 4096 - into_block);
    c2u_len = min((loff_t)c2u_len, filp->f_inode->i_size - *ppos);

    printk(KERN_INFO "read block %lu, offset in block %llu, returning %lu bytes\n",
           at_block, into_block, c2u_len);
    
    bh = sb_bread(filp->f_inode->i_sb, at_block);
    if (!bh){
        printk(KERN_ERR "Error reading block %lu\n", at_block);
        return -EFAULT;
    }
    c2u_ret = copy_to_user(buf, bh->b_data + into_block, c2u_len);
    brelse(bh);
    if (c2u_ret)
    {
        printk(KERN_ERR "Error in copy_to_user()\n");
        return -EFAULT;
    }

    *ppos += c2u_len;
    return c2u_len;
}

static const struct file_operations HUST_fs_file_ops = {
    .owner = THIS_MODULE,
    .llseek = generic_file_llseek,
    .mmap = generic_file_mmap,
    .read = HUST_fs_read,
};

static int
HUST_fs_iterate(struct file *filp, struct dir_context *ctx)
{
    /* Emit the standard entries "." and ".." and quit. */
    dir_emit_dots(filp, ctx);
    if (ctx->pos == 2)
    {
        dir_emit(ctx, "file", 4, 2, DT_REG);
        ctx->pos++;
    }
    return 0;
}

static const struct file_operations HUST_fs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = HUST_fs_iterate,
};

static struct dentry *
HUST_fs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
                   unsigned int flags)
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
    if(!inode) {
        printk(KERN_ERR "lookup: iget_locked() return NULL\n");
        return ERR_PTR(-ENOMEM);
    }

    if(inode->i_state &I_NEW) {
        bh = sb_bread(parent_inode->i_sb, 0);
        if (!bh) {
            printk(KERN_ERR "lookup: sb_bread for block 0 failed\n");
            return ERR_PTR(-EFAULT);
        }
        filesize = ((uint64_t *)bh->b_data)[0];
        brelse(bh);

        inode_init_owner(inode, parent_inode, S_IFREG |
                         S_IRUSR | S_IXUSR |
                         S_IRGRP | S_IXGRP |
                         S_IROTH | S_IXOTH);
        inode->i_ino = 2;
        inode->i_atime = inode->i_mtime = inode->i_ctime =current_time(inode);
        inode->i_size = (loff_t)filesize;
        inode->i_fop = &HUST_fs_file_ops;

        unlock_new_inode(inode);
    }

    d_add(child_dentry, inode);
    return NULL;
}

static const struct inode_operations HUST_fs_inode_ops = {
    .lookup = HUST_fs_lookup,
};


static int
HUST_fs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;

    if (sb->s_blocksize != 4096)
    {
        printk(KERN_ERR "onefilerofs expects a blocksize of %d\n", 4096);
        return -EFAULT;
    }
    printk(KERN_INFO "onefilerofs: blocksize is %lu, okay\n", sb->s_blocksize);

    root_inode = new_inode(sb);
    if (!root_inode)
        return -ENOMEM;

    /* Our root inode. It doesn't contain useful information for now.
     * Note that i_ino must not be 0, since valid inode numbers start at
     * 1. */
    inode_init_owner(root_inode, NULL, S_IFDIR | 
                     S_IRUSR | S_IXUSR |
                     S_IRGRP | S_IXGRP |
                     S_IROTH | S_IXOTH);

    root_inode->i_ino = 1;
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
                          current_time(root_inode);

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

    return 0;
}

static struct dentry *
HUST_fs_mount(struct file_system_type *fs_type, int flags,
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

static void
HUST_fs_kill_superblock(struct super_block *s)
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
static struct file_system_type HUST_fs_type = {
    .owner = THIS_MODULE,
    .name = "HUST_fs",
    .mount = HUST_fs_mount,
    .kill_sb = HUST_fs_kill_superblock,   /* unmount */
};

/* Called when the module is loaded. */
static int
HUST_fs_init(void)
{
    int ret;

    ret = register_filesystem(&HUST_fs_type);
    if (ret == 0)
        printk(KERN_INFO "Sucessfully registered HUST_fs\n");
    else
        printk(KERN_ERR "Failed to register HUST_fs. Error: [%d]\n", ret);

    return ret;
}

/* Called when the module is unloaded. */
static void
HUST_fs_exit(void)
{
    int ret;

    ret = unregister_filesystem(&HUST_fs_type);

    if (ret == 0)
        printk(KERN_INFO "Sucessfully unregistered HUST_fs\n");
    else
        printk(KERN_ERR "Failed to unregister HUST_fs. Error: [%d]\n", ret);
}

module_init(HUST_fs_init);
module_exit(HUST_fs_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("cv");
