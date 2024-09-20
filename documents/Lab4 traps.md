# Lab: traps

## RISC-V assembly ([easy](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

**g函数**

先看g函数，传入参数x，返回x+3.

先通过减少栈顶指针sp来增大栈空间。将s0的值传给(sp+8)处,此处应为保护原来的s0的值，因为下一步要对s0进行修改。结合下图，可推测s0为帧指针。

这个文件特地汇编和C语言混合，可能方便阅读把。

return x+3;转化为汇编就是地址6~c的指令。a0是保存参数的寄存器，也是返回值。

![image-20240808112333378](<Lab4 traps/image-20240808112333378.png>)

```c
int g(int x) {
   0:	1141                	addi	sp,sp,-16
   2:	e422                	sd	s0,8(sp)
   4:	0800                	addi	s0,sp,16
  return x+3;
}
   6:	250d                	addiw	a0,a0,3
   8:	6422                	ld	s0,8(sp)
   a:	0141                	addi	sp,sp,16
   c:	8082                	ret
```

**f函数**

和g函数的汇编一致，但是注意这里的C语言是return g（x),让我想起了c++的内联函数。

```c
000000000000000e <f>:

int f(int x) {
   e:	1141                	addi	sp,sp,-16
  10:	e422                	sd	s0,8(sp)
  12:	0800                	addi	s0,sp,16
  return g(x);
}
  14:	250d                	addiw	a0,a0,3
  16:	6422                	ld	s0,8(sp)
  18:	0141                	addi	sp,sp,16
  1a:	8082                	ret

```

**main**

栈的大小为16，ra存储返回地址，s0存储上一个栈帧，将他俩暂存在栈中。

然后执行 printf("%d %d\n", f(8)+1, 13);由五条汇编指令构成。a0,a1,a2分别是从左到右的参数。

前两条好理解，赋值,注意这里没有调用函数f。a0存储的是格式化字符串的地址。

auipc把当前指令的地址加上直接数0，存储在a0中。然后addi将刚刚地址距离目标地址的偏移量加上，就得到目标地址。

剩下就是函数跳转。jalr将下一条指令地址存在ra中，以便从printf中返回。

```c
000000000000001c <main>:

void main(void) {
  1c:	1141                	addi	sp,sp,-16
  1e:	e406                	sd	ra,8(sp)
  20:	e022                	sd	s0,0(sp)
  22:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
  28:	00000517          	auipc	a0,0x0
  2c:	50513          	addi	a0,a0,1976 # 7e0 <malloc+0xe6>
  30:	00000097          	auipc	ra,0x0
  34:	612080e7          	jalr	1554(ra) # 642 <printf>
  exit(0);
  38:	4501                	li	a0,0
  3a:	00000097          	auipc	ra,0x0
  3e:	28e080e7          	jalr	654(ra) # 2c8 <exit>
```

### Which registers contain arguments to functions? For example, which register holds 13 in main's call to `printf`?

- 刚刚碰到了a0,a1,a2。实际上还有a3~a7.

- a2

### Where is the call to function `f` in the assembly code for main? Where is the call to `g`? (Hint: the compiler may inline functions.)

- 都被编译器优化了。

### At what address is the function `printf` located?

- 0x7e0

### What value is in the register `ra` just after the `jalr` to `printf` in `main`?

- 0x38

### output

0x00646c72由于小端序，0x72是第一位,即r。0x64->l,0x64->l。

- He110 World

如果是大端序，0x00是第一位，即末尾。

- He110 Wo

要达到相同的效果i修改为 0x726c6400

### In the following code, what is going to be printed after `'y='`? (note: the answer is not a specific value.) Why does this happen?

```
	printf("x=%d y=%d", 3);
```

把a2里的数字输出来了。printf函数不知道，a2的数字是传递的还是本来的。

## Backtrace ([moderate](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

hint1,hint2照做就行了。再根据hint3得到fp的值等于返回地址的地址加8。根据hint4得到遍历终止条件：当不在同一页时，循环终止。

```c
void 
backtrace(void)
{
	printf("backtrace:\n");
	uint64 fp =  r_fp();
	uint64 page_ad = PGROUNDDOWN(fp);
	while(PGROUNDDOWN(fp) == page_ad){
		uint64 ret_ad = fp-8;
		printf("%p\n", *(uint64*)ret_ad);
		fp = *(uint64*)(fp-16);
	}
}
```

这里比较坑的是，这个%p自动有前置0，不用自己写(%016p)。

看了实际调用printstr这个函数：

```c
static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}
```

每4位输出一次，输出16次，刚好全部输出。

最后在panic中调用backtrace，便于调试。

## Alarm ([hard](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

### test0

前几hint跟着做就好。

再sysproc.c中添加系统调用：

```c
uint64
sys_sigreturn(void)
{
	return 0;
}

uint64
sys_sigalarm(void)
{
	int interval;
	uint64 handle;
	argint(0,&interval);
	argaddr(1,&handle);
	
	struct proc* proc = myproc();
	proc->alarmitv = interval;
	proc->alarmhd = handle;

	

	return 0;
}
```

- Every tick, the hardware clock forces an interrupt, which is handled in `usertrap()` in `kernel/trap.c`.

先分析下代码：

```c
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)//查看进程是否来自用户模式，如果来自则spp设为0，否则为1
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);// 修改stvec寄存器，使trap由kernelvec控制

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){// 获取原因
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}
```

再详细看下usertrapret

```c
//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);// 这里的satp只是个变量不是寄存器。

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}
```

根据后面几个hint，再usertrap()添加如下代码：

```c
 // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
  {
	if(p->alarmhd != 0 || p->alarmitv != 0){
		p->trickscnt ++;
		if(p->trickscnt == p->alarmitv){
			p->trickscnt = 0;
			p->trapframe->epc = p->alarmhd;
		}
	}
    yield();
  }
```

结果：

![image-20240809110955517](<Lab4 traps/image-20240809110955517.png>)

点比示例多，不过最后一条hint说只要打印“alarm!"就行。

### test1/test2()/test3(): resume interrupted code

只需要修改proc.h proc.c sysproc.c trap.c即可。

由于执行完handler后直接返回到计时器中断前的用户指令。所以需要保存pc值，另外再从alarmtest.asm 查看所有handler需要的寄存器，这些也是需要保存的值。hint有提示这些值可以保存在proc结构体中，另外需要一个变量标记是否有handle已经在执行了。再写sys_sigretur系统调用时，注意返回值是a0，所以需要返回你之前保存a0的值。

```c
uint64
sys_sigreturn(void)
{
	struct proc *p = myproc();
	p->trapframe->epc = p->epc;
	p->trapframe->sp = p->sp;
	p->trapframe->ra = p->ra;
	p->trapframe->s0 = p->s0;
	p->trapframe->a4 = p->a4;
	p->trapframe->a5 = p->a5;
	p->trapframe->a1 = p->a1;
	p->has_one = 0;
	return p->a0;
}
```

trap.c文件中usertrap()的再修改：

```c
 if(which_dev == 2)
  {
	if(p->alarmhd != 0 || p->alarmitv != 0){
		p->trickscnt ++;
		if(p->trickscnt == p->alarmitv && p->has_one == 0 ){
			p->has_one = 1;
			p->trickscnt = 0;
			p->epc = p->trapframe->epc;
			p->sp = p->trapframe->sp;
			p->a1 = p->trapframe->a1;
			p->ra = p->trapframe->ra;
			p->s0 = p->trapframe->s0;
			p->a0 = p->trapframe->a0;
			p->a4 = p->trapframe->a4;
			p->a5 = p->trapframe->a5;
			p->trapframe->epc = p->alarmhd;
		}
	}
    yield();
  }
```

最后说下，当时忘记看其他handle，只看了periodic。后来发现其他的测试用的不是这个handle，有的传了两个参数，以致漏了一个a1参数寄存器。

结果：

**![image-20240809203751932](<Lab4 traps/image-20240809203751932.png>)**

## 总结

首先了解了risc-v汇编，然后了解栈结构。这些都是有接触过的。

最新的部分还是最后一问。

了解了traps(这里只是系统调用)的过程，从用户态到内核态，再从内核态返回用户态，以及一系列措施来实现隔离和恢复。计时器中断的应用。
