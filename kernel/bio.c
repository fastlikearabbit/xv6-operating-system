// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf *table[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  int i;
  // Create hash table of buffers
  for (i = 0, b = bcache.buf; b < bcache.buf + NBUF; b++, i++) {
    uint hash = b->blockno % NBUCKET;
    b->next = bcache.table[hash];
    bcache.table[hash] = b;
    initsleeplock(&b->lock, "buffer");
  }

    for (int i = 0; i < NBUCKET; i++) {
      struct buf *b = bcache.table[i];
      while (b) {
      if (b->blockno % NBUCKET != i) printf("dont match init\n");
      for (int j = i + 1; j < NBUCKET; j++) {
        struct buf *bb = bcache.table[j];
        while (bb) {
          if (b == bb) panic("duplicate in init\n");
          bb = bb->next;
        }
      }
      b = b->next;}
    }

  for (i = 0; i < NBUCKET; i++) {
    struct buf *b = bcache.table[i];
    printf("bucket %d : --> ", i);
    while (b) {
      printf("_x_ --> ");
      b = b->next;
    }
    printf(" 0 \n");
  }

  for (int i = 0; i < NBUCKET; i++)
    initlock(&bcache.bucket_lock[i], "bcache.bucket");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
    for (int i = 0; i < NBUCKET; i++) {
      struct buf *b = bcache.table[i];
      //printf("bucket %d: ", i);
      while (b) {
      //printf("_%d_ ---> ", b->blockno % NBUCKET);
      if (b->blockno % NBUCKET != i) printf("dont match\n");
      for (int j = i + 1; j < NBUCKET; j++) {
        struct buf *bb = bcache.table[j];
        while (bb) {
          if (b != 0 && b == bb) {
          printf("hash1 %d, hash2 %d\n", i, j);
          panic("duplicate\n");
          }
          bb = bb->next;
        }
      }
      b = b->next;}
      //printf("\n");
    }
  b = bcache.table[hash];
  // Is the block already cached?
  while (b) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;

      release(&bcache.bucket_lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }
  release(&bcache.bucket_lock[hash]);

  // Not cached.
  acquire(&bcache.lock);
  for (int i = 0; i < NBUCKET; i++) {
    acquire(&bcache.bucket_lock[i]);
    b = bcache.table[i];
    struct buf *prev = 0;
    while (b) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
		b->refcnt = 1;
        
        // Move block to corresponding bucket
        if (i != hash) {
          if (!prev)
            bcache.table[i] = b->next;
          else
            prev->next = b->next;
          acquire(&bcache.bucket_lock[hash]);  
          b->next = bcache.table[hash];
          bcache.table[hash] = b;
          release(&bcache.bucket_lock[hash]);
        } 

        release(&bcache.lock);
        release(&bcache.bucket_lock[i]);
        acquiresleep(&b->lock);

        return b;
      }
      prev = b;
      b = b->next;
    }
  release(&bcache.bucket_lock[i]);
    for (int i = 0; i < NBUCKET; i++) {
      struct buf *b = bcache.table[i];
      while (b) {
      for (int j = i + 1; j < NBUCKET; j++) {
        struct buf *bb = bcache.table[j];
        while (bb) {
          if (b == bb) panic("duplicate\n");
          bb = bb->next;
        }
      }
      b = b->next;}
    }
  }
  for (int i = 0; i < NBUCKET; i++) {
    struct buf *b = bcache.table[i];
    printf("bucket %d : --> ", i);
    while (b) {
      printf("_%d_ --> ", b->refcnt);
      b = b->next;
    }
    printf(" 0 \n");
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
  b->refcnt--;
  release(&bcache.bucket_lock[hash]);
  
}

void
bpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
  b->refcnt++;
  release(&bcache.bucket_lock[hash]);
}

void
bunpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
  b->refcnt--;
  release(&bcache.bucket_lock[hash]);
}


