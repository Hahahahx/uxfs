#include <linux/init.h>

// 每个空间固定存储的数据量
#define BLOCK_SIZE 1024

/**
 * 
 *  这里只限定了文件的大小固定就为1024
 *  issue：那么如果我们的文件可能有1G、2G、nG呢，这样该如何定义这个数据结构才好？
 */
unsigned char block[BLOCK_SIZE]

    /**
 * 构建出的super_block
 */
    struct file_system_type minfs_type = {
        /* data */
        .owner = THIS_MODULE,
        // 其中*name就代表着在mount -t时想用什么名字就写什么名字
        .name = "minfs",
        // 调用mount时触发
        .mount = minfs_mount,
        // 调用umount时触发
        .kill_sb = minfs_umount,
};

struct file_operations minfs_inode_fops = {
    .write = minfs_write,
    .read = minfs_read,
};

/**
 * inode写入
 */
ssize_t minfs_write(struct file *, const char __user *buffer, size_t, loff_t *)
{
    // 并非真正意义上的写入，而是一种copy，从一段内存（用户空间）中copy到block中
    copy_from_user(buffer, block, length);
}

/**
 * inode读取
 */
ssize_t minfs_read(struct file *, char __user *buffer, size_t, loff_t *)
{
    copy_to_user(block, buffer, length);
}

static struct inode_operations minfs_inode_ops = {
    // 让文件系统拥有创建的能力
    .create = minfs_create,
    .mkdir = minfs_mkdir,
    .rmdir = minfs_rmdir,
};

/**
 * 创建文件
 * touch a.txt
 * @param *dir 在根目录下面创建一个文件  也就是dir拿到的就是根目录这个inode，找到其对应的super_block，我们只需要在这个sb上添加新的inode（a.txt）
 */
static int minfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool boolean)
{
    // 对数据内容操作，就是对inode的处理
    struct super_block *sb = dir->i_sb;
    struct inode *inode = new_inode(sb);

    // 需要申请一块空间来存储数据，保证其有一块空间可以使用
    // 存储数据到i_private中
    inode->i_private = block;
    // 所创建出来的文件是否具备了写入或者读取等文件操作，
    // 需要有对应的file_operations，也就是其对应的操作函数
    inode->i_fop = &minfs_inode_fops;
    inode->i_sb = sb;
    struct inode *ct = new_inode()
}

/**
 * 创建文件夹
 */
static int minfs_mkdir(struct inode *inode, struct dentry *dentry, umode_t)
{
}

/**
 * 删除文件夹
 */
int minfs_rmdir(struct inode *inode, struct dentry *dentry)
{
}

/**
 * 填充super_block
 */
int minfs_fill_super(struct super_block *sb, void *data, int silent)
{

    // 创建一个新的节点，根节点
    // 就是假设/mnt下存在a.txt，但是在我们挂载以后，就会变成一个新的空目录，也就是/mnt的inode指向变成了这个新的节点
    struct inode *root_inode = new_inode(sb);
    root_inode->i_sb = sb;
    // 创建的inode需要有对应的操作
    root_inode->i_op = minfs_inode_ops;
}

/**
 * 
 * mount时
 */
struct dentry *minfs_mount(struct file_system_type *fs_type, int flags, const char *devname, void *data)
{

    // mount
    // nodev不带磁盘的虚拟文件系统，不带存储的系统，在内存中的
    // 在mount的时候填充super_block
    return mount_nodev(fs_type, flags, data, minfs_fill_super);
}

/**
 * 
 * umount时
 */
void minfs_umount(struct super_block *sb)
{
}

/**
 * 固定的模式，往linux模块中插入
 * 在insmod时执行
 */
static int minfs_init(void)
{
    //1、注册一个文件系统,
    int ret = register_filesystem(&minfs_type);
    if (ret)
    {
        printk("register minfs failed\n");
    }

    return ret;
}

module_init(minfs_init);