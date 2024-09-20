# Lab: mmap ([hard](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

map 和 munmap 系统调用 允许unix程序对地址空间实施细致的控制。这两个用于在进程中分享内存，映射文件到进程地址空间中，作为页错误机制的一部分，像垃圾回收算法。这个实验专注于内存

映射文件。

mmap的声明:

```
void *mmap(void *addr, size_t len, int prot, int flags,
           int fd, off_t offset);
```

mmap可以被多种方式调用，但这个实验只要求内存映射文件。你可以假定addr将常为0，意味着内核应该决定在哪个虚拟地址映射文件。mmap返回哪个地址，或者0xffffffffffffffff如果失败。len是需要映射的字节数。它可能和文件长度不同。prot 表明这段内存是否可读，可写，可执行；可以假定prot是prot_read 或者 PROT_WRITE 或者都有。flags要么是MAP_SHARED，意味着修改会被写回文件，要么是MAP_PRIVATE，不写回。你没必要在flags中实现其他字段。你可以假定offset为0（这是映射起始地址。）

如果 多个进程用MAP_SHARED映射了相同文件，不共享物理页是可以的。

```
int munmap(void *addr, size_t len);
```

munmap 应该在地址范围的mmap映射段。如果有MAP_SHARED，则写回。Munmap 调用可能只覆盖 mmap-ed 区域的一部分，但是您可以假定它将在开始、结束或整个区域取消映射(但是不会在区域的中间打一个洞)。

任务：

实现mmap和munmap函数使得mmaptest 奏效。

提示：

- Start by adding `_mmaptest` to `UPROGS`, and `mmap` and `munmap` system calls, in order to get `user/mmaptest.c` to compile. For now, just return errors from `mmap` and `munmap`. We defined `PROT_READ` etc for you in `kernel/fcntl.h`. Run `mmaptest`, which will fail at the first mmap call.

修改Makefile，user/user.h,user/usys.pl,syscall.c,syscall.h，sysproc.c,

![image-20240914212400531](Lab10 mmap (hard)/image-20240914212400531.png)

- Keep track of what `mmap` has mapped for each process. Define a structure corresponding to the VMA (virtual memory area) described in the "virtual memory for applications" lecture. This should record the address, length, permissions, file, etc. for a virtual memory range created by `mmap`. Since the xv6 kernel doesn't have a variable-size memory allocator in the kernel, it's OK to declare a fixed-size array of VMAs and allocate from that array as needed. A size of 16 should be sufficient.

[How to correctly use the extern keyword in C - Stack Overflow](https://stackoverflow.com/questions/496448/how-to-correctly-use-the-extern-keyword-in-c)

```c
enum vmastate{VMA_UNUSED, VMA_USED};

// virtual memory area
struct vma{
	enum vmastate state;
	uint64 start;
	uint64 sz;
	int flags;
	int prot;
	struct file* f;
};

struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  
  struct vma vmas[NVMA]; // vma

  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

```

把vma声明到proc结构体中，

- Implement `mmap`: find an unused region in the process's address space in which to map the file, and add a VMA to the process's table of mapped regions. The VMA should contain a pointer to a `struct file` for the file being mapped; `mmap` should increase the file's reference count so that the structure doesn't disappear when the file is closed (hint: see `filedup`). Run `mmaptest`: the first `mmap` should succeed, but the first access to the mmap-ed memory will cause a page fault and kill `mmaptest`.

寻找一个未使用的进程空间来映射文件，一直没搞懂在什么地方才是没使用的，看了exec的代码后恍然大悟。大体意思就是将elf文件的内容复制到进程空间，并分配栈到stack guard，分配到此为止。所以后面就是unused的空间了：

```c
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
```

![image-20240919134120238](Lab10 mmap (hard)/image-20240919134120238.png)

fd对应proc结构体中的ofile数组的下标。

```c
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
```

![image-20240917155018316](Lab10 mmap (hard)/image-20240917155018316.png)

- Add code to cause a page-fault in a mmap-ed region to allocate a page of physical memory, read 4096 bytes of the relevant file into that page, and map it into the user address space. Read the file with `readi`, which takes an offset argument at which tok read in the file (but you will have to lock/unlock the inode passed to `readi`). Don't forget to set the permissions correctly on the page. Run `mmaptest`; it should get to the first `munmap`.

![image-20240811192549361](Lab10 mmap (hard)/image-20240811192549361.png)

![image-20240917163716230](Lab10 mmap (hard)/image-20240917163716230.png)

一开始的思路是先用uvmalloc来分配内存，然后readi写入。但是我忽略了uvmmalloc传入的权限没有写入权限，导致readi崩溃。修正的思路：无脑传入PTE_W，readi后再修复页表。

```c
if(r_scause() == 15 || r_scause() == 13 ){
		// printf("page-fault%p\n",r_stval());
		int i,perm = 0;
		uint64 va = r_stval();
		uint64 sz = 0;
		if(va > p->sz){
			goto bad;
		}
		
		// find the vma;
		struct vma *v = p->vmas;
		for(i=0; i<NVMA; i++){
			if(v[i].state == VMA_USED && v[i].start <= va 
					&& v[i].start + v[i].sz > va){
				break;
			}
		}
		if(i == NVMA){
			goto bad;
		}

		if(v[i].prot & PROT_EXEC)
			perm |= PTE_X;
		perm |= PTE_W;
		perm |= PTE_D;
		if((sz = uvmalloc(p->pagetable,va & ~0xfff,(va &~0xfff)+PGSIZE,perm)) == 0){
			goto bad;
		}
		// copy from file to mem
		struct file* fp = v[i].f;
		struct inode* ip = fp->ip;
	
		// printf("ip:%d",i);
		ilock(ip);

		if(readi(ip,1,va & ~0xfff,v[i].off + ((va & ~0xfff) - v[i].start ),PGSIZE) == -1){
			iunlock(ip);
			goto bad;
		}

		iunlock(ip);

		if((v[i].prot & PROT_WRITE) == 0)
		{
			pte_t *pte = walk(p->pagetable,va,0);
			*pte &= ~PTE_W;
		}
	  }else{
bad:	printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
		printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
		setkilled(p);
	  }

```

在munmap处报错：

![image-20240919134728998](Lab10 mmap (hard)/image-20240919134728998.png)

- Implement `munmap`: find the VMA for the address range and unmap the specified pages (hint: use `uvmunmap`). If `munmap` removes all pages of a previous `mmap`, it should decrement the reference count of the corresponding `struct file`. If an unmapped page has been modified and the file is mapped `MAP_SHARED`, write the page back to the file. Look at `filewrite` for inspiration.

查看filewrite：

```c
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

```

这里不懂如何确认页是否被修改。那就粗鲁一些，用permission来确定，可写就算被修改。

- Ideally your implementation would only write back `MAP_SHARED` pages that the program actually modified. The dirty bit (`D`) in the RISC-V PTE indicates whether a page has been written. However, `mmaptest` does not check that non-dirty pages are not written back; thus you can get away with writing pages back without looking at `D` bits.

这里说可以用PTE_D来说明是否是脏页，但是xv6实际上没有实现，所以需要自己添加。但是由于mmaptest没检查，所以也可以不使用PTE_D，按照我上面的说法也是可以。但还是修改一下吧。

> An `munmap` call might cover only a portion of an mmap-ed region, but you can assume that it will either unmap at the start, or at the end, or the whole region (but not punch a hole in the middle of a region)

由于只会到一个vma中，所以检测到地址区间属于某个vma后，并执行完释放操作就可以中断循环，不需要考虑多个vma的情况。

- Modify `exit` to unmap the process's mapped regions as if `munmap` had been called. Run `mmaptest`; all tests through `test mmap two files` should pass, but probably not `test fork`.

![image-20240919195848232](Lab10 mmap (hard)/image-20240919195848232.png)

报错，因为可读文件被映射到可写内存。

查看file结构体和open函数中关于readable，writable的使用，得出可写文件 设置writable，可读文件设置readable，可读可写两个均设置。

two file map出错

报错，因为在if检测范围越界了

```c
for(i=0; i<NVMA; i++){
			if(v[i].state == VMA_USED && v[i].start <= va 
					&& v[i].start + v[i].sz >= va){
				break;
			}
		}

```

去掉等于号，因为start+end位置的数据实际上不在这个区间。这个等于号导致，两个vma从同一个文件读取数据。

修改后，这个小结全部通过。

```c
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
```

![image-20240920115017100](Lab10 mmap (hard)/image-20240920115017100.png)

- Modify `fork` to ensure that the child has the same mapped regions as the parent. Don't forget to increment the reference count for a VMA's `struct file`. In the page fault handler of the child, it is OK to allocate a new physical page instead of sharing a page with the parent. The latter would be cooler, but it would require more implementation work. Run `mmaptest`; it should pass all the tests.

注意在exit中要实现与munmap类似的功能。以及尽可能将进程空间恢复成map之前的样子，这样父进程才能将子进程正确释放。

```c
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  // vma copy and increment reference counts on mapped files
  struct vma* v = p->vmas;
  struct vma* nv = np->vmas;
  for(i = 0; i < NVMA; i++)
	  if(v[i].state == VMA_USED){
		  nv[i] = v[i];
		  filedup(nv[i].f);
	  }
  	
  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}
```

uvmcopy中要注意，如果虚拟地址已经映射了，需要将父进程的相应页面复制过来，不能等到page-fault时再复制，因为此时不知父进程的相应内存是否修改：

```c

static int
waitforalloc(uint64 ad){
	struct proc* p = myproc();
	struct vma* v = p->vmas;
	int i;
	for(i = 0; i < NVMA; i++)
		if( v[i].state == VMA_USED && v[i].start <= ad && v[i].sz + v[i].start > ad )
			return 1;
	return 0;
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
	int wait = waitforalloc(i);
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if(!wait && (*pte & PTE_V) == 0 )
      panic("uvmcopy: page not present");
	else if(wait && (*pte & PTE_V) == 0)
	  continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

![image-20240920191159320](Lab10 mmap (hard)/image-20240920191159320.png)

![image-20240920201631365](Lab10 mmap (hard)/image-20240920201631365.png)

## 小结

这次实验主要实现mmap和munmap。综合了前面的一些内容，尤其是cow-lab和pgtl-lab，还有一些文件系统的内容。难点是快速找到一个unused space，我选择的方法是将p->sz 扩大，在unmap时缩小。还有区域边界的问题。这里没有实现文件映射从父进程到子进程的copy-on-write,感觉很复杂，需要检测那个空间是否修改（从fork以后),并且就不能使用uvmalloc。
