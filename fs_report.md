# Lab: file system
## 1. Large files (moderate)
### 1) 实验目的
- 在xv6系统中，当前文件的最大大小限制为 `268` blocks，不足以通过Large Files的实验。需要扩大一个文件的最大大小限制为 `11 + 256 + 256*256` blocks。

### 2) 实验步骤
1. 在kernel/fs.h中修改宏定义。将MAXFILE修改成`11 + 256 + 256*256` blocks
    ```c
    #define NDIRECT 11
    #define NINDIRECT (BSIZE / sizeof(uint))
    #define NININDIRECT (NINDIRECT * NINDIRECT)
    #define MAXFILE (NDIRECT + NINDIRECT + NININDIRECT)

    // On-disk inode structure
    struct dinode {
      short type;           // File type
      short major;          // Major device number (T_DEVICE only)
      short minor;          // Minor device number (T_DEVICE only)
      short nlink;          // Number of links to inode in file system
      uint size;            // Size of file (bytes)
      uint addrs[NDIRECT+2];   // >>> 这里由+1变成+2 <<<
    };

    ```

2. 在kernel/file.h中修改inode的结构
    ```c
    // in-memory copy of an inode
    struct inode {
      uint dev;           // Device number
      uint inum;          // Inode number
      int ref;            // Reference count
      struct sleeplock lock; // protects everything below here
      int valid;          // inode has been read from disk?

      short type;         // copy of disk inode
      short major;
      short minor;
      short nlink;
      uint size;
      uint addrs[NDIRECT+2];   // >>> 这里由+1变成+2 <<<
    };
    ```

3. 在kernel/fs.h中的`bmap`寻找block的函数里加上双层间接映射的逻辑, 同时清除掉itrunc里对应的双层连接
  ```c
  bmap(struct inode *ip, uint bn)
  {
    uint addr, *a, *b;
    struct buf *inbp, *ininbp;

    if(bn < NDIRECT){
      if((addr = ip->addrs[bn]) == 0)
        ip->addrs[bn] = addr = balloc(ip->dev);
      return addr;
    }
    bn -= NDIRECT;

    if(bn < NINDIRECT){
      // Load indirect block, allocating if necessary.
      if((addr = ip->addrs[NDIRECT]) == 0)
        ip->addrs[NDIRECT] = addr = balloc(ip->dev);
      inbp = bread(ip->dev, addr);
      a = (uint*)inbp->data;
      if((addr = a[bn]) == 0){
        a[bn] = addr = balloc(ip->dev);
        log_write(inbp);
      }
      brelse(inbp);
      return addr;
    }
    bn -= NINDIRECT;

    // after subtraction, [0, 65535] is doubly-indirect block
    if (bn < NININDIRECT){
      // Load 1st indirect block, allocating if necessary
      // index 10是最后一个直接映射, index 11是单层间接映射, index 12是双层间接映射
      if ((addr = ip->addrs[NDIRECT+1]) == 0)
        ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
      inbp = bread(ip->dev, addr);
      a = (uint*)inbp->data;
      if ((addr = a[bn/NINDIRECT]) == 0) { // 之后的每一个映射可以吃下NINDIRECT个blocks
        a[bn/NINDIRECT] = addr = balloc(ip->dev);
        log_write(inbp);
      }
      brelse(inbp);

      // Load the 2nd indirect block, allocating if necessary
      ininbp = bread(ip->dev, addr);
      b = (uint*)ininbp->data;
      if ((addr = b[bn % NINDIRECT]) == 0) { // 取余数
        b[bn % NINDIRECT] = addr = balloc(ip->dev);
        log_write(ininbp);
      }
      brelse(ininbp);
      return addr;
    }
    panic("bmap: out of range");
  }
  ```

4. 在xv6中运行`bigfile`和`usertests`
  ```bash
  $ bigfile
  ..................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................
  wrote 65803 blocks
  bigfile done; ok
  $ usertests
  usertests starting
  ...
  ALL TESTS PASSED
  ```

### 3) 实验中遇到的问题和解决办法
- 在实现`bmap`的双层间接映射时，需要注意inode在addr等于12的指向块，在延伸至双层映射的时候要注意第一层为`addr = a[bn/NINDIRECT]`，第二层为`addr = b[bn % NINDIRECT]) == 0`。

### 4) 实验心得
- 掌握了块多层映射的逻辑
- 熟悉了多层映射的实现方法
- 深刻理解了逻辑块与物理地址的映射逻辑

## 2. Symbolic links (moderate)
### 1) 实验目的
- 向 xv6 添加符号链接。符号链接（或软链接）通过路径名引用链接文件；当符号链接打开时，内核会跟随链接到被引用的文件。符号链接类似于硬链接，但硬链接是仅限于指向同一磁盘上的文件，而符号链接可以跨磁盘设备。

### 2) 实验步骤
1. 在kernel/syscall.h中添加`sys_symlink`的调用号：
  ```c
  #define SYS_symlink 22
  ```

2. 在kernel/syscall.c中添加`sys_symlink`的声明以及其与调用号的匹配：
  ```c
  extern int sys_symlink(void);

  static uint64 (*syscalls[])(void) = {
  [SYS_symlink] sys_symlink,
  };
  ```

3. 在user/user.pl中添加`symlink`的entry：
  ```c
  entry("symlink");
  ```

4. 在user/user.h中添加`symlink`的声明:
  ```c
  int symlink(char*, char*);
  ```

5. 在kernel/file.h中的file结构中添加一个`T_SYMLINK`的状态:
  ```c
  struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE, T_SYMLINK } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
  };
  ```

65. 在kernel/sysflie.c中实现`sys_symlink`:
  ```c
  int sys_symlink(char *target, char *path) {
    char kpath[MAXPATH], ktarget[MAXPATH];
    memset(kpath, 0, MAXPATH);
    memset(ktarget, 0, MAXPATH);
    struct inode *ip;
    int n, r;

    if((n = argstr(0, ktarget, MAXPATH)) < 0)
      return -1;

    if ((n = argstr(1, kpath, MAXPATH)) < 0)
      return -1;

    int ret = 0;
    begin_op();

    // 这个软链接已经存在了
    if((ip = namei(kpath)) != 0){
      // symlink already exists
      ret = -1;
      goto final;
    }

    // 为这个软链接allocate一个新的inode
    ip = create(kpath, T_SYMLINK, 0, 0);
    if(ip == 0){
      ret = -1;
      goto final;
    }
    // 把target path写入这个软链接inode的数据[0, MAXPATH]位置内
    if ((r = writei(ip, 0, (uint64)ktarget, 0, MAXPATH)) < 0)
      ret = -1;
    iunlockput(ip);

  final:
    end_op();
    return ret;
  }
  ```

7. 在kernel/fsntl.h中添加`#define O_NOFOLLOW 0x800`

8. 在kernel/sysfile.c中`sys_open`这个函数里，如果传入的path是一个软链接的话，我们需要为用户递归去"寻址"，直到找到第一个不是软链接的path。
  ```c
  sys_open(void)
  {

    // ... 省略 ...
    int r;

    // ... 省略 ...
 
    // 是软链接且O_NOFOLLOW没被设立起来
    int depth = 0;
    while (ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
      char ktarget[MAXPATH];
      memset(ktarget, 0, MAXPATH);
      // 从软链接的inode的[0, MAXPATH]读出它所对应的target path
      if ((r = readi(ip, 0, (uint64)ktarget, 0, MAXPATH)) < 0) {
        iunlockput(ip);
        end_op();
        return -1;
      }
      iunlockput(ip);
      if((ip = namei(ktarget)) == 0){ // target path 不存在
        end_op();
        return -1;
      }

      ilock(ip);
      depth++;
      if (depth > 10) {
        // maybe form a cycle 默认死循环
        iunlockput(ip);
        end_op();
        return -1;
      }
    }

    // ... 省略 ...
  }
  ```

9. 在xv6中运行`symlinktest`
  ```bash
  $ symlinktest
  Start: test symlinks
  test symlinks: ok
  Start: test concurrent symlinks
  test concurrent symlinks: ok
  $ usertests
  usertests starting
  ...
  ALL TESTS PASSED
  ```

### 3) 实验中遇到的问题和解决办法
- 在实现`sys_open`函数里的递归寻址时，需要注意递归寻址的条件是`ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)`，即path是软链接且O_NOFOLLOW没被设立起来的情况下进行递归寻址，直到状态变为`O_NOFOLLOW`。否则会出现死循环。

### 4) 实验心得
- 熟悉符号链接(软链接)的逻辑实现
- 熟悉了文件系统中路径名引用链接文件的逻辑方式
