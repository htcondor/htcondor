#include <stdio.h>
#include <libdeltacloud/libdeltacloud.h>
#include <string>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dcloudgahp_commands.h"
#include "dcloudgahp_common.h"

DcloudGahpCommand::DcloudGahpCommand(const char *cmd, workerfn workerfunc)
{
    command = cmd;
    workerfunction = workerfunc;
}

static int verify_number_args(const int want, const int actual)
{
    if (actual != want) {
        dcloudprintf("Expected %d args, saw %d args\n", want, actual);
        return FALSE;
    }

    return TRUE;
}

/* an ID in deltacloud can be any combination of characters, including
 * space.  Since condor uses spaces in the GAHP protocol to signify the
 * end of arguments, we need to escape any spaces in ID's so GridManager
 * handles them properly
 */
static char *escape_id(const char *id)
{
    int len;
    char *ret;
    int i, j;

    len = strlen(id);

    /* since we are only possibly adding \ characters, the most we can
     * expand the string is by doubling it
     */
    ret = (char *)malloc(len * 2 + 1);
    if (ret == NULL) {
        dcloudprintf("Failed to allocate memory\n");
        return NULL;
    }

    j = 0;
    for (i = 0; i < len; i++) {
        if (id[i] == ' ' || id[i] == '\\')
            ret[j++] = '\\';
        ret[j++] = id[i];
    }
    ret[j] ='\0';

    return ret;
}

static ssize_t full_read(int fd, void *ptr, size_t nbytes)
{
    int nleft, nread;

    nleft = nbytes;
    while (nleft > 0) {

#ifndef WIN32
    REISSUE_READ:
#endif
        nread = read(fd, ptr, nleft);
        if (nread < 0) {

#ifndef WIN32
            /* error happened, ignore if EINTR, otherwise inform the caller */
            if (errno == EINTR)
                goto REISSUE_READ;
#endif
            /* The caller has no idea how much was actually read in this
             * scenario and the file offset is undefined */
            return -1;
        }
        else if (nread == 0)
            /* We've reached the end of file marker, so stop looping. */
            break;

        nleft -= nread;
        /* On Win32, void* does not default to "byte", so we cast it */
        ptr = ((char *)ptr) + nread;
    }

    /* return how much was actually read, which could include 0 in an
     * EOF situation */
    return (nbytes - nleft);
}

static char *read_password_file(const char *path)
{
    int fd;
    struct stat stat_buf;
    char *password = NULL;
    int idx;

    if (path == NULL)
        return NULL;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    if (fstat(fd, &stat_buf) < 0)
        goto cleanup;

    password = (char *)calloc(stat_buf.st_size + 1, 1);
    if (password == NULL)
        goto cleanup;

    if (full_read(fd, password, stat_buf.st_size) < 0) {
        free(password);
        password = NULL;
        goto cleanup;
    }

    for (idx = strlen(password) - 1; idx >= 0 && isspace(password[idx]); idx--)
        password[idx] = '\0';

cleanup:
    close(fd);
    return password;
}

static std::string create_failure(const char *req_id, const char *err_msg, ...)
{
    std::string buffer;
    va_list ap;
    char *tmp;
    unsigned int i;

    buffer += req_id;
    buffer += ' ';

    va_start(ap, err_msg);
    vasprintf(&tmp, err_msg, ap);
    va_end(ap);

    for (i = 0; i < strlen(tmp); i++) {
        if (tmp[i] == ' ')
            buffer += '\\';
        buffer += tmp[i];
    }
    free(tmp);

    buffer += '\n';

    dcloudprintf(buffer.c_str());

    return buffer;
}

static std::string create_instance_output(char * reqid,
                                          struct deltacloud_instance *inst)
{
    struct deltacloud_action *act;
    struct deltacloud_address *addr;
    std::string output_string;
    char *esc_id;

    output_string += reqid;
    output_string += " NULL ";

    esc_id = escape_id(inst->id);
    output_string += "id=";
    output_string += esc_id;
    output_string += ' ';
    free(esc_id);

    output_string += "state=";
    output_string += inst->state;
    output_string += ' ';

    output_string += "actions=";
    act = inst->actions;
    while (act != NULL) {
        output_string += act->rel;
        act = act->next;
        if (act != NULL)
            output_string += ',';
    }
    output_string += ' ';

    output_string += "public_addresses=";
    addr = inst->public_addresses;
    while (addr != NULL) {
        output_string += addr->address;
        addr = addr->next;
        if (addr != NULL)
            output_string += ',';
    }
    output_string += ' ';

    output_string += "private_addresses=";
    addr = inst->private_addresses;
    while (addr != NULL) {
        output_string += addr->address;
        addr = addr->next;
        if (addr != NULL)
            output_string += ',';
    }

    output_string += '\n';

    return output_string;
}

static int assign_parameter(struct deltacloud_create_parameter **params,
                            int *params_size, const char *name,
                            const char *value)
{
    struct deltacloud_create_parameter *tmpparams;

    if (STRCASEEQ(value, NULLSTRING))
        return 0;

    /* use a temporary variable here so that if we fail, we don't lose the
     * pointer to params
     */
    tmpparams = (struct deltacloud_create_parameter *)realloc(*params,
                                                              sizeof(struct deltacloud_create_parameter) * (*params_size + 1));
    if (tmpparams == NULL)
        return -1;

    *params = tmpparams;

    /* if prepare_parameter failed, then we have allocated additional memory
     * for the array, but placed nothing inside the structure.  As long as we
     * don't update params_size, it is safe to return here as the cleanup
     * code will just free the other members, plus the array
     */
    if (deltacloud_prepare_parameter(*params + *params_size, name, value) < 0)
        return -1;

    (*params_size)++;

    return 0;
}

/*
 * DELTACLOUD_VM_SUBMIT <reqid> <url> <user> <password_file> <image_id> <name> <realm_id> <hwp_id> <hwp_memory> <hwp_cpu> <hwp_storage> <keyname> <userdata>
 *  where all arguments are required.  <reqid>, <url>, <user>, <password_file>,
 *  and <image_id> all have to be non-NULL; <name>, <realm_id>, <hwp_id>,
 *  <hwp_memory>, <hwp_cpu>, <hwp_storage>, <keyname>, and <userdata> should
 *  either be the string "NULL" to let deltacloud pick, or a particular value.
 */
bool dcloud_start_worker(int argc, char **argv, std::string &output_string)
{
    char *url, *user, *password_file, *password, *image_id, *name, *realm_id;
    char *hwp_id, *hwp_memory, *hwp_cpu, *hwp_storage, *reqid;
    char *keyname, *userdata;
    struct deltacloud_api api;
    bool ret = FALSE;
    struct deltacloud_create_parameter *params = NULL;
    int params_size = 0;
    char *instid = NULL;
    int i;
    char *esc_id;

    if (!verify_number_args(14, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];
    image_id = argv[5];
    name = argv[6];
    realm_id = argv[7];
    hwp_id = argv[8];
    hwp_memory = argv[9];
    hwp_cpu = argv[10];
    hwp_storage = argv[11];
    keyname = argv[12];
    userdata = argv[13];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s, image_id %s, name %s, realm_id %s, hwp_id %s, hwp_memory %s, hwp_cpu %s, hwp_storage %s, keyname %s, userdata %s\n",
                 reqid, url, user, password_file, image_id, name, realm_id,
                 hwp_id, hwp_memory, hwp_cpu, hwp_storage, keyname, userdata);


    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }
    if (STRCASEEQ(image_id, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_Image_ID");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;
    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    if (assign_parameter(&params, &params_size, "name", name) < 0 ||
        assign_parameter(&params, &params_size, "realm_id", realm_id) < 0 ||
        assign_parameter(&params, &params_size, "keyname", keyname) < 0 ||
        assign_parameter(&params, &params_size, "hwp_id", hwp_id) < 0 ||
        assign_parameter(&params, &params_size, "hwp_memory", hwp_memory) < 0 ||
        assign_parameter(&params, &params_size, "hwp_cpu", hwp_cpu) < 0 ||
        assign_parameter(&params, &params_size, "hwp_storage", hwp_storage) < 0 ||
        assign_parameter(&params, &params_size, "user_data", userdata) < 0) {
        output_string = create_failure(reqid,
                                       "Create_Instance_Failure: Failed to allocate parameter memory");
        goto cleanup_library;
    }

    if (deltacloud_create_instance(&api, image_id, params, params_size,
                                   &instid) < 0) {
        output_string = create_failure(reqid, "Create_Instance_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_library;
    }

    /* deltacloud_create_instance only returns an ID to us.  Output that ID
     * plus a hard-coded state of PENDING to the upper layers; it will be
     * their responsibility to get the instance details as appropriate
     */

    output_string += reqid;
    output_string += " NULL ";

    esc_id = escape_id(instid);
    output_string += "id=";
    output_string += esc_id;
    free(esc_id);
    output_string += " state=PENDING\n";

    ret = TRUE;

cleanup_library:
    free(instid);
    for (i = 0; i < params_size; i++)
        deltacloud_free_parameter_value(&params[i]);
    free(params);
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_VM_ACTION <reqid> <url> <user> <password_file> <instance_id> <action>
 *  where reqid, url, user, password_file, instance_id, and action have to be
 *  non-NULL.
 */
bool dcloud_action_worker(int argc, char **argv, std::string &output_string)
{
    char *url, *user, *password_file, *password, *instance_id, *action, *reqid;
    struct deltacloud_api api;
    struct deltacloud_instance instance;
    int action_ret;
    bool ret = FALSE;

    if (!verify_number_args(7, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];
    instance_id = argv[5];
    action = argv[6];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s, instance_id %s, action %s\n", reqid, url, user, password_file, instance_id, action);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }
    if (STRCASEEQ(instance_id, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_Instance_ID");
        return FALSE;
    }
    if (STRCASEEQ(action, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid action");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    if (deltacloud_get_instance_by_id(&api, instance_id, &instance) < 0) {
        output_string = create_failure(reqid, "Instance_Lookup_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_library;
    }

    if (STRCASEEQ(action, "STOP"))
        action_ret = deltacloud_instance_stop(&api, &instance);
    else if (STRCASEEQ(action, "REBOOT"))
        action_ret = deltacloud_instance_reboot(&api, &instance);
    else if (STRCASEEQ(action, "START"))
        action_ret = deltacloud_instance_start(&api, &instance);
    else if (STRCASEEQ(action, "DESTROY"))
        action_ret = deltacloud_instance_destroy(&api, &instance);
    else {
        output_string = create_failure(reqid, "Invalid_Action %s", action);
        goto cleanup_instance;
    }

    if (action_ret < 0) {
        output_string = create_failure(reqid,
                                       "Action_Failure for instance %s: %s",
                                       instance_id,
                                       deltacloud_get_last_error_string());
        goto cleanup_instance;
    }

    output_string += reqid;
    output_string += " NULL\n";

    ret = TRUE;

cleanup_instance:
    deltacloud_free_instance(&instance);

cleanup_library:
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_VM_INFO <reqid> <url> <user> <password_file> <instance_id>
 *  where reqid, url, user, password_file, and instance_id have to be non-NULL.
 */
bool dcloud_info_worker(int argc, char **argv, std::string &output_string)
{
    char *url, *user, *password_file, *password, *instance_id, *reqid;
    struct deltacloud_api api;
    struct deltacloud_instance inst;
    bool ret = FALSE;

    if (!verify_number_args(6, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];
    instance_id = argv[5];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s, instance_id %s\n", reqid, url, user, password_file, instance_id);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }
    if (STRCASEEQ(instance_id, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_Instance_ID");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    if (deltacloud_get_instance_by_id(&api, instance_id, &inst) < 0) {
        output_string = create_failure(reqid, "Instance_Lookup_Failure %s: %s",
                                       instance_id,
                                       deltacloud_get_last_error_string());
        goto cleanup_library;
    }

    output_string = create_instance_output(reqid, &inst);

    ret = TRUE;

    deltacloud_free_instance(&inst);

cleanup_library:
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_VM_STATUS_ALL <reqid> <url> <user> <password_file>
 *  where reqid, url, user, and password_file have to be non-NULL.
 */
bool dcloud_statusall_worker(int argc, char **argv, std::string &output_string)
{
    char *url, *user, *password_file, *password, *reqid;
    struct deltacloud_api api;
    struct deltacloud_instance *instances;
    struct deltacloud_instance *curr;
    bool ret = FALSE;
    char *esc_id;

    if (!verify_number_args(5, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s\n", reqid, url, user, password_file);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    if (deltacloud_get_instances(&api, &instances) < 0) {
        output_string = create_failure(reqid, "Instance_Fetch_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_library;
    }

    output_string += reqid;
    output_string += " NULL";

    curr = instances;
    while (curr != NULL) {
        esc_id = escape_id(curr->id);
        output_string += " ";
        output_string += esc_id;
        output_string += " ";
        output_string += curr->state;
        free(esc_id);
        curr = curr->next;
    }
    output_string += "\n";

    ret = TRUE;

    deltacloud_free_instance_list(&instances);

cleanup_library:
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_VM_FIND <reqid> <url> <user> <password_file> <name>
 *  where reqid, url, user, password_file, and name have to be non-NULL.
 */
bool dcloud_find_worker(int argc, char **argv, std::string &output_string)
{
    char *url, *user, *password_file, *password, *name, *reqid;
    struct deltacloud_api api;
    struct deltacloud_instance inst;
    int rc;
    bool ret = FALSE;
    char *esc_id;
    struct deltacloud_error *last;

    if (!verify_number_args(6, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];
    name = argv[5];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s, name %s\n", reqid, url, user, password_file, name);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }
    if (STRCASEEQ(name, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_Name");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    rc = deltacloud_get_instance_by_name(&api, name, &inst);
    if (rc == 0) {
        /* found the instance, output the id */
        output_string = reqid;
        output_string += " NULL ";
        esc_id = escape_id(inst.id);
        output_string += esc_id;
        free(esc_id);
        output_string += '\n';
        deltacloud_free_instance(&inst);
    }
    else if (rc < 0) {
        last = deltacloud_get_last_error();
        if (!last) {
            output_string = create_failure(reqid,
                                           "Instance_Fetch_Failure %s",
                                           name);
            goto cleanup_library;
        }
        else if(last->error_num != DELTACLOUD_NAME_NOT_FOUND_ERROR) {
            /* failed to find the instance, output an error */
            output_string = create_failure(reqid,
                                           "Instance_Fetch_Failure %s: %s",
                                           name, last->details);
            goto cleanup_library;
        }
        else {
            /* could not find the instance, which is not an error */
            output_string = reqid;
            output_string += " NULL NULL\n";
        }
    }

    ret = TRUE;

cleanup_library:
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_GET_MAX_NAME_LENGTH <reqid> <url> <user> <password_file>
 *  where reqid, url, user, and password_file have to be non-NULL.
 */
bool dcloud_max_name_length_worker(int argc, char **argv,
                                   std::string &output_string)
{
    char *url, *user, *password_file, *password, *reqid;
    struct deltacloud_api api;
    struct deltacloud_link *link;
    struct deltacloud_feature *feature;
    struct deltacloud_feature_constraint *constraint;
    char *max = NULL;
    bool ret = FALSE;

    if (!verify_number_args(5, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s\n", reqid, url, user, password_file);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    /* here, we search through the api.links->features->constraints to try and
     * find a constraint of "max_length".  If we find it, we return it; if not
     * we just let the higher layers choose a default
     */
    deltacloud_for_each(link, api.links) {
        if (STRCASENEQ(link->rel, "instances"))
            continue;

        /* OK, we are in the instances collection.  Look at the features */
        deltacloud_for_each(feature, link->features) {
            if (STRCASENEQ(feature->name, "user_name"))
                continue;

            /* OK, we saw the user_name feature; see if it has a length */
            deltacloud_for_each(constraint, feature->constraints) {
                if (STRCASEEQ(constraint->name, "max_length")) {
                    max = strdup(constraint->value);
                    /* need to use goto here to break all of the loops */
                    goto done;
                }
            }
        }
    }

done:
    output_string = reqid;
    output_string += " NULL ";
    if (max == NULL)
        output_string += "NULL";
    else {
        output_string += max;
        free(max);
    }
    output_string += "\n";

    deltacloud_free(&api);

    ret = TRUE;

cleanup_password:
    free(password);

    return ret;
}

/*
 * DELTACLOUD_START_AUTO <reqid> <url> <user> <password_file>
 *  where reqid, url, user, and password_file have to be non-NULL.
 */
bool dcloud_start_auto_worker(int argc, char **argv,
			      std::string &output_string)
{
    char *url, *user, *password_file, *password, *reqid;
    struct deltacloud_api api;
    struct deltacloud_instance_state *instance_states;
    struct deltacloud_instance_state *state;
    struct deltacloud_instance_state_transition *transition;
    bool automatically = FALSE;
    bool ret = FALSE;

    if (!verify_number_args(5, argc)) {
        output_string = create_failure("0", "Wrong_Argument_Number");
        return FALSE;
    }

    reqid = argv[1];
    url = argv[2];
    user = argv[3];
    password_file = argv[4];

    dcloudprintf("Arguments: reqid %s, url %s, user %s, password_file %s\n", reqid, url, user, password_file);

    if (STRCASEEQ(url, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_URL");
        return FALSE;
    }
    if (STRCASEEQ(user, NULLSTRING)) {
        output_string = create_failure(reqid, "Invalid_User");
        return FALSE;
    }

    if (STRCASEEQ(password_file, NULLSTRING))
        password_file = NULL;

    password = read_password_file(password_file);
    if (!password) {
        output_string = create_failure(reqid, "Invalid_Password_File");
        return FALSE;
    }

    if (deltacloud_initialize(&api, url, user, password) < 0) {
        output_string = create_failure(reqid, "Deltacloud_Init_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_password;
    }

    if (deltacloud_get_instance_states(&api, &instance_states) < 0) {
        output_string = create_failure(reqid, "Instance_State_Failure: %s",
                                       deltacloud_get_last_error_string());
        goto cleanup_library;
    }

    deltacloud_for_each(state, instance_states) {
        if (STRCASENEQ(state->name, "pending"))
            continue;

        deltacloud_for_each(transition, state->transitions) {
            if (STRCASEEQ(transition->to, "running") &&
                transition->automatically != NULL &&
                STRCASEEQ(transition->automatically, "true")) {
                automatically = TRUE;
                goto done;
            }
        }
    }

done:
    output_string = reqid;
    output_string += " NULL ";
    if (automatically) {
        output_string += "TRUE";
    }
    else {
        output_string += "FALSE";
    }
    output_string += "\n";

    deltacloud_free_instance_state_list(&instance_states);

    ret = TRUE;

cleanup_library:
    deltacloud_free(&api);

cleanup_password:
    free(password);

    return ret;
}
