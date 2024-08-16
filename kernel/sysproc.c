#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
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
  if(n < 0)
    n = 0;
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

  //backtrace();
  return 0;
}

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

uint64
sys_sigalarm(void)
{
  int interval;
  argint(0, &interval);

  uint64 handler;
  argaddr(1, &handler);

  struct proc *p = myproc();
  if (!p) return -1;

  p->interval = interval;
  p->handler = handler;

  return 0;
}

uint64
sys_sigreturn(void)
{
  
  struct proc *p = myproc();
  if (!p) return -1;

  p->handlerret = 1;

  p->trapframe->ra = p->saved_regs.ra;
  p->trapframe->sp = p->saved_regs.sp;
  p->trapframe->s0 = p->saved_regs.s0;
  p->trapframe->s1 = p->saved_regs.s1;
  p->trapframe->s2 = p->saved_regs.s2;
  p->trapframe->s3 = p->saved_regs.s3;
  p->trapframe->s4 = p->saved_regs.s4;
  p->trapframe->s5 = p->saved_regs.s5;
  p->trapframe->s6 = p->saved_regs.s6;
  p->trapframe->s7 = p->saved_regs.s7;
  p->trapframe->s8 = p->saved_regs.s8;
  p->trapframe->s9 = p->saved_regs.s9;
  p->trapframe->s10 = p->saved_regs.s10;
  p->trapframe->s11 = p->saved_regs.s11;

  p->trapframe->a0 = p->saved_a0;
  p->trapframe->a1 = p->saved_a1;
  p->trapframe->epc = p->saved_epc;

  return p->saved_a0;
}

