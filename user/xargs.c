#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/param.h"

#define N 100

char arg[MAXARG][N];
char* args[MAXARG];

int 
main(int argc,char* argv[])
{
	//memset(args,0,sizeof args);
	if(argc <= 1){
		fprintf(2,"usage: xargs command [arg1]...\n");
		exit(1);
	}

	int nopara = !strcmp(argv[1],"-n");

	int offset = 1,size = MAXARG,i = 0; 

	if(nopara){ offset = 3;size = atoi(argv[2]);}

	for(int j = 0; j < MAXARG; j++)
		args[j] = arg[j];
	
	for(i = 0; i < argc-offset ; i++){
		memmove(arg[i],argv[i+offset],sizeof (argv+i+offset));
	}
	//fprintf(1,"aaa\n");
	int first = 1;
	for(int temp = i; i< MAXARG; i++){
		if((i-temp) % size == 0 ){
			if(first) first = 0;
			else{
				args[i] = 0;
				if(fork() == 0){
					exec(argv[offset],args);
				}else{
					wait(0);
				}
			}
			i = temp;
		}
		int flag = 0;
		for(int j = 0; j < N; j++){
			char x;
			if(read(0,&x,1) == 0){flag = 1; break;}
			if(x == '\n') {arg[i][j] = '\0'; break;} 
			else arg[i][j] = x;
		}
		if(flag) break;
	}

	/*
	for(int j=0; j < i; j++)
		fprintf(1,args[j]);
	*/

	args[i] = 0;
	//fprintf(1,argv[offset]);
	if(fork() == 0)
		exec(argv[offset],args);

	wait(0);
	/* never get here */
	exit(0);
}
