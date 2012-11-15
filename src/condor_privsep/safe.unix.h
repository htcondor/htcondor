/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef SAFE_H_
#define SAFE_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include "config.h"
#include "safe_is_path_trusted.h"
#include "safe_id_range_list.h"

#if !HAVE_ID_T
typedef uid_t id_t;
#endif

#if !HAVE_SETEUID
#define seteuid(euid) setreuid(-1, euid)
#endif

#if !HAVE_SETEGID
#define setegid(egid) setregid(-1, egid)
#endif

/* id used when an error occurs */
extern const id_t err_id;

typedef struct string_list {
    int count;
    int capacity;
    char **list;
} string_list;

void nonfatal_write(const char *fmt, ...);
void fatal_error_exit(int status, const char *fmt, ...);
void setup_err_stream(int fd);
int get_error_fd(void);

void init_string_list(string_list *list);
void add_string_to_list(string_list *list, char *s);
void prepend_string_to_list(string_list *list, char *s);
void destroy_string_list(string_list *list);
char **null_terminated_list_from_string_list(string_list *list);
int is_string_in_list(string_list *list, const char *s);
int is_string_list_empty(string_list *list);

const char *skip_whitespace_const(const char *s);
char *skip_whitespace(char *s);
char *trim_whitespace(const char *s, const char *endp);

int safe_reset_environment(void);
int safe_close_fds_between(int min_fd, int max_fd);
int safe_close_fds_starting_at(int min_fd);
int safe_close_fds_except(safe_id_range_list *ids);

int safe_open_std_file(int fd, const char *filename);
int safe_open_std_files_to_null(void);

int safe_switch_to_uid(uid_t uid,
                       gid_t tracking_gid,
                       safe_id_range_list *safe_uids,
                       safe_id_range_list *safe_gids);
int safe_switch_effective_to_uid(uid_t uid,
                                 safe_id_range_list *safe_uids,
                                 safe_id_range_list *safe_gids);

int safe_exec_as_user(uid_t uid,
                      gid_t tracking_gid,
                      safe_id_range_list *safe_uids,
                      safe_id_range_list *safe_gids,
                      const char *exec_name,
                      char **args,
                      char **env,
                      safe_id_range_list *keep_open_fds,
                      const char *stdin_filename,
                      const char *stdout_filename,
                      const char *stderr_filename,
                      const char *initial_dir,
                      int is_std_univ);

typedef int (*safe_dir_walk_func) (const char *path,
                                   const struct stat * sb, void *data);

int safe_dir_walk(const char *path, safe_dir_walk_func func, void *data,
                  int num_fds);

int safe_open_no_follow(const char* path, int* fd_ptr, struct stat* st);

#endif
