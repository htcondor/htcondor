#include <stdio.h>
#include <sys/types.h>


caddr_t sbrk();

extern char *etext;
extern char *edata;
extern char *end;

char	Buf[] = "Buff";
void	foo();
char	*brk;

int
main( argc, argv )
int argc;
char *argv[];
{
	brk = sbrk( 0 );
	printf( "etext = 0x%x\n", etext );
	printf( "&etext = 0x%x\n", &etext );
	printf( "edata = 0x%x\n", edata );
	printf( "&edata = 0x%x\n", &edata );
	printf( "end = 0x%x\n", end );
	printf( "&end = 0x%x\n", &end );
	printf( "Buf = 0x%x\n", Buf );
	printf( "foo = 0x%x\n", foo );
	printf( "brk = 0x%x\n", brk );
}

void
foo()
{
}
