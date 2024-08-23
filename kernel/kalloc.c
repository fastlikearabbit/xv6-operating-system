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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem freelists[NPROC];

void
kinit()
{
  for (int i = 0; i < NPROC; i++) {
    initlock(&freelists[i].lock, "kmem");
    uint64 memchunk = (PHYSTOP - (uint64)end) / NPROC;
    freerange(end + i * memchunk, end + (i + 1) * memchunk);
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // get cpu number
  push_off();
  int cid = cpuid();
  pop_off();

  acquire(&freelists[cid].lock);
  r->next = freelists[cid].freelist;
  freelists[cid].freelist = r;
  release(&freelists[cid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  // was a free spot found?
  int found = 0;

  // get cpu number
  push_off();
  int cid = cpuid();
  pop_off();

  struct run *r = 0;

  // search in current free lists
  acquire(&freelists[cid].lock);
  r = freelists[cid].freelist;
  if (r) {
    freelists[cid].freelist = r->next;
    found = 1;
  }
  release(&freelists[cid].lock);

  if (found) goto end;

  // steal from other cpu's freelist
  for (int i = cid + 1; i != cid; i = (i + 1) % NPROC) {
    acquire(&freelists[i].lock);
    r = freelists[i].freelist;
    if (r) {
        freelists[i].freelist = r->next;
        release(&freelists[i].lock);
        break;
    }
    release(&freelists[i].lock);
  }

 end:
    if(r)
      memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
