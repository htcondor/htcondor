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


#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "safe.unix.h"
#include "parse_config.unix.h"

extern char **environ;

#if !HAVE_LCHOWN
#define lchown chown
#endif

/* user and group ids that are secure, if these can be compromised so can root */
#undef  CONF_SAFE_UIDS
#define CONF_SAFE_GIDS			"root"

/* the path the main configuration file */
#define CONF_FILE			"/etc/condor/privsep_config"

safe_id_range_list conf_safe_uids;
safe_id_range_list conf_safe_gids;

FILE *cmd_conf_stream = 0;
const char *cmd_conf_stream_name = 0;

typedef struct configuration {
    safe_id_range_list valid_caller_uids;
    safe_id_range_list valid_caller_gids;

    safe_id_range_list valid_target_uids;
    safe_id_range_list valid_target_gids;

    string_list valid_dirs;

    char *transferd_executable;
    char *procd_executable;

    int limit_user_executables;
    string_list valid_user_executables;
    string_list valid_user_executable_dirs;
} configuration;

static void init_configuration(configuration *c)
{
    safe_init_id_range_list(&c->valid_caller_uids);
    safe_init_id_range_list(&c->valid_caller_gids);

    safe_init_id_range_list(&c->valid_target_uids);
    safe_init_id_range_list(&c->valid_target_gids);
    init_string_list(&c->valid_dirs);

    c->transferd_executable = 0;
    c->procd_executable = 0;

    c->limit_user_executables = 0;
    init_string_list(&c->valid_user_executables);
    init_string_list(&c->valid_user_executable_dirs);
}

static void validate_configuration(configuration *c)
{
    if (safe_is_id_list_empty(&c->valid_caller_uids)) {
        fatal_error_exit(1,
                         "valid-caller-uids not set in configuration file %s",
                         CONF_FILE);
    }

    if (safe_is_id_list_empty(&c->valid_caller_gids)) {
        fatal_error_exit(1,
                         "valid-caller-gids not set in configuration file %s",
                         CONF_FILE);
    }

    if (safe_is_id_list_empty(&c->valid_target_uids)) {
        fatal_error_exit(1,
                         "valid-target-uids not set in configuration file %s",
                         CONF_FILE);
    }

    if (safe_is_id_list_empty(&c->valid_target_gids)) {
        fatal_error_exit(1,
                         "valid-target-gids not set in configuration file %s",
                         CONF_FILE);
    }

    if (!c->procd_executable) {
        fatal_error_exit(1,
                         "procd-executable not set in configuration file %s",
                         CONF_FILE);
    }

    if (!is_string_list_empty(&c->valid_user_executables)) {
        fatal_error_exit(1,
                         "valid-user-executables not implemented in configuration file %s",
                         CONF_FILE);
    }

    if (!is_string_list_empty(&c->valid_user_executable_dirs)) {
        fatal_error_exit(1,
                         "valid-user-executable-dirs not implemented in configuration file %s",
                         CONF_FILE);
    }
}

typedef struct exec_params {
    uid_t user_uid;
    string_list exec_args;
    string_list exec_env;
    char *exec_filename;
    char *exec_init_dir;
    char *stdin_filename;
    char *stdout_filename;
    char *stderr_filename;
    int is_std_univ;
    gid_t tracking_group;
    safe_id_range_list keep_open_fds;
} exec_params;

static void init_exec_params(exec_params *c)
{
    c->user_uid = 0;

    init_string_list(&c->exec_args);
    init_string_list(&c->exec_env);

    c->exec_filename = 0;
    c->exec_init_dir = 0;
    c->stdin_filename = 0;
    c->stdout_filename = 0;
    c->stderr_filename = 0;

    c->is_std_univ = 0;

    c->tracking_group = 0;

    safe_init_id_range_list(&c->keep_open_fds);
}

static void validate_exec_params(exec_params *c)
{
    if (c->user_uid == 0) {
        fatal_error_exit(1, "user-uid not set");
    }

    if (!c->exec_filename) {
        fatal_error_exit(1, "exec-path not set");
    }

    if (!c->exec_init_dir) {
        fatal_error_exit(1, "exec-init-dir not set");
    }
}

typedef struct dir_cmd_params {
    uid_t user_uid;
    char *user_dir;
    uid_t chown_source_uid;
} dir_cmd_params;

static void init_dir_cmd_params(dir_cmd_params *c)
{
    c->user_uid = 0;
    c->user_dir = 0;
    c->chown_source_uid = 0;
}

static void validate_dir_cmd_params(dir_cmd_params *c, int is_rmdir, int is_chown_dir)
{
    if (!is_rmdir && (c->user_uid == 0)) {
        fatal_error_exit(1, "user-uid not set");
    }

    if (!c->user_dir) {
        fatal_error_exit(1, "user-dir not set");
    }

    if (is_chown_dir && (c->chown_source_uid == 0)) {
        fatal_error_exit(1, "chown-source-uid not set");
    }
}

typedef struct transferd_params {
    uid_t user_uid;
} transferd_params;

static void init_transferd_params(transferd_params *c)
{
    c->user_uid = 0;
}

static void validate_transferd_params(transferd_params *c)
{
    if (c->user_uid == 0) {
        fatal_error_exit(1, "user-uid not set");
    }
}

typedef enum config_key_enum {
    VALID_CALLER_UIDS,
    VALID_CALLER_GIDS,
    VALID_TARGET_UIDS,
    VALID_TARGET_GIDS,
    VALID_DIRS,
    TRANSFERD_EXECUTABLE,
    PROCD_EXECUTABLE,
    VALID_USER_EXECUTABLES,
    VALID_USER_EXECUTABLE_DIRS,

    USER_UID,
    USER_DIR,

    EXEC_ARG,
    EXEC_ENV,
    EXEC_PATH,
    EXEC_INIT_DIR,
    EXEC_STDIN,
    EXEC_STDOUT,
    EXEC_STDERR,
    EXEC_IS_STD_UNIV,
    EXEC_TRACKING_GROUP,
    EXEC_KEEP_OPEN_FDS,

    CHOWN_SOURCE_UID,

    INVALID
} config_key_enum;

typedef struct key_to_enum {
    config_key_enum e;
    const char *s;
} key_to_enum;

key_to_enum key_to_enum_map[] = {
    {VALID_CALLER_UIDS, "valid-caller-uids"},
    {VALID_CALLER_GIDS, "valid-caller-gids"},
    {VALID_TARGET_UIDS, "valid-target-uids"},
    {VALID_TARGET_GIDS, "valid-target-gids"},
    {VALID_DIRS, "valid-dirs"},
    {TRANSFERD_EXECUTABLE, "transferd-executable"},
    {PROCD_EXECUTABLE, "procd-executable"},
    {VALID_USER_EXECUTABLES, "valid-user-executables"},
    {VALID_USER_EXECUTABLE_DIRS, "valid-user-executable-dirs"},

    {USER_UID, "user-uid"},
    {USER_DIR, "user-dir"},

    {EXEC_ARG, "exec-arg"},
    {EXEC_ENV, "exec-env"},
    {EXEC_PATH, "exec-path"},
    {EXEC_INIT_DIR, "exec-init-dir"},
    {EXEC_STDIN, "exec-stdin"},
    {EXEC_STDOUT, "exec-stdout"},
    {EXEC_STDERR, "exec-stderr"},
    {EXEC_IS_STD_UNIV, "exec-is-std-univ"},
    {EXEC_TRACKING_GROUP, "exec-tracking-group"},
    {EXEC_KEEP_OPEN_FDS, "exec-keep-open-fd"},

    {CHOWN_SOURCE_UID, "chown-source-uid"}
};

static config_key_enum key_name_to_enum(const char *key)
{
    unsigned int i;

    for (i = 0; i < sizeof key_to_enum_map / sizeof key_to_enum_map[0];
         ++i) {
        if (!strcmp(key_to_enum_map[i].s, key)) {
            return key_to_enum_map[i].e;
        }
    }

    return INVALID;
}

static void
check_id_error(const char *key, const char *value, config_file *cf,
               const char *endptr, const char *id_name)
{
    const char *last_nonwhite_char = skip_whitespace_const(endptr);

    if (errno == ERANGE) {
        fatal_error_exit(1,
                         "option '%s' has an out of range %s in file: %s:%d",
                         key, id_name, cf->filename, cf->line_num);
    } else if (errno == EINVAL
               || (value == endptr && *last_nonwhite_char != '\0')) {
        fatal_error_exit(1, "option '%s' has an invalid %s in file: %s:%d",
                         key, id_name, cf->filename, cf->line_num);
    } else if (value == endptr) {
        fatal_error_exit(1,
                         "option '%s' has an empty value in file: %s:%d",
                         key, cf->filename, cf->line_num);
    } else if (errno != 0) {
        fatal_error_exit(1,
                         "option '%s' has an unexpected error (%s) in file: %s:%d",
                         key, strerror(errno), cf->filename, cf->line_num);
    } else if (*last_nonwhite_char != '\0') {
        fatal_error_exit(1,
                         "option '%s' has a syntax error in %s list in file: %s:%d",
                         key, id_name, cf->filename, cf->line_num);
    }
}

#if 0
static void
config_parse_id(id_t *id, const char *key, const char *value,
                config_file *cf)
{
    const char *endptr;

    *id = parse_id(value, &endptr);

    check_id_error(key, value, cf, endptr, "id");
}
#endif

static void
config_parse_uid(uid_t * uid, const char *key, const char *value,
                 config_file *cf)
{
    const char *endptr;

    *uid = safe_strto_uid(value, &endptr);

    check_id_error(key, value, cf, endptr, "uid");
}

static void
config_parse_gid(gid_t * gid, const char *key, const char *value,
                 config_file *cf)
{
    const char *endptr;

    *gid = safe_strto_gid(value, &endptr);

    check_id_error(key, value, cf, endptr, "gid");
}

static void
config_parse_id_list(safe_id_range_list *list, const char *key,
                     const char *value, config_file *cf)
{
    const char *endptr;

    safe_strto_id_list(list, value, &endptr);

    check_id_error(key, value, cf, endptr, "id");
}

static void
config_parse_uid_list(safe_id_range_list *list, const char *key,
                      const char *value, config_file *cf)
{
    const char *endptr;

    safe_strto_uid_list(list, value, &endptr);

    check_id_error(key, value, cf, endptr, "uid");
}

static void
config_parse_gid_list(safe_id_range_list *list, const char *key,
                      const char *value, config_file *cf)
{
    const char *endptr;

    safe_strto_gid_list(list, value, &endptr);

    check_id_error(key, value, cf, endptr, "gid");
}

static void
config_parse_string(char **s, const char *key, char *value,
                    config_file *cf)
{
    if (!value) {
        fatal_error_exit(1,
                         "option '%s' requires a value in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    if (value[0] == '\0') {
        fatal_error_exit(1,
                         "option '%s' requires a non-empty value in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    if (*s) {
        fatal_error_exit(1,
                         "option '%s' already set in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    *s = value;
}

static void
config_parse_string_list(string_list *list, const char *key, char *value,
                         config_file *cf)
{
    if (!value) {
        fatal_error_exit(1,
                         "option '%s' requires a value in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    if (value[0] == '\0') {
        fatal_error_exit(1,
                         "option '%s' requires a non-empty value in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    add_string_to_list(list, value);
}

static void
config_parse_bool(int *b, const char *key, const char *value,
                  config_file *cf)
{
    if (value) {
        fatal_error_exit(1,
                         "option '%s' not allowed to take a value in configuration: %s:%d",
                         key, cf->filename, cf->line_num);
    }

    *b = 1;
}

static int process_config_file(configuration *c, const char *filename)
{
    config_file cf;
    char *key;
    char *value;
    int r;

    init_configuration(c);

    r = safe_is_path_trusted(filename, &conf_safe_uids, &conf_safe_gids);
    if (r < 0) {
        fatal_error_exit(1,
                         "error in checking safety of configuration file path (%s)",
                         filename);
    } else if (r == SAFE_PATH_UNTRUSTED) {
        fatal_error_exit(1,
                         "unsafe permissions in configuration file path (%s)",
                         filename);
    }

    if (open_config_file(&cf, filename)) {
        return 1;
    }

    while (next_cf_value(&cf, &key, &value)) {
        config_key_enum e = key_name_to_enum(key);
        switch (e) {
        case VALID_CALLER_UIDS:
            config_parse_uid_list(&c->valid_caller_uids, key, value, &cf);
            break;
        case VALID_CALLER_GIDS:
            config_parse_gid_list(&c->valid_caller_gids, key, value, &cf);
            break;
        case VALID_TARGET_UIDS:
            config_parse_uid_list(&c->valid_target_uids, key, value, &cf);
            break;
        case VALID_TARGET_GIDS:
            config_parse_gid_list(&c->valid_target_gids, key, value, &cf);
            break;
        case VALID_DIRS:
            config_parse_string_list(&c->valid_dirs, key, value, &cf);
            break;
        case TRANSFERD_EXECUTABLE:
            config_parse_string(&c->transferd_executable, key, value, &cf);
            break;
        case PROCD_EXECUTABLE:
            config_parse_string(&c->procd_executable, key, value, &cf);
            break;
        case VALID_USER_EXECUTABLES:
            config_parse_string_list(&c->valid_user_executables, key,
                                     value, &cf);
            break;
        case VALID_USER_EXECUTABLE_DIRS:
            config_parse_string_list(&c->valid_user_executable_dirs, key,
                                     value, &cf);
            break;
        default:
            fatal_error_exit(1,
                             "unknown option '%s' in configuration: %s:%d",
                             key, cf.filename, cf.line_num);
            break;
        }

        free((char *) key);
    }

    close_config_file(&cf);

    validate_configuration(c);

    return 0;
}

static int process_user_exec_config(exec_params *c, int do_validate)
{
    config_file cf;
    char *key;
    char *value;

    init_exec_params(c);

    if (open_config_stream(&cf, cmd_conf_stream_name, cmd_conf_stream)) {
        return 1;
    }

    while (next_cf_value(&cf, &key, &value)) {
        config_key_enum e = key_name_to_enum(key);
        switch (e) {
        case USER_UID:
            config_parse_uid(&c->user_uid, key, value, &cf);
            break;
        case EXEC_ARG:
            config_parse_string_list(&c->exec_args, key, value, &cf);
            break;
        case EXEC_ENV:
            config_parse_string_list(&c->exec_env, key, value, &cf);
            break;
        case EXEC_PATH:
            config_parse_string(&c->exec_filename, key, value, &cf);
            break;
        case EXEC_INIT_DIR:
            config_parse_string(&c->exec_init_dir, key, value, &cf);
            break;
        case EXEC_STDIN:
            config_parse_string(&c->stdin_filename, key, value, &cf);
            break;
        case EXEC_STDOUT:
            config_parse_string(&c->stdout_filename, key, value, &cf);
            break;
        case EXEC_STDERR:
            config_parse_string(&c->stderr_filename, key, value, &cf);
            break;
        case EXEC_IS_STD_UNIV:
            config_parse_bool(&c->is_std_univ, key, value, &cf);
            break;
        case EXEC_TRACKING_GROUP:
            config_parse_gid(&c->tracking_group, key, value, &cf);
            break;
        case EXEC_KEEP_OPEN_FDS:
            config_parse_id_list(&c->keep_open_fds, key, value, &cf);
            break;
        default:
            fatal_error_exit(1,
                             "unknown option '%s' in configuration: %s:%d",
                             key, cf.filename, cf.line_num);
            break;
        }

        free((char *) key);
    }

    close_config_stream(&cf);

    if (do_validate) {
        validate_exec_params(c);
    }

    return 0;
}

static int process_dir_cmd_config(dir_cmd_params *c, int is_rmdir, int is_chown_dir)
{
    config_file cf;
    char *key;
    char *value;

    init_dir_cmd_params(c);

    if (open_config_stream(&cf, cmd_conf_stream_name, cmd_conf_stream)) {
        return 1;
    }

    while (next_cf_value(&cf, &key, &value)) {
        config_key_enum e = key_name_to_enum(key);
        switch (e) {
        case USER_UID:
            config_parse_uid(&c->user_uid, key, value, &cf);
            break;
        case USER_DIR:
            config_parse_string(&c->user_dir, key, value, &cf);
            break;
        case CHOWN_SOURCE_UID:
            config_parse_uid(&c->chown_source_uid, key, value, &cf);
            break;
        default:
            fatal_error_exit(1,
                             "unknown option '%s' in configuration: %s:%d",
                             key, cf.filename, cf.line_num);
            break;
        }

        free((char *) key);
    }

    close_config_stream(&cf);

    validate_dir_cmd_params(c, is_rmdir, is_chown_dir);

    return 0;
}

static int process_transferd_config(transferd_params *c)
{
    config_file cf;
    char *key;
    char *value;

    init_transferd_params(c);

    if (open_config_stream(&cf, cmd_conf_stream_name, cmd_conf_stream)) {
        return 1;
    }

    while (next_cf_value(&cf, &key, &value)) {
        config_key_enum e = key_name_to_enum(key);
        switch (e) {
        case USER_UID:
            config_parse_uid(&c->user_uid, key, value, &cf);
            break;
        default:
            fatal_error_exit(1,
                             "unknown option '%s' in configuration: %s:%d",
                             key, cf.filename, cf.line_num);
            break;
        }

        free((char *) key);
    }

    close_config_stream(&cf);

    validate_transferd_params(c);

    return 0;
}

void safe_conf_check_id_error(const char *value, const char *endptr,
                              const char *id_name);

void
safe_conf_check_id_error(const char *value, const char *endptr,
                         const char *id_name)
{
    const char *last_nonwhite_char = skip_whitespace_const(endptr);

    if (errno == ERANGE) {
        fatal_error_exit(1,
                         "<built-in> safe conf %s list has an out of range %s",
                         id_name, id_name);
    } else if (errno == EINVAL
               || (value == endptr && *last_nonwhite_char != '\0')) {
        fatal_error_exit(1,
                         "<built-in> safe conf %s list has an invalid %s",
                         id_name, id_name);
    } else if (value == endptr) {
        fatal_error_exit(1, "<built-in> safe conf %s list is empty",
                         id_name);
    } else if (errno != 0) {
        fatal_error_exit(1,
                         "<built-in> safe conf %s list has an unexpected error (%s)",
                         id_name, strerror(errno));
    } else if (*last_nonwhite_char != '\0') {
        fatal_error_exit(1,
                         "<built-in> safe conf %s list has a syntax error",
                         id_name);
    }
}

static void init_safe_conf_uids(void)
{
    const char *endptr;
    (void) endptr;

    safe_init_id_range_list(&conf_safe_uids);
    safe_init_id_range_list(&conf_safe_gids);

#ifdef CONF_SAFE_UIDS
    safe_strto_uid_list(&conf_safe_uids, CONF_SAFE_UIDS, &endptr);

    safe_conf_check_id_error(CONF_SAFE_UIDS, endptr, "uid");
#endif

#ifdef CONF_SAFE_GIDS
    safe_strto_gid_list(&conf_safe_gids, CONF_SAFE_GIDS, &endptr);

    safe_conf_check_id_error(CONF_SAFE_GIDS, endptr, "gid");
#endif
}

static void validate_caller_ids(configuration *c)
{
    uid_t caller_uid = getuid();
    gid_t caller_gid = getgid();

    if (!safe_is_id_in_list(&c->valid_caller_uids, caller_uid)) {
        fatal_error_exit(1, "invalid caller uid (%lu)",
                         (unsigned long) caller_uid);
    }

    if (!safe_is_id_in_list(&c->valid_caller_gids, caller_gid)) {
        fatal_error_exit(1, "invalid caller gid (%lu)",
                         (unsigned long) caller_gid);
    }
}

static char *do_common_dir_cmd_tasks(configuration *c,
                                     dir_cmd_params *dir_cmd_conf,
                                     int is_rmdir,
                                     int is_chown_dir)
{
    const char *pathname;
    int r;
    char *dir_parent;
    char *dir_name;

    process_dir_cmd_config(dir_cmd_conf, is_rmdir, is_chown_dir);

    pathname = dir_cmd_conf->user_dir;

    dir_parent = strdup(pathname);
    if (dir_parent == NULL) {
        fatal_error_exit(1, "strdup of path failed");
    }

    dir_name = strrchr(dir_parent, '/');
    if (dir_name == NULL) {
        fatal_error_exit(1,
                         "directory without parent directory specified");
    }

    *dir_name++ = '\0';

    if (!is_string_in_list(&c->valid_dirs, dir_parent)) {
        fatal_error_exit(1,
                         "directory is not in list of valid dirs");
    }

    r = safe_is_path_trusted(dir_parent, &conf_safe_uids, &conf_safe_gids);
    if (r < 0) {
        fatal_error_exit(1,
                         "error in checking safety of directory parent (%s)",
                         dir_parent);
    } else if (r == SAFE_PATH_UNTRUSTED) {
        fatal_error_exit(1,
                         "unsafe permissions in directory parent (%s)",
                         dir_parent);
    }

    r = chdir(dir_parent);
    if (r == -1) {
        fatal_error_exit(1, "error chdir to directory parent (%s)",
                         dir_parent);
    }

    /* check for empty, '.', and '..' leaf directory names */
    if (!*dir_name || !strcmp(dir_name, ".") || !strcmp(dir_name, "..")) {
        fatal_error_exit(1, "invalid leaf directory (%s)", dir_name);
    }

    dir_name = strdup(dir_name);
    if (dir_name == NULL) {
        fatal_error_exit(1, "strdup of dir name failed");
    }

    free(dir_parent);

    return dir_name;
}

static void do_mkdir(configuration *c)
{
    dir_cmd_params dir_cmd_conf;
    char *dir_name;
    int r;
    uid_t uid;
    gid_t gid;

    dir_name = do_common_dir_cmd_tasks(c, &dir_cmd_conf, 0, 0);

    uid = dir_cmd_conf.user_uid;
    r = safe_switch_effective_to_uid(uid,
                                     &c->valid_target_uids,
                                     &c->valid_target_gids);
    if (r < 0) {
        r = safe_switch_effective_to_uid(uid,
                                         &c->valid_caller_uids,
                                         &c->valid_caller_gids);
    }
    if (r < 0) {
        fatal_error_exit(1, "error switching user to uid %lu",
                         (unsigned long) uid);
    }

    gid = getegid();

    r = seteuid(0);
    if (r == -1) {
        fatal_error_exit(1, "error switching user to root");
    }

    // make the directory private by default.  ideally, the permissions to use
    // should be a parameter that is passed in.  ZKM TODO
    r = mkdir(dir_name, 0700);
    if (r == -1) {
        fatal_error_exit(1, "error creating directory %s", dir_name);
    }

    r = chown(dir_name, uid, gid);
    if (r == -1) {
        fatal_error_exit(1, "error chown'ing dir (%s) to uid=%lu gid=%lu",
                         dir_name, (unsigned long) uid,
                         (unsigned long) gid);
    }

    free(dir_name);
}

static int
rmdir_func(const char *filename, const struct stat *stat_buf, void *data)
{
    int r = 0;
    (void) data;

    if (stat_buf) {
        if (S_ISDIR(stat_buf->st_mode)) {
            r = rmdir(filename);
        } else {
            r = unlink(filename);
        }
    } else {
        /* no stat, try both */
        r = unlink(filename);
        if (r == -1 && errno == EISDIR) {
            r = rmdir(filename);
        }
    }

    return r;
}

static void do_rmdir(configuration *c)
{
    dir_cmd_params dir_cmd_conf;
    char *dir_name;
    int r;
    struct stat stat_buf;

    dir_name = do_common_dir_cmd_tasks(c, &dir_cmd_conf, 1, 0);

    r = lstat(dir_name, &stat_buf);
    if (r == -1) {
        fatal_error_exit(1, "error stat'ing leaf dir (%s)", dir_name);
    }

    if (!S_ISDIR(stat_buf.st_mode)) {
        fatal_error_exit(1, "leaf dir is not a directory (%s)", dir_name);
    }

    r = safe_dir_walk(dir_name, rmdir_func, 0, 64);
    if (r != 0) {
        fatal_error_exit(1, "error in recursive delete of directory");
    }

    r = rmdir_func(dir_name, &stat_buf, 0);
    if (r != 0) {
        fatal_error_exit(1, "error in rmdir of directory (%s)", dir_name);
    }

    free(dir_name);
}

static int
dirusage_func(const char *, const struct stat *stat_buf, void *data)
{
    int r = 0;
	if (data == NULL) {
		// should never happen.  return error
		r = 1;
	} else if (stat_buf == NULL) {
		// should never happen.  return error
		r = 1;
	} else {
		off_t file_size = stat_buf->st_size;
		*((off_t*)data) += file_size;
	}
    return r;
}

static int do_dirusage(configuration *c)
{
    dir_cmd_params dir_cmd_conf;
    char *dir_name;
    int r;
    struct stat stat_buf;

    dir_name = do_common_dir_cmd_tasks(c, &dir_cmd_conf, 0, 0);

    r = lstat(dir_name, &stat_buf);
    if (r == -1) {
        fatal_error_exit(1, "error stat'ing leaf dir (%s)", dir_name);
    }

    if (!S_ISDIR(stat_buf.st_mode)) {
        fatal_error_exit(1, "leaf dir is not a directory (%s)", dir_name);
    }

    off_t total_size = 0;

    r = safe_dir_walk(dir_name, dirusage_func, &total_size, 64);
    if (r != 0) {
        fatal_error_exit(1, "error in recursive delete of directory");
    }
    free(dir_name);

    nonfatal_write("%ju", (intmax_t)total_size);
    return 0;
}

typedef struct uid_pair {
    uid_t uid;
    gid_t gid;
} uid_pair;

static int
chown_func(const char *filename, const struct stat *buf, void *data)
{
    int fd;
    struct stat stat_buf;
    uid_t source_uid;
    uid_pair *ids = (uid_pair *) data;
    int r;

    (void)buf;

    /*
       A job may hard link to something outside of its sandbox, which can
       even be owned by someone else. Therefore, we'll open and fstat (as
       the expected source user), then only fchown (as root) if the expected
       source user is correct. It is not an error if the file is already
       owned by the target uid, which might happen if the job hardlinks
	   two filenames togther, and the recursive chown already got one of
	   them.  Note that this function expects to be called
       with our EUID set to the (expected) source user. Also, detect and skip
       symlinks since their ownership is inconsequential.
    */
    if (safe_open_no_follow(filename, &fd, NULL) == -1) {
		if( stat(filename, &stat_buf)==0 && stat_buf.st_uid == ids->uid && stat_buf.st_gid == ids->gid ) {
				/* We don't have permission to open this file as
				 * the source user, but it is already owned by
				 * the target user, so all is well.
				 */
			return 0;
		}
        return -1;
    }
    if (fd == -1) {
        /* detected a symlink; skip it */
        return 0;
    }
    r = fstat(fd, &stat_buf);
    if (r == -1) {
        close(fd);
        return -1;
    }
    source_uid = geteuid();
    if ((stat_buf.st_uid != source_uid) && (stat_buf.st_uid != ids->uid)) {
        close(fd);
        return -1;
    }
    if (seteuid(0) == -1) {
        fatal_error_exit(1, "error switching user to root\n");
    }
    r = fchown(fd, ids->uid, ids->gid);
    if (seteuid(source_uid) == -1) {
        fatal_error_exit(1, "error switching user to uid %lu",
                         (unsigned long) source_uid);
    }
    close(fd);
    return r;
}

static void do_chown_dir(configuration *c)
{
    dir_cmd_params dir_cmd_conf;
    char *dir_name;
    uid_pair ids;
    uid_t uid;
    int r;

    dir_name = do_common_dir_cmd_tasks(c, &dir_cmd_conf, 0, 1);

    uid = dir_cmd_conf.user_uid;
    r = safe_switch_effective_to_uid(uid,
                                     &c->valid_target_uids,
                                     &c->valid_target_gids);
    if (r < 0) {
        r = safe_switch_effective_to_uid(uid,
                                         &c->valid_caller_uids,
                                         &c->valid_caller_gids);
    }
    if (r < 0) {
        fatal_error_exit(1, "error switching user to uid %lu",
                         (unsigned long) uid);
    }

    ids.uid = geteuid();
    ids.gid = getegid();

    r = seteuid(0);
    if (r == -1) {
        fatal_error_exit(1, "error switching user to root");
    }
    r = safe_switch_effective_to_uid(dir_cmd_conf.chown_source_uid,
                                     &c->valid_target_uids,
                                     &c->valid_target_gids);
    if (r < 0) {
        r = safe_switch_effective_to_uid(dir_cmd_conf.chown_source_uid,
                                         &c->valid_caller_uids,
                                         &c->valid_caller_gids);
    }
    if (r < 0) {
        fatal_error_exit(1, "error switching user to uid %lu",
                         (unsigned long) dir_cmd_conf.chown_source_uid);
    }

    r = safe_dir_walk(dir_name, chown_func, &ids, 64);
    if (r != 0) {
        fatal_error_exit(1, "error in recursive chown of directory");
    }

    r = chown_func(dir_name, 0, &ids);
    if (r != 0) {
        fatal_error_exit(1, "error in chown of directory (%s)", dir_name);
    }

    free(dir_name);
}

static void do_start_procd(configuration *c)
{
    safe_id_range_list all_ids;
    exec_params procd_conf;
    char **procd_argv;
    int r;

    safe_init_id_range_list(&all_ids);
    safe_add_id_range_to_list(&all_ids, 0, UINT_MAX);

    // read in the execution config (we'll only be looking
    // at the executable path and the argument list)
    //
    r = process_user_exec_config(&procd_conf, 0);
    if (r != 0) {
        fatal_error_exit(1, "error reading config for command: pd");
    }
    // checks on the given executable:
    //   1) does it match the configured procd binary?
    //   2) is its path safe?
    //
    if (strcmp(procd_conf.exec_filename, c->procd_executable) != 0) {
        fatal_error_exit(1,
                         "invalid procd executable given: %s",
                         procd_conf.exec_filename);
    }
    r = safe_is_path_trusted(procd_conf.exec_filename,
                             &conf_safe_uids, &conf_safe_gids);
    if (r < 0) {
        fatal_error_exit(1,
                         "error in checking safety of procd file path (%s)",
                         procd_conf.exec_filename);
    } else if (r == SAFE_PATH_UNTRUSTED) {
        fatal_error_exit(1, "unsafe permissions procd file path (%s)",
                         procd_conf.exec_filename);
    }
    // it's a go
    //
    procd_argv =
        null_terminated_list_from_string_list(&procd_conf.exec_args);
    r = safe_exec_as_user(0,
                          0,
                          &all_ids,
                          &all_ids,
                          procd_conf.exec_filename,
                          procd_argv,
                          environ,
                          &all_ids,
                          NULL,
                          NULL,
                          NULL,
                          "/",
                          0);
    if (r) {
        fatal_error_exit(1, "error exec'ing user job");
    }
}

static void do_start_transferd(configuration *c)
{
    transferd_params td_conf;
    int r;
    char *args[2];
    int error_fd;
    safe_id_range_list keep_open_fds;

    safe_init_id_range_list(&keep_open_fds);

    r = process_transferd_config(&td_conf);
    if (r != 0) {
        fatal_error_exit(1, "error reading transferd config");
    }

    error_fd = get_error_fd();
    if (error_fd != -1) {
        safe_add_id_to_list(&keep_open_fds, error_fd);
    }

    args[0] = c->transferd_executable;
    args[1] = 0;

    r = safe_exec_as_user(td_conf.user_uid,
                          0,
                          &c->valid_target_uids,
                          &c->valid_target_gids,
                          args[0],
                          args,
                          environ,
                          &keep_open_fds,
                          0,
                          0,
                          0,
                          "/",
                          0);
    if (r) {
        fatal_error_exit(1, "error exec'ing user job");
    }
}

static void do_exec_job(configuration *c)
{
    int r;
    int error_fd;
    exec_params exec_conf;

    r = process_user_exec_config(&exec_conf, 1);
    if (r != 0) {
        fatal_error_exit(1, "error reading user job config");
    }

    error_fd = get_error_fd();
    if (error_fd != -1) {
        safe_add_id_to_list(&exec_conf.keep_open_fds, error_fd);
    }

    r = safe_exec_as_user(exec_conf.user_uid,
                          exec_conf.tracking_group,
                          &c->valid_target_uids,
                          &c->valid_target_gids,
                          exec_conf.exec_filename,
                          null_terminated_list_from_string_list(&exec_conf.
                                                                exec_args),
                          null_terminated_list_from_string_list(&exec_conf.
                                                                exec_env),
                          &exec_conf.keep_open_fds,
                          exec_conf.stdin_filename,
                          exec_conf.stdout_filename,
                          exec_conf.stderr_filename,
                          exec_conf.exec_init_dir,
                          exec_conf.is_std_univ);

    if (r) {
        fatal_error_exit(1, "error exec'ing user job");
    }
}

static void configure_cmd_conf_stream(int fd)
{
    if (fd == 0) {
        cmd_conf_stream = stdin;
        cmd_conf_stream_name = "<stdin>";
    } else {
        cmd_conf_stream = fdopen(fd, "r");
        cmd_conf_stream_name = "<option_fd>";
    }

    if (cmd_conf_stream == NULL) {
        fatal_error_exit(1, "error opening option stream (%s)",
                         strerror(errno));
    }
}

static void do_command(const char *cmd, int cmd_fd, configuration *c)
{
    configure_cmd_conf_stream(cmd_fd);

    if (!strcmp(cmd, "pd")) {
        do_start_procd(c);
    } else if (!strcmp(cmd, "td")) {
        do_start_transferd(c);
    } else if (!strcmp(cmd, "exec")) {
        do_exec_job(c);
    } else if (!strcmp(cmd, "mkdir")) {
        do_mkdir(c);
    } else if (!strcmp(cmd, "rmdir")) {
        do_rmdir(c);
    } else if (!strcmp(cmd, "dirusage")) {
        do_dirusage(c);
    } else if (!strcmp(cmd, "chowndir")) {
        do_chown_dir(c);
    } else {
        fatal_error_exit(1, "unknown command: %s", cmd);
    }
}

int main(int argc, char **argv)
{
    configuration conf;
    char *cmd;
    int cmd_fd;
    int err_fd;

    if (argc != 4) {
        fatal_error_exit(1, "wrong number of arguments");
    }
    cmd = argv[1];
    cmd_fd = atoi(argv[2]);
    err_fd = atoi(argv[3]);

    setup_err_stream(err_fd);
    safe_reset_environment();
    init_safe_conf_uids();
    process_config_file(&conf, CONF_FILE);
    validate_caller_ids(&conf);
    do_command(cmd, cmd_fd, &conf);

    return 0;
}
