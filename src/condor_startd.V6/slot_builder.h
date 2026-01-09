class MachAttributes;
class CpuAttributes;
enum BuildSlotFailureMode;

bool string_to_BuildSlotFailureMode(const char * str, enum BuildSlotFailureMode & failmode);
const char * BuildSlotFailureMode_to_string(BuildSlotFailureMode failmode);

void initTypes( int max_types, std::vector<std::string>& type_strings, BuildSlotFailureMode failmode, std::vector<bool>& fail_types);
//CpuAttributes *buildSlot( MachAttributes *m_attr, int slot_id, const std::string&, unsigned int type_id, BuildSlotFailureMode failmode);
// These are called on startup and on reconfig to build the full set of slots
int countTypes( int max_types, int num_cpus, int **type_nums, bool **backfill_types, BuildSlotFailureMode failmode);

// init the table of volumes that are used for EXECUTE directories
// this populates the volumes table in the MachAttributes instance
// for most EPs there will be a single volume, but it is permitted to have one per slot
// via SLOT<n>_EXECUTE
bool initExecutePartitionTable (
	MachAttributes *m_attr,
	int max_types,
	int num_res,
	int *type_nums,
	bool *backfill_types,
	BuildSlotFailureMode failmode,
	std::vector<bool>& fail_types);

// provision startup slot resources
CpuAttributes **buildCpuAttrs(
	MachAttributes *m_attr,
	int max_types,
	const std::vector<std::string>& type_strings,
	int num_res,
	int *type_nums,
	bool *backfill_types,
	BuildSlotFailureMode failmode,
	std::vector<bool>& fail_types);
