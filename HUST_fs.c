/*
 * Based on psankar's simplefs:
 * https://github.com/psankar/simplefs */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

static int
HUST_fs_iterate(struct file *filp, struct dir_context *ctx)
{
    /* Emit the standard entries "." and ".." and quit. */
    dir_emit_dots(filp, ctx);
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
    /* We don't do anything here, but it's important that this operation
     * is defined in inode->i_op. See vfs.txt in the kernel docs on what
     * this function usually does. */
    printk(KERN_INFO "lookup called unexpectedly with %p, %p, and %u\n",
           parent_inode, child_dentry, flags);
    return NULL;
}

static const struct inode_operations HUST_fs_inode_ops = {
    .lookup = HUST_fs_lookup,
};

/* Called as a callback function by mount_bdev(). We don't read anything
 * from disk but construct a "virtual" root inode on the fly. We also
 * make that root inode available in our superblock struct. */
static int
HUST_fs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;

    root_inode = new_inode(sb);
    if (!root_inode)
        return -ENOMEM;

    /* Our root inode. It doesn't contain useful information for now.
     * Note that i_ino must not be 0, since valid inode numbers start at
     * 1. */
    inode_init_owner(root_inode, NULL, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
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
