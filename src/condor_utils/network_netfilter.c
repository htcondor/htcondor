
#include <sys/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <limits.h>
#include <net/if.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Names are taken from libiptc.c
static const char *hooknames[] = {
	[NF_IP_PRE_ROUTING]      = "PREROUTING",
	[NF_IP_LOCAL_IN]         = "INPUT",
	[NF_IP_FORWARD]          = "FORWARD",
	[NF_IP_LOCAL_OUT]        = "OUTPUT",
	[NF_IP_POST_ROUTING]     = "POSTROUTING",
};

static int find_rules(struct ipt_entry * entry, int (*match_fcn)(const unsigned char *, long long, void *), void * callback_data) {

	unsigned char *elems = entry->elems;
	
	size_t idx = 1, offset = 0;
	//printf("Target offset: %d\n", entry->target_offset);
	struct xt_entry_match * match = NULL;
	while (offset < entry->target_offset) {
		match = (struct xt_entry_match *)(elems + offset);
		if (strcmp(match->u.user.name, "comment") == 0) {
			match_fcn(match->data, entry->counters.bcnt, callback_data);
		}
		size_t next_jump = match->u.match_size;
		if (next_jump)
			offset += next_jump;
		else
			break;
		idx ++;
	}

	return 0;
}

int perform_accounting(const char * chain, int (*match_fcn)(const unsigned char *, long long, void *), void * callback_data) {
	int sockfd;

	// Create a raw socket.  We won't actually be communicating with this socket, but
	// rather using it to call getsockopt for firewall interaction.
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		fprintf(stderr, "Unable to open raw socket. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	struct ipt_getinfo info;
	socklen_t info_size = sizeof(struct ipt_getinfo);

	// Set the name of the table to query.  Right now, we hardcode the "filter" table, which
	// Linux uses for the firewall.
	// Note info.name is a fixed-size array of length XT_TABLE_MAXNAMELEN.
	const char TABLE_NAME[] = "filter";
	// I am unsure if the table name must be null-terminated, so I err on the safe side here.
	if (strlen(TABLE_NAME) > XT_TABLE_MAXNAMELEN-1) {
		fprintf(stderr, "Table name %s is too long.\n", TABLE_NAME);
		return 1;
	}
	strncpy(info.name, "filter", XT_TABLE_MAXNAMELEN);

	// Using the fixed-size ipt_getinfo structure, query to see how large the
	// table is.
	if (getsockopt(sockfd, IPPROTO_IP, IPT_SO_GET_INFO, &info, &info_size) < 0) {
		fprintf(stderr, "Unable to get socket info. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	// Now that we know the size of the table, we will do the actual query with an
	// appropriately-sized data structure.  Note that the size is the query object
	// itself, followed by a binary blob we will later parse.
	info_size = sizeof(struct ipt_get_entries) + info.size;
	struct ipt_get_entries *entries = (struct ipt_get_entries *)malloc(info_size);
	if (!entries) {
		fprintf(stderr, "Unable to malloc memory for routing table.\n");
		return 1;
	}
	// We are less careful about string sizes here as we already checked above.
	strcpy(entries->name, TABLE_NAME);
	entries->size = info.size;

	// Finally, read out the firewall.
	if (getsockopt(sockfd, IPPROTO_IP, IPT_SO_GET_ENTRIES, entries, &info_size) < 0) {
		fprintf(stderr, "Unable to get table entries. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	size_t offset = 0, idx = 1;
	struct ipt_entry * entry = entries->entrytable;
	int hook_idx;
	const unsigned char * chain_name = NULL, *old_chain_name = NULL;
	while (1) {
		struct xt_entry_target *my_target = ipt_get_target(entry);
		if (strcmp(my_target->u.user.name, XT_ERROR_TARGET) == 0) {
			// User chain
			chain_name = my_target->data;
			if (strcmp((const char *)chain_name, XT_ERROR_TARGET) == 0) {
				break;
			}
		} else {
			// See if this is start of a new chain
			for (hook_idx=0; hook_idx < NF_IP_NUMHOOKS; hook_idx++) {
				if (info.valid_hooks & (1 << hook_idx)) {
					if  (info.hook_entry[hook_idx] == offset) {
						chain_name = (const unsigned char *)hooknames[hook_idx];
					}
				}
			}
		}

		if ((chain_name == old_chain_name) && (strcmp((const char *)chain, (const char *)chain_name) == 0)) {
			fprintf(stderr, "Entry %ld, offset %ld, chain %s\n", idx, offset, chain_name);
			find_rules(entry, match_fcn, callback_data);

		} else {
			old_chain_name = chain_name;
		}

		// Calculate the next entry.
		if (entry->next_offset)
			offset += entry->next_offset;
		else
			break;
		idx++;
		if (offset < entries->size)
			entry = (struct ipt_entry *)((char *)entries->entrytable + offset);
		else
			break;
	}

	return 0;
}

