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

void binit(void) {
  struct buf *b;
  initlock(&bcache.lock, "bcache");

  // Initialize hash table and locks for each bucket
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket_lock[i], "bcache.bucket");
    bcache.table[i] = 0;
  }

  // Distribute buffers into buckets (initial distribution isn't critical)
  int bucket_index = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.table[bucket_index];
    bcache.table[bucket_index] = b;
    bucket_index = (bucket_index + 1) % NBUCKET;
  }
}

static struct buf* bget(uint dev, uint blockno) {
  struct buf *b;
  uint hash = blockno % NBUCKET;

  // Check if the block is cached in the target bucket
  acquire(&bcache.bucket_lock[hash]);
  for (b = bcache.table[hash]; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket_lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[hash]);

  // Not cached; evict a buffer from another bucket
  acquire(&bcache.lock);

  for (int i = 0; i < NBUCKET; i++) {
    int current_bucket = (hash + i) % NBUCKET;
    acquire(&bcache.bucket_lock[current_bucket]);
    for (b = bcache.table[current_bucket]; b != 0; b = b->next) {
      if (b->refcnt == 0) {
        // Remove buffer from current bucket
        struct buf **prev = &bcache.table[current_bucket];
        while (*prev != b) {
          prev = &(*prev)->next;
        }
        *prev = b->next;
        release(&bcache.bucket_lock[current_bucket]);

        // Initialize and lock the buffer before inserting into target bucket
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        acquiresleep(&b->lock);

        // Insert into target bucket
        acquire(&bcache.bucket_lock[hash]);
        b->next = bcache.table[hash];
        bcache.table[hash] = b;
        release(&bcache.bucket_lock[hash]);

        release(&bcache.lock);
        return b;
      }
    }
    release(&bcache.bucket_lock[current_bucket]);
  }
  release(&bcache.lock);
  panic("bget: no buffers");
}

struct buf* bread(uint dev, uint blockno) {
  struct buf *b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  uint hash = b->blockno % NBUCKET;
  releasesleep(&b->lock);

  acquire(&bcache.bucket_lock[hash]);
  b->refcnt--;
  release(&bcache.bucket_lock[hash]);
}

void bpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
  b->refcnt++;
  release(&bcache.bucket_lock[hash]);
}

void bunpin(struct buf *b) {
  uint hash = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[hash]);
  b->refcnt--;
  release(&bcache.bucket_lock[hash]);
}