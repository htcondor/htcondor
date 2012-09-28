
#include "stdio.h"

#include "network_manipulation.h"

int print_rule(struct ipt_entry * entry)
{
    unsigned char *elems = entry->elems;
    struct xt_entry_match * match = NULL;
    printf("\tRule:\n")
    printf("\t\tByte Count:%ld; Packet Count:%ld\n", entry->counters.bcnt, entry->counters.pcnt)
    while (offset < entry->target_offset)
    {
        match = (struct xt_entry_match *)(elems + offset);
        printf("\t\t%s = %s\n", match->u.user.name, match->data);

        size_t next_jump = match->u.match_size;
        if (next_jump)
            offset += next_jump;
        else
            break;
    }
    return 0;
}

int main()
{
    struct ipt_get_entries *entries;
    struct ipt_getinfo info;

    int error;
    if ((error = get_firewall_entries(&info, &entries)))
    {
        return error;
    }

    size_t offset = 0, idx = 1;
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
}
