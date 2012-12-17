
#include <stdio.h>
#include <string.h>

#include "network_manipulation.h"

#include <netinet/in.h>
#include <net/if.h>
#include <limits.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <arpa/inet.h>

// Names are taken from libiptc.c
// TODO: this is going to be a maintenance nightmare!  I'm assuming this is
// part of the kernel's stable interface.
static const char *hooknames[] = {
    [NF_IP_PRE_ROUTING]      = "PREROUTING",
    [NF_IP_LOCAL_IN]         = "INPUT",
    [NF_IP_FORWARD]          = "FORWARD",
    [NF_IP_LOCAL_OUT]        = "OUTPUT",
    [NF_IP_POST_ROUTING]     = "POSTROUTING",
};

static int print_ip(struct ipt_ip * ip)
{
    char src_array[INET_ADDRSTRLEN];
    char src_msk_array[INET_ADDRSTRLEN];
    const char *src = inet_ntop(AF_INET, &ip->src, src_array, INET_ADDRSTRLEN);
    const char *src_msk = inet_ntop(AF_INET, &ip->smsk, src_msk_array, INET_ADDRSTRLEN);
    printf("\t\tSource: %s/%s", src, src_msk);
    char dst_array[INET_ADDRSTRLEN];
    char dst_msk_array[INET_ADDRSTRLEN];
    const char *dst = inet_ntop(AF_INET, &ip->dst, dst_array, INET_ADDRSTRLEN);
    const char *dst_msk = inet_ntop(AF_INET, &ip->dmsk, dst_msk_array, INET_ADDRSTRLEN);
    printf(", Dest: %s/%s\n", dst, dst_msk);
    if (*ip->iniface == '\0') strcpy(ip->iniface, "(any)");
    if (*ip->outiface == '\0') strcpy(ip->outiface, "(any)");
    printf("\t\tIn interface: %s, Out interface: %s\n", ip->iniface, ip->outiface);

    if (ip->flags & IPT_F_GOTO)
    {
    printf("\t\t[goto]\n");
    }
    return 0;
}

static int print_rule(struct ipt_entry * entry, size_t rule_offset)
{
    struct xt_entry_match * match = NULL;
    size_t offset = sizeof(struct ipt_entry);
    printf("\tRule: (offset %lu; come from %u)\n", rule_offset, entry->comefrom);
    print_ip(&entry->ip);
    printf("\t\tByte Count:%lld; Packet Count:%lld\n", entry->counters.bcnt, entry->counters.pcnt);
    while (offset < entry->target_offset)
    {
        match = (struct xt_entry_match *)((char *)entry + offset);
        if (strcmp(match->u.user.name, "state") == 0)
        {
            printf("\t\tstate = %u\n", *match->data);
        }
        else
        {
            printf("\t\t%s = %s\n", match->u.user.name, match->data);
        }

        size_t next_jump = match->u.match_size;
        if (next_jump)
            offset += next_jump;
        else
            break;
    }
    struct xt_entry_target * target = (struct xt_entry_target *)((char *)entry + entry->target_offset);
    typedef union target_type { unsigned char *c; int *i; } target_type_u;
    target_type_u target_data;
    target_data.c = target->data;
    if (*target->u.user.name)
    {
        printf("\t\ttarget = %s\n", target->u.user.name);
    }
    else if (*target_data.i > 0)
    {
        printf("\t\ttarget = rule at offset %d\n", *target_data.i);
    }
    else if (*target_data.i == -NF_ACCEPT-1)
    {
        printf("\t\ttarget = ACCEPT\n");
    }
    else
    {
        printf("\t\ttarget = (unknown) %d\n", *target_data.i);
    }
    return 0;
}

int main()
{
    struct ipt_get_entries *entries;
    struct ipt_getinfo info;

    int error;
    if ((error = get_firewall_data(&info, &entries)))
    {
        return error;
    }

    size_t offset = 0;
    struct ipt_entry * entry = entries->entrytable;
    const char * chain_name = NULL, *old_chain_name = NULL;

    while (1)
    {
        // Detect chain transitions.
        struct xt_entry_target *my_target = ipt_get_target(entry);
        if (strcmp(my_target->u.user.name, XT_ERROR_TARGET) == 0)
        {
            chain_name = (char *)my_target->data;
        }
        else
        {
            int hook_idx;
            for (hook_idx=0; hook_idx < NF_IP_NUMHOOKS; hook_idx++)
            {
                if ((info.valid_hooks & (1 << hook_idx)) && (info.hook_entry[hook_idx] == offset))
                {
                    chain_name = hooknames[hook_idx];
                    printf("(Hook entry: %d; hook underflow %d)\n", info.hook_entry[hook_idx], info.underflow[hook_idx]);
                }
            }
        }
        if (chain_name != old_chain_name)
        {
            printf("Chain: %s (offset %lu)\n", chain_name, offset);
            old_chain_name = chain_name;
            //if (!strcmp(my_target->u.user.name, XT_ERROR_TARGET)) print_rule(entry, offset);
            print_rule(entry, offset);
        }
        else
        {
            print_rule(entry, offset);
        }

        if (entry->next_offset)
            offset += entry->next_offset;
        else
            break;
        if (offset < entries->size)
            entry = (struct ipt_entry *)((char *)entries->entrytable + offset);
        else
            break;
    }

    return 0;
}
