#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void){
	uint64 addr;
	off_t off;
	size_t len;
	int prot,flags,fd;
	argaddr(0,&addr);
	argaddr(1,&len);
	argint(2,&prot);
	argint(3,&flags);
	argint(4,&fd);
	argaddr(5,(uint64*)&off);

		
	struct proc *p = myproc();

	// read-only file can't mmaped to writable mem
	if(p->ofile[fd]->readable 
			&& !p->ofile[fd]->writable
		   	&& (prot & PROT_WRITE) 
			&& (flags & MAP_SHARED))
		return -1;

	struct vma *v = p->vmas;
	for(int i=0; i<NVMA; i++){
		if(v[i].state == VMA_UNUSED){
			// printf("mmap:%d\n",i);
			v[i].start = p->sz;
			v[i].sz = len;
			v[i].flags = flags;
			v[i].prot = prot;
			v[i].f = p->ofile[fd];
			v[i].off = off;
			filedup(v[i].f);
			v[i].state = VMA_USED;
			p->sz += len;
			return v[i].start;
		}
	}
	
	return (uint64)-1;
}

uint64
sys_munmap(void){
	uint64 addr;
	size_t len;
	argaddr(0,&addr);
	argaddr(1,&len);

	struct proc* p = myproc();
	struct vma* v = p->vmas;

	for(int i=0; i<NVMA; i++){
		if(v[i].state == VMA_USED && (v[i].start < addr +len || v[i].start + v[i].sz > addr)){
			int start = v[i].start>addr ? v[i].start:addr;
			int end = v[i].start+v[i].sz < addr + len ? v[i].start+v[i].sz : addr + len;
			// check the page if it has modified
			if((v[i].prot & PROT_WRITE) && (v[i].flags & MAP_SHARED)){
				int temp = start;
				while(temp < end){
					if(*(walk(p->pagetable,temp,0)) & PTE_D)
						filewrite(v[i].f,temp,PGSIZE);
					temp += PGSIZE;
				}
			}
			if(*walk(p->pagetable,start,0) & PTE_V)
				uvmunmap(p->pagetable,start,(end-start)/PGSIZE,1);
			p->sz -= (end-start);
			if(start > v[i].start) end = start;
			else {v[i].start = end; end = start + v[i].sz;}// the vma is full covered or only its front part is covered
			v[i].sz = end-v[i].start;
			// check if vma removes all of its pages
			if(v[i].sz == 0)
			{
				fileclose(v[i].f);
				v[i].state = VMA_UNUSED;
				v[i].off =  v[i].prot = v[i].start = v[i].flags = 0;
			}
			return 0;
		}
	}

	return 0;
}
