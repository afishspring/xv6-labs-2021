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

#define NBUK 13

struct bucket{
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  struct bucket buckets[NBUK];
} bcache;

int
hash(uint dev, uint blockno)
{
  return (dev*blockno)%NBUK;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  struct buf *prev_b;
  for(int i=0;i < NBUK;i++){
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    bcache.buckets[i].head.next=(void*)0;
    if(i==0){
      prev_b= &bcache.buckets[i].head;

      for(b=bcache.buf; b<bcache.buf+NBUF;b++){
        if(b==bcache.buf+NBUF-1){
          b->next=(void*)0;
        }
        prev_b->next=b;
        b->prev=prev_b;
        b->timestamp=ticks;
        initsleeplock(&b->lock,"buffer");
        prev_b=b;
      }
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int buk_id=hash(dev, blockno);

  acquire(&bcache.buckets[buk_id].lock);

  // Is the block already cached?
  for(b = bcache.buckets[buk_id].head.next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[buk_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.buckets[buk_id].lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  int max_timestamp = 0;
  int lru_buk_id = -1;
  struct buf*lru_buk = (void*)0;
  struct buf*prev_lru_buk = (void*)0;
  struct buf*curr_b = (void*)0;
  int flag=0;
  for(int i=0;i<NBUK;i++){
    acquire(&bcache.buckets[i].lock);
    for(curr_b = &bcache.buckets[i].head;curr_b->next;curr_b=curr_b->next){
      if(curr_b->next->refcnt == 0 && curr_b->next->timestamp >= max_timestamp){
        max_timestamp = curr_b->next->timestamp;
        prev_lru_buk = curr_b;
        flag=1;
      }
    }
    if(flag){
      if(lru_buk_id!=-1)
        release(&bcache.buckets[lru_buk_id].lock);
      lru_buk_id=i;
    }
    else
      release(&bcache.buckets[i].lock);
    flag=0;
  }

  lru_buk=prev_lru_buk->next;

  if(lru_buk){
    prev_lru_buk->next = prev_lru_buk->next->next;
    release(&bcache.buckets[lru_buk_id].lock);
  }

  acquire(&bcache.lock);
  acquire(&bcache.buckets[buk_id].lock);

  if(lru_buk){
    lru_buk->next = bcache.buckets[buk_id].head.next;
    bcache.buckets[buk_id].head.next = lru_buk;
  }

  for(b= bcache.buckets[buk_id].head.next; b; b=b->next){ 
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[buk_id].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;   
    }
  }

  if(lru_buk == 0)
    panic("bget: no buffers");

  lru_buk->dev = dev;
  lru_buk->blockno = blockno;
  lru_buk->valid = 0;
  lru_buk->refcnt = 1;

  release(&bcache.buckets[buk_id].lock);
  release(&bcache.lock);
  acquiresleep(&lru_buk->lock);
  return lru_buk;
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int buk_id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[buk_id].lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->timestamp = ticks;
  release(&bcache.buckets[buk_id].lock);
}

void
bpin(struct buf *b) {
  int buk_id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[buk_id].lock);
  b->refcnt++;
  release(&bcache.buckets[buk_id].lock);
}

void
bunpin(struct buf *b) {
  int buk_id = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[buk_id].lock);
  b->refcnt--;
  release(&bcache.buckets[buk_id].lock);
}


