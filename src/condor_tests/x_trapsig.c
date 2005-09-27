#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int signalcaught = 0;

void catchsig( int sig )
{
		/*
		printf("Generate lots of output.................\n");
        printf("%d\n",sig);
		fflush(stdout);
		*/
		/*fsync(stdout);*/
        printf("%d\n",sig);
		fflush(stdout);
		signalcaught = sig;
		exit(sig);
}


struct sigaction new = 
{
	&catchsig,
	0,
	0, /*SA_NOMASK,*/
	0, /*SA_NOMASK,*/
};

struct sigaction old;

int main(int argc, char **argv)
{
	int res = 0;
	/*signal( 1, (sighandler_t)&catchsig);*/
	res = sigaction( 1, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 1\n");
	}
	res = sigaction( 2, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 2\n");
	}
	res = sigaction( 3, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 3\n");
	}
	res = sigaction( 4, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 4\n");
	}
	res = sigaction( 5, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 5\n");
	}
	res = sigaction( 6, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 6\n");
	}
	res = sigaction( 7, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 7\n");
	}
	res = sigaction( 8, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 8\n");
	}
	/*
	**res = sigaction( 9, &new, &old);
	**if(res != 0 )
	**
		**printf("failed to replace handler intr 9\n");
	**
	*/
	res = sigaction( 10, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 10\n");
	}
	res = sigaction( 11, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 11\n");
	}
	res = sigaction( 12, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 12\n");
	}
	res = sigaction( 13, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 13\n");
	}
	res = sigaction( 14, &new, &old);
	if(res != 0 )
	{
		printf("failed to replace handler intr 14\n");
	}
	while(1);
}
