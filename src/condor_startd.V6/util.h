#ifndef _UTIL_H
#define _UTIL_H

class ClassAd;
class Stream;

// Our utilities 
void	cleanup_execute_dir(void);
void	check_perms(void);
float	compute_rank( ClassAd*, ClassAd* );
int		create_port( int* );
void	log_ignore( int, State );
char*	command_to_string( int );
int		reply( Stream*, int );
int		caInsert( ClassAd* target, ClassAd* source, const char* attr, int verbose = 0 );

// Utils from the util_lib that aren't prototyped
extern "C" {
	int		get_random_int();
	int		set_seed( int );
}

#endif _UTIL_H
