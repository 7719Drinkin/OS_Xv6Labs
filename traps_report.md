# Lab: traps
## 1. RISC-V assembly (easy)
### 1) 实验目的
- 通过阅读对user/call.c编译生成的user/call.asm并回答以下问题以理解RISC-V assembly。
  > Which registers contain arguments to functions? For example, which register    holds 13 in main's call to `printf`?

  > Where is the call to function f in the assembly code for main? 

  > Where is the call to g? (Hint: the compiler may inline functions.)

  > At what address is the function printf located?

  > What value is in the register ra just after the jalr to printf in main?

  > Run the following code.
    `
        unsigned int i = 0x00646c72;
        printf("H%x Wo%s", 57616, &i);
    `
  What is the output? Here's an ASCII table that maps bytes to characters.
  The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set to in order to yield the same output? Would you need to change to a different value?`i57616`
  Here's a description of little- and big-endian and a more whimsical description.
  In the following code, what is going to be printed after ? (note: the answer is not a specific value.) Why does this happen? `'y='`
	`printf("x=%d y=%d", 3);`

### 2) 实验步骤
1. 在终端中输入`make fs.img`以生成编译文件user/call.asm
   ```asm

   ```
2. 观察生成的编译文件内容并得出以上问题答案：
   1. 寄存器`a2`存放了`13`这个值
   2. 一整个函数调用链`f(g(8))+1`都被编译器直接优化成inline了并算出了答案`12`, 放置在了`a1`寄存器里。
      由这一行汇编源码可以看出:
      ```asm
      26:	45b1                	li	a1,12
      ```
   3. 由这一行汇编源码:
      ```asm
       0000000000000638 <printf>
      ```
      我们可以得知printf在0x638的位置
    4. `jalr`这个指令会把下一个要执行的指令的地址压入`ra`, 即`0x34+4 = 0x38`
    5. 输出的答案是 `HE110 World`
       `57616`的`hex`是`0x0000E110`
       `i=0x00646c72`, 字节翻译是`0dlr`. 因为RISC-V是little-endian, 存放在连续内存里的顺序即是`rld0`.
       如果RISC-V是big-endian, `57616`不用变, `i`需要变成`0x726c6400`
    6. `printf`的format字符串在寄存器`a0`, `3`在寄存器`a1`, 所以当试图`print y`的时候, 留在寄存器`a2`的某个随机值就会被`print`出来.

### 3) 实验中遇到的问题和解决办法
- 在观察`call.asm`文件时由于没有语法intelligence会导致整个文件是以纯文本形式显示，对阅读有一定的影响，如果是VS code，可以在安装一个masm-code的扩展，这样语法就可以高亮显示，并且对各个部分的代码有解释，对阅读有极大的帮助。

### 4) 实验心得
- 熟悉了对.asm文件的阅读
- 熟悉了RISC-V的一些指令
- 理解了RISC-V这个系统指令集的精简的特征

## 2. Backtrace (moderate)
### 1) 实验目的
- 实现一个`backtrace()`函数对call栈进行递归遍历并找到错误发生的位置的指针。

### 2) 实验步骤
1. 阅读实验手册，知道了栈空间在xv6里只占1页，所以迭代到这一页的页底作为结束条件。
2. 将实验手册提供的代码添加到kernel/riscv.h中
   ```c
   static inline uint64
   r_fp()
   {
     uint64 x;
     asm volatile("mv %0, s0" : "=r" (x) );
     return x;
   }
   ```
3. 在kernel/printf.c中实现`backtrace()`的代码
   ```c
   void backtrace(void)
   {
     printf("backtrace:\n");
     // 1. 获取当前栈顶指针
     uint64 curr_fp = r_fp();
     uint64 page_bottom = PGROUNDDOWN(curr_fp); // 栈页的底部
     while (page_bottom < curr_fp) {
       // 2. 获取上一个栈的返回地址和栈顶指针
       uint64 ret = *(pte_t *)(curr_fp - 0x8);
       uint64 prev_fp = *(pte_t *)(curr_fp - 0x10);
       // 3. 打印返回地址
       printf("%p\n", ret);
       // 4. 跳转到上一个栈的栈顶指针所指位置
       curr_fp = prev_fp;
     }
   }
   ```

4. 将`bttest`加入到Makefile中的UPROGS变量中
   ```Makefile
   UPROGS=\
	$U/_bttest\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\

   ```

5. 将`backtrace()`在`sys_sleep()`中添加一个调用
   ```c
   uint64
   sys_sleep(void)
   {
     int n;
     uint ticks0;
  
     backtrace();  // 添加一个backtrace的调用
     if(argint(0, &n) < 0)
       return -1;
     acquire(&tickslock);
     ticks0 = ticks;
     while(ticks - ticks0 < n){
       if(myproc()->killed){
         release(&tickslock);
         return -1;
       }
       sleep(&ticks, &tickslock);
     }
     release(&tickslock);
     return 0;
   }
   ```

6. 在xv6中运行`bttest`
   ```bash
   xv6 kernel is booting

   hart 2 starting
   hart 1 starting
   binit: starting sh
   $ bttest
   exec bbttest failed
   $ bttest
   backtrace:
   0x0000000080002110
   0x0000000080001fee
   0x0000000080001cd8
   0x0000000000000012
   $ QEMU: Terminated

   ```

7. 退出qemu后在终端中运行`addr2line -e kernel/kernel`
   ```bash
   drinkin@DrinkinsLaptop:~/xv6-labs-2021$ addr2line -e kernel/kernel
   0x0000000080002110
   0x0000000080001fee
   0x0000000080001cd8
   0x0000000000000012/home/drinkin/xv6-labs-2021/kernel/sysproc.c:62
   /home/drinkin/xv6-labs-2021/kernel/syscall.c:140 (discriminator 1)
   /home/drinkin/xv6-labs-2021/kernel/trap.c:76
   ```

### 3) 实验中遇到的问题和解决办法
- 在调用`bttest`时，由于忘记了要将`backtrace()`调用添加到`sys_sleep`中，导致在xv6中运行`bttest`时只sleep了1tick并且什么都没有输出。需要注意题目要求将`backtrace()`调用添加到`sys_sleep`中。

### 4) 实验心得
- 熟悉了backtrace在栈中的使用
- 熟悉了xv6中栈的特征

## 3. Alarm (hard)
### 1) 实验目的
- 实现一个`sigalarm(interval， handler)`系统调用，如果应用程序调用 `sigalarm(n， fn)`，则在程序每消耗 `n` 个 CPU ticks后，内核应该调用应用程序函数 `fn` 以实现定期发出警报的功能，当 `fn` 返回时，应用程序应该从上次中断的地方恢复。

### 2) 实验步骤
1. 将2个新的system call`sigalarm`和`sigreturn`加入到相对应的头文件里，使得其能被用户端调用
   ```c
   // kernel/syscall.h
   #define SYS_sigalarm 22
   #define SYS_sigreturn 23
   ```
   ```c
   // kernel/syscall.c
   static uint64 (*syscalls[])(void) = {
   [SYS_sigalarm]   sys_sigalarm,
   [SYS_sigreturn]   sys_sigreturn,
   }
   // user/user.pl
   entry("sigalarm")
   entry("sigreturn")
   ```

2. 在`struct proc`里加入新的需要管理的`fields`。
   ```c
   struct proc {
     // ......
     // alarm
     uint64 tracemask;            // the sys calls this proc is tracing
     pagetable_t kpagetable;      // the kernel table per process
     int alarm_period;            // the alarm period set
     void (*alarm_handler)();     // the alarm function handler
     int ticks_since_last_alarm;  // how many ticks has elapsed since last alarm
   }
   ```

3. 在kernel/sysproc.c中实现`sigalarm()`，把提供的参数记录到struct proc里。
   ```c
   // set an alarm to call the handler function
   // every period ticks
   uint64
   sys_sigalarm(void) {
     struct proc *my_proc = myproc();
     int period;
     if (argint(0, &period) < 0)
       return -1;
     uint64 p;
     if(argaddr(1, &p) < 0)
       return -1;
     my_proc->alarm_period = period;
     my_proc->alarm_handler = (void (*)()) p;
     my_proc->ticks_since_last_alarm = 0;
     return 0;
   }
   ```

4. 在kernel/proc.c中的`allocproc`函数里初始化新的fields.
   ```c
   // Look in the process table for an UNUSED proc.
   // If found, initialize state required to run in the kernel,
   // and return with p->lock held.
   // If there are no free procs, or a memory allocation fails, return 0.
   static struct proc*
   allocproc(void)
   {
     struct proc *p;
     // ...省略...
     // Zero initializes the tracemask for a new process
     p->tracemask = 0;

     // Zero initializes the alarm releated fields
     p->alarm_period = 0;
     p->alarm_handler = 0;
     p->ticks_since_last_alarm = 0;
     return p;
   }
   ```

5. 修改一下kernel/trap.c，使其从trap中返回到用户态后, 优先跳转回需要执行的alarm函数.
   ```c
   // handle an interrupt, exception, or system call from user space.
   // called from trampoline.S
   //
   void
   usertrap(void)
   {
     int which_dev = 0;

     // ...省略...

     // give up the CPU if this is a timer interrupt.
     if(which_dev == 2) {
         p->ticks_since_last_alarm += 1;
         if (p->alarm_period != 0 && p->ticks_since_last_alarm == p->alarm_period) {
           // 设立返回到用户态后到跳转指令地址
           // jump to the alarm handler when returning back to user space
           p->trapframe->epc = (uint64)p->alarm_handler;  // 修改跳转位置到alarm函数
           p->ticks_since_last_alarm = 0;
         }
         yield();
     }
     usertrapret();
   }
   ```

6. 在struct proc增加新的两个field, 并为之初始化和析构。
   ```c
   // kernel/proc.h
   // Per-process state
   struct proc {
     struct spinlock lock;
     //... 省略...
     struct trapframe *alarmframe;// data page to restore all register when going back from alarm handler
     int alarm_period;            // the alarm period set
     void (*alarm_handler)();     // the alarm function handler
     int ticks_since_last_alarm;  // how many ticks has elapsed since last alarm
     int inalarm;                 // if the alarm handler is going on
   };

   // kernel/proc.c
   // Look in the process table for an UNUSED proc.
   // If found, initialize state required to run in the kernel,
   // and return with p->lock held.
   // If there are no free procs, or a memory allocation fails, return 0.
   static struct proc*
   allocproc(void)
   {
     // ...省略...
     p->pid = allocpid();

     // Allocate a trapframe page.
     if((p->trapframe = (struct trapframe *)kalloc()) == 0){
       release(&p->lock);
       return 0;
     }

     // Allocate a alarmframe page. 为这个备份alarmframe分配1页物理空间
     if((p->alarmframe = (struct trapframe *)kalloc()) == 0){
       release(&p->lock);
       return 0;
     }

     // Zero initializes the alarm releated fields
     p->alarm_period = 0;
     p->alarm_handler = 0;
     p->ticks_since_last_alarm = 0;
     p->inalarm = 0;
     return p;
   }


   // free a proc structure and the data hanging from it,
   // including user pages.
   // p->lock must be held.
   static void
   freeproc(struct proc *p)
   {
     if(p->trapframe)
       kfree((void*)p->trapframe);
     //  ...省略...
     if (p->alarmframe) // 释放物理内存
       kfree((void *)p->alarmframe);
     p->alarmframe = 0;
   }
   ```

7. 在kernel/trap.c的函数里，  每当要触发用户设置的`alarm`函数时，保存当前所有的寄存器到`p->alarmframe`里，并设立一个`p->inalarm`的flag，使得在上一个`alarm`函数还没处理完前，不会重复进入这个alarm函数。
   ```c
   // handle an interrupt, exception, or system call from user space.
   // called from trampoline.S
   //
   void
   usertrap(void)
   {
     int which_dev = devintr();

     // ...省略...

     // give up the CPU if this is a timer interrupt.
     if(which_dev == 2) {
         p->ticks_since_last_alarm += 1;
         if (p->inalarm == 0 && p->alarm_period != 0 && p->ticks_since_last_alarm == p->alarm_period) {
           p->inalarm = 1;  // 避免重复进入
           *p->alarmframe = *p->trapframe; // 备份当前寄存器
           // save all the trapframe to alarmframe for later restore
           // jump to the alarm handler when returning back to user space
           p->trapframe->epc = (uint64)p->alarm_handler; // 修改跳转位置到alarm函数
         }
         yield();
     }

     usertrapret();
   }
   ```

8. 在`sigreturn`函数里, 我们重新取回被alarm中断前到所有寄存器状态并去掉`p->inalarm`旗帜, 使得可以再次alarm中断.
   ```c
   uint64
   sys_sigreturn(void) {
     struct proc* p = myproc();
     if (p->inalarm) {
       p->inalarm = 0;
       *p->trapframe = *p->alarmframe;
       p->ticks_since_last_alarm = 0;
     }
     return 0;
   }
   ```

9. - 在xv6中运行`alarmtest`和`usertests`
     ```bash
     xv6 kernel is booting

     hart 2 starting
     hart 1 starting
     init: starting sh
     $ alarmtest
     test0 start
     ...............................alarm!
     test0 passed
     test1 start
     ...alarm!
     ..alarm!
     ....alarm!
     ..alarm!
     ...alarm!
     ...alarm!
     ...alarm!
     ...alarm!
     ..alarm!
     ...alarm!
     test1 passed
     test2 start
     .....................................alarm!
     test2 passed
     $ usertests
     usertests starting
     ...
     ALL TESTS PASSED
     ```

   - 使用lab进行批分
     ```bash
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     == Test answers-traps.txt == answers-traps.txt: OK
     == Test backtrace test ==
     $ make qemu-gdb
     backtrace test: OK (3.8s)
     == Test running alarmtest ==
     $ make qemu-gdb
     (3.5s)
     == Test   alarmtest: test0 ==
       alarmtest: test0: OK
     == Test   alarmtest: test1 ==
       alarmtest: test1: OK
     == Test   alarmtest: test2 ==
       alarmtest: test2: OK
     == Test usertests ==
     $ make qemu-gdb
     usertests: OK (125.5s)
     == Test time ==
     time: OK
     Score: 85/85
     ```
### 3) 实验中遇到的问题和解决办法
- 在修改`usertrap`的时候，没有注意避免重复进入发生了重复调用alarm的错误，所以需要添加多一个inalarm的域以作为标志位对重复进入进行避免。

- 在`sigreturn`函数里能够重新取回alarm中断前到所有寄存器状态后，未去掉p->inalarm标志，导致不能被alarm中断。需要将p->inalarm标志去除。

### 4) 实验心得
- 理解了alarm与时钟之间的逻辑关系
- 熟悉了在每个时钟tick时对alarm的调用关系
- 熟悉了alarm在操作系统中的实现