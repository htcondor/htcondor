
#include <stdio.h>
#include <string.h>

#include "network_manipulation.h"

#include <netinet/in.h>
#include <net/if.h>
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
    return 0;
}

static int print_rule(struct ipt_entry * entry)
{
    struct xt_entry_match * match = NULL;
    size_t offset = sizeof(struct ipt_entry);
    printf("\tRule:\n");
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
    if (!*target->u.user.name) printf("\t\ttarget = ACCEPT\n");
    else printf("\t\ttarget = %s\n", target->u.user.name);
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
    int hook_idx;
    const unsigned char * chain_name = NULL, *old_chain_name = NULL;

    while (1)
    {
        // Detect chain transitions.
        struct xt_entry_target *my_target = ipt_get_target(entry);
        if (strcmp(my_target->u.user.name, XT_ERROR_TARGET) == 0)
        {
            chain_name = my_target->data;
        }
        else
        {
            for (hook_idx=0; hook_idx < NF_IP_NUMHOOKS; hook_idx++)
            {
                if ((info.valid_hooks & (1 << hook_idx)) && (info.hook_entry[hook_idx] == offset))
                {
                    chain_name = (const unsigned char *)hooknames[hook_idx];
                }
            }
        }
        if (chain_name != old_chain_name)
        {
            printf("Chain: %s\n", chain_name);
            old_chain_name = chain_name;
            print_rule(entry);
        }
        else
        {
            print_rule(entry);
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
