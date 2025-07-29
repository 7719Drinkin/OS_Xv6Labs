# Lab: system calls 
## 1.System call tracing (moderate)
### 1) 实验目的
- 创建一个新的跟踪系统调用来控制跟踪。它应该接受一个整数“掩码”参数，来指定要跟踪的系统调用。例如，为了跟踪分叉系统调用，程序调用 trace（1 << SYS_fork），其中 SYS_fork 是来自 kernel/syscall.h 的系统调用号。如果系统调用的编号在掩码中设置，则必须修改 xv6 内核以在每个系统调用即将返回时打印出一行。该行应包含进程 ID、系统调用的名称和返回值，但不需要打印系统调用参数。跟踪系统调用应启用对调用它的进程及其随后分支的任何子进程的跟踪，但不应影响其他进程。

### 2) 实验步骤
1. 在user/user.h理提供一个用户接口。
```c
// ...省略...
int sleep(int);
int uptime(void);
int trace(int); // 新系统调用trace的函数原型签名
```

2. 在user/usys.pl中为这个函数签名加入entry
```pl
# ...省略...
entry("sleep");
entry("uptime");
entry("trace");
```

3. 在kernel/proc.h和kernel/proc.c里，为struct proc中加入记忆需要的trace的bit mask。
- kernel/proc.h添加一个field
```c
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  uint64 tracemask;            // the sys calls this proc is tracing  << 新的field加在这里
}
```
- kernel/proc.c修改两个函数，即在创建进程的两个函数中添加对进程的追踪
```c
static struct proc*
allocproc(void)
{
  // ...省略...
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
  p->tracemask = 0; // 新的进程默认不追踪sys call

  return p;
}

int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // inherit parent's trace mask << fork出的新进程继承父进程的bit mask
  np->tracemask = p->tracemask;
  // ...省略...
}
```

4. 实现内核态下的sys_trace函数, 用argint函数从寄存器拿参数, 用myproc获得当前进程的一个指针
```c
uint64
sys_trace(void)
{
  int trace_sys_mask;
  if(argint(0, &trace_sys_mask) < 0)
    return -1;
  myproc()->tracemask |= trace_sys_mask;
  return 0;
}
```

5. 为这个sys_trace提供一个代号和函数指针的mapping. 并且顺加上一个从代号到名字的mapping, 以便于print
- kernel/sys_call.h
```c
// ...省略...
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_trace  22
```

- kernel/sys_call.c
```c
extern nint64 sys_trace(void);
static uint64 (*syscalls[])(void) = {
// ...省略...
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace,
};

static char *sysnames[] = {
  "", //0留空
  "fork",
  "exit",
  "wait",
  "pipe",
  "read",
  "kill",
  "exec",
  "fstat",
  "chdir",
  "dup",
  "getpid",
  "sbrk",
  "sleep",
  "uptime",
  "open",
  "write",
  "mknod",
  "unlink",
  "link",
  "mkdir",
  "close",
  "trace",
};
```

6. 最后我们通用入口syscall函数, 让它print出需要trace的系统调用
```c
void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7; // 系统调用代号存在a7寄存器内
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num](); // 返回值存在a0寄存器内
    if (p->tracemask & (1 << num)){ // 判断是否需要trace这个系统调用
      // this process traces this sys call num
      printf("%d: syscall %s -> %d/n", p->pid, sysnames[num], p->trapframe->a0)
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

7. 把trace加入到Makefile内的UPROGS变量中，
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
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_trace\ # 此项，此注释不应该出现在Makefile文件里
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
```
8. - 在xv6中调用```trace```，结果如下
     ```bash
     $ trace 32 grep hello README
     7: syscall read -> 1023
     7: syscall read -> 968
     7: syscall read -> 235
     7: syscall read -> 0
     $ trace 2147483647 grep hello README
     8: syscall trace -> 0
     8: syscall exec -> 3
     8: syscall open -> 3
     8: syscall read -> 1023
     8: syscall read -> 968
     8: syscall read -> 235
     8: syscall read -> 0
     8: syscall close -> 0
        $ grep hello README
     $ trace 2 usertests forkforkfork
     usertests starting
     10: syscall fork -> 11
     test forkforkfork: 10: syscall fork -> 12
     12: syscall fork -> 13
     13: syscall fork -> 14
     13: syscall fork -> 15
     14: syscall fork -> 16
     15: syscall fork -> 17
     13: syscall fork -> 18
     16: syscall fork -> 19
     14: syscall fork -> 20
     13: syscall fork -> 21
     16: syscall fork -> 22
     15: syscall fork -> 23
     14: syscall fork -> 24
     13: syscall fork -> 25
     15: syscall fork -> 26
     16: syscall fork -> 27
     13: syscall fork -> 28
     15: syscall fork -> 29
     14: syscall fork -> 30
     16: syscall fork -> 31
     13: syscall fork -> 32
     14: syscall fork -> 33
     15: syscall fork -> 34
     16: syscall fork -> 35
     14: syscall fork -> 36
     13: syscall fork -> 37
     15: syscall fork -> 38
     14: syscall fork -> 39
     16: syscall fork -> 40
     13: syscall fork -> 41
     14: syscall fork -> 42
     16: syscall fork -> 43
     13: syscall fork -> 44
     14: syscall fork -> 45
     15: syscall fork -> 46
     13: syscall fork -> 47
     16: syscall fork -> 48
     15: syscall fork -> 49
     14: syscall fork -> 50
     13: syscall fork -> 51
     16: syscall fork -> 52
     14: syscall fork -> 53
     13: syscall fork -> 54
     16: syscall fork -> 55
     14: syscall fork -> 56
     13: syscall fork -> 57
     15: syscall fork -> 58
     14: syscall fork -> 59
     16: syscall fork -> 60
     15: syscall fork -> 61
     14: syscall fork -> 62
     13: syscall fork -> 63
     16: syscall fork -> 64
     14: syscall fork -> 65
     15: syscall fork -> 66
     43: syscall fork -> 67
     13: syscall fork -> 68
     14: syscall fork -> 69
     17: syscall fork -> 70
     13: syscall fork -> 71
     14: syscall fork -> 72
     15: syscall fork -> -1
     16: syscall fork -> -1
     13: syscall fork -> -1
     14: syscall fork -> -1
     OK
     10: syscall fork -> 73
     ALL TESTS PASSED
     ```
   - 或者使用lab批分，结果如下
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-syscall trace
     make: 'kernel/kernel' is up to date.
     == Test trace 32 grep == trace 32 grep: OK (1.7s)
     == Test trace all grep == trace all grep: OK (0.8s)
     == Test trace nothing == trace nothing: OK (1.1s)
     == Test trace children == trace children: OK (11.9s)
     ```

### 3) 实验中遇到的问题和解决办法
- 在`kernel/syscall.c`中添加`sysnames[]`时未将第一个命名留空，导致追踪输出的系统调用名不对。由于系统调用代号在syscall.h是从1开始，而在sysnames[]中第一个是0对应，所以应该要将第一个命名留空，从第二个开始命名。这样输出的系统调用名才是对的。

- 实验进入新的分支后，未将新的Makefile中的Werror删除，导致报错。将Makefile中的Werror删除即可解决。

### 4) 实验心得
- 理解了用户系统调用的逻辑
- 掌握了对系统调用追踪的方法
- 更加深了Makefile中的Werror的印象

## 2. Sysinfo (moderate)
### 1) 实验目的
- 添加一个系统调用 sysinfo 用于收集有关正在运行系统的信息。系统调用采用一个参数：指向结构体 sysinfo 的指针（参见 kernel/sysinfo.h）。内核应该填写这个结构的字段：freemem 字段应该设置为可用内存的字节数，nproc 字段应该设置为状态不是 UNUSED 的进程数。分支仲提供了测试程序 sysinfotest；如果它打印“sysinfotest： OK”，则通过。

### 2) 实验步骤
1. 在`kernel/kalloc.c`中实现一个能获得空余内存的字节数的方法`kfreemem`:
```c
// Return the number of bytes of free memory
// should be multiple of PGSIZE
uint64
kfreemem(void) {
  struct run *r;
  uint64 free = 0;
  acquire(&kmem.lock); // 上锁, 防止数据竞态
  r = kmem.freelist;
  while (r) {
    free += PGSIZE; // PGSIZE=4096，每一页固定4096字节
    r = r->next; // 遍历单链表
  }
  release(&kmem.lock);
  return free;
}
```

2. 在`kernel/proc.c`中实现一个能获得分配出去的进程数量的方法`count_free_proc`:
```c
// Count how many processes are not in the state of UNUSED
#include "sysinfo.h"

uint64
count_free_proc(void) {
  struct proc *p;
  uint64 count = 0;
  for(p = proc; p < &proc[NPROC]; p++) {
    // 此处不一定需要加锁, 因为该函数是只读不写
    // 但proc.c里其他类似的遍历时都加了锁, 那我们也加上
    acquire(&p->lock);
    if(p->state != UNUSED) {
      count += 1;
    }
    release(&p->lock);
  }
  return count;
}
```

3. 在`kernel/sysproc.c`中实现`sysinfo`系统调用内核函数`sys_sysinfo`:
```c
// collect system info
uint64
sys_sysinfo(void) {
  struct proc *my_proc = myproc();
  uint64 p;
  if(argaddr(0, &p) < 0) // 获取用户提供的buffer地址
    return -1;
  // construct in kernel first 在内核态先构造出这个sysinfo struct
  struct sysinfo s;
  s.freemem = kfreemem();
  s.nproc = count_free_proc();
  // copy to user space // 把这个struct复制到用户态地址里去
  if(copyout(my_proc->pagetable, p, (char *)&s, sizeof(s)) < 0)
    return -1;
  return 0;
}
```

4. 将测试文件`sysinfotest`加入到Makefile内的`UPROGS`变量中
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
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_sysinfotest\ # 此项，此注释不应该出现在Makefile文件里
	$U/_trace\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
```

5. 在user/user.h理提供一个用户接口。
```c
// user/user.h
struct stat;
struct rtcdate;
struct sysinfo; // sysinfo结构体声明
// ...省略...
int uptime(void);
int trace(int); // 新系统调用trace的函数原型签名
int sysinfo(struct sysinfo *); // 新系统调用sysinfo的函数原型签名
```

6. 在user/usys.pl中为这个函数签名加入entry
```pl
# ...省略...
entry("uptime");
entry("trace");
entry("sysinfo")
```

7. 为这个sys_sysinfo提供一个代号
- kernel/syscall.h
  ```c
  // ......
  #define SYS_close  21
  #define SYS_trace  22
  #define SYS_sysinfo 23
  ```
- kernel/syscall.c
  ```c
  extern uint64 sys_sysinfo(void);

  static uint64 (*syscalls[])(void) = {
  // ......
  [SYS_close]   sys_close,
  [SYS_trace]   sys_trace,
  [sys_sysinfo] sys_sysinfo,
  }


  ```

7. - 在xv6中调用`sysinfotest`，结果如下：
     ```bash
     xv6 kernel is booting

     hart 1 starting
     hart 2 starting
     init: starting sh
     $ sysinfotest
     sysinfotest: start
     sysinfotest: OK
     ```
   - 或者使用lab批分，结果如下
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-syscall sysinfo
     make: 'kernel/kernel' is up to date.
     == Test sysinfotest == sysinfotest: OK (2.5s)
     ```


### 3) 实验中遇到的问题和解决办法
- 在用户端提供sysinfo的用户接口时，没有把函数签名在user/usys.pl中加入entry。在user/usys.pl中加入entry即可。

- 在`kernel/sysproc.c`中实现`sysinfo`系统调用内核函数`sys_sysinfo`时，需要正确调用`copyout`:
```c
// 从内核态拷贝到用户态
// 拷贝len字节数的数据, 从src指向的内核地址开始, 到由pagetable下的dstv用户地址
// 成功则返回 0, 失败返回 -1
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
```

### 4) 实验心得
- 再次理解了用户系统调用的逻辑
- 掌握了查看系统信息的方法
- 了解了copyout的调用