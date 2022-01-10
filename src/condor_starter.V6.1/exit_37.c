
#ifdef X86_64

// This binary needs to be statically linked, so that we can run it
// inside an arbitrary container that may not even have a shared libc in it.
// Statically linking a program with glibc results in a (stripped) binary
// almost a megabyte in size, which is almost 10% of our glidein download size
// writing the following assembly, which just syscalls exit( syscall #60)
// results in a 1k binary
void _start() {
	asm(
			"mov $60, %rax;"
			"mov $37, %rdi;"
			"syscall;");
}


#else

// All other platforms go here
#include <stdlib.h>

int main() {
	exit(37);
}

#endif
