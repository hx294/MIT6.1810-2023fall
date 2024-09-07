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

