#include<stdlib.h>
#include<stdio.h>
#include<string.h>

int main(int argc, char *argv[])
{
	int a = atoi(argv[1]);
	int b = atoi(argv[3]);
	char op = argv[2][0];
	if(op == '+')
		printf("%d\n",a+b);
	else if(op == 'x')
		printf("%d\n",a*b);
	else if(op == '/')
		printf("%d\n",a/b);
	else if(op == '-')
		printf("%d\n",a-b);
	else
		printf("re-enter\n");
	return 0;
}
