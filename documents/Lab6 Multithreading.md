# Lab: Multithreading

### Uthread: switching between threads ([moderate](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

由于新线程需要有自己的寄存器。先在thread结构体中添加新变量context。context存储了保存寄存器、sp和ra。注意栈是由高地址开始。由于刚创建时，没有自己的栈，即没有需要返回到的地址（保存在栈中），让ra直接等于需要跳转的函数。

```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->context.ra = (uint64)func;
  t->context.sp = (uint64)t->stack + STACK_SIZE;
}
```

添加thread_switch。由于更换了ra,sp。相当于更换了栈和代码段。

```c
if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
	thread_switch((uint64)&t->context,(uint64)&next_thread->context);
  } else
    next_thread = 0;
```

除了ra和sp，还需要保存保存寄存器。

- `thread_switch` needs to save/restore only the callee-save registers. Why?

这里不知道。我是根据内核中的swtch.asm，来确定需要保存的寄存器。加上这个函数没有参数，所以a0~a7不用保存。等下再查。

查到：

![image-20240820123930008](<Lab6 Multithreading/image-20240820123930008.png>)

然后callee-savd registers 包括：

![image-20240820125349572](<Lab6 Multithreading/image-20240820125349572.png>)

ra不属于callee-saved register,但由于第一次需要跳转到特定函数，所以也需要它。第一次后都不需要ra发生改变，这里还是和内核的切换有所区别。

```c
	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
		sd ra, 0(a0)
		sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

		ld ra, 0(a1)
		ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

	ret    /* return to ra */
```

- The breakpoint may (or may not) be triggered before you even run `uthread`. How could that happen?

![image-20240820132001188](<Lab6 Multithreading/image-20240820132001188.png>)

可以看出qemu还没运行uthread，便有了断点。

原因：

> 因为gdb的实现依赖于监视`pc`寄存器，我们在`b some_func`的时候实际上是记录的某个地址。如果`uthread`内的指令地址与内核的指令地址有重复，那么当内核运行到这个地址的时候就会触发本应该在`uthread`内的断点。此外，很容易验证不同的用户态程序也会干扰。比如在`uthread`内部的`0x3b`之类的地址打下个断点，再运行`ls`或者其他用户态程序，如果在`0x3b`地址的指令是合法的，那么也会触发本应该在`uthread`程序内部的断点。
>
> [xv6的用户态线程(协程) | Blurred code](https://www.blurredcode.com/2021/02/98bbdc4e/)

同时发现打了断点后，在两个地址空间有了断点。

![image-20240820140209854](<Lab6 Multithreading/image-20240820140209854.png>)

查了，应该是编译器优化造成的。查看uthread.asm文件：

```c
 3e:	4791                	li	a5,4
  for(int i = 0; i < MAX_THREAD; i++){ //here
    if(t >= all_thread + MAX_THREAD)
  40:	00009817          	auipc	a6,0x9
  44:	f5880813          	addi	a6,a6,-168 # 8f98 <base>
      t = all_thread;
    if(t->state == RUNNABLE) {
  48:	6689                	lui	a3,0x2
  4a:	4609                	li	a2,2
      next_thread = t;
      break;
    }
    t = t + 1;
  4c:	07868893          	addi	a7,a3,120 # 2078 <__global_pointer$+0xaef>
  50:	a809                	j	62 <thread_schedule+0x3c>
    if(t->state == RUNNABLE) {
  52:	00d58733          	add	a4,a1,a3
  56:	4318                	lw	a4,0(a4)
  58:	02c70963          	beq	a4,a2,8a <thread_schedule+0x64>
    t = t + 1;
  5c:	95c6                	add	a1,a1,a7
  for(int i = 0; i < MAX_THREAD; i++){ // here
  5e:	37fd                	addiw	a5,a5,-1
  
```

### Using threads ([moderate](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

- Why are there missing keys with 2 threads, but not with 1 thread? Identify a sequence of events with 2 threads that can lead to a key being missing. Submit your sequence with a short explanation in `answers-thread.txt`.

因为两个线程可能同时访问同一个链表，某一个线程的更新可能被被覆盖。更新和访问应该序列化。不过这题读和写是分开的，所以不会发生读时正在写的情况，不用给get加锁。

```c
static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
	pthread_mutex_lock(&lock);
    // update the existing key.
    e->value = value;
	pthread_mutex_unlock(&lock);
  } else {
	pthread_mutex_lock(&lock);
    // the new is new.
    insert(key, value, &table[i], table[i]);
	pthread_mutex_unlock(&lock);
  }

}
```

一共有两种种竞争：

- 更新旧节点和更新纠结点
- 产生新节点和产生新节点

由于table[i]当作参数,所以不能将锁写在函数内部。

![image-20240820110409624](<Lab6 Multithreading/image-20240820110409624.png>)

一个不理解的点：

- 为什么快了不止两倍？



### Barrier([moderate](https://pdos.csail.mit.edu/6.S081/2023/labs/guidance.html))

更新nthread会发生竞争。判断也会。由于只有最后一个进程才能更新round和重置nthread，所以不用锁这期间。

```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if(bstate.nthread == nthread){
	  pthread_mutex_unlock(&bstate.barrier_mutex);
	  bstate.nthread = 0;
	  bstate.round++;
	  pthread_cond_broadcast(&bstate.barrier_cond);
  }
  else {
	  pthread_cond_wait(&bstate.barrier_cond,&bstate.barrier_mutex);
	  pthread_mutex_unlock(&bstate.barrier_mutex);
  }

}
```

![image-20240820122327311](<Lab6 Multithreading/image-20240820122327311.png>)

### 总结

三个实验都是关于进程调度。第一个关于进程切换的过程，可以看得出来仅仅只是在一个cpu上。后面几个实验明显在多处理器上运行。都是用户层面的，比较简单。
