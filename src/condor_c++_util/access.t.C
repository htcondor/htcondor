#include "condor_common.h"
#include "access.h"

int main(int argc, char **argv)
{
	int result;
	char *filename;
	int uid = getuid();
	int gid = getgid();

	if(argc != 2)
	{
		fprintf(stderr, "usage: access.t filename\n");
		exit(EXIT_FAILURE);
	}
	filename = argv[1];

	config();

	printf("Attempting to ask schedd about read access to %s\n", filename);	
	result = attempt_access(argv[1], ACCESS_READ, getuid(), getgid());
	
	if( result == 0 )
	{
		printf("Schedd says it can't read that file.\n");
		exit(0);
	}
	else
	{
		printf("Schedd says it can read that file.\n");
	}
	
	printf("Attempting to ask schedd about write access to %s\n", filename);
	result = attempt_access(argv[1], ACCESS_WRITE, uid, gid);
	
	if( result == 0 )
	{
		printf("Schedd says it can't write to that file.\n");
	}
	else
	{
		printf("Schedd says it can write to that file.\n");
	}
}
