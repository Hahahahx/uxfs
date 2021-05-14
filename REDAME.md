# Linux 文件系统组成结构

linux 文件系统有两个重要的特点：一个是文件系统抽象除了一个通用文件表示层——虚拟文件系统（VFS）。另外一个重要特点就是它的文件系统支持动态安装（挂载），大多数文件系统都可以作为根文件系统的叶子节点被挂载到根文件目录树下的子目录上。

## 虚拟文件系统

虚拟文件系统为用户空间程序提供了文件系统接口。系统中所有文件系统不但依赖 VFS 共存，而且也依靠 VFS 系统协同工作通过虚拟文件系统我们可以利用标准的 UNIX 文件系统调用不同的文件系统进行读写操作。虚拟文件系统的目的是为了屏蔽各个不同的文件系统的相异操作形式，使得异构的文件系统可以在统一的形势下，以标准化的方法访问、操作。实现虚拟文件系统利用的主要思想是引入一个通用文件模型——该文件模型抽象除了文件系统的所有基本操作（该通用模型源于 UNIX 风格的文件系统），比如读、写操作等。同时实际文件系统如果希望利用虚拟文件系统，即被虚拟文件系统支持，也必须将自身的注入“打开文件”、“读写文件”等操作行为以及“什么是文件”，“什么是目录”等概念“修饰”成虚拟文件系统所需要的（定义的）形式，这样才能够被虚拟文件系统支持和使用。</br>
我们可以借用面向对象的思想来理解虚拟文件系统，可以想象成面向对象中的多态。

## 虚拟文件系统的相关对象

### 虚拟文件系统的核心概念

- VFS 通过树状结构来管理文件系统，树状结构的任何一个节点都是“目录节点”
- 树状结构具有一个“根节点”
- VFS 通过“超级块”来了解一个虚拟文件系统所需要的所有信息。具体文件系统必须先向 VFS 注册。注册后，VFS 就可以获得该文件系统的“超级块”。
- 具体文件系统可以被安装（挂载）到某个“目录节点”上，安装后，具体文件系统才可以被使用。
- 用户的文件的操作，解释通过 VFS 的接口，找到对应文件的“目录节点”，然后调用该“目录及欸但”对应的操作接口。

> 关于虚拟文件系统的四个对象
> 超级块对象（super_block）：代表特定已安装的文件系统
> 索引节点对象（inode）：代表特定文件
> 目录项对象（dentry）：代表特定的目录项
> 文件对象（file）：代表被进程打开的文件

#### inode

inode 用来描述文件物理上的属性，比如创建时间、uid、gid 等。其对应的操作方法为 file_operation，文件被打开后，inode 和 file_operation 等都已经在内存中建立，file_operations 的指针也指向了具体的文件系统提供的函数，此后文件的操作，都由这些函数来完成。

#### dentry

本来，inode 中应该包括“目录节点”的名称，但是由于符号链接的存在，导致一个物理文件可能有多个文件名，因此把和“目录节点”名相关的部分从 inode 结构中分开，放在一个专门的 dentry 结构中，这样：<br/>
1、一个 dentry 通过成员 d_inode 对应到一个 inode 上，寻找 inode 的过程变成了寻找 dentry 的过程。因此，dentry 变得更加关键，inode 常常被 dentry 所遮掩。可以说，dentry 是文件系统中最核心的数据结构，他的身影无处不在。<br/>
2、由于符号链接的存在，导致多个 dentry 可能对应到同一个 inode 上。

#### super_block

super_block 保存了文件系统的整体信息，如访问权限：<br/>
我们通过分析“获取一个 inode”的过程来理解超级块的重要性。<br/>
在文件系统的操作中，经常需要获得一个“目录节点”对应的 inode，这个 inode 有可能已经存在于内存中了，也可能还没有，需要创建一个新的 inode，并从磁盘上读取相应的信息来填充。在内核中对应的代码是 iget5_locked，对应代码的过程如下：<br/>
1、通过 iget5_locked()获取 inode。如果 iode 在内存中已经存在，则直接返回；否则创建一个新的 inode<br/>
2、如果创建一个新的 inode，通过 super_block->s_op->read_inode()来填充它。也就是说，如何填充一个新创建的 inode，是由具体文件系统提供的函数实现的。<br/>
iget5_locked()首先在全局的 inode hash table 中寻找，如果找不到，则调用 get_new_inode()，进而调用 alloc_inode()来创建一个新的 inode，在 alloc_inode()中可以看到，如果具体文件系统提供了创建 inode 的方法，则由具体文件系统负责创建，否则采用系统默认的创建方法。

```C++

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

super_block 是在安装文件系统的时候创建的，后面会看到他和其他结构之间的关系
为什么说 super_block 如此重要？
我们来看下打开文件的过程：

- 分配文件描述符
- 获得新文件对象
- 获得目标文件的目录项对象和其索引节点对象，主要通过 open_namei()函数——具体而言就是通过调用索引节点对象（该索引节点或是安装点或是档期那目录）的 lookup 方法找到目录项对应的索引节点号 ino，然后调用 iget(sb,ino)从磁盘读入相应索引节点并在内核中简历起来相应的索引节点（inode）对象（其实还是通过调用 sb->s_op->read_inode()超级块提供的方法），最后还要使用 d_add(dentry,inode)函数将目录项对象与 inode 对象连接起来。
- 初始化目标文件对象的域，特别是把 f_op 域设置成索引节点中 i_fop 指向文件对象的操作表——以后对文件的所有操作将调用该表中的实际方法。

它是一些文件操作的源头。

**当你要实现一个文件系统的时候，你首先要做的就是生产自己的 super_block，即要重载内核的 get_sb()函数，这个函数是在 mount 时调用的。**

# 注册文件系统

一个具体的文件系统必须先向 VFS 注册才能使用。通过 register_filesystem()，可以将一个“文件系统类型”结构 file_system_type 注册到内核中一个全局的链表 file_systems 上。
**文件系统注册的主要目的，就是让 VFS 创建该文件系统的“超级块”结构**
一个文件系统在内核中用 struct file_system_type 来表示：

```C++
struct file_system_type{
    const char *name;
    int fs_flags;
    int (*get_sb)(struct file_system_type *,int,const char *,void *,struct vfsmount *);
    void (*kill_sb)(struct super_block *);
    struct module *owner;
    struct file_system_type *next;
    struct list_head fs_supers;     //超级块对象链表

    struct lock_class_key s_lock_key;
    struct lock_class_key s_umount_key;

    struct lock_class_key i_lock_key;
    struct lock_class_key i_mutex_key;
    struct lock_class_key imutex_dir_key;
    struct locak_class_key i_alloc_sem_key;
}
```

这个结构中最关键的就是 get_sb()这个函数指针，它就是用于创建并设置 super_block 的目的的。
因为安装一个文件系统的关键一步就是为了“被安装设备”创建和设置一个 super_block，而不同的具体的文件系统的 super_block 又自己特定的信息，因此要求具体的文件系统首先向内核注册，并提供 read_super()的实现。

# 安装文件系统

一个注册了的文件系统必须经过安装才能被 VFS 所接受，安装一个虚拟文件系统，必须指定一个目录作为安装点。一个设备可以同时被安装到多个目上。一个目录节点下可以同时安装多个设备。

## “根安装点”、“根设备”、“根文件系统”

安装（挂载）一个文件系统，除了需要“被安装设备”外，还要指定一个“安装点”。“安装点”是已经存在的一个目录节点。例如把/dev/sda1 安装到/mnt/win 下，那么/mnt/win 就是“安装点”。可是文件系统要先安装后使用。因此，/mnt/win 这个“安装点”必然要求它所在的文件系统也已经被安装，也就是说，安装一个文件系统，需要另外一个文件系统已经被安装。

### 最顶层的文件系统是如何被安装的？

答案是，最顶层的文件系统在内核初始化的时候被安装在“根安装点”上的，而根安装点不属于任何文件系统，它对应的 dentry、inode 等结构是由内核在初始化阶段创造出来的。

## 安装连接件 vfsmount

安装一个文件系统设计“被安装设备”和“安装点”两个部分，安装的过程就是把“安装点”和“被安装设备”关联起来，这是通过一个“安装连接件”结构 vfsmount 来完成的。
vfsmount 将“安装点”dentry 和“被安装设备”的根目录节点 dentry 关联起来
所以，在安装文件系统时，内核的主要工作是：

- 创建一个 vfsmount
- 为“被安装设备”创建一个 super_block，并由具体的文件系统来设置这个 super_block
- 为被安装设备的根目录节点创建 dentry
- 为被安装设备的根目录节点创建 inode，并且 super_operations->read_inode()来设置此 inode
- 将 super_block 与“被安装设备”根目录节点 dentry 关联起来
- 将 vfsmount 与“被安装设备”根目录节点 dentry 关联起来

来看下内核中的代码：

```C++

int get_sb_single(struct file_system_type *fs_type,
        int flags,void *data,
        int (*fill_super)(struct super_block *,void *,int ),
        struct vfsmount *mnt)
{
    struct super_block *s;
    int error;
    s = sget(fs_type,compare_single,set_anon_super,NULL);
    if(IS_ERR(s)){
        return PTR_ERR(s);
    }
    if(!s->s_root){
        s->s_flags = flags;
        error = fill_super(s,data,flags & MS_SLIENT?1:0);
        if(error){
            deactivate_locked_super(s);
            return error;
        }
        s->s_flags != MS_ACTIVE;
    }else{
        do_remount_sb(s,flags,data,0);
    }
    simple_set_mnt(mnt,s);
    return 0;
}


```

这个函数中的 fill_super 是个函数指针，是由我们自己实现的，去填充 super_block 并且为被安装设备的根目录分配 inode 和 dentry。最后通过 simple_set_mnt()函数将 super_block 和 dentry 与 vfsmount 连接起来。（这样做的目的就是为了后面查找文件）

```C++

void simple_set_mnt(struct vfsmount *mnt , struct super_block *sb)
{
    mnt->mnt_sb = sb;
    mnt->mnt_root = dget(sb->s_root);
}

```

## 寻找目标节点
VFS中一个最关键以及最频繁的操作，就是根据路径名寻找目标系欸DNA的dentry以及inode。
例如要打开/mnt/win/dir1/abc这个文件，就是根据这个路径，找到“abc”对应的dentry，进而得到inode的过程。
* 首先找到根文件系统的根目录节点dentry和inode。
* 由这个inode提供的操作接口i_op-lookup()，找到下一层节点‘mnt’的dentry和inode。
* 由‘mnt’的inode找到‘win’的dentry和inode。
* 由于‘win’是个“安装点”，因此需要找到“被安装设备”/dev/sda1根目录节点的dentry和inode，只要找到vfsmount B，就可以完成这个任务。
* 然后由/dev/sda1根目录节点的inode负责找到下一层节点‘dir1’的dentry和inode。
* 最后由这个inode负责找到‘abc’的dentry和inode。

# 文件的读写
一个文件每被打开一次，就对应着一个file结构，我们指定，每个文件对应着一个dentry和inode，每打开一个文件，只要找到对应的dentry和inode不久可以了么？为什么还要引入这个file结构？
这是因为一个文件可以被同时打开多次，每次打开的方式也可以不一样。而dentry和inode只能描述一个物理的文件，无法描述“打开”这个概念。
因此有必要引入file结构，来描述一个“被打开的文件”，每打开一个文件，就创建一个file结构。
**实际上，打开文件的过程正式建立file，dentry，inode之间关联的过程**

文件一旦被打开，数据结构之间的关系已经建立，后面对文件的读写以及其他操作都变得很简单。就是根据fd找到file结构，然后找到dentry和inode，最后通过inode->i_fop中对应的函数进行具体的读写等操作即可。






——————————————————————————————————————————
代码部分见uxfs.c

insmod模块后，执行mount -t uxfs 目录 挂载点目录



