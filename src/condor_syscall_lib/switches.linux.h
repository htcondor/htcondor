#include <unistd.h>

#define PIPE(x) (__libc_pipe(x))
#define FORK(x) (__libc_fork(x))
#define GETEGID(x)  (__libc_getegid(x))
#define GETPPID(x)  (__libc_getppid(x))
#define GETEUID(x)  (__libc_geteuid(x))


#define REMAP_LINUX_ZERO(old,new) \
new( arg_1 ) \
{ \
return old(); \
}

#define REMAP_LINUX_ONE(old,new,type_1) \
new( arg_1 ) \
type_1 arg_1; \
{ \
return old( arg_1 ); \
}

#define REMAP_LINUX_TWO(old,new,type_1,type_2) \
new( arg_1, arg_2, arg_3 ) \
type_1 arg_1; \
type_2 arg_2; \
{ \
return old( arg_1, arg_2 ); \
}

#define REMAP_LINUX_THREE(old,new,type_1,type_2,type_3) \
new( arg_1, arg_2, arg_3 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
{ \
return old( arg_1, arg_2, arg_3 ); \
}

#define REMAP_LINUX_FIVE(old,new,type_1,type_2,type_3,type_4,type_5) \
new( arg_1, arg_2, arg_3, arg_4, arg_5 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
type_4 arg_4; \
type_5 arg_5; \
{ \
return old( arg_1, arg_2, arg_3, arg_4, arg_5 ); \
}

#define REMAP_LINUX_SIX(old,new,type_1,type_2,type_3,type_4,type_5,type_6) \
new( arg_1, arg_2, arg_3, arg_4, arg_5, arg_6 ) \
type_1 arg_1; \
type_2 arg_2; \
type_3 arg_3; \
type_4 arg_4; \
type_5 arg_5; \
type_6 arg_6; \
{ \
return old( arg_1, arg_2, arg_3, arg_4, arg_5, arg_6 ); \
}

/*
extern int __access(__const char *__name, int __type)
{
	return(access(__name, __type));
}
extern int __pipe(int __pipedes[2])
{
	return(pipe(__pipedes));
}
extern int __chown(__const char *__file, __uid_t __owner, __gid_t __group)
{
	return(chown(__file, __owner, __group));
}
extern int __fchown(int __fd, __uid_t __owner, __gid_t __group)
{
	return(fchown(__fd, __owner, __group));
}
extern int __fchdir(int __fd)
{
	return(fchdir(__fd));
}
extern int __chdir(__const char *__path)
{
	return(chdir(__path));
}
extern int __dup(int __fd)
{
	return(dup(__fd));
}
extern int __dup2(int __fd, int __fd2)
{
	return(dup2(__fd, __fd2));
}
extern int __execve(__const char *__path, char *__const __argv[], char *__const __envp[])
{
	return(execve(__path, __argv, __envp));
}
extern long int __pathconf(__const char *__path, int __name)
{
	return(pathconf(__path, __name));
}
extern long int __fpathconf(int __fd, int __name)
{
	return(fpathconf(__fd, __name));
}
extern long int __sysconf(int __name)
{
	return(sysconf(__name));
}
extern __pid_t __setsid(void)
{
	return(setsid());
}
extern __uid_t __getuid(void)
{
	return(getuid());
}
extern __uid_t __geteuid(void)
{
	return(geteuid());
}
extern __gid_t __getgid(void)
{
	return(getgid());
}
extern __gid_t __getegid(void)
{
	return(getegid());
}
extern int __getgroups(int __size, __gid_t __list[])
{
	return(getgroups(__size, __list));
}
extern int __setuid(__uid_t __uid)
{
	return(setuid(__uid));
}
extern int __setreuid(__uid_t __ruid, __uid_t __euid)
{
	return(setreuid(__ruid, __euid));
}
extern int __setgid(__gid_t __gid)
{
	return(setgid(__gid));
}
extern int __setregid __P ((__gid_t __rgid, __gid_t __egid));
extern __pid_t __fork __P ((void));
extern __pid_t __vfork __P ((void));
extern int __isatty __P ((int __fd));
extern int __link __P ((__const char *__from, __const char *__to));
extern int __symlink __P ((__const char *__from, __const char *__to));
extern int __readlink __P ((__const char *__path, char *__buf, size_t __len));
extern int __unlink __P ((__const char *__name));
extern int __rmdir __P ((__const char *__path));
extern int __gethostname __P ((char *__name, size_t __len));
extern size_t __getpagesize __P ((void));
extern int __getdtablesize __P ((void));
extern int	__brk __P ((void* __end_data_segment));
extern void*	__sbrk __P ((ptrdiff_t __increment));
*/


extern ssize_t __write(int __fd, const __ptr_t __buf, size_t __nbytes)
{
	return(write(__fd, __buf, __nbytes));
}
extern __off_t __lseek( int __fd, __off_t __offset, int __whence)
{
	return(lseek(__fd, __offset, __whence));
}
extern ssize_t __read(int __fd, __ptr_t __buf, size_t __nbytes)
{
	return(read(__fd, __buf, __nbytes));
}
extern int __fcntl(int __fd, int __cmd, long __arg)
{
	return(fcntl(__fd, __cmd, __arg));
}
