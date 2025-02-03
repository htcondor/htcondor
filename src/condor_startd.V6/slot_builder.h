class MachAttributes;
class CpuAttributes;
enum BuildSlotFailureMode;

bool string_to_BuildSlotFailureMode(const char * str, enum BuildSlotFailureMode & failmode);
const char * BuildSlotFailureMode_to_string(BuildSlotFailureMode failmode);

void initTypes( int max_types, std::vector<std::string>& type_strings, BuildSlotFailureMode failmode, std::vector<bool>& fail_types);
//CpuAttributes *buildSlot( MachAttributes *m_attr, int slot_id, const std::string&, unsigned int type_id, BuildSlotFailureMode failmode);
// These are called on startup and on reconfig to build the full set of slots
int countTypes( int max_types, int num_cpus, int **type_nums, bool **backfill_types, BuildSlotFailureMode failmode);

CpuAttributes **buildCpuAttrs(
	MachAttributes *m_attr,
	int max_types,
	const std::vector<std::string>& type_strings,
	int num_res,
	int *type_nums,
	bool *backfill_types,
	BuildSlotFailureMode failmode,
	std::vector<bool>& fail_types);
