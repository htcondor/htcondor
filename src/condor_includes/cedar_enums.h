#ifndef CEDAR_ENUMS_H
#define CEDAR_ENUMS_H

/* Special enums we use inorder to have nice overloads of the cedar code()
	function. It is very important that we have these assigned to intmax like
	you see here. This makes the enum able to be a 32-bit quantity.
	Scary, isn't it? */
enum condor_signal_t { __signal_t_dummy_value = INT_MAX };
enum open_flags_t { __open_flags_t_dummy_value = INT_MAX };
enum fcntl_cmd_t { __fcntl_cmd_t_dummy_value = INT_MAX };

#endif
