class MachAttributes;
class CpuAttributes;
void initTypes( int max_types, StringList **type_strings, int);
CpuAttributes *buildSlot( MachAttributes *m_attr, int slot_id, StringList *, int type, bool should_except);
int countTypes( int max_types, int num_cpus, int **type_nums, bool );
CpuAttributes **buildCpuAttrs( MachAttributes *m_attr, int max_types, StringList **type_strings, int num_res, int *type_nums, bool );
