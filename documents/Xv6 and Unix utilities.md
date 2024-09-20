## Xv6 and Unix utilities

### Boot xv6(easy)

构建xv6，跟着做一步步做。

### sleep ([easy](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

#### 查看echo的使用

很简单，把参数全部输出即可。

#### 查看grep的使用

需要学习一下正则表达式。

**^** 表示开头

*表示匹配零次或多次

$表示结尾

.匹配除换行符外的所有字符

一种是这种：

![image-20240713225441246](<Xv6 and Unix utilities/image-20240713225441246.png>)

一种是跟着文件，有两个参数（或更多：

![image-20240713225531779](<Xv6 and Unix utilities/image-20240713225531779.png>)

#### 查看rm

删除文件，其实就是解除链接。

#### 编写sleep

- sysproc.c 

``` c
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
```

syscall中有三个传参的函数：

```c
// syscall.c
void            argint(int, int*);
int             argstr(int, char*, int);
void            argaddr(int, uint64 *);
```

第一个参数代表第几个参数。

acquire 得到锁，release释放锁，这里锁是自旋锁。

ticks记录时钟中断的次数，tickslock是它的锁。

myproc()返回当前的struct proc *（当前没有就返回0）；struct proc\*是描述进程或任务的数据结构。

```c
int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}
```

killed()从获取当前进程的killed变量的值。

如果进程的killed变量被设置为1，则退出进程。

- user.h

```c
int sleep(int);
```

- usys.s

```ass
sleep:
 li a7, SYS_sleep
 ecall
 ret
```

![image-20240713232658455](<Xv6 and Unix utilities/image-20240713232658455.png>)

这小节做完去补risc-v。

**代码**:

``` c
#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int
main (int argc, char *argv[])
{

        if(argc <= 1){
                fprintf(2,"sleep: missing operand\n");
                exit(1);
        }
        int time = atoi(argv[1]);

        sleep(time);
        exit(0);
}
```

**结果**

![image-20240714160158304](<Xv6 and Unix utilities/image-20240714160158304.png>)

### pingpong

使用pipe来传输字节

```c
#include "../kernel/types.h"
#include "user.h"

int
main (int argc, char *argv[])
{
	if(argc > 1){
		fprintf(2,"usage: pingpong\n");
		exit(1);
	}
	int p[2];
	pipe(p);
	char buf[2];
	if(fork() == 0){
		read(p[0],buf,1);
		close(p[0]);
		fprintf(1,"%d: received ping\n",getpid());
		write(p[1],"a",1);
		close(p[1]);
		exit(0);
	}else{
		write(p[1],"a",1);
		close(p[1]);
		read(p[0],buf,1);
		close(p[0]);
		fprintf(1,"%d: received pong\n",getpid());
	}
	exit(0);
}
```

![image-20240714162617595](<Xv6 and Unix utilities/image-20240714162617595.png>)

### primes ([moderate](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))/([hard](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

#### 阅读 [Bell Labs and CSP Threads (swtch.com)](https://swtch.com/~rsc/thread/)

**介绍**

并发编程是为了使程序更加清晰，而不是更高效。

**这不是什么**

这篇文章讲的不是那种需要关注细节的线程模型，而是csp

**通信顺序进程**

截止1978，共享内存是嘴常用的进程通信机制，信号，临界区，管程是同步机制。

Hoare用同步通信解决了这两个问题。

Hoare的csp语言通过未缓冲通道进行发送和接收值，由于通道未缓冲，发送操作会阻塞到值被发送到接收器，这样就完成了同步机制。

Hoare的一个例子是把80行的卡片重排为125行的输出。它的解决方案是，一个进程读取一个卡片，一个一个字符发给另一个进程。接收字符的进程将字符排成125个一组发给打印机。这看起简单，但是如果没有io缓冲区，处理过程会很繁琐。实际上，io缓冲库只是这两个进程的封装，他们对外提供单字符通道接口。

**另一个例子**，生成小于1000的所有素数。伪代码如下：

```
p = get a number from left neighbor
print p
loop:
    n = get a number from left neighbor
    if (p does not divide n)
        send n to right neighbor
```

![image-20240715200028121](<Xv6 and Unix utilities/image-20240715200028121.png>)

这个线性管道的性质不能代表一般csp的性质，但是即使是严格的线性管道，也是很强大。在linux中管道和过滤器方法非常著名。

Hoare的进程通信比传统unix的shell管道更加普遍。事实上，Hoare给了一个例子，一个3*3的进程可以用于计算一个向量和3\*3的矩阵相乘。

由于通信的管道不是一级对象，不能被存储在变量，作为参数，或者通过通道。导致在写程序时，必须固定通信结构。所以我们写程序打印开始的1000素数而不是n个素数，乘以一个向量3*3的矩阵而不是n\*n的矩阵。

**Pan and Promela**

pan的csp方言有连接，选择和循环。

**Newsqueak**

 unlike in CSP and Squeak, channels *can* be stored in variables, passed as arguments to functions, and sent across channels. 

**Alef**

将Newsqueak的思想应用于一种成熟的系统编程语言。

#### 回到题目

```
p = get a number from left neighbor
print p
loop:
    n = get a number from left neighbor
    if (p does not divide n)
        send n to right neighbor
```

思路：

写一个函数，递归，没有数字传给右边时就停止递归。

```c
#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

#define N 35

int nums[N];

void prime(){
	int p[2];
	pipe(p);
	if(fork() == 0){
		close(p[1]);
		int i;
		for(i=0; 1; i++ )
			if(read(p[0],nums+i,4) == 0) break;
		close(p[0]);
		nums[i] = 0;
		if(i) 
			prime();
		exit(0);
	}else{
		close(p[0]);
		printf("prime %d\n",nums[0]);
		for(int i=1; nums[i]; i++){
			if(nums[i] % nums[0])
				write(p[1],nums+i,4);
		}
		close(p[1]);
	}	
	wait(0);
	exit(0);
}

int main(int argc ,char* argv[]){
	for(int i=0; i<N-1; i++){
		nums[i] = i+2;
	}
	prime();
	exit(0);
}

```

![image-20240716001358341](<Xv6 and Unix utilities/image-20240716001358341.png>)

### find ([moderate](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

查看**ls.c**

```c
void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;//目录项 
  struct stat st;//文件信息结构

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }/* 获取目录的属性结构体 */

  switch(st.type){
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}
```

dirent的结构体：

```c
struct dirent {
  ushort inum;\* inode number *\
  char name[DIRSIZ];
};
```

stat ：

```c
struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};
```

文件直接打印，目录通过遍历打印。

memmove用于复制字符串。

思路：

和ls的思路类似，可以直接将ls的代码复制过来加以修改。

代码：

```c
void
find(char* path,char* name){
	char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: %s: No such file or directory\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
	for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
	p++;
	if(!strcmp(p,name))
    	printf("%s\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0 || !strcmp(de.name,".") || !strcmp(de.name,".."))
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
	  find(buf,name);
    }
    break;
  }
  close(fd);

}


int 
main(int argc, char* argv[]){

	if(argc <= 2){
		fprintf(2,"usage: find path target\n");
		exit(0);
	}
	find(argv[1],argv[2]);

	exit(0);
}

```

## ![image-20240716235853586](<Xv6 and Unix utilities/image-20240716235853586.png>)

### ![image-20240717000057760](<Xv6 and Unix utilities/image-20240717000057760.png>)



一开始以为是和正常shell中的find，就直接按照正常的设计：

```c
#include"../kernel/types.h"
#include"../kernel/stat.h"
#include"user.h"
#include"../kernel/fs.h"


void 
find(char* path){
	char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: %s: No such file or directory\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf("%s\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0 || !strcmp(de.name,".") || !strcmp(de.name,".."))
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
	  find(buf);
    }
    break;
  }
  close(fd);

}

int 
main(int argc, char* argv[]){
	int i;

	if(argc < 2){
		find(".");
		exit(0);
	}
	for(i = 1; i<argc ; i++)
		find(argv[i]);

	exit(0);
}
```

要注意的是："." 、".."这两个文件要注意不要死迭代了。

**结果**

![image-20240716233307366](<Xv6 and Unix utilities/image-20240716233307366.png>)

![image-20240717000132613](<Xv6 and Unix utilities/image-20240717000132613.png>)

休息了。

### xargs ([moderate](https://pdos.csail.mit.edu/6.828/2021/labs/guidance.html))

思路：

​	有参数和没参数的。有参数需要每n个参数一组作为命令的参数。没参数的就按每MAXARG个参数一组。这样就统一了。

**结果**

![image-20240718160919970](<Xv6 and Unix utilities/image-20240718160919970.png>)

![image-20240718160955520](<Xv6 and Unix utilities/image-20240718160955520.png>)

## 总结

​	回顾了一下基本的指令。文件描述符，输入输出重定向，管道。
