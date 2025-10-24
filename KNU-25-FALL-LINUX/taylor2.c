#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/stat.h>
#define _USE_MATH_DEFINES
#include <math.h>
#define N 4
#define MAXLINE 100
#define TERMS 3
//자식에서 생성한 값을 부모가 받을 때 어떤 순서로 값이 들어올지 모른다
void sinx_taylor(int num_elements, int terms, double* x, double* result){
	int pid;
	int child_id;
	int fd[2*N],length;
	char message[MAXLINE], line[MAXLINE];
	//child 4개 생성
	for(int i=0;i<num_elements;i++){
		pipe(fd + 2*i);
		child_id = i; //몇번째 child인지 알려줌
		pid = fork();
	       if(pid == 0) {//child
			break;
	       }else{//parent
	       		close(fd[2*i+1]);
	       }	
	}		

	if(pid==0){//child
		close(fd[2*child_id]);

		double value =x[child_id];
		double numer = x[child_id]*x[child_id]*x[child_id];
		double denom=6.;
		int sign =-1;

			for(int j =1; j<=terms; j++){
				value +=(double)sign * numer/denom;
				numer *=x[child_id] *x[child_id];
				denom *= (2.*(double)j+2.) *(2.*(double)j+3);
				sign *= -1;
			}
		result[child_id] = value;

		sprintf(message, "%lf", result[child_id]);
		length =strlen(message)+1;
		write(fd[2*child_id+1],message,length);
		exit(child_id);
	}else{//parent

	      for(int i=0;i<num_elements;i++){
		      int status;
		      wait(&status);
		      read(fd[2*i],line,MAXLINE);
	      	      result[i] = atof(line);
	}
	}
}
int main()
{
	double x[N] ={ 0, M_PI/6., M_PI/3.,0.134};
	double res[N];

	sinx_taylor(N,3,x, res);
	for(int i=0; i<N ; i++){
		printf("sin(%.2f) by Taylor series = %f\n", x[i],res[i]);
		printf("sin(%.2f) = %f\n", x[i],sin(x[i]));
	}
	return 0;
}

