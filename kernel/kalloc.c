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

void setrc(uint64 pa, int new);
void incrc(uint64 pa);
void decrc(uint64 pa);
int getid(uint64 pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int refcounts[((PHYSTOP) - (KERNBASE)) / PGSIZE];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int flag = 1;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  flag = 0;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
  for(int i = PGROUNDUP((uint64)end)/PGSIZE; i < (PHYSTOP - KERNBASE) / PGSIZE; i++)
	if (refcounts[i] != 0)
	  printf("incorrect init of refcounts\n");
}


// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)


void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (!flag) 
    refcounts[getid((uint64)pa)] -= 1;

  int rc = refcounts[getid((uint64)pa)];
  if (rc < 0) printf("incorrect!!!\n");
  if (rc > 0) return;
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
  
  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    setrc((uint64)r, 1);
  }
  return (void*)r;
}

 
int
getid(uint64 pa) 
{
  //printf("index %d\n", ((pa) - (KERNBASE)) / PGSIZE);
  int id =  ((pa) - KERNBASE) / PGSIZE;
  if (id >= 32768 || id < 0)
	printf("incorrect index! -> %d\n", id);
  if (pa % PGSIZE != 0) printf("unaligned!!!\n");
  return id;
}

void 
incrc(uint64 pa) 
{
  refcounts[getid(pa)] += 1;
}

void
decrc(uint64 pa)
{
  refcounts[getid(pa)] -= 1;
}

void 
setrc(uint64 pa, int new) 
{
  refcounts[getid(pa)] = new;
}
