/*
 * This implement bridge related operations provided by openvswitch.
 * Currently, all the bridge related operations are implemented by
 * fork() and exec() to spawn the corresponding openvswitch commands.
 * Eventually, these will be replaced by codes similar in openvswitch
 * source code to avoid new process creation.
 */
#include "condor_common.h"
#include "condor_config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <linux/if_bridge.h>

#include "condor_debug.h"

#include "network_manipulation.h"

// According to the openvswitch doc, the bridge identiifer should not be
// more than 8 bytes long.
#define OVS_BRNAME_MAX_LENGTH 8

int
ovs_create_bridge(const char * bridge_name)
{   
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);

    pid_t pid = fork();

    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_create_bridge (errno=%d, %s).\n", errno, strerror(errno));
        return errno;
    }
    else if (pid == 0) { // This is the child process
        // equivalent to execute 'ovs-vsctl -- --may-exist add-br bridge_name'
        // with --may-exist, this command will do nothing is the bridge with the same name is already existed.
        char * args[] = {"ovs-vsctl", "--", "--may-exist", "add-br", brname, NULL};
        if(execvp("ovs-vsctl", args) < 0){
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else { // This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_create_bridge. (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if(WIFEXITED(status)){
            dprintf(D_FULLDEBUG, "ovs-vsctl add-br %s terminated normally. Exit status = %d\n", brname, WEXITSTATUS(status));
            errno =  WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status)){
            dprintf(D_ALWAYS, "ovs-vsctl add-br %s terminated abnormally. Killed by signal %d\n", brname, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}

int
ovs_delete_bridge(const char * bridge_name)
{
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);

    pid_t pid = fork();

    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_delete_bridge (errno=%d, %s).\n", errno, strerror(errno));
        return errno;
    }
    else if (pid == 0) { // This is the child process
        // equivalent to execute 'ovs-vsctl del-br bridge_name'
        char * args[] = {"ovs-vsctl", "del-br", brname, NULL};
        if(execvp("ovs-vsctl", args) < 0){
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else { // This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_delete_bridge. (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if(WIFEXITED(status)){
            dprintf(D_FULLDEBUG, "ovs-vsctl del-br %s terminated normally. Exit status = %d\n", brname, WEXITSTATUS(status));
            errno = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)){
            dprintf(D_ALWAYS, "ovs-vsctl del-br %s terminated abnormally. Killed by signal %d\n", brname, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}

int
ovs_add_interface_to_bridge(const char * bridge_name, const char * dev)
{
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);
    char devname[IFNAMSIZ];
    strncpy(devname, dev, IFNAMSIZ);
    
    int ifindex = if_nametoindex(dev);
    if(ifindex == 0) {
        return ENODEV;
    }

    pid_t pid = fork();

    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_add_interface_to_bridge (errno=%d, %s).\n", errno, strerror(errno));
        return errno;
    }
    else if (pid == 0) { // This is the child process
        // equivalent to execute 'ovs-vsctl -- --may-exist add-port bridge_name device_name'
        char * args[] = {"ovs-vsctl", "--", "--may-exist", "add-port", brname, devname, NULL};
        if(execvp("ovs-vsctl", args) < 0){
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else { // This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_add_interface_to_bridge. (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if (WIFEXITED(status)) {
            dprintf(D_FULLDEBUG, "ovs-vsctl add-port %s %s terminated normally. Exit status = %d\n", brname, dev, WEXITSTATUS(status));
            errno = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            dprintf(D_ALWAYS, "ovs-vsctl add-port %s %s terminated abnormally. Killed by signal %d\n", brname, dev, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}

int
ovs_delete_interface_from_bridge(const char * bridge_name, const char * dev)
{
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);
    char devname[IFNAMSIZ];
    strncpy(devname, dev, IFNAMSIZ);

    int ifindex = if_nametoindex(dev);
    if(ifindex == 0) {
        return ENODEV;
    }

    pid_t pid = fork();

    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_delete_interface from bridge (errno=%d, %s).\n", errno, strerror(errno));
        return errno;
    }
    else if (pid == 0) { // This is the child process
        // equivalent to execute 'ovs-vsctl -- --if-exist del-port bridge_name device_name'
        char * args[] = {"ovs-vsctl", "--", "--if-exists", "del-port", brname, devname, NULL};
        if(execvp("ovs-vsctl", args) < 0){
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else { // This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_delete_interface_from_bridge. (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if(WIFEXITED(status)) {
            dprintf(D_FULLDEBUG, "ovs-vsctl del-port %s %s terminated normally. Exit status = %d\n", brname, dev, WEXITSTATUS(status));
            errno = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            dprintf(D_ALWAYS, "ovs-vsctl del-port %s %s terminated abnormally. Killed by signal %d\n", brname, dev, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}

int
ovs_set_bridge_fd(const char * bridge_name, unsigned delay)
{
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);

    int ifindex = if_nametoindex(bridge_name);
    if(ifindex == 0) {
        return ENODEV;
    }

    pid_t pid = fork();
    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_set_bridge_fd (errno=%d, %s)\n", errno, strerror(errno));
        return errno;
    }
    else if(pid == 0) {// This is the child process
        // equivalent to execute 'ovs-vsctl set Bridge brname other_config:stp-forward-delay=delay'
        char delay_value[4];
        sprintf(delay_value, "%d", delay);
        char delay_str[40] = "other_config:stp-forward-delay=";
        strcat(delay_str, delay_value);
        char * args[] = {"ovs-vsctl", "set", "Bridge", "brname", delay_str, NULL};
        if(execvp("ovs-vsctl", args) < 0) {
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else {// This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_set_bridge_fd (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if (WIFEXITED(status)) {
            dprintf(D_FULLDEBUG, "ovs-vsctl set Bridge %s other_config:stp-forward-delay=%d terminated normally. Exit status = %d\n", brname, delay, WEXITSTATUS(status));
            errno = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            dprintf(D_ALWAYS, "ovs-vsctl set Bridge %s other_config:stp-forward-delay=%d terminated abnormally. Killed by signal %d\n", brname, delay, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}

/* Here what we want is to have the ability to set the forward delay of openvswitch bridge
 * to be 0, otherwise, DHCP cannot guaranteed to work correctly. However, based on the docs
 * of openvswitch, the range of forward delay of openvswitch bridge is from 4-30. Thus, we 
 * disable the stp on bridge when it is created.
 */
int
ovs_disable_stp(const char * bridge_name)
{
    errno = 0;
    char brname[OVS_BRNAME_MAX_LENGTH];
    strncpy(brname, bridge_name, OVS_BRNAME_MAX_LENGTH);
    
    int ifindex = if_nametoindex(bridge_name);
    if(ifindex == 0) {
        return ENODEV;
    }
    
    pid_t pid = fork();

    if(pid < 0) {
        dprintf(D_ALWAYS, "Error in creating child process in ovs_disable_stp (errno=%d, %s)\n", errno, strerror(errno));
        return errno;
    }
    else if (pid == 0) {// This is the child process
        // equivalent to execute 'ovs-vsctl set Bridge brname stp_enable=false'
        char * args[] = {"ovs-vsctl", "set", "Bridge", brname, "stp_enable=false", NULL};
        if(execvp("ovs-vsctl", args) < 0) {
            dprintf(D_ALWAYS, "execvp in child to spawn ovs-vsctl command failed. (errno=%d, %s)\n", errno, strerror(errno));
            exit(errno);
        }
        exit(0);
    }
    else { // This is the parent process
        int status;
        if(waitpid(pid, &status, 0) == -1){
            dprintf(D_ALWAYS, "waitpid error in ovs_disable_stp. (errno=%d, %s)\n", errno, strerror(errno));
            return errno;
        }
        if(WIFEXITED(status)) {
            dprintf(D_FULLDEBUG, "ovs_vsctl set Bridge %s stp_enable=false terminiated normally. Exit status = %d\n", brname, WEXITSTATUS(status));
            errno = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            dprintf(D_ALWAYS, "ovs-vsctl set Bridge %s stp_enable=false terminated abnormally. Killed by signal %d\n", brname, WTERMSIG(status));
            errno = WTERMSIG(status);
        }
        return errno;
    }
}
