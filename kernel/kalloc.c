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

//计数数组
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

//
struct {
  struct spinlock lock;
  int count[PHYSTOP / PGSIZE];
} refcnt;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    refcnt.count[(uint64)p / PGSIZE] = 1;//随后减1
    kfree(p);
  }
    
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&refcnt.lock);

  refcnt.count[(uint64)pa / PGSIZE]--;
  int refs = refcnt.count[(uint64)pa / PGSIZE];

  release(&refcnt.lock);

  if(refs > 0)//引用计数仍大于0，直接返回
    return;

  if(refs < 0)
    panic("kfree: negative reference");

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

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk

    acquire(&refcnt.lock);
    refcnt.count[(uint64)r / PGSIZE] = 1;
    release(&refcnt.lock);
  }
   
  return (void*)r;
}

//增加引用计数的函数
void kaddref(uint64 pa)
{
  if(pa >= PHYSTOP || pa % PGSIZE != 0)
    panic("kaddref");

  acquire(&refcnt.lock);
  refcnt.count[pa / PGSIZE]++;
  release(&refcnt.lock);
}