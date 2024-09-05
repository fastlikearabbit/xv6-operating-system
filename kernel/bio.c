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
          // Remove entry from current bucket
          if (!prev)
            bcache.table[i] = b->next;
          else
            prev->next = b->next;
          // Release the bucket lock to avoid deadlock when two threads
          // try to do mutual swapping: thread A moves from bucket i -> j
          // and thread B moves j -> i.
          // This would cause a deadlock if both threads would acquire
          // both bucket_lock[i] and bucket_lock[j]
          // Example:
          // THREAD A                        THREAD B
          // acquire(buck[i])                acquire(buck[j])
          // acquire(buck[j])  <-deadlock->  acquire(buck[i])
          // So either thread must first release the bucket_lock after removing an
          // element from it, then reaquire it again.
          release(&bcache.bucket_lock[i]);

          acquire(&bcache.bucket_lock[hash]);
          acquire(&bcache.bucket_lock[i]);
          struct buf *c = bcache.table[hash];
          if (!c) {
              bcache.table[hash] = b;
          } else {
              while (c->next)
                c = c->next;
              c->next = b;
          }
          b->next = 0;
          release(&bcache.bucket_lock[hash]);
        }
        release(&bcache.bucket_lock[i]);
        release(&bcache.lock);
        acquiresleep(&b->lock);
        // TODO: after releasing the bucket_lock[hash], where b was moved
        // some other thread searching for the same blockno in the cache
        // might find this block and start using it
        // Then here we return the block with the valid bit set to 1
        // so the bread doesn't look in the disk to read the new file content
        // and instead uses the old one.
        if (b->refcnt != 1) {
            printf("who tf uses this? refcnt: %d, valid: %d\n", b->refcnt, b->valid);
        }
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        return b;
      }
      prev = b;
      b = b->next;
    }
    release(&bcache.bucket_lock[i]);
  }

  panic("bget: no free blocks");
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

  uint hash = b->blockno % NBUCKET;
  releasesleep(&b->lock);

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
