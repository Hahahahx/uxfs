#include <linux/module.h>
#include <linux/fs.h>

/**
 * 
 * 执行make，insmod testfs.ko 然后cat /proc/filesystems|grep testfs就能看到testfs名列其中，说明我们的文件系统已经注册到了内核当中，
 * 接下来我们需要挂载文件系统，但是挂载的过程中会导致panic，因为我们还没有定义文件系统super_block的获取和释放函数。
 */
static struct file_system_type test_fs_type = {
    .owner = THIS_MODULE,
    .name = "testfs"};

static int __init testfs_init(void)
{
    register_filesystem(&test_fs_type);
    return 0;
}

static void __exit testfs_exit(void)
{
    unregister_filesystem(&test_fs_type);
}

module_init(testfs_init);
module_exit(testfs_exit);
MODULE_LICENSE("GPL");

/**
 * 
 * 光有上面的还不够，因为会在挂载文件系统的过程中出现panic，应为我们还没有定义文件系统super_block的获取和释放函数
 * 挂载文件系统的时候以来这两个函数，不然就会导致空指针。接下来我们定义文件系统的两个接口。“kill_sb”使用的是内核函数kill_litter_super，
 * 它会对super_block的内容进行释放。“get_sb”这个接口调用了“testfs_get_sb”函数。
 * 这个函数也是调用了内核函数get_sb_nodev，这个的函数会创建对应的super_block，
 * 这个函数对的是不依赖/dev的文件系统，如果依赖/dev的话，需要调用别的函数，另外会根据/dev对应的设备获取super_block
 * 比如说ext4会读对应的被格式化后的块设备的头来实例化超级块
 * 我们需要传入一个函数指针用于填充空白的super_block，就是“testfs_fill_super”，然后“testfs_fill_super”也调用了内核函数
 * 
 */
static int testfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *inode = NULL;
    struct dentry *root;
    int err;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = TESTFS_MAGIC;

    // 用于在形参给定的super_block上申请一个新的inode
    inode = new_inode(sb);
    if (!inode)
    {
        err = -ENOMEM;
        goto fail;
    }
    inode->i_mode = 0755;
    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_mode |= S_IFDIR;
    inode->i_fop = &simple_dir_operations;
    inode->i_op = &simple_dir_inode_operations;
    // inc reference count for "."
    inc_nlink(inode);

    root = d_alloc_root(inde);
    sb->s_root = root;
    if (!root)
    {
        err = -ENOMEM;
        goto fail;
    }
    return 0;
fail:
    return err;
}

/**
 * 
 * 为了填充super_block，需要初始化sb以及创建根目录的inode和dentry。s_blocksize指定了文件系统的块大小，一般是一个PAGE_SIZE的大小
 * 这里的PAGE_CACHE_SIZE和PAGE_SIZE是一样的，PAGE_CACHE_SIZE_SHIFT是对应的位数， 
 * 所以s_blicksize_bits是块大小的bit位位数，接着是inode初始化，new_inode为sb创建一个关联的inode结构体，
 * 并对inode初始化，包括uid、git、i_mode。对应的i_fop和i_op使用了内核默认的接口simple_dir(_inode)_operations。后面会仔细讨论，
 * 这里先加上方便展示代码，如果对应的接口未定义的话，初始化的时候文件系统根目录会出现不会被认作目录的情况。
 * 
 * 接下来安装模块，然后挂载文件系统，mount -t testfs none tmp，因为我们的文件系统没有对应的设备类型所以参数会填none，对应的目录是tmp，
 * 这样tmp就成为了testfs的根目录，如果吧ls一把tmp，里面是什么都没有的，我们cd tmp && touch x返回的结果是不被允许，因为我们还没有定义对应的接口，不能创建文件
 * 
 * 我们继续让这个文件系统可以创建目录，那我们需要定义目录inode的接口，一组inode_operations和一组file_operations
 * 
 */
/*

// 做完读写mapping以后会修改该方法
static struct inode *testfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);
    if (inode)
    {
        inode->i_mode = mode;
        inode->i_uid = current_fsuid();
        inode->i_gid = current_fsgid();
        inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
        switch (mode & S_IFMT)
        {
        case S_IFDIR:
            //TODO:
            inode->i_op = &testfs_dir_inode_operations;
            inode->i_fop = &simple_dir_operations;
            // i_nlink = 2
            inc_nlink(inode);
        }
    }
    return inode;
}


*/
// 修改，在下面
static struct inode *testfs_get_inode(struct super_block *sb, int mode, dev_t dev);

static int testfs_mknod(sturct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
    struct inode *inode = testfs_get_inode(dir->i_sb, mode, dev);
    int error = -ENOSPC;
    if (inode)
    {
        //inherits gid
        if (dir->i_mode & S_ISGID)
        {
            inode->i_gid = dir->i_gid;
            if (S_ISDIR(mode))
            {
                inode->imode |= S_ISGID;
            }
        }
        d_instantiate(dentry, inode);
        // get dentry reference count
        dget(dentry);
        error = 0;
        dir->i_mtime = dir->i_ctime = CURRENT_TIME:
    }
    return error;
}

static int testfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
    int retval;
    retval = testfs_mknod(dir, dentry, mode | S_IFDIR, 0);
    printk("testfs:mkdir");
    if (!retval)
    {
        inc_nlink(dir);
    }
    return retval;
}

static int testfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
    return testfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

int simple_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);

static const struct inode_operations testfs_dir_inode_operations = {
    .create = testfs_create,
    .lookup = simple_lookup, // get dentry
    .link = simple_link,     // same inode, different dentry
    .unlink = simple_unlink,
    .symlink = testfs_symlink, // 目前没有做mapping会panic
    .mkdir = testfs_mkdir,
    .rmdir = simple_rmdir,
    .mknod = testfs_mknod,
    .rename = simple_rename, // exchange dentry and dir
}

/**
 * 
 * 其实很简单，testfs_get_inode只创建目录类型的inode，并且赋值对应的函数指针，file_operations使用的默认接口，这个后面再提
 * inode_operations主要是inode的创建，testfs_create和testfs_mkdir都是对testfs_mknod针对不同mode的封装，testfs_symlink暂时不讲
 * 因为inode还没有做mapping，软链接的时候不可写会导致panic，
 * 进行上面类似的编译和挂载以后我们就能创建简单文件和目录了，但是创建的文件不能做任何操作，因为我们没有定义对应的接口
 * 
 * 挑个接口说一下，比如link接口就是创建了一个dentry指向了同一个inode，并且增加inode引用计数，unlink就是把dentry删掉，inode保留
 */

/*
int
simple_link(struct dentry * old_dentry, struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = old_dentry->d_inode;

    inode->i_ctime = dir->i_ctiem = dir->i_mtime = CURRENT_TIEM;
    inc_nlink(inode);
    atomic_inc(&inode->i_count);
    dget(dentry);
    d_instantiate(dnetry, inode);
    return 0;
}

*/

/**
 * 
 * 软链有点复杂，所以放到后面讲
 * 当我们能完成目录和文件的创建和删除之后，我们可以继续文件的读写了，换句话说我们要定义普通文件的inode的file_operations接口
 * 为了能够添加文件我们增加如下代码
 */
static const struct address_space_operations testfs_aops = {
    .readpage = simple_readpage, .write_begin = simple_write_begin, .write_end = simple_write_end,
    // .set_page_dirty = __set_page_dirty_no_writeback, 内核私有函数
}

static const struct file_operations testfs_file_operations = {
    .read = do_sync_read, // file read get mapping page and copy to userspace;
    .aio_read = generic_file_aio_read,
    .write = do_sync_write,
    .aio_write = generic_file_aio_write,
    .mmap = generic_file_mmap,
    .fsync = simple_sync_file,
    .splice_read = generic_file_splice_read,
    .splice_write = generic_file_splice_write,
    .llseek = generic_file_llseek,
}

static const struct inode_operations testfs_file_inode_operations = {
    .getattr = simple_getattr,
}

/**
 * 修改testfs_get_inode
 */
static struct inode* testfs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);
    if (inode)
    {
        inode->i_mode = mode;
        inode->i_uid = current_fsuid();
        inode->i_git = current_fsgit();
        inode->i_mapping->a_ops = &testfs_aops;
        mapping_set_gfp_mask(inode->i_mapping,GFP_HIGHUSER);
        mapping_set_unevictable(inode->i_mapping);
        inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
        switch(mode & S_IFMT){
            default:
                init_special_inode(inode,mode,dev);
                break;
            case S_IFDIR:
                inode->i_op = &testfs_dir_inode_operations;
                inode->i_fop = &simple_dir_operations;
                // i_nlink = 2 for "."
                inc_nlink(inode)；
                break;
            case S_IFREG:
                inode->i_op = &testfs_dir_inode_operations;
                inode->i_fop = &testfs_file_operations;
                break;
            case S_IFLNK:
                inode->i_op = &page_symlink_inode_operations;
                break;
        }
    }
    return inode;
}


/**
 * 
 * 这样以后，我们就可以对文件进行读写了，实际上文件的读写首先要依赖于mmap操作
 * 把对应的页映射到虚拟内存当中来进行读写，编译并添加模块再挂载以后我们发现touch文件可以读写了
 * 现在具体举一段代码路径分析一下，从read开始
 * 
*/


