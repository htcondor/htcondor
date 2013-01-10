
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
#include <unistd.h>

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

/**
 * Parse an iptables rule, looking for the comments and the number of bytes which matched.
 */
static int parse_rule(struct ipt_entry * entry, int (*match_fcn)(const unsigned char *, long long, void *), void * callback_data) {

	unsigned char *elems = entry->elems;
	
	size_t idx = 1, offset = 0;
	//printf("Target offset: %d\n", entry->target_offset);
	struct xt_entry_match * match = NULL;
	// The rule is composed of multiple matches.  We iterate through all
	// of them until we find the comment.
	while (offset < entry->target_offset) {
		match = (struct xt_entry_match *)(elems + offset);
		if (strcmp(match->u.user.name, "comment") == 0) {
			match_fcn(match->data, entry->counters.bcnt, callback_data);
		}
		// End of the matches is detected by a zero-sized match->u.match_size
		size_t next_jump = match->u.match_size;
		if (next_jump)
			offset += next_jump;
		else
			break;
		idx ++;
	}

	return 0;
}

/**
 * Retrieve the firewall data from the kernel.
 *
 * Fills in the provided struct ipt_getinfo and allocates a ipt_get_entries.
 *
 * The struct ipt_getinfo pointer must be a valid pointer prior to calling.
 * The caller is responsible for calling free() on *result_entries once they are
 * done with the data.
 *
 * Returns 0 on success and non-zero on failure.
 * On failure, the contents of info and result_entries are undefined.
 *
 */
int get_firewall_data(struct ipt_getinfo *info, struct ipt_get_entries **result_entries) {

	int sockfd;

	// Create a raw socket.  We won't actually be communicating with this socket, but
	// rather using it to call getsockopt for firewall interaction.
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		fprintf(stderr, "Unable to open raw socket. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

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
	strncpy(info->name, TABLE_NAME, XT_TABLE_MAXNAMELEN);

	// Using the fixed-size ipt_getinfo structure, query to see how large the
	// table is.
	if (getsockopt(sockfd, IPPROTO_IP, IPT_SO_GET_INFO, info, &info_size) < 0) {
		fprintf(stderr, "Unable to get socket info. (errno=%d) %s\n", errno, strerror(errno));
		return errno;
	}

	// Now that we know the size of the table, we will do the actual query with an
	// appropriately-sized data structure.  Note that the size is the query object
	// itself, followed by a binary blob we will later parse.
	info_size = sizeof(struct ipt_get_entries) + info->size;
	struct ipt_get_entries *entries = (struct ipt_get_entries *)malloc(info_size);
	if (!entries) {
		fprintf(stderr, "Unable to malloc memory for routing table.\n");
		return 1;
	}
	// We are less careful about string sizes here as we already checked above.
	strcpy(entries->name, TABLE_NAME);
	entries->size = info->size;

	// Read out the firewall from the kernel
	if (getsockopt(sockfd, IPPROTO_IP, IPT_SO_GET_ENTRIES, entries, &info_size) < 0) {
		fprintf(stderr, "Unable to get table entries. (errno=%d) %s\n", errno, strerror(errno));
		free(entries);
		return errno;
	}

	close(sockfd);

	*result_entries = entries;
	return 0;
}

/**
 * Upload a new set of firewall entries to the kernel.
 * - info:
 * - result_entries:
 * - rule_mappings:
 * - input_rule_count:
 * - replacement_rule_count:
 */
static int write_firewall_entries(const struct ipt_getinfo *info, struct ipt_replace *result_entries, const size_t *rule_mappings)
{

	// Ok, I found the counter business completely confusing and undocumented.  Below
	// explains as much as I could figure out.
	//
	// Let rule[i] be the rule at place i we read from the kernel originally.
	//  - Let original_counter[i] be the corresponding counter.
	//  - Let mapping[k] be the new rule number corresponding to k.
	//    (so new_rules[k] = rule[mapping[k]])
	//
	// On the replacement call, the kernel will return the updated counters
	// corresponding to the original rules (updated_counters[i]) and start all
	// the new rules at a counter of zero.
	//
	// Then we update the counters for the new table.  So, if we don't want to lose
	// any packet counts, if k = mapping[i], we set:
	// new_counters[mapping[i]] = updated_counters[i]
	// and call the update function.  The kernel will add new_counters[k] to the
	// current count for the new rule.
	//
	// For example, say we have rule 1 which was unchanged to rule 2 after the update.
	// From the results of the IPT_SO_SET_REPLACE call (updated_counters) and set
	// new_counters[2] = updated_counters[1]
	// and call IPT_SO_ADD_COUNTERS
	//
	// At time=1 we call IPT_SO_GET_ENTRIES.  Counter value is X
	// At time=2 we call IPT_SO_REPLACE.  Counter value is X+Y and returned to userspace;
	//   in the kernel, the counter value is set back to 0.
	// At time=2 we call IPT_SO_ADD_COUNTERS.  We set the input counter value to X+Y, and
	//   the kernel value is Z.  After the call completes, the kernel value is set to X+Y+Z.
	// All this is done atomically in the kernel.  Exercise for the user: Convince yourself no
	// packets were left uncounted!

	// Put together the ipt_replace object from the provided information.
        result_entries->num_counters = info->num_entries;
	struct xt_counters *new_counters = malloc(sizeof(struct xt_counters)*info->num_entries);
	if (!new_counters)
	{
		fprintf(stderr, "Unable to allocate new counters");
		return 1;
	}
        result_entries->counters = new_counters;

	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		fprintf(stderr, "Unable to open raw socket. (errno=%d) %s\n", errno, strerror(errno));
		free(new_counters);
		return errno;
	}

	if (setsockopt(sockfd, IPPROTO_IP, IPT_SO_SET_REPLACE, result_entries, result_entries->size + sizeof(result_entries)) < 0) {
		fprintf(stderr, "Unable to set firewall entries. (errno=%d) %s\n", errno, strerror(errno));
		free(new_counters);
		close(sockfd);
		return errno;
	}

	size_t add_counters_size = sizeof(struct xt_counters_info)+sizeof(struct xt_counters)*result_entries->num_entries;
	struct xt_counters_info *add_counters = malloc(add_counters_size);
	if (!add_counters)
	{
		fprintf(stderr, "Unable to allocate replacement counters.\n");
		free(new_counters);
		close(sockfd);
		return 1;
	}
	strncpy(add_counters->name, info->name, XT_TABLE_MAXNAMELEN);
	add_counters->num_counters = result_entries->num_entries;
	size_t idx;
	for (idx=0; idx<info->num_entries; idx++)
	{
		add_counters->counters[rule_mappings[idx]] = new_counters[idx];
	}

	if (setsockopt(sockfd, IPPROTO_IP, IPT_SO_SET_ADD_COUNTERS, add_counters, add_counters_size) < 0)
	{
		fprintf(stderr, "Unable to set firewall counters. (errno=%d) %s\n", errno, strerror(errno));
		free(new_counters);
		free(add_counters);
		close(sockfd);
	}
	close(sockfd);
	free(new_counters);
	free(add_counters);
	return 0;
}

int perform_accounting(const char * chain, int (*match_fcn)(const unsigned char *, long long, void *), void * callback_data) {

	struct ipt_get_entries *entries;
	struct ipt_getinfo info;

	int error;

	// Read out the firewall.
	if ((error = get_firewall_data(&info, &entries))) {
		return error;
	}

	// Iterate through the table binary blob, searching for the desired chain
	size_t offset = 0, idx = 1;
	// The entry pointer will point at the currne rule, and hold the
	// offset of the next rule - we will iterate through this like a linked-list.
	struct ipt_entry * entry = entries->entrytable;
	int hook_idx;
	const unsigned char * chain_name = NULL, *old_chain_name = NULL;
	// Note that the table is a list of rules, grouped into chains.  We will iterate through
	// the rules, taking special notice when the chain name changes.
	while (1) {
		// There are two types of chains - user-defined and built-in.
		// The user-defined ones match the if statement below, while the
		// built-in ones have offsets as specified in the "info" object.
		struct xt_entry_target *my_target = ipt_get_target(entry);
		if (strcmp(my_target->u.user.name, XT_ERROR_TARGET) == 0) {
			// User chain
			chain_name = my_target->data;
			if (strcmp((const char *)chain_name, XT_ERROR_TARGET) == 0) {
				break;
			}
		} else {
			// See if this is start of a built-in chain.
			for (hook_idx=0; hook_idx < NF_IP_NUMHOOKS; hook_idx++) {
				if (info.valid_hooks & (1 << hook_idx)) { // Make sure this hook is valid, AND
					if  (info.hook_entry[hook_idx] == offset) { // Make sure the offset matches.
						// If true, then we look up the name in the table.
						chain_name = (const unsigned char *)hooknames[hook_idx];
					}
				}
			}
		}

		// We will parse each rule if we are in the correct chain *and* we have skipped past the header
		// entry which denotes the start of the chain.
		if ((chain_name == old_chain_name) && (strcmp((const char *)chain, (const char *)chain_name) == 0)) {
			//fprintf(stderr, "Entry %ld, offset %ld, chain %s\n", idx, offset, chain_name);
			parse_rule(entry, match_fcn, callback_data);

		} else {
			old_chain_name = chain_name;
		}

		// Calculate the beginning of the next rule.
		// Make sure to detect the end of the chain.
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

static int rule_has_jump_target(const struct ipt_entry * entry)
{
	typedef union const_target { const struct ipt_entry * centry; struct ipt_entry * entry; } const_target_u;
	const_target_u const_target_data;
	const_target_data.centry = entry;
	struct xt_entry_target *my_target = ipt_get_target(const_target_data.entry);
	if (!*(my_target->u.user.name))
	{
		typedef union target_type { const unsigned char *c; int *i; } target_type_u;
		target_type_u target_data;
		target_data.c = my_target->data;
		if (*target_data.i > 0)
		{
			return *target_data.i;
		}
	}
	return -1;
}

static void set_rule_jump_target(struct ipt_entry *entry, size_t target)
{
	struct xt_entry_target *my_target = ipt_get_target(entry);
	typedef union target_type { unsigned char *c; int *i; } target_type_u;
	target_type_u target_data;
	target_data.c = my_target->data;
	*target_data.i = target;
}

/*
 * Given a set of entries and a list of entries to delete, rewrite the entries (fixing up the internal references).
 * Args:
 * - entries: a set of entries in the firewall table.
 * - rules_to_delete: An array of offsets; each entry in the array is the offset (in bytes in the entries blob)
 *   of a rule to delete.
 * - rules_to_delete_count: Length of the rules_to_delete array.
 * - output_entries: a struct ipt_replace for this function to populate.
 * - output_mappings: An array of size (info->num_entries) that specifies the mapping from old rules to
 *   new rules.  Input rule i corresponds to output rule (*output_mappings)[i].
 *
 * Returns 0 on success, non-zero on failure.
 *
 * On success, caller is responsible for calling free() on *output_entries and output_mappings.
 */
static int delete_rules(const struct ipt_getinfo *info, const struct ipt_get_entries *entries, const size_t *rules_to_delete, size_t rules_to_delete_count, struct ipt_replace **output_entries, size_t **output_mappings)
{
	// Iterate twice through the rules - once to compute the new offsets, a second to correct jumps in the chain.

	size_t rules_count = 0, rules_size = 5, offset = 0, offset_adjust = 0;
	size_t *new_offsets = malloc(sizeof(size_t)*2*rules_size);
	if (!new_offsets)
	{
		fprintf(stderr, "Failed to allocate new_offsets array.\n");
		return 1;
	}
	const struct ipt_entry *entry = entries->entrytable;
	while (1)
	{
		if (rules_count >= rules_size)
		{
			rules_size += 5;
			if (!(new_offsets = realloc(new_offsets, sizeof(size_t)*2*rules_size)))
			{
				fprintf(stderr, "Failed to reallocate new offsets array.\n");
				return 1;
			}
		}
		new_offsets[2*rules_count] = offset;
		new_offsets[2*rules_count+1] = offset - offset_adjust;
		rules_count++;
		int to_delete = 0;
		size_t idx;
		for(idx=0; idx<rules_to_delete_count; idx++) if (rules_to_delete[idx] == offset) to_delete = 1;
		if (to_delete)
		{
			offset_adjust += entry->next_offset;
		}
		if (entry->next_offset) offset += entry->next_offset;
		else break;
		if (offset < entries->size) entry = (const struct ipt_entry *)((const char *)entries->entrytable + offset);
		else break;
	}

	struct ipt_replace *oentries = malloc(sizeof(struct ipt_replace) + entries->size);
	if (!oentries)
	{
		fprintf(stderr, "Unable to allocate output entries.\n");
		free(new_offsets);
		return 1;
	}

	*output_mappings = malloc(sizeof(size_t)*info->num_entries);
	if (!(*output_mappings))
	{
		fprintf(stderr, "Unable to allocate output mappings array.\n");
		free(oentries);
		free(new_offsets);
		return 1;
	}
	size_t ooffset = 0, oprevoffset = 0;
	offset = 0;
	entry = entries->entrytable;
	struct ipt_entry *oentry = oentries->entries;
	size_t idx, input_idx=0, output_idx=0;
	unsigned int tmp_hook_entry[NF_INET_NUMHOOKS];
	memcpy(tmp_hook_entry, info->hook_entry, sizeof(tmp_hook_entry[0])*NF_INET_NUMHOOKS);
	memset(oentries->underflow, '\0', NF_IP_NUMHOOKS*sizeof(int));
	while (1)
	{
		(*output_mappings)[input_idx] = output_idx;
		input_idx++;
		int keep_rule = 1;
		for (idx=0; idx<rules_to_delete_count; idx++) if (rules_to_delete[idx] == offset)
		{
			keep_rule = 0;
			break;
		}
		if (keep_rule)
		{
			for (idx=0; idx < NF_IP_NUMHOOKS; idx++) if (info->valid_hooks & (1 << idx))
			{
				if (tmp_hook_entry[idx] == offset)
				{
					oentries->hook_entry[idx] = ooffset;
					//printf("Hook %lu start entry: %lu\n", idx, ooffset);
				}
				if (info->underflow[idx] == offset)
				{
					oentries->underflow[idx] = ooffset;
					//printf("Hook %lu underflow entry: %lu\n", idx, ooffset);
				}
			}
			output_idx++;
			memcpy(oentry, entry, entry->next_offset);
			int target = rule_has_jump_target(entry);
			if (target >= 0)
			{
				//printf("Rule jump target = %d\n", target);
				for (idx=0; idx<rules_count; idx++) if (new_offsets[2*idx] == (unsigned)target)
				{
					target = new_offsets[2*idx+1];
					break;
				}
				//printf("Rule new jump target = %d\n", target);
				set_rule_jump_target(oentry, target);
			}
			oentry->comefrom = oprevoffset;
			//printf("Writing rule at entry %ld (size %d; comefrom %lu; original offset %lu).\n", ooffset, entry->next_offset, oprevoffset, offset);
			oprevoffset = ooffset;
			ooffset += entry->next_offset;
			oentry = (struct ipt_entry *)((char *)oentries->entries + ooffset);
		}
		else
		{
			for (idx=0; idx < NF_IP_NUMHOOKS; idx++) if (info->valid_hooks & (1 << idx)) {
				// We will be deleting this rule.  The next one is the beginning of the chain.
				if (tmp_hook_entry[idx] == offset)
				{
					tmp_hook_entry[idx] += entry->next_offset;
				}
				// The last valid output entry is now the end of the chain.
				if (info->underflow[idx] == offset)
				{
					oentries->underflow[idx] = ooffset;
				}
			}
			//printf("Will delete rule at offset %lu.\n", offset);
		}
		if (entry->next_offset) offset += entry->next_offset;
		else break;
		if (offset < entries->size) entry = (const struct ipt_entry *)((const char *)entries->entrytable + offset);
		else break;
	}
	oentries->size = ooffset;
	oentries->num_entries = output_idx;
	//printf("Number of entries: %lu; size: %lu\n", output_idx, ooffset);

	// Fill in the boundaries of the builtin tables.
	strncpy(oentries->name, info->name, XT_TABLE_MAXNAMELEN);
	oentries->valid_hooks = info->valid_hooks;

	*output_entries = oentries;
	return 0;
}

static int delete_rules_chain(const struct ipt_getinfo *info, const struct ipt_get_entries *entries, const size_t *input_rules_to_delete, size_t input_rules_to_delete_count, struct ipt_replace **output_entries, size_t **output_mappings)
{
	// Iterate through the rules.  Delete the referenced rules and any rules which reference those.
	int deleted_rule = 1, rules_to_delete_count = input_rules_to_delete_count, rules_to_delete_size = input_rules_to_delete_count+5;
	size_t *rules_to_delete = malloc(sizeof(size_t)*rules_to_delete_size);
	if (!rules_to_delete)
	{
		fprintf(stderr, "Failed to allocate rules_to_delete.\n");
		return 1;
	}
	memcpy(rules_to_delete, input_rules_to_delete, sizeof(size_t)*rules_to_delete_count);
	while (deleted_rule)
	{
		deleted_rule = 0;
		size_t offset = 0;
		const struct ipt_entry *entry = entries->entrytable;
		while (1)
		{
			int keep_rule = 1, idx;
			for (idx=0; idx<rules_to_delete_count; idx++) if (rules_to_delete[idx] == offset)
			{
				keep_rule = 0;
				break;
			}
			if (keep_rule)
			{
				int target = rule_has_jump_target(entry);
				if (target >= 0)
				{
					for (idx=0; idx<rules_to_delete_count; idx++)
					{
						if ((size_t)target == rules_to_delete[idx])
						{
							if (rules_to_delete_count >= rules_to_delete_size)
							{
								rules_to_delete_size += 5;
								if (!(rules_to_delete = realloc(rules_to_delete, rules_to_delete_size)))
								{
									free(rules_to_delete);
									return 1;
								}
							}
							rules_to_delete[rules_to_delete_count] = offset;
							rules_to_delete_count++;
							deleted_rule = 1;
							//printf("Deleting chained rule at offset %ld (array %u).\n", offset, rules_to_delete_count-1);
						}
					}
				}
			}
			if (entry->next_offset) offset += entry->next_offset;
			else break;
			if (offset < entries->size) entry = (const struct ipt_entry *)((const char *)entries->entrytable + offset);
			else break;
		}
	}
	//printf("Deleting rules - all dependent rules found.\n");
	int result = delete_rules(info, entries, rules_to_delete, rules_to_delete_count, output_entries, output_mappings);
	free(rules_to_delete);
	return result;
}

/**
 * Remove a chain from the firewall, along with any rules in other chains
 * which reference it.
 *
 * Returns 0 on success, non-zero on failure.
 */
int cleanup_chain(const char *chain)
{
	struct ipt_get_entries *entries;
	struct ipt_getinfo info;

	int error;

        // Read out the firewall.
	if ((error = get_firewall_data(&info, &entries))) {
		return error;
	}

	// Locate the chain we want to remove.
	size_t offset = 0, rule_count=0;
	struct ipt_entry * entry = entries->entrytable;
	size_t chain_first_offset = 0, chain_last_offset=0, in_chain = 0;
	size_t rules_to_delete_count = 0, rules_to_delete_size=5;
	size_t *rules_to_delete = malloc(sizeof(size_t)*rules_to_delete_size);
	const char * chain_name = 0, * old_chain_name = 0;
	if (!rules_to_delete)
	{
		fprintf(stderr, "Unable to allocate rules_to_delete.\n");
		free(entries);
		return 1;
	}
	while (1)
	{
		rule_count++;
		// Detect chain transitions.  Don't detect builtin chains - can't delete those.
		struct xt_entry_target *my_target = ipt_get_target(entry);
		if (strcmp(my_target->u.user.name, XT_ERROR_TARGET) == 0)
		{
			chain_name = (const char *)my_target->data;
		}
		if (chain_name != old_chain_name)
		{
			if (strcmp(chain, chain_name) == 0)
			{
				in_chain = 1;
				chain_first_offset = offset;
			}
			else if ((in_chain) && (strcmp(chain, old_chain_name) == 0))
			{
				chain_last_offset = offset;
				break;
			}
			old_chain_name = chain_name;
		}
		if (in_chain)
		{
			if (rules_to_delete_count >= rules_to_delete_size)
			{
				//printf("Reallocate rules_to_delete.\n");
				rules_to_delete_size += 5;
				if (!(rules_to_delete = realloc(rules_to_delete, sizeof(rules_to_delete[0])*rules_to_delete_size)))
				{
					fprintf(stderr, "Unable to reallocate rules_to_delete.\n");
					free(entries);
					return 1;
				}
			}
			//printf("Delete rule %ld (offset %lu).\n", offset, rules_to_delete_count);
			rules_to_delete[rules_to_delete_count] = offset;
			rules_to_delete_count++;
		}
		if (entry->next_offset) offset += entry->next_offset;
		else break;
		if (offset < entries->size) entry = (struct ipt_entry *)((char *)entries->entrytable + offset);
		else break;
	} // NOTE: there's guaranteed to be the ERROR entry as the last rule, so we are guaranteed to find the last offset.
	if (chain_last_offset <= chain_first_offset)
	{
		fprintf(stderr, "Invalid iptables chain %s.\n", chain);
		free(rules_to_delete);
		free(entries);
		return 1;
	}
	//printf("Cleaning rules %ld through %ld.\n", chain_first_offset, chain_last_offset);

	struct ipt_replace * output_entries;
	size_t *output_mappings;
	int result = delete_rules_chain(&info, entries, rules_to_delete, rules_to_delete_count, &output_entries, &output_mappings);
	free(rules_to_delete);

	if (result) return result;
	if (!output_entries)
	{
		fprintf(stderr, "Unable to create firewall entries.\n");
		free(entries);
		return 1;
	}

	// Write out firewall.
	result = write_firewall_entries(&info, output_entries, output_mappings);
	free(output_entries);
	free(output_mappings);
	free(entries);
	return result;
}

