#ifndef _UTIL_H
#define _UTIL_H

class ClassAd;
class Stream;

// Our utilities 
void	cleanup_execute_dir(void);
void	check_perms(void);
float	compute_rank( ClassAd*, ClassAd* );
int		wants_vacate( Resource* );
int		wants_suspend( Resource* );
int		eval_kill( Resource* );
int		eval_vacate( Resource* );
int		eval_suspend( Resource* );
int		eval_continue( Resource* );
int		create_port( int* );
int		config_classad( Resource* );
void	log_ignore( int, State );
char*	command_to_string( int );
int		reply( Stream*, int );

// Utils from the util_lib that aren't prototyped
extern "C" {
	int		get_random_int();
	int		set_seed( int );
}

#endif _UTIL_H
