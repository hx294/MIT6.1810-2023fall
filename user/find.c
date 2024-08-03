#include"../kernel/types.h"
#include"../kernel/stat.h"
#include"user.h"
#include"../kernel/fs.h"

/*
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
*/
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
