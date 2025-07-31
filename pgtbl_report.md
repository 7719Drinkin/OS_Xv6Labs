# Lab: page tables
## 1. Speed up system calls (easy)
### 1) 实验目的
- 一些操作系统(例如Linux)通过在用户空间和内核之间的只读区域共享数据来加速某些系统调用。这消除了在执行这些系统调用时需要跨内核的需求。为了了解如何将映射插入到页表中，此任务将为xv6中的getpid()系统调用实现这种优化。

### 2) 实验步骤
1. 在kernel/proc.h中的`struct proc`增加一个域`usyscall`，存储共享内存块的物理地址。
   ```c
   // Per-process state
   struct proc {
       //......
       struct file *ofile[NOFILE];  // Open files
       struct inode *cwd;           // Current directory
       struct usyscall *usyscall    // share with kernel << 添加一个共享内存块的物理地址
       char name[16];               // Process name (debugging)
   };
   ```

2. 在kernel/proc.c的`allocproc`函数中增加申请共享内存页。
   ```c
   // Allocate a share page
   if((p->usyscall = (struct usyscall *)kalloc()) == 0){
     freeproc(p);
     release(&p->lock);
     return 0;
   }

   memmove(p->usyscall, &p->pid, 8);
   ```

3. 在kernel/proc.c的`proc_pagetable`函数中增加在内核中共享内存页的初始化，以及对共享内存块的页表初始化。
   ```c
   pagetable_t
   proc_pagetable(struct proc *p)
   {
     // ......
     // map the USYSCALL just below TRAPFRAME.
     if(mappages(pagetable, USYSCALL, PGSIZE, (uint64)(p->usyscall), PTE_R | PTE_U) < 0){
       uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmunmap(pagetable, TRAPFRAME, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
     }

     return pagetable;
   }
   ```

4. 在kernel/proc.c的`freeproc`函数中增加释放共享内存块部分代码。
   ```c
   freeproc(struct proc *p)
   {
   // ......
     if (p->trapframe)
       kfree((void*)p->usyscall);
     p->usyscall = 0;
   // ......
   }
   ```

5. 在kernel/proc.c的`proc_freepagetable`函数中增加一行释放页表中共享内存页项。
   ```c
   // Free a process's page table, and free the
   // physical memory it refers to.
   void
   proc_freepagetable(pagetable_t pagetable, uint64 sz)
   {
     uvmunmap(pagetable, TRAMPOLINE, 1, 0);
     uvmunmap(pagetable, TRAPFRAME, 1, 0);
     uvmunmap(pagetable, USYSCALL, 1, 0); // 添加释放页表中共享内存页项
     uvmfree(pagetable, sz);
   }
   ```

6. 将测试文件`pgtbltest`加入到Makefile内的UPROGS变量中，
   ```Makefile
   UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_pgtbltest\ # 此项，此注释不应出现在Makefile中
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
   ```

7. - 在xv6中调用`pgtbltest`:
     ```bash
     xv6 kernel is booting

     hart 1 starting
     hart 2 starting
     init: starting sh
     $ pgtbltest
     ugetpid_test starting
     ugetpid_test: OK
     ```
   - 使用lab批分:
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-pgtbl pgtbltest
     /home/drinkin/xv6-labs-2021/./grade-lab-pgtbl:22: SyntaxWarning: invalid escape sequence '\s'
       INDENT_ESC = "\\\s*\.\.\\\s*"
     make: 'kernel/kernel' is up to date.
     == Test pgtbltest == (0.9s)
     == Test   pgtbltest: ugetpid ==
       pgtbltest: ugetpid: OK
     == Test   pgtbltest: pgaccess ==
       pgtbltest: pgaccess: FAIL
         ...
              ugetpid_test starting
              ugetpid_test: OK
              pgaccess_test starting
              pgtbltest: pgaccess_test failed: incorrect access bits set, pid=3
              $ qemu-system-riscv64: terminating on signal 15 from pid 12894 (make)
         MISSING '^pgaccess_test: OK$'
     ```
     ugetpid通过即可。

### 3) 实验中遇到的问题和解决办法
- 在kernel/proc.c的`allocproc`函数中书写增加申请共享内存页的内存分配错误情况的代码时，指针错误指到了`trapframe`域，导致`$ make qemu`时报`panic: kerneltrap` 的错。将指针指向之前添加的`usyscall`域即可。 

### 4) 实验心得
- 掌握了对进程中域的添加方法
- 掌握了对进程申请的添加方法
- 熟悉了对共享内存页以及共享内存块的页表的初始化和释放逻辑

## 2. Print a page table (easy)
### 1) 实验目的
- 定义一个名为`vmprint()`的函数用于输出页表的内容。它有一个pagetable_t类型的参数，按下面描述的格式输出页表。在exec.c中的`return argc`;之前插入
  ```c
  if(p->pid==1) vmprint(p->pagetable); 
  ```
  语句来输出第一个进程的页表。需要通过了make grade的pte printout测试。

### 2) 实验步骤
1. 在kernel/vm.c中编写`vmprint()`函数
   ```c
   // Recursively print the page table entries for the given pagetable at the specified level.
   // Used for debugging and understanding the page table structure.
   void
   printwalk(pagetable_t pagetable, uint level)
   {
     char* prefix;
     if (level == 2) prefix = "..";
     else if (level == 1) prefix = ".. ..";
     else prefix = ".. .. ..";
     for(int i = 0; i < 512; i++) { // 每个页表有512项
       pte_t pte = pagetable[i];
       if(pte & PTE_V){ // 该页表项有效
         uint64 pa = PTE2PA(pte); // 将虚拟地址转换为物理地址
         printf("%s%d: pte %p pa %p\n", prefix, i, pte, pa);
         if((pte & (PTE_R|PTE_W|PTE_X)) == 0){ // 有下一级页表
           printwalk((pagetable_t)pa, level - 1);
         }
       }
     }
   }


   // Print the page table for debugging.
   void
   vmprint(pagetable_t pagetable) {
     printf("page table %p\n", pagetable);
     printwalk(pagetable, 2);
   }
   ```

2. 在kernel/defs.h中定义`vmprint()`，以便可以在exe.c中调用
   ```c
   // vm.c
   // ......
   int             copyin(pagetable_t, char *, uint64, uint64);
   int             copyinstr(pagetable_t, char *, uint64, uint64);
   void            vmprint(pagetable_t pagetable);
   ```

3. 在exec.c中的返回argc之前插入if (p->pid==1) vmprint(p->pagetable)，以输出第一个进程的页表。
   ```c
   // ......
   if (p->pid == 1) vmprint(p->pagetable);

   return argc; // this ends up in a0, the first argument to main(argc, argv)
   ```

4. `make qemu`输出页表信息:
   ```bash
   xv6 kernel is booting

   hart 2 starting
   hart 1 starting
   page table 0x0000000087f6e000
   ..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
   .. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
   .. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
   .. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
   .. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
   ..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
   .. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
   .. .. ..509: pte 0x0000000021fdd813 pa 0x0000000087f76000
   .. .. ..510: pte 0x0000000021fddc07 pa 0x0000000087f77000
   .. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
   init: starting sh
   $
   ```

### 3) 实验中遇到的问题和解决办法
- 在实现`printwalk()`递归遍历打印页表页方法时，可以参考xv6中函数原本有的用于实现递归释放页表页的函数`freewalk()`:
  ```c
  // Recursively free page-table pages.
  // All leaf mappings must already have been removed.
  void
  freewalk(pagetable_t pagetable)
  {
    // there are 2^9 = 512 PTEs in a page table.
    for(int i = 0; i < 512; i++){
      pte_t pte = pagetable[i];
      if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table.
        uint64 child = PTE2PA(pte);
        freewalk((pagetable_t)child);
        pagetable[i] = 0;
      } else if(pte & PTE_V){
        panic("freewalk: leaf");
      }
    }
    kfree((void*)pagetable);
  }
  ```

### 4) 实验心得
- 掌握了打印页表的方法
- 熟悉了递归遍历页表的逻辑
- 熟悉了exe.c的调用逻辑

## 3. Detecting which pages have been accessed (hard)
### 1) 实验目的
- 实现一个能检测并报告哪些页表被访问过的`pgaccess()`系统调用函数。其接受三个参数，第一个参数是需要检查第一个用户页面的起始虚拟地址。第二个参数是需要检查页数。最后一个参数用户缓冲区的地址，检查结果以位掩码（一种数据结构，每页使用一位，其中第一页对应于最低有效位）的形式存储在这个缓冲区中。运行`pgtbltest`，目标是`pgaccess`测试用例通过。

### 2) 实验步骤
1. 在kernel/riscv.h中定义常量`PTE_A`
   ```c
   /// ......
   #define PTE_V (1L << 0) // valid
   #define PTE_R (1L << 1)
   #define PTE_W (1L << 2)
   #define PTE_X (1L << 3)
   #define PTE_U (1L << 4) // 1 -> user can access
   #define PTE_A (1L << 6) // 1 -> Accessed
   // .......
   ```

2. 在kernel/sysproc.c中编写`sys_pgaccess`函数。
   ```c
   // detects and reports this information to userspace by inspecting the access bits in the RISC-V page table.
  int
   sys_pgaccess(void)
   {
     // lab pgtbl: your code here.
     uint64 base;
     uint64 mask;
     int len;

     pagetable_t pagetable = 0;
     unsigned int access_mask = 0;
     pte_t *pte;

     struct proc *p = myproc();

     // 获取参数：起始虚拟地址base，页数len，用户空间mask地址
     if(argaddr(0, &base) < 0 || argint(1, &len) < 0 || argaddr(2, &mask) < 0)
       return -1;
     // 最多检测32页
     if(len > sizeof(int)*8)
     len = sizeof(int)*8;

     for(int i = 0; i < len; i++){
       pagetable = p->pagetable;

       // 检查虚拟地址是否越界
       if(base >= MAXVA)
         panic("pgaccess");

       // 遍历多级页表，找到对应的PTE
       for(int level = 2; level > 0; level--){
         pte = &pagetable[PX(level, base)];
         if(*pte & PTE_V){
           pagetable = (pagetable_t)PTE2PA(*pte);
         } else {
          return -1;
         }
       }
       pte = &pagetable[PX(0, base)];
       if(pte == 0)
         return -1;
       if((*pte & PTE_V) == 0)
         return -1;
       if((*pte & PTE_V) == 0)
         return -1;
       // 检查访问位PTE_A，如果被访问过则设置access_mask对应位，并清除访问位
       if(*pte & PTE_A){
         access_mask = access_mask | (1L << i);
         *pte = *pte & (~PTE_A);
       }
       // 检查下一页
       base += PGSIZE;
     }

     pagetable = p->pagetable;
     // 将access_mask结果拷贝到用户空间
     return copyout(pagetable, mask, (char *)&access_mask, sizeof(unsigned int));
   }
   ```

3. - 在xv6中运行`pgtbltest`:
     ```bash
     xv6 kernel is booting

     hart 1 starting
     hart 2 starting
     page table 0x0000000087f6e000
     ..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
     .. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
     .. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
     .. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
     .. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
     ..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
     .. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
     .. .. ..509: pte 0x0000000021fdd813 pa 0x0000000087f76000
     .. .. ..510: pte 0x0000000021fddc07 pa 0x0000000087f77000
     .. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
     init: starting sh
     $ pgtbltest
     ugetpid_test starting
     ugetpid_test: OK
     pgaccess_test starting
     pgaccess_test: OK
     pgtbltest: all tests succeeded
     ```

   - 使用lab批分:
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-pgtbl pgtbltest
     /home/drinkin/xv6-labs-2021/./grade-lab-pgtbl:22: SyntaxWarning: invalid escape sequence '\s'
       INDENT_ESC = "\\\s*\.\.\\\s*"
     make: 'kernel/kernel' is up to date.
     == Test pgtbltest == (1.8s)
     == Test   pgtbltest: ugetpid ==
       pgtbltest: ugetpid: OK
     == Test   pgtbltest: pgaccess ==
       pgtbltest: pgaccess: OK
     ```

### 3) 实验中遇到的问题和解决办法
- 在遍历页表的时候，需要使用`pagetable = (pagetable_t)PTE2PA(*pte);`而非`PTE_A(*pte)`，因为遍历的时候需要遍历的是物理地址，而`PTE_A`只是访问位，不是地址。

- 在清除访问位的时候，需要使用`*pte = *pte & (~PTE_A)`进行与非运算。

### 4) 实验心得
- 熟悉了对物理内存的访问遍历逻辑
- 熟悉了对页表的使用情况的检测和报告逻辑
- 认识了在RICS-V中页表的真正实现方法
