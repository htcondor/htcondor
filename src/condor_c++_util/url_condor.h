#if !defined(_URL_CONDOR_H)
#define _URL_CONDOR_H

#include <sys/types.h>
#include <limits.h>

typedef int (*open_function_type)(const char *, int, size_t);

class URLProtocol {
public:
	URLProtocol(char *protocol_key, char *protocol_name, 
				open_function_type protocol_open_func);
/*				int (*protocol_open_func)(const char *, int, size_t)); */
	~URLProtocol();
	char	*get_key() { return key; }
	char	*get_name() { return name; }
	int		call_open_func(const char *fname, int flags, size_t n_bytes)
	{ return open_func( fname, flags, n_bytes); }
	URLProtocol		*get_next() { return next; }
	void	init() { }
private:
	char	*key;
	char	*name;
	open_function_type open_func;
/*	int		(*open_func)(const char *, int, size_t); */
	URLProtocol	*next;
};

URLProtocol	*FindProtocolByKey(const char *key);
extern "C" int open_url( const char *name, int flags, size_t n_bytes );
#endif
