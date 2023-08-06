// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int PG_total;
} kmem;

int PG_cnt[PHYSTOP/PGSIZE];//采用引用计数对每一个PG的引用数量计数
//这里的PHYSTOP/PGSIZE肯定是大于kmem.PG_total的，
//因为并不是所有内存都被划分为了页表（猜测）
int
page_index(uint64 pa)
{
  pa = PGROUNDDOWN(pa);
  int ans = (pa - (uint64)end)/PGSIZE;
  if(ans < 0 || ans >= kmem.PG_total){
    panic("page_index() fail");
  }
  return ans;
}

void 
incr(void *pa)
{
  int index = page_index((uint64)pa);
  PG_cnt[index]++;
}

void 
desc(void *pa)
{
  int index = page_index((uint64)pa);
  PG_cnt[index]--;
}

void
count_PG(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kmem.PG_total++;
  }
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  count_PG(end, (void*)PHYSTOP);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int index = page_index((uint64)pa);
  if(PG_cnt[index] > 1){//只有当他的引用计数为0的时候才释放内存
    desc(pa);//如果引用计数大于1说明有至少两个进程对该页表引用了
    //那么只要将其减一就表示去除一个引用
    return;
  }
  if(PG_cnt[index] == 1){
    desc(pa);
  }
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    incr(r);//申请内存时引用计数加一
  }
  return (void*)r;
}
