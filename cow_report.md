# Lab: Copy-on-Write Fork for xv6
## Implement copy-on write(hard)
### 1) 实验目的
- 在xv6内核中实现写时复制分支的功能
### 2) 实验步骤
1. 在kernel/kalloc.中添加一个静态列表的形式存储计数器，以记录一个物理页同时被多少个进程的虚拟页所指向，只有当一个物理页的计数器降为0后才能真正释放它。
   ```c
    // reference count for each physical page to facilitate COW
    #define PA2INDEX(pa) (((uint64)pa)/PGSIZE)

    int cowcount[PHYSTOP/PGSIZE];

    void
    freerange(void *pa_start, void *pa_end)
    {
        char *p;
        p = (char*)PGROUNDUP((uint64)pa_start);
        for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
            cowcount[PA2INDEX(p)] = 1; // 初始化的时候把每个物理页都加入freelist
            kfree(p);
        }
    }

    void
    kfree(void *pa)
    {
        struct run *r;

        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
            panic("kfree");

        // 需要加锁保证原子性
        acquire(&kmem.lock);
        int remain = --cowcount[PA2INDEX(pa)];
        release(&kmem.lock);

        if (remain > 0) {
            // 只有最后1个reference被删除时需要真正释放这个物理页
         return;
        }

        // ...省略...
    }

    void *
    kalloc(void)
    {
        struct run *r;

        acquire(&kmem.lock);
        r = kmem.freelist;
        if(r)
            kmem.freelist = r->next;
        release(&kmem.lock);

        if(r) {
            memset((char *)r, 5, PGSIZE); // fill with junk
            int idx = PA2INDEX(r);
            if (cowcount[idx] != 0) {
            panic("kalloc: cowcount[idx] != 0");
            }
            cowcount[idx] = 1; // 新allocate的物理页的计数器为1
        }
        return (void*)r;
    }

    // helper函数
    void adjustref(uint64 pa, int num) {
        if (pa >= PHYSTOP) {
            panic("addref: pa too big");
        }
        acquire(&kmem.lock);
        cowcount[PA2INDEX(pa)] += num;
        release(&kmem.lock);
    }
   ```
2. 在kernel/vm.c中修改`uvmcopy()`使得`fork()`时, 不再额外分配复制物理页，而是让子进程和父进程共享只读页面。
    ```c
    int
    uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
    {
        pte_t *pte;
        uint64 pa, i;
        uint flags;

        for(i = 0; i < sz; i += PGSIZE){
            if((pte = walk(old, i, 0)) == 0)
                panic("uvmcopy: pte should exist");
            if((*pte & PTE_V) == 0)
                panic("uvmcopy: page not present");
            pa = PTE2PA(*pte);
            *pte &= ~PTE_W;  // 变为只读页面, 不允许写. 一旦试图写, 会触发num=15的trap
            flags = PTE_FLAGS(*pte);
            if(mappages(new, i, PGSIZE, pa, flags) != 0){
                goto err;
            }
            adjustref(pa, 1); // 增加计数器
        }
        return 0;

        err:
        uvmunmap(new, 0, i / PGSIZE, 1);
        return -1;
    }
    ```

3. 在kernel/vm.c中实现一个`cowwalloc()`函数，用于实现当一个进程想要对于一个只读的COW页面进行修改时，把这一页复制一遍赋给这个进程的功能。并将其声明添加到kernel/defs.h中
    ```c
    // kernel/vm.c
    int
    cowalloc(pagetable_t pagetable, uint64 va) {
        if (va >= MAXVA) {
        printf("cowalloc: exceeds MAXVA\n");
        return -1;
        }

        pte_t* pte = walk(pagetable, va, 0); // should refer to a shared PA
        if (pte == 0) {
            panic("cowalloc: pte not exists");
        }
        if ((*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
            panic("cowalloc: pte permission err");
        }
        uint64 pa_new = (uint64)kalloc();
        if (pa_new == 0) {
            printf("cowalloc: kalloc fails\n");
            return -1;
        }
        uint64 pa_old = PTE2PA(*pte);
        memmove((void *)pa_new, (const void *)pa_old, PGSIZE);
        kfree((void *)pa_old); // 减少COW页面的reference count
        *pte = PA2PTE(pa_new) | PTE_FLAGS(*pte) | PTE_W;
        return 0;
    }

    // kernel/defs.h
    int             cowalloc(pagetable_t pagetable, uint64 va);
    ```

4. 在kernel/traps.c中对`usertrap()`进行修改，使得当任何一个进程试图去写共享页面时，处理为进程复制一页这个共享物理页，并允许写权限。
    ```c
    // handle an interrupt, exception, or system call from user space.
    // called from trampoline.S
    //
    void
    usertrap(void)
    {
    // ...省略...
  
        if(r_scause() == 8){
            // system call

            if(p->killed)
            exit(-1);

            // sepc points to the ecall instruction,
            // but we want to return to the next instruction.
            p->trapframe->epc += 4;

            // an interrupt will change sstatus &c registers,
            // so don't enable until done with those registers.
            intr_on();

            syscall();
        } else if (r_scause() == 15) {
            // 试图在一个COW只读页面上进行写操作, 为该进程额外分配复制一页
            if (cowalloc(p->pagetable, r_stval()) < 0) {
                p->killed = 1;
            }
        } else if((which_dev = devintr()) != 0){
            // ok
        } else {
            printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
            printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
            p->killed = 1;
        }
        // ...省略...
    }
    ```

5. 在kernel/vm.c中对`copyout()`进行修改，使得在呼叫`copyout`时，通过查看一个页的pte里的W写flag是否被关闭，判断此页是不是一个共享COW页并对其进行额外的复制操作。
    ```c
    // Copy from kernel to user.
    // Copy len bytes from src to virtual address dstva in a given page table.
    // Return 0 on success, -1 on error.
    int
    copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
    {
        uint64 n, va0, pa0;

        while(len > 0){
            va0 = PGROUNDDOWN(dstva);
            if (va0 >= MAXVA) {
                printf("copyout: va exceeds MAXVA\n");
                return -1;
            }
            pte_t *pte = walk(pagetable, va0, 0);
            if (pte == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_V) == 0) {
                printf("copyout: invalid pte\n");
                return -1;
            }
            if ((*pte & PTE_W) == 0) {
                // 写的目的地是COW共享页, 需要复制一份
                if (cowalloc(pagetable, va0) < 0) {
                    return -1;
                }
            }
            //...省略...
        }
        return 0;
    }
    ```

6. - 在xv6中运行`cowtest`和`usertests`
     ```bash
     $ cowtest
     simple: ok
     simple: ok
     three: ok
     three: ok
     three: ok
     file: ok
     ALL COW TESTS PASSED
     $ usertests
     ...
     ALL TESTS PASSED
     ```

   - 使用lab进行批分
     ```bash
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     == Test running cowtest ==
     $ make qemu-gdb
     (7.8s)
     == Test   simple ==
       simple: OK
     == Test   three ==
       three: OK
     == Test   file ==
       file: OK
     == Test usertests ==
     $ make qemu-gdb
     (118.7s)
     == Test   usertests: copyin ==
       usertests: copyin: OK
     == Test   usertests: copyout ==
       usertests: copyout: OK
     == Test   usertests: all tests ==
       usertests: all tests: OK
     == Test time ==
     time: OK
     Score: 110/110
     ```

### 3) 实验中遇到的问题和解决办法
- 在新实现`adjustref`和`cowalloc`时忘记将其在defs.h中声明，导致出现报错。将其在在defs.h中声明后即可。
- 在修改`usertrap`时，调用的`cowalloc`没有被正确调用，导致运行`cowtest`时出现发生了用户态程序访问无效内存地址的异常。将其声明了之后问题解决。

### 4) 实验心得
- 熟悉了copy-on-write的逻辑
- 深刻理解了子进程和父进程实现共享进程信息所需要实现的读写逻辑
- 掌握了xv6中新函数的声明和实现逻辑


