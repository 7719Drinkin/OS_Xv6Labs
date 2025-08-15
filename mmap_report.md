# Lab: mmap (hard)
### 1) 实验目的
- `mmap` 和 `munmap` 系统调用允许 UNIX 程序对其地址空间进行详细控制。它们可用于在进程之间共享内存，将文件映射到进程地址空间，并作为用户级页面错误方案的一部分。
- 把 `mmap` 和 `munmap` 添加到 xv6 中，重点关注内存映射文件。

### 2) 实验步骤
1. 按照实验要求，在kernel/proc.h中为每个`struct proc`增加一个长度为16的vma表
    ```c
    // Number of virtual memory area per process
    #define NVMA 16

    // Virtual Memory Area
    struct vma {
      int valid;
      uint64 addr;
      int length;
      int prot;
      int flags;
      int fd;
      int offset;
      struct file* f;
    };

    // Per-process state
    struct proc {
      // ...省略...
      char name[16];               // Process name (debugging)
      struct vma vmas[NVMA];       // Virtual memory area array
    };
    ```

2. 把两个新的系统调用`mmap`和`munmap`的一系列函数签名, 调用代码, 跳转指针等常规操作都做好
    ```c
    // kernel/syscall.h
    #define SYS_mmap   22
    #define SYS_munmap 23
    ```

    ```c
    // kernel/syscall.c
    extern uint64 sys_mmap(void);
    extern uint64 sys_munmap(void);

    static uint64 (*syscalls[])(void) = {
    [SYS_mmap]    sys_mmap,
    [SYS_munmap]  sys_munmap,
    };
    ```

    ```pl
    # user/user.pl
    entry("mmap");
    entry("munmap");
    ```

    ```c
    // user/user.h
    void* mmap(void *addr, uint length, int prot, int flags, int fd, int offset);
    int munmap(void *addr, uint length);
    ```

3. 在kernel/sysfile.c中实现`mmap`
    ```c
    uint64
    sys_mmap(void) {
      uint64 failure = (uint64)((char *) -1);
      struct proc* p = myproc();
      uint64 addr;
      int length, prot, flags, fd, offset;
      struct file* f;

      // parse argument 
      if (argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0
          || argint(3, &flags) < 0 || argfd(4, &fd, &f) < 0 || argint(5, &offset) < 0)
        return failure;

      // sanity check 安全检查
      length = PGROUNDUP(length);
      if (MAXVA - length < p->sz)
        return failure;
      if (!f->readable && (prot & PROT_READ))
        return failure;
      if (!f->writable && (prot & PROT_WRITE) && (flags == MAP_SHARED))
        return failure;

      // find an empty vma slot and fill in
      for (int i = 0; i < NVMA; i++) {
        struct vma* vma = &p->vmas[i];
        if (vma->valid == 0) {
          vma->valid = 1;
          vma->addr = p->sz;
          p->sz += length; // 虚拟的增加进程大小, 但没有实际分配物理页
          vma->length = length;
          vma->prot = prot;
          vma->flags = flags;
          vma->fd = fd;
          vma->f = f;
          filedup(f); // 增加文件的引用数, 保证它在mmap期间一定不会被关闭
          vma->offset = offset;
          return vma->addr;
        }
      }

      // all vma are in use
      return failure;
    }
    ```

4. 写完`mmap`后，当用户试图去访问`mmap`所返回的地址时，由于没有分配物理页，将会触发缺页中断。这个时候我们就需要在`usertrap`里把对应`offset`的文件内容读到一个新分配的物理页中，并把这个物理页加入这个进程的虚拟内存映射表里。
    ```c
    void
    usertrap(void)
    {
      // ...省略...

      if(r_scause() == 8){
        // ...省略...
      } else if(r_scause() == 13 || r_scause() == 15) { // 读或写造成的缺页中断
        uint64 va = r_stval();
        struct proc* p = myproc();
        if (va > MAXVA || va > p->sz) {
          // sanity check安全检查
          p->killed = 1;
        } else {
          int found = 0;
          for (int i = 0; i < NVMA; i++) {
            struct vma* vma = &p->vmas[i];
            if (vma->valid && va >= vma->addr && va < vma->addr+vma->length) {
              // 找到对应的vma, 分配一个新的4096字节的物理页
              // 并把对应的文件内容读进这个页, 插入进程的虚拟内存映射表
              va = PGROUNDDOWN(va);
              uint64 pa = (uint64)kalloc();
              if (pa == 0) {
                break;
              }
              memset((void *)pa, 0, PGSIZE);
              ilock(vma->f->ip);
              if(readi(vma->f->ip, 0, pa, vma->offset + va - vma->addr, PGSIZE) < 0) {
                iunlock(vma->f->ip);
                break;
              }
              iunlock(vma->f->ip);
              int perm = PTE_U; // 权限设置
              if (vma->prot & PROT_READ)
            perm |= PTE_R;
              if (vma->prot & PROT_WRITE)
                perm |= PTE_W;
              if (vma->prot & PROT_EXEC)
                perm |= PTE_X;
              if (mappages(p->pagetable, va, PGSIZE, pa, perm) < 0) {
                kfree((void*)pa);
                break;
              }
              found = 1;
              break;
            }
          }

          if (!found)
            p->killed = 1;
        }
      } 
      // ...省略...
    }
    ```

5. 在kernel/sysfile.c中实现`munmap`，把分配的物理页释放掉。如果flag是MAP_SHARED, 直接把unmap的区域复写回文件中。
    ```c
    uint64
    sys_munmap(void) {
      uint64 addr;
      int length;
      if (argaddr(0, &addr) < 0 || argint(1, &length) < 0)
        return -1;
      struct proc *p = myproc();
      struct vma* vma = 0;
      int idx = -1;
      // find the corresponding vma
      for (int i = 0; i < NVMA; i++) {
        if (p->vmas[i].valid && addr >= p->vmas[i].addr && addr <= p->vmas[i].addr + p->vmas[i].length) {
          idx = i;
          vma = &p->vmas[i];
          break;
        }
      }
      if (idx == -1)
        // not in a valid VMA
        return -1;

      addr = PGROUNDDOWN(addr);
      length = PGROUNDUP(length);
      if (vma->flags & MAP_SHARED) {
        // write back 将区域复写回文件
        if (filewrite(vma->f, addr, length) < 0) {
          printf("munmap: filewrite < 0\n");
        }
      }
    // 删除虚拟内存映射并释放物理页
      uvmunmap(p->pagetable, addr, length/PGSIZE, 1); 

      // change the mmap parameter
      if (addr == vma->addr && length == vma->length) {
        // fully unmapped 完全释放
        fileclose(vma->f);
        vma->valid = 0;
      } else if (addr == vma->addr) {
        // cover the beginning 释放区域包括头部
        vma->addr += length;
        vma->length -= length;
        vma->offset += length;
      } else if ((addr + length) == (vma->addr + vma->length)) {
        // cover the end 释放区域包括尾部
        vma->length -= length;
      } else {
        panic("munmap neither cover beginning or end of mapped region");
      }
      return 0;
    }
    ```

6. 在`exit`时要把mmap的区域释放掉。在`fork`时需要拷贝vma表，并增加一次文件`f`的引用数量。所以在kernel/proc.c中修改
    ```c
    void
    exit(int status)
    {
      // ...省略...

      // unmap any mmapped region
      for (int i = 0; i < NVMA; i++) {
        if (p->vmas[i].valid) {
          if (p->vmas[i].flags & MAP_SHARED) {
            filewrite(p->vmas[i].f, p->vmas[i].addr, p->vmas[i].length);
          }
          fileclose(p->vmas[i].f);
          uvmunmap(p->pagetable, p->vmas[i].addr, p->vmas[i].length / PGSIZE, 1);
          p->vmas[i].valid = 0;
        }
      }

      begin_op();
      // ...省略...
    }

    int
    fork(void)
    {
      // ...省略..
      for (int i = 0; i < NVMA; i++) {
        np->vmas[i].valid = 0;
        if (p->vmas[i].valid) { // 复制vma entry
          memmove(&np->vmas[i], &p->vmas[i], sizeof(struct vma));
          filedup(p->vmas[i].f); // 增加引用次数
        }
      }

      np->state = RUNNABLE;

      release(&np->lock);

      return pid;
    }
    ```

7. - 在xv6中运行`mmaptest`
    ```bash
    $ mmaptest
    mmap_test starting
    test mmap f
    test mmap f: OK
    test mmap private
    test mmap private: OK
    test mmap read-only
    test mmap read-only: OK
    test mmap read/write
    test mmap read/write: OK
    test mmap dirty
    test mmap dirty: OK
    test not-mapped unmap
    munmap: filewrite < 0
    test not-mapped unmap: OK
    test mmap two files
    test mmap two files: OK
    mmap_test: ALL OK
    fork_test starting
    munmap: filewrite < 0
    fork_test OK
    mmaptest: all tests succeeded
    $ usertests
    usertests starting
    ...
    ALL TESTS PASSED
    ```

   - 使用lab批分
    ```bash
    == Test running mmaptest ==
    $ make qemu-gdb
    (4.2s)
    == Test   mmaptest: mmap f ==
      mmaptest: mmap f: OK
    == Test   mmaptest: mmap private ==
      mmaptest: mmap private: OK
    == Test   mmaptest: mmap read-only ==
      mmaptest: mmap read-only: OK
    == Test   mmaptest: mmap read/write ==
      mmaptest: mmap read/write: OK
    == Test   mmaptest: mmap dirty ==
      mmaptest: mmap dirty: OK
    == Test   mmaptest: not-mapped unmap ==
      mmaptest: not-mapped unmap: OK
    == Test   mmaptest: two files ==
      mmaptest: two files: OK
    == Test   mmaptest: fork_test ==
      mmaptest: fork_test: OK
    == Test usertests ==
    $ make qemu-gdb
    usertests: OK (129.7s)
    == Test time ==
    time: OK
    Score: 140/140
    ```
### 3) 实验中遇到的问题和解决办法
- 测试`mmap_test`的时候，因为受到`uvmunmap`的not mapped限制，导致在`test not-mapped unmap`的时候，报panic。只需在`uvmunmap`中当出现not mapped的情况时将panic中断改成continue跳过即可
- 测试`fork_test`的时候，因为在实现的时候将 `mmap` 区间塞进了 `p->sz`（把 `p->sz += length`），但这些页是 lazy 的、并没有真正建立 PTE ，其受到`uvmcopy`的page not present限制，导致在`fork_test`报panic。只需在`uvmcopy`中当出现`(*pte & PTE_V) == 0`的情况的时候选择continue跳过而非panic中断即可。

### 4) 实验心得
- 熟悉了用户逻辑地址与内核内存地址映射的逻辑
- 熟悉了trap中断的逻辑以及严格性控制
- 熟悉了exit以及fork时对地址映射的释放处理以及拷贝虚拟映射表的处理