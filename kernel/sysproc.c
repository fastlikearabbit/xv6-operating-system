#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

#ifdef LAB_PGTBL

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
int
sys_pgaccess(void)
{
  // location to hold access bits
  unsigned int abits = 0;
  // number of pages to check
  int npg;

  // starting virtual address
  uint64 vastart;

  // user space buffer address
  uint64 ubuf;

  // get args from user space
  argaddr(0, &vastart);
  argint(1, &npg);
  argaddr(2, &ubuf);

  struct proc *p = myproc();

  // process and fill in abits
  for (int i = 0; i < MIN(npg, 32); i++) {
    pte_t *pte = walk(p->pagetable, vastart, 0);
    if (pte == 0) return -1;
    if ((*pte & PTE_V) && (*pte & PTE_A)) {
      // set ith bit of abits
      abits = (1 << i) | abits;

      // unset a bit of pte
      *pte = *pte & ~PTE_A;
    }
    vastart += PGSIZE;
  }

  // copy bits from kernel to user
  copyout(p->pagetable, ubuf, (char *)&abits, sizeof(abits));
  
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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
