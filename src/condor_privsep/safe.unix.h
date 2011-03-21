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

typedef struct id_range_list_elem {
    id_t min_value;
    id_t max_value;
} id_range_list_elem;

typedef struct id_range_list {
    int count;
    int capacity;
    id_range_list_elem *list;
} id_range_list;

typedef struct string_list {
    int count;
    int capacity;
    char **list;
} string_list;

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

void init_id_range_list(id_range_list *list);
void add_id_range_to_list(id_range_list *list, id_t min_id, id_t max_id);
void add_id_to_list(id_range_list *list, id_t id);
void destroy_id_range_list(id_range_list *list);
int is_id_in_list(id_range_list *list, id_t id);
int is_id_list_empty(id_range_list *list);

const char *skip_whitespace_const(const char *s);
char *skip_whitespace(char *s);
char *trim_whitespace(const char *s, const char *endp);

id_t parse_id(const char *value, const char **endptr);
uid_t parse_uid(const char *value, const char **endptr);
gid_t parse_gid(const char *value, const char **endptr);
void parse_id_list(id_range_list *list, const char *value,
                   const char **endptr);
void parse_uid_list(id_range_list *list, const char *value,
                    const char **endptr);
void parse_gid_list(id_range_list *list, const char *value,
                    const char **endptr);

int safe_reset_environment(void);
int safe_close_fds_between(int min_fd, int max_fd);
int safe_close_fds_starting_at(int min_fd);
int safe_close_fds_except(id_range_list *ids);

int safe_open_std_file(int fd, const char *filename);
int safe_open_std_files_to_null(void);

int safe_switch_to_uid(uid_t uid,
                       gid_t tracking_gid,
                       id_range_list *safe_uids,
                       id_range_list *safe_gids);
int safe_switch_effective_to_uid(uid_t uid,
                                 id_range_list *safe_uids,
                                 id_range_list *safe_gids);

int safe_exec_as_user(uid_t uid,
                      gid_t tracking_gid,
                      id_range_list *safe_uids,
                      id_range_list *safe_gids,
                      const char *exec_name,
                      char **args,
                      char **env,
                      id_range_list *keep_open_fds,
                      const char *stdin_filename,
                      const char *stdout_filename,
                      const char *stderr_filename,
                      const char *initial_dir,
                      int is_std_univ);

enum { PATH_UNTRUSTED =
        0, PATH_TRUSTED_STICKY_DIR, PATH_TRUSTED, PATH_TRUSTED_CONFIDENTIAL
};

int safe_is_path_trusted(const char *pathname,
                         id_range_list *trusted_uids,
                         id_range_list *trusted_gids);
int safe_is_path_trusted_fork(const char *pathname,
                              id_range_list *trusted_uids,
                              id_range_list *trusted_gids);
int safe_is_path_trusted_r(const char *pathname,
                           id_range_list *trusted_uids,
                           id_range_list *trusted_gids);

typedef int (*safe_dir_walk_func) (const char *path,
                                   const struct stat * sb, void *data);

int safe_dir_walk(const char *path, safe_dir_walk_func func, void *data,
                  int num_fds);

int safe_open_no_follow(const char* path, int* fd_ptr, struct stat* st);

#endif
