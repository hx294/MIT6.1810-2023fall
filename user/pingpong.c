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
