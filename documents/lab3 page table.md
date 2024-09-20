# lab3 page table

## Speed up system calls ([easy](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

proc.c看到这个：

```c
// Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
```

所以要把用kalloc分配一页给usyscall ,但是不知放在哪里，所以在proc结构体中加了usyscall结构体的指针。并模仿写了：

```c
// Allocate a usyscall page.
	if((p->usyscall = (struct usyscall *)kalloc()) == 0){  
    freeproc(p);
    release(&p->lock);
    return 0;
  }
p->usyscall->pid = p->pid;
```

进入freeproc(p):

```c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}
```

释放进程需要将usyscall页面释放,加上usyscall页面的释放：

```c
if(p->usyscall)
	kfree((void*)p->usyscall);
  p->usyscall  = 0;
```

进入proc_freepagetable，添加取消usyscall的映射：

```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 0);
  uvmfree(pagetable, sz);
}
```

穿建页表：

```c
// An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
```

进入proc_pagetable函数，这个函数创建页表，并把trampoline和trapframe段映射到页表，模仿trapframe的映射，写下usyscall的映射：

```c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }
  
  // map the usyscall page below the trapframe page
  if(mappages(pagetable, USYSCALL,PGSIZE,
			  (uint64)(p->usyscall),PTE_R | PTE_U) < 0){ // 用户态也需要访问
	uvmunmap(pagetable, TRAMPOLINE, 2, 0);// 2代表几个页
      uvmfree(pagetable, 0);
	return 0;
  }

  return pagetable;
}
```

![image-20240804161256265](<lab3 page table/image-20240804161256265-17227591778131.png>)

Which other xv6 system call(s) could be made faster using this shared page? Explain how.

除了getpid(),还有fstate,将state结构体放到共享页面，就可以不用进入内核就可以读取这个结构体。

## Print a page table ([easy](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

查看freewalk：

```c
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
```

可以看到，使用了递归，这个代码比较好，因为可以适应无论多少级的页表，递归能够停止的原因，页的地址是0x1000的倍数。而需要实现的打印页表的函数，需要打印深度，需要加一个深度的参数，但是参数只有一个，所以选择了三层的循环嵌套。

```c
// Recursively print valid page-table pages
void
vmprint(pagetable_t pagetable){
	printf("page table %p\n",pagetable);// print p->pagetable.

	for(int i = 0; i < 512 ; i ++){
		pte_t pte = pagetable[i];
		if(pte & PTE_V){// valid 
			pagetable_t child = (pagetable_t)PTE2PA(pte);//extract pa from pte
			printf(" ..%d: pte %p pa %p\n",i,pte,child);
			for(int j=0; j< 512 ; j++){
				pte_t childpte = child[j];
				if(childpte & PTE_V){
					pagetable_t chichi = (pagetable_t)PTE2PA(childpte);
					printf(" .. ..%d: pte %p pa %p\n",j,childpte,chichi);
					for(int w = 0 ; w<512; w ++){
						pte_t chichipte = chichi[w];
						if(chichipte & PTE_V){
							pagetable_t chichichi = (pagetable_t)PTE2PA(chichipte);
							printf(" .. .. ..%d: pte %p pa %p\n",w,chichipte,chichichi);
						}
					}
				}	
			}
		}
	}
}
```

结果：

![image-20240804193802943](<lab3 page table/image-20240804193802943.png>)

回答问题：

For every leaf page in the `vmprint` output, explain what it logically contains and what its permission bits are. Figure 3.4 in the xv6 book might be helpful, although note that the figure might have a slightly different set of pages than the `init` process that's being inspected here.

低10位为权限。和3.4图对比

```c
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000
 .. .. ..0: pte 0x0000000021fda01b pa 0x0000000087f68000
 .. .. ..1: pte 0x0000000021fd9417 pa 0x0000000087f65000
 .. .. ..2: pte 0x0000000021fd9007 pa 0x0000000087f64000
 .. .. ..3: pte 0x0000000021fd8c17 pa 0x0000000087f63000
 ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000
 .. ..511: pte 0x0000000021fda401 pa 0x0000000087f69000
 .. .. ..509: pte 0x0000000021fdcc13 pa 0x0000000087f73000
 .. .. ..510: pte 0x0000000021fdd007 pa 0x0000000087f74000
 .. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
init: starting sh
```

![image-20240804235550357](<lab3 page table/image-20240804235550357.png>)

![image-20240804225949297](<lab3 page table/image-20240804225949297.png>)

第一个01b,可执行可读用户。text

第二个417，可读可写用户。data

第三个007，可读可写不用户。估计是guardpage

第四个c17,可读可写可用户。stack

第五个c13,可读可用户。按理来说是heap，但是heap是可读可写的。

第六个007，可读可写不用户。trapframe

第六个c0b,可读可执行不用户。trampoline

## Detect which pages have been accessed ([hard](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

一些垃圾收集器（自动内存管理的一种形式）可以从哪些页面被访问过（读或写）的信息中受益。在本实验的这一部分，你将为 xv6 添加一个新功能，通过检查 RISC-V 页表中的访问位来检测和报告这些信息到用户空间。每当 RISC-V 硬件页遍历器解决 TLB 未命中时，它就会在 PTE 中标记这些位。

根据hints一步步做

- Read `pgaccess_test()` in `user/pgtbltest.c` to see how `pgaccess` is used.

```c
void
pgaccess_test()
{
  char *buf;
  unsigned int abits;
  printf("pgaccess_test starting\n");
  testname = "pgaccess_test";
  buf = malloc(32 * PGSIZE);
  if (pgaccess(buf, 32, &abits) < 0)// 第一个参数va，第二个是页数，第三个记录结果，一页一位，这里刚好int(32位)
    err("pgaccess failed");
  buf[PGSIZE * 1] += 1;
  buf[PGSIZE * 2] += 1;
  buf[PGSIZE * 30] += 1;
  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess failed");
  if (abits != ((1 << 1) | (1 << 2) | (1 << 30)))
    err("incorrect access bits set");
  free(buf);
  printf("pgaccess_test: OK\n");
}
```

这里通过使用第1,2,30个页面，来验证pgacess的正确性。

- Start by implementing `sys_pgaccess()` in `kernel/sysproc.c`.

- You'll need to parse arguments using `argaddr()` and `argint()`.

```c
#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va,mask;
  int count;
  argaddr(0,&va);
  argint(1,&count);
  argaddr(2,&mask);
   
  return 0;
}
#endif
```

- For the output bitmask, it's easier to store a temporary buffer in the kernel and copy it to the user (via `copyout()`) after filling it with the right bits.
- It's okay to set an upper limit on the number of pages that can be scanned.

通过copyout将kernel里的mask复制到用户空间的mask，并检查count的大小：

```c
#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va,mask;
  int count;
  argaddr(0,&va);
  argint(1,&count);
  argaddr(2,&mask);
  if(count > 64) return -1;

  uint64 kmask = //
  
  if(copyout(p->pagetable,mask,(char*)&kmask,sizeof kmask) < 0)
	  return -1;
   
  return 0;
}
#endif
```

kmask 需要其他函数提供。

- `walk()` in `kernel/vm.c` is very useful for finding the right PTEs.

这个函数返回va对应的pte。如果alloc=0，不创建所需的页表，否则创建。

risc-v sv39框架有三级页表，一个页表有512个pte，每个pte64位。

```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

从level2开始，查看PX宏：

```c
// extract the three 9-bit page table indices from a virtual address.
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)
```

![image-20240804225949297](<lab3 page table/image-20240804225949297.png>)

12位是偏移，即pgshift，L2则对应右移12+2*9=30位后的低9位，px表达的就是这个意思。

通过L2找到偏移的项，把地址保存在pte中。如果pte有效，则继续向下一级走；否则先判断是否要分配，需要分配才会进行alloc。对分配的页表进行初始化，并赋值PTE_V。

**L1**也一样。最后返回那个表项的地址。

- You'll need to define `PTE_A`, the access bit, in `kernel/riscv.h`. Consult the [RISC-V privileged architecture manual](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMFDQC-and-Priv-v1.11/riscv-privileged-20190608.pdf) to determine its value.

从上面那幅图或者链接文件都能找到答案。

```c
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access
#define PTE_A (1L << 6)
```

- Be sure to clear `PTE_A` after checking if it is set. Otherwise, it won't be possible to determine if the page was accessed since the last time `pgaccess()` was called (i.e., the bit will be set forever).

补充好sys_pgacess中剩余的代码。

```c
#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va,mask;
  int count;
  argaddr(0,&va);
  argint(1,&count);
  argaddr(2,&mask);
  if(count > 64) return -1;

  uint64 kmask = 0;

  struct proc *proc = myproc();

  for(int i=0; i<count; i++){
	pte_t* pte;
	if((pte = walk(proc->pagetable,va,0)) != 0){// have check the PTE_V
		if(*pte & PTE_A){
			kmask += 1 << i;
			*pte &= (~PTE_A);
		}
	}else{
		return -1;
	}
	va += PGSIZE;
  }
  
  if(copyout(proc->pagetable,mask,(char*)&kmask,sizeof kmask) < 0)
	  return -1;
   
  return 0;
}
#endif
```

结果：

![image-20240804235113783](<lab3 page table/image-20240804235113783.png>)

**总结果**

![image-20240805001422598](<lab3 page table/image-20240805001422598.png>)

文件不知道填啥。

## 总结

感觉第三问比第一问还简单，毕竟第一问的hint比较少，需要靠自己去模仿。

学了创建页，物理空间映射到虚拟空间。由于内核和用户态使用的页表不一样，所以独立。但有些页面是共享的。

