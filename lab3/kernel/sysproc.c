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
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 addr;//起始虚拟地址
  int len;//长度
  int bitmask;//要传递给用户态的目标参数
  if(argaddr(0, &addr) < 0)return -1;//通过以下三个函数来接收参数
  if(argint(1, &len) < 0)return -1;
  if(argint(2, &bitmask) < 0)return -1;

  if(len > 32 || len < 0){
    return -1;//It's okay to set an upper limit on the number of pages that can be scanned.
  }

  struct proc *p = myproc();
  int ans = 0;
  for(int i=0;i<len;i++){
    int va = addr + i * PGSIZE;//计算虚拟地址的大小
    int abit = vm_pgaccess(p->pagetable, va);
    //上述函数检测va对应的pa的页属性的PTE_A有没有被修改过，修改过了返回1，否则返回0
    ans = ans | abit << i;
  }

  if(copyout(p->pagetable, bitmask, (char*)&ans, sizeof(ans)) < 0)
    return -1;

  return 0;
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
