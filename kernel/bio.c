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

#define bkts 13
struct bucket{
	struct buf head;
	struct spinlock lock;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bucket bucket[bkts];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i=0; i<bkts ; i++)
	  initlock(&bcache.bucket[i].lock,"bcache");

  // init heads of buckets
  for(int i=0; i<bkts; i++){
	  bcache.bucket[i].head.next = &bcache.bucket[i].head;
	  bcache.bucket[i].head.prev = &bcache.bucket[i].head;
  }
  // linked in bucket[0] firstly
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //printf("dev:%d blockno:%d\n",dev,blockno);
  struct buf *b;

  //acquire(&bcache.lock);

  // Is the block with dev and blockno already cached?
  acquire(&bcache.bucket[blockno%bkts].lock);
  for(b = bcache.bucket[blockno%bkts].head.next; b != &bcache.bucket[blockno%bkts].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
	  release(&bcache.bucket[blockno%bkts].lock);
      //release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[blockno%bkts].lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  // firstly should check the cached buffer , because the bufs may have the same blockno
  acquire(&bcache.bucket[blockno%bkts].lock);
  for(b = bcache.bucket[blockno%bkts].head.next; b != &bcache.bucket[blockno%bkts].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
	  release(&bcache.bucket[blockno%bkts].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[blockno%bkts].lock);

  for(int i=0; i<bkts; i++){
	  acquire(&bcache.bucket[i].lock);
	  b = bcache.bucket[i].head.next;
	  while(b != &bcache.bucket[i].head){
		if(b->refcnt == 0) {
			b->dev = dev;
			b->blockno = blockno;
			b->valid = 0;
			b->refcnt = 1;
			if( i!=blockno%bkts ){
				b->next->prev = b->prev;
				b->prev->next = b->next;
				// to avoid deadlock
				release(&bcache.bucket[i].lock);

				acquire(&bcache.bucket[blockno%bkts].lock);
				b->prev = &bcache.bucket[blockno%bkts].head;
				b->next = bcache.bucket[blockno%bkts].head.next;
				bcache.bucket[blockno%bkts].head.next->prev = b;
				bcache.bucket[blockno%bkts].head.next = b;
				release(&bcache.bucket[blockno%bkts].lock);
			}else
				release(&bcache.bucket[i].lock);
			release(&bcache.lock);
			acquiresleep(&b->lock);
			return b;
      	}
		b = b->next;
	  }
	  release(&bcache.bucket[i].lock);
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  //acquire(&bcache.lock);
  acquire(&bcache.bucket[b->blockno%bkts].lock);
  b->refcnt--;
  /*
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[b->blockno%bkts].head.next;
    b->prev = &bcache.bucket[b->blockno%bkts].head;
    bcache.bucket[b->blockno%bkts].head.next->prev = b;
    bcache.bucket[b->blockno%bkts].head.next = b;
  }
  */
  release(&bcache.bucket[b->blockno%bkts].lock);
  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucket[b->blockno%bkts].lock);
  b->refcnt++;
  release(&bcache.bucket[b->blockno%bkts].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket[b->blockno%bkts].lock);
  b->refcnt--;
  release(&bcache.bucket[b->blockno%bkts].lock);
}
