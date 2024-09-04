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
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
  for(int i=0; i<NCPU; i++)
  	initlock(&kmem.lock[i], "kmem");
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

  push_off();
  int current_cpu=cpuid();
  acquire(&kmem.lock[current_cpu]);
  r->next = kmem.freelist[current_cpu];
  kmem.freelist[current_cpu] = r;
  release(&kmem.lock[current_cpu]);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int current_cpu = cpuid();
  acquire(&kmem.lock[current_cpu]);
  r = kmem.freelist[current_cpu];
  if(r)
    kmem.freelist[current_cpu] = r->next;
  else
	for(int i = 0; i<NCPU; i++)
	{
		if(i == current_cpu)// 不能重复获得锁。
			continue;
		release(&kmem.lock[current_cpu]);
		acquire(&kmem.lock[i]);
		if(!kmem.freelist[i]){
			release(&kmem.lock[i]);
			acquire(&kmem.lock[current_cpu]);
			continue;
		}
		r = kmem.freelist[i];
		kmem.freelist[i]= r->next;
		release(&kmem.lock[i]);
		acquire(&kmem.lock[current_cpu]);
        if(r)break;// 拿到可以用的就退出。
	}
  release(&kmem.lock[current_cpu]);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
