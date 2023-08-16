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
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i=0;i < NCPU; i++){
    initlock(&kmem[i].lock, "kmem");
    if(i==0){
      freerange(end, (void*)PHYSTOP);
    }
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu_id=cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu_id=cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r){
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }
  else{
    release(&kmem[cpu_id].lock);

    for(int i=0;i < NCPU;i++){
      if(i!=cpu_id){
        acquire(&kmem[i].lock);

        if(i==NCPU-1&&kmem[i].freelist==0){
          release(&kmem[i].lock);
          return (void*)0;
        }

        if(kmem[i].freelist){
          struct run *steal_r = kmem[i].freelist;
          kmem[i].freelist = steal_r->next;
          release(&kmem[i].lock);
          memset((char*)steal_r, 5, PGSIZE); // fill with junk
          return ( void*)steal_r;
        }

        release(&kmem[i].lock);
      }
    }
  }
  return (void*)0;
}
