#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


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

#ifdef LAB_PGTBL
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
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
