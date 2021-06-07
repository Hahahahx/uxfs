#include <linux/module.h>
#include <linux/fs.h>

static struct file_system_type test_fs_type ={
    .owner = THIS_MODULE,
    .name = "testfs"
};


static int __init testfs_init(void){
    register_filesystem(&test_fs_type);
    return 0;
}


static void __exit testfs_exit(void){
    unregister_filesystem(&test_fs_type);
}

module_init(testfs_init);
module_exit(testfs_exit);
MODULE_LICENSE("GPL");