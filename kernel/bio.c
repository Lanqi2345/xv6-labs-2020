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

extern uint ticks;

struct bucket{
  struct spinlock lock;
  struct buf head;
};

struct {
  //串行化缓存未命中后的淘汰过程
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bucket buckets[NBUCKET];
} bcache;

//哈希函数
static int
bhash(uint blockno)
{
  return blockno % NBUCKET;
}

//插入桶头
static void
binsert(struct bucket *bucket, struct buf *b)
{
  b->next = bucket->head.next;
  b->prev = &bucket->head;

  bucket->head.next->prev = b;
  bucket->head.next = b;
}

//从桶里删除
static void
bremove(struct buf *b)
{
  b->prev->next = b->next;
  b->next->prev = b->prev;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.buckets[i].lock, "bcache.bucket");
    //空桶
    //head.next == &head
    //head.prev == &head
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  
  for(int i=0;i<NBUF;i++)
  {
    b=&bcache.buf[i];

    initsleeplock(&b->lock, "buffer");

    b->valid = 0;
    b->disk = 0;
    b->refcnt = 0;
    b->timestamp = 0;

    //将缓存块均匀分布到各个桶
    int h = i % NBUCKET;
    b->bucket = h;

    binsert(&bcache.buckets[h], b);

  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *victim;
  int h;

  h=bhash(blockno);

  //目标桶里查找
  acquire(&bcache.buckets[h].lock);

  // Is the block already cached?
  //循环双向链表
  for(b = bcache.buckets[h].head.next; b != &bcache.buckets[h].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.buckets[h].lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //保证同一时刻只有一个线程执行缓存淘汰
  acquire(&bcache.lock);

  //再次检查，防止第一次查找失败到获取 bcache.lock 之间，其他线程已经把这个磁盘块加入缓存

  acquire(&bcache.buckets[h].lock);

  for (b = bcache.buckets[h].head.next;b != &bcache.buckets[h].head;b = b->next) {
    if (b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[h].lock);
      release(&bcache.lock);

      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.buckets[h].lock);

  //选择 refcnt == 0 且 timestamp 最小的缓存块。
  
retry_victim:

  victim = 0;
  uint oldest = (uint)-1;

  for (int i = 0; i < NBUF; i++)
  {
    b = &bcache.buf[i];

    int bh = b->bucket;

    acquire(&bcache.buckets[bh].lock);

    if (b->refcnt == 0 &&(victim == 0 || b->timestamp < oldest)) {
      victim = b;
      oldest = b->timestamp;
    }

    release(&bcache.buckets[bh].lock);
  }

  //都有进程
  if (victim == 0)
  {
    release(&bcache.lock);
    panic("bget: no buffers");
  }

  //扫描期间普通的缓存命中仍可增加 victim->refcnt，重新获取 victim 所在桶的锁，并再次检查

  int oldh = victim->bucket;

  acquire(&bcache.buckets[oldh].lock);

  if (victim->refcnt != 0) {
    release(&bcache.buckets[oldh].lock);
    goto retry_victim;
  }

  if (oldh == h)//旧块和新块映射到同一个桶。
  {
    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->refcnt = 1;
    victim->timestamp = ticks;

    release(&bcache.buckets[oldh].lock);
  }
  else//victim要从旧桶移动到新桶
  {
    //当前持有旧桶锁。由于 bcache.lock 保证只有一个淘汰线程，而普通命中只会获取一把桶锁，所以这里获取新桶锁不会与另一个淘汰线程形成锁环。

    acquire(&bcache.buckets[h].lock);

    bremove(victim);

    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->refcnt = 1;
    victim->timestamp = ticks;
    victim->bucket = h;

    binsert(&bcache.buckets[h], victim);

    release(&bcache.buckets[h].lock);
    release(&bcache.buckets[oldh].lock);
  }


  release(&bcache.lock);

  //缓冲区缓存为每个缓冲区使用一把睡眠锁，确保任意时刻只有一条线程使用该缓冲区
  acquiresleep(&victim->lock);
  return victim;

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

  int h =b->bucket;

  acquire(&bcache.buckets[h].lock);

  b->refcnt--;

  if (b->refcnt == 0) {
    b->timestamp=ticks;
  }
  
  release(&bcache.buckets[h].lock);
}

void
bpin(struct buf *b)
{
  int h =b->bucket;

  acquire(&bcache.buckets[h].lock);
  b->refcnt++;
  release(&bcache.buckets[h].lock);

}

void
bunpin(struct buf *b)
{
  int h =b->bucket;

  acquire(&bcache.buckets[h].lock);

  b->refcnt--;

  if (b->refcnt == 0) {
    b->timestamp=ticks;
  }
  
  release(&bcache.buckets[h].lock);
}


