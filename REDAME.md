# Linux文件系统组成结构

linux文件系统有两个重要的特点：一个是文件系统抽象除了一个通用文件表示层——虚拟文件系统（VFS）。另外一个重要特点就是它的文件系统支持动态安装（挂载），大多数文件系统都可以作为根文件系统的叶子节点被挂载到根文件目录树下的子目录上。

## 虚拟文件系统

虚拟文件系统为用户空间程序提供了文件系统接口。系统中所有文件系统不但依赖VFS共存，而且也依靠VFS系统协同工作通过虚拟文件系统我们可以利用标准的UNIX文件系统调用不同的文件系统进行读写操作。虚拟文件系统的目的是为了屏蔽各个不同的文件系统的相异操作形式，使得异构的文件系统可以在统一的形势下，以标准化的方法访问、操作。实现虚拟文件系统利用的主要思想是引入一个通用文件模型——该文件模型抽象除了文件系统的所有基本操作（该通用模型源于UNIX风格的文件系统），比如读、写操作等。同时实际文件系统如果希望利用虚拟文件系统，即被虚拟文件系统支持，也必须将自身的注入“打开文件”、“读写文件”等操作行为以及“什么是文件”，“什么是目录”等概念“修饰”成虚拟文件系统所需要的（定义的）形式，这样才能够被虚拟文件系统支持和使用。</br>
我们可以借用面向对象的思想来理解虚拟文件系统，可以想象成面向对象中的多态。

## 虚拟文件系统的相关对象
### 虚拟文件系统的核心概念
* VFS通过树状结构来管理文件系统，树状结构的任何一个节点都是“目录节点”
* 树状结构具有一个“根节点”
* VFS通过“超级块”来了解一个虚拟文件系统所需要的所有信息。具体文件系统必须先向VFS注册。注册后，VFS就可以获得该文件系统的“超级块”。
* 具体文件系统可以被安装（挂载）到某个“目录节点”上，安装后，具体文件系统才可以被使用。
* 用户的文件的操作，解释通过VFS的接口，找到对应文件的“目录节点”，然后调用该“目录及欸但”对应的操作接口。

> 关于虚拟文件系统的四个对象
> 超级块对象（super_block）：代表特定已安装的文件系统
> 索引节点对象（inode）：代表特定文件
> 目录项对象（dentry）：代表特定的目录项
> 文件对象（file）：代表被进程打开的文件


#### inode 
inode用来描述文件物理上的属性，比如创建时间、uid、gid等。其对应的操作方法为file_operation，文件被打开后，inode和file_operation等都已经在内存中建立，file_operations的指针也指向了具体的文件系统提供的函数，此后文件的操作，都由这些函数来完成。

#### dentry
本来，inode中应该包括“目录节点”的名称，但是由于符号链接的存在，导致一个物理文件可能有多个文件名，因此把和“目录节点”名相关的部分从inode结构中分开，放在一个专门的dentry结构中，这样：<br/>
1、一个dentry通过成员d_inode对应到一个inode上，寻找inode的过程变成了寻找dentry的过程。因此，dentry变得更加关键，inode常常被dentry所遮掩。可以说，dentry是文件系统中最核心的数据结构，他的身影无处不在。<br/>
2、由于符号链接的存在，导致多个dentry可能对应到同一个inode上。

#### super_block
super_block保存了文件系统的整体信息，如访问权限：<br/>
我们通过分析“获取一个inode”的过程来理解超级块的重要性。<br/>
在文件系统的操作中，经常需要获得一个“目录节点”对应的inode，这个inode有可能已经存在于内存中了，也可能还没有，需要创建一个新的inode，并从磁盘上读取相应的信息来填充。在内核中对应的代码是iget5_locked，对应代码的过程如下：<br/>
1、通过iget5_locked()获取inode。如果iode在内存中已经存在，则直接返回；否则创建一个新的inode<br/>
2、如果创建一个新的inode，通过super_block->s_op->read_inode()来填充它。也就是说，如何填充一个新创建的inode，是由具体文件系统提供的函数实现的。<br/>
iget5_locked()首先在全局的inode hash table中寻找，如果找不到，则调用get_new_inode()，进而调用alloc_inode()来创建一个新的inode，在alloc_inode()中可以看到，如果具体文件系统提供了创建inode的方法，则由具体文件系统负责创建，否则采用系统默认的创建方法。
```C

// 初始化inode
static struct inode *alloc_inode(struct super_block *sb){
    struct inode *inode;
    if(sb->s_cp->alloc_inode){
        inode = sb->s_cp->alloc_inode(sb);
    }else{
        // GFP_KERNEL，代表分配的是内核内存空间，内核空间与用户空间最基本的区别就是内核空间，是被所有进程共享的
        inode = kmem_cache_alloc(inode_cachep,GFP_KERNEL);
    }

    if(!inode){
        return NULL;
    }
    // likely代表执行if的可能性比较大，在gcc编译的时候会前置if语句块的内容，以避免语句跳转后所带来的性能丢失
    // unlikely则与之相反，代表else执行的可能性大，汇编代码时会前置else的代码块
    // 所以说执行这里面的代码的可能性并不高，即inode_init_always()这个函数的结果很有可能都是否定的，即sb会在上面的sb->s_cp->alloc_inode的if模块中完成初始化
    if(unlikely(inode_init_always(sb,inode))){

        // 销毁与释放inode，有一个问题，不知道什么情况下才会在一个需要初始化inode的功能模块里销毁inode
        if(inode->i_sb->s_cp->destroy_inode){
            inode->i_sb->s_op->destory_inode(inode);
        }else{
            kmem_cache_free(inode_cachep,inode);
        }
    }
    return inode;
}

```