/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/* 
    This file defines the ResAttributes class which stores various
	attributes about a given resource such as load average, available
	swap space, disk space, etc.

   	Written 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _RES_ATTRIBUTES_H
#define _RES_ATTRIBUTES_H

#include "condor_common.h"
#include "condor_classad.h"

#include <map>
#include <stdint.h>
#include <string>
#include <stack>
using std::string;

enum BuildSlotFailureMode { 
	AllOrNothing, // do nothing if you can't do everything
	BestEffort,   // build the slots that fit, then stop
	Except,       // Legacy behavior, kill the process if you can't comply
};

// codes for use with CpuAttributes::set_broken
#define BROKEN_CODE_NO_RES       1   // not enough resources to build the slot
#define BROKEN_CODE_BIND_FAIL    2   // could not bind enough non-fungible resources
#define BROKEN_CODE_NO_EXEC_DIR  3   // could not set an Execute dir
#define BROKEN_CODE_UNCLEAN      4   // could not clean up after job
#define BROKEN_CODE_UNCLEAN_LV   5   // could not clean up Logical Volume after job
#define BROKEN_CODE_HUNG_PID     6   // could not delete all job processes after job

class VolumeManager;
class Resource;

// we cast share to int before comparison, because it might be a float
// and we don't want the compiler to complain
#define IS_AUTO_SHARE(share) ((int)share == AUTO_SHARE)
const int AUTO_SHARE = -9999;

// This is used as a place-holder value when configuring memory share
// for a slot.  It is later updated by dividing the remaining resources
// evenly between slots using AUTO_MEM.
const int AUTO_RES = AUTO_SHARE;


// This is used with the AttribValue structure to identify the datatype
// for the value (selects an entry in the union)
enum {
	AttribValue_DataType_String = 0,
	AttribValue_DataType_Double,
	AttribValue_DataType_Int,

	AttribValue_DataType_Max // this should be last
};

// holds a single value to be written into a classad, note that
// fields with names that begin with ix are offsets from the start
// of the structure to the actual field data.  In use, one would 
// normally allocate (sizeof(AttribValue) + size_of_strings + size_of_extra_data)
// and then setup index fields to make it possible to locate the extra data.
//
typedef struct _AttribValue {
	const char * pszAttr;	// Classad attrib name, may be a pointer to a constant
                            // or a pointer into the allocation for this structure.

	void * pquery;			// pointer to the original query used to get the data.
                            // may be null. do not free from this structure.

	int cb;                 // allocation size of this structure, may be larger than sizeof(AttribValue)
	int vtype;              // an AttribValue_DataType_xxx value, the Attrib data type decodes the value union
	union {                 // 0=string, 1=double, 2=int
		struct {
			int       ix;   // offset from the start of this structure to the string data
			int       cb;   // size of the string data in bytes including terminating null.
			} zstr;         // vtype == AttribValue_DataType_String
		double    d;        // vtype == AttribValue_DataType_Double
		int       i;        // vtype == AttribValue_DataType_Int
		long long ll;       // vtype == AttribValue_DataType_Int64 (future)
		//struct {
		//	int       ix;
		//	int		  cb;
		//    } bin;          // vtype == AttribValue_DataType_Bin
	} value;

	// helper function to fetch the value of string type values.
    // AttribValue is a packed structure, a single allocation with a header
	//
	const char * StringValue() const {
        if ((this->vtype == AttribValue_DataType_String) &&
            (this->value.zstr.ix >= (int)sizeof(*this)) &&
            (this->value.zstr.ix < this->cb)) {
			return (const char *)this + this->value.zstr.ix;
        }
        return NULL;
    }

    void AssignToClassAd(ClassAd* cp) const {
        switch (this->vtype) {
            case AttribValue_DataType_String:
                cp->Assign(this->pszAttr, this->StringValue());
                break;
            case AttribValue_DataType_Double:
                cp->Assign(this->pszAttr, this->value.d);
                break;
            case AttribValue_DataType_Int:
                cp->Assign(this->pszAttr, this->value.i);
                break;
        }
    }

    // allocate an initialized packed attrib and value, this consists of a
    // header (struct _AttribValue) followed by the attribute string, followed by
    // the value string.
    //
    static struct _AttribValue * Allocate(const char * pszAttr, const char * value) {
        size_t cchValue = value ? strlen(value)+1 : 1;
        size_t cchAttr  = strlen(pszAttr) + 1;
        size_t cb = sizeof(struct _AttribValue) + cchValue + cchAttr;
        struct _AttribValue * pav = (struct _AttribValue *)malloc(cb);
        if (pav) {
            pav->cb = (int)cb;
            strcpy((char*)(pav+1), pszAttr);
            pav->pszAttr = (char*)(pav+1);

            pav->vtype = AttribValue_DataType_String;
            pav->value.zstr.ix = (int)(sizeof(struct _AttribValue) + cchAttr);
            pav->value.zstr.cb = (int)cchValue;
            if (value)
               strcpy(const_cast<char*>(pav->StringValue()), value);
            else
               (const_cast<char*>(pav->StringValue()))[0] = 0;
        }
        return pav;
    }

} AttribValue;

//
class NFROwner
{
public:
	unsigned short int id{0};     // static or partitionable id slot id
	unsigned short int dyn_id{0}; // dynamic id, dslot_id or broken id
	unsigned short int group{0};  // if non-zero the resource is shared by a group of slots
	unsigned short int active:1{0}; // non-zero if resource is owned by an active slot (set only when dyn_id is > 0)
	NFROwner(int i, int j=0) : id(i), dyn_id(j) {};

	bool empty() { return id == 0 && dyn_id == 0; }
};

class NonFungibleRes
{
public:
	NonFungibleRes(const std::string & _id) : id(_id), owner(0), bkowner(0) {}
	std::string id;
	NFROwner  owner;
	NFROwner  bkowner;

	// report if resource is "owned" by the given slot
	// if the given slot matches the primary owner, then return 1
	// if the given slot matches the backfill owner and the primary owner is not set return 2
	// if owned by both backfill and an active primary, return 3
	int is_owned(int slot_id, int sub_id) const {
		if (owner.id == slot_id && owner.dyn_id == sub_id) return 1;
		if (bkowner.id == slot_id && bkowner.dyn_id == sub_id) {
			// if the NFR is backfill owned by the given slot, and *also* by any active slot
			if (owner.dyn_id > 0 || owner.active) return 3;
			return 2;
		}
		return 0;
	}
};

class NonFungibleType
{
public:
	std::vector<NonFungibleRes> ids;
	std::map<std::string, ClassAd> props;
};

// Machine-wide attributes.  
class MachAttributes
{
public:
	// quantity (double) of resource for each resource tag
	typedef std::map<string, double, classad::CaseIgnLTStr> slotres_map_t;

	// these are used for non-fungible ids to track resource IDs that cannot be assigned
	// for each resource tag, which resource IDs are offline. note that this is a SET rather than a VECTOR
	typedef std::set<std::string> slotres_offline_ids_t;
	typedef std::map<string, slotres_offline_ids_t, classad::CaseIgnLTStr> slotres_offline_ids_map_t;

	// some slots may have constraints on which non-fungible resources they can use, this is handled
	// by having an optional property classad for each unique non-fungible id, and a optional constraint
	// for each type of nonfungible id for each slot
	typedef std::map<std::string, std::string, classad::CaseIgnLTStr> slotres_constraint_map_t;

	typedef std::map<std::string, NonFungibleType, classad::CaseIgnLTStr> slotres_nft_map_t;
	// these are used as lists of non-fungible ids for various purposes
	typedef std::vector<std::string> slotres_assigned_ids_t;
	typedef std::map<string, slotres_assigned_ids_t, classad::CaseIgnLTStr> slotres_devIds_map_t;

	MachAttributes();
	~MachAttributes();

	void init();
    void init_user_settings(); // read STARTD_PUBLISH_WINREG param and parse it
                               // creating data structure needed in compute and publish
    void init_machine_resources();

	void publish_static(ClassAd*);     // things that can only change on reconfig
	void publish_common_dynamic(ClassAd*, bool global=false); // things that can change at runtime
	void publish_slot_dynamic(ClassAd*, int slotid, int slotsubid, bool backfill, const std::string & res_conflict); // things that can change at runtime
	void compute_config();      // what compute(A_STATIC | A_SHARED) used to do
	void compute_for_update();  // formerly compute(A_UPDATE | A_SHARED) -  before we send ads to the collector
	void compute_for_policy();  // formerly compute(A_TIMEOUT | A_SHARED) - before we evaluate policy like PREEMPT

	void update_condor_load(double load) {
		m_condor_load = load;
		if( m_condor_load > m_load ) {
			m_condor_load = m_load;
		}
	}

	void set_nft_activity(std::map<int,bool> &active_slotids_map);
	bool has_nft_conflicts(int slotid, int slotsubid) const;

		// Initiate benchmark computations benchmarks on the given resource
	void start_benchmarks( Resource*, int &count );	
	void benchmarks_finished( Resource* );

#if defined(WIN32)
		// For testing communication with the CredD, if one is
		// configured
	void credd_test();
	void reset_credd_test_throttle() { m_last_credd_test = 0; }
#endif

		// On shutdown, print one last D_IDLE debug message, so we don't
		// lose statistics when the startd is restarted.
	void final_idle_dprintf() const;

		// Functions to return the value of shared attributes
	double			num_cpus()	const { return m_num_cpus; };
	double			num_real_cpus()	const { return m_num_real_cpus; };
	int				phys_mem()	const { return m_phys_mem; };
	long long		virt_mem()	const { return m_virt_mem; };
	long long		total_disk() const { return m_total_disk; }
	bool			always_recompute_disk() const { return m_always_recompute_disk; }
	double		machine_load()			const { return m_load; };
	double		machine_condor_load()	const { return m_condor_load; };
	time_t		machine_keyboard_idle() const { return m_idle; };
	time_t		machine_console_idle()	const { return m_console_idle; };
	const slotres_map_t& machres() const { return m_machres_map; }
	const slotres_nft_map_t& machres_devIds() const { return m_machres_nft_map; }
	const ClassAd& machres_attrs() const { return m_machres_attr; }
	const char * AllocateDevId(const std::string & tag, const char* request, int assign_to, int assign_to_sub, bool backfill, int assign_from_sub);
	bool         ReleaseDynamicDevId(const std::string & tag, const char * id, int was_assign_to, int was_assign_to_sub, int new_sub=0);
	bool         DevIdMatches(const NonFungibleType & nft, int ixid, ConstraintHolder & require);
	const char * DumpDevIds(std::string & buf, const char * tag = NULL, const char * sep = "\n");
	void         ReconfigOfflineDevIds();
	int          RefreshDevIds(const std::string & tag, slotres_assigned_ids_t & slot_res_devids, int assign_to, int assign_to_sub);
	int          ReportBrokenDevIds(const std::string & tag, slotres_assigned_ids_t & devids, int broken_sub_id);
	bool         ComputeDevProps(ClassAd & ad, const std::string & tag, const slotres_assigned_ids_t & ids);
	//bool ReAssignDevId(const std::string & tag, const char * id, void * was_assigned_to, void * assign_to);

	// return WithinResourceLimits, these also calculates it on the first call
	const char * withinLimitsExpression(); // regular WithinResourceLimits
	const char * consumptionLimitsExpression(); // consumption policy variant

private:

		// Dynamic info
	double			m_load;
	double			m_condor_load;
	double			m_owner_load;
	long long		m_virt_mem;
	time_t			m_idle;
	time_t			m_console_idle;
	int				m_mips;
	int				m_kflops;
	time_t			m_last_benchmark;   // Last time we computed benchmarks
	time_t			m_last_keypress; 	// Last time m_idle decreased
	bool			m_seen_keypress;    // Have we seen our first keypress yet?
	int				m_clock_day;
	int				m_clock_min;
	std::vector<AttribValue *> m_lst_dynamic;    // list of user specified dynamic Attributes
	int64_t			m_docker_cached_image_size;  // Size in bytes of our cached docker images -1 means unknown
	time_t			m_docker_cached_image_size_time;
#if defined(WIN32)
	char*			m_local_credd;
	time_t			m_last_credd_test;
#endif
		// Static info
	double			m_num_cpus;
	double			m_num_real_cpus;
	int				m_phys_mem;
	bool			m_always_recompute_disk; // set from STARTD_RECOMPUTE_DISK_FREE knob and DISK knob
	bool			m_no_job_networking_aware{false}; // include expressions for NO_JOB_NETWORKING
	long long		m_total_disk; // the value of total_disk if m_recompute_disk is false
	slotres_map_t   m_machres_map;
	slotres_nft_map_t m_machres_nft_map;
	slotres_devIds_map_t m_machres_offline_devIds_map; // startup list of offline ids
	slotres_offline_ids_map_t m_machres_runtime_offline_ids_map; // dynamic list of offline ids (unique set of ids, startup + runtime)
	// the various maps above look like this
	// m_machres_map["GPUs"]                     = 2.0
	// m_machres_offline_devIds_map["GPUs"]      = ["GPU-AAA", ...]
	// m_machres_runtime_offline_ids_map["GPUs"] = std::set("GPU-AAA", ...)
	// m_machres_nft_map["GPUs"] ={[("GPU-AAA", 1_1), ("GPU-BBB", 1_2), ...], [ "GPU-AAA" -> props; "GPU-BBB" -> props] }

	static bool init_machine_resource(MachAttributes * pme, HASHITER & it);
	double init_machine_resource_from_script(const char * tag, const char * script_cmd);
	ClassAd         m_machres_attr;

	std::string		m_within_limits_expr_str;       // Expression for WithinResourceLimits
	std::string		m_consumption_limits_expr_str;  // Expression for consumption policy WithinResourceLimits

	char*			m_arch;
	char*			m_opsys;
	int 			m_opsysver;
	char*			m_opsys_and_ver;
	int			m_opsys_major_ver;
	char*			m_opsys_name;
	char*			m_opsys_long_name;
	char*			m_opsys_short_name;
	char*			m_opsys_legacy;
	char*			m_uid_domain;
	char*			m_filesystem_domain;
	int				m_idle_interval; 	// for D_IDLE dprintf messages
	std::vector<AttribValue *> m_lst_static;     // list of user-specified static attributes

     	// temporary attributes for raw utsname info
	char*			m_utsname_sysname;
	char*			m_utsname_nodename;
	char*			m_utsname_release;
	char* 			m_utsname_version;
	char*			m_utsname_machine;


	// this holds strings that m_lst_static and m_lst_dynamic point to
	// it is initialized from the param STARTD_PUBLISH_WINREG and then parsed/modified and used
	// to initialize m_lst_static and m_lst_dynamic.  these two lists
	// continue to hold pointers into this.  do not free it unless you first empty those
	// lists. -tj
	std::vector<std::string> m_user_specified;
	int             m_user_settings_init;  // set to true when init_user_settings has been called at least once.

	std::string		m_named_chroot;
#if defined ( WIN32 )
	int				m_got_windows_version_info;
	OSVERSIONINFOEX	m_window_version_info;
    char*           m_dot_Net_Versions;
#endif

};	

class ResBag;

// CPU-specific attributes.  
class CpuAttributes
{
public:
    typedef MachAttributes::slotres_map_t slotres_map_t;
	typedef MachAttributes::slotres_devIds_map_t slotres_devIds_map_t;
	typedef std::map<std::string, ClassAd> slotres_props_t;
	typedef MachAttributes::slotres_offline_ids_map_t slotres_offline_ids_map_t;
	typedef MachAttributes::slotres_assigned_ids_t slotres_assigned_ids_t;
	typedef MachAttributes::slotres_constraint_map_t slotres_constraint_map_t;

	friend class AvailAttributes;
	friend class ResBag;

	struct _slot_request {
		double num_cpus=0;
		double virt_mem_fraction=0;
		double disk_fraction=0;
		int num_phys_mem=0;
		bool allow_fractional_cpus=false;
		MachAttributes::slotres_map_t slotres;
		MachAttributes::slotres_constraint_map_t slotres_constr;
		const char * dump(std::string & buf) const;
	};

	CpuAttributes( unsigned int slot_type, double num_cpus,
				   int num_phys_mem, double virt_mem_fraction,
				   double disk_fraction,
				   const slotres_map_t& slotres_map,
				   const slotres_constraint_map_t& slotres_req_map,
				   const std::string &execute_dir, const std::string &execute_partition_id );

	// init a slot_request from config strings
	static bool buildSlotRequest(
		_slot_request & request,
		MachAttributes *m_attr,
		const std::string& list,
		unsigned int type_id,
		BuildSlotFailureMode failmode);

	// construct from a _slot_request
	CpuAttributes(unsigned int slot_type,
		const struct _slot_request & req,
		const std::string &execute_dir, const std::string &execute_partition_id )
		: rip(nullptr)
		, c_condor_load(-1.0)
		, c_owner_load(-1.0)
		, c_idle(-1)
		, c_console_idle(-1)
		, c_virt_mem(0)
		, c_disk(0)
		, c_total_disk(0)
		, c_phys_mem(req.num_phys_mem)
		, c_slot_mem(req.num_phys_mem)
		, c_allow_fractional_cpus(req.allow_fractional_cpus)
		, c_num_cpus(req.num_cpus)
		, c_slotres_map(req.slotres)
		, c_slottot_map(req.slotres)
		, c_slotres_constraint_map(req.slotres_constr)
		, c_num_slot_cpus(req.num_cpus)
		, c_virt_mem_fraction(req.virt_mem_fraction)
		, c_disk_fraction(req.disk_fraction)
		, c_slot_disk(0)
		, c_execute_dir(execute_dir)
		, c_execute_partition_id(execute_partition_id)
		, c_type_id(slot_type)
		, c_broken_code(0)
		, c_broken_reason()
	{
	}

	void attach( Resource* );	// Attach to the given Resource
	// bind non-fungable resource ids to a slot
	bool bind_DevIds(MachAttributes* map, int slot_id, int slot_sub_id, bool backfill_slot, bool abort_on_fail, int donor_sub_id);
	// release non-fungable resource ids back to parent
	void unbind_DevIds(MachAttributes* map, int slot_id, int slot_sub_id, int new_sub_id);
	// check for offline changes for non-fungible resource ids
	void reconfig_DevIds(MachAttributes* map, int slot_id, int slot_sub_id);

	void publish_static(ClassAd*, const ResBag * inuse, const ResBag * broken) const;  // Publish desired info to given CA
	void publish_dynamic(ClassAd*) const;  // Publish desired info to given CA
	void compute_virt_mem_share(double virt_mem);
	void compute_disk();
	void set_condor_load(double load) { c_condor_load = load; }

		// Load average methods
	double condor_load() const { return c_condor_load; };
	double owner_load() const { return c_owner_load; };
	double total_load() const { return c_owner_load + c_condor_load; };
	void set_owner_load( double v ) { c_owner_load = v; };

		// Keyboad activity methods
	void set_keyboard( time_t k ) { c_idle = k; };
	void set_console( time_t k ) { c_console_idle = k; };
	time_t keyboard_idle() const { return c_idle; };
	time_t console_idle() const { return c_console_idle; };
	unsigned int type_id() const { return c_type_id; };

	void display_load(int dpf_flags) const;
	void dprintf( int, const char*, ... ) const;
	const char * cat_totals(std::string & buf) const;

	double total_cpus() const { return c_num_slot_cpus; }
	double num_cpus() const { return c_num_cpus; }
	bool allow_fractional_cpus(bool allow) { bool old = c_allow_fractional_cpus; c_allow_fractional_cpus = allow; return old; }
	long long get_disk() const { return c_disk; }
	double get_disk_fraction() const { return c_disk_fraction; }
	long long get_total_disk() const { return c_total_disk; }
	char const *executeDir() { return c_execute_dir.c_str(); }
	char const *executePartitionID() { return c_execute_partition_id.c_str(); }
    const slotres_map_t& get_slotres_map() { return c_slotres_map; }
    const slotres_devIds_map_t & get_slotres_ids_map() { return c_slotres_ids_map; }

	void set_broken(int code, std::string_view reason) { c_broken_code = code; c_broken_reason = reason; }
	unsigned int is_broken(std::string * reason=nullptr) const { if (reason) *reason = c_broken_reason; return c_broken_code; }

	void init_total_disk(const CpuAttributes* r_attr) {
		if (r_attr && (r_attr->c_execute_partition_id == c_execute_partition_id)) {
			c_total_disk = r_attr->c_total_disk;
		}
	}
	bool set_total_disk(long long total, bool refresh, VolumeManager * volman);

	CpuAttributes& operator+=( CpuAttributes& rhs);
	CpuAttributes& operator-=( CpuAttributes& rhs);

private:
	Resource*	 	rip;

		// Dynamic info
	double			c_condor_load;
	double			c_owner_load;
	time_t			c_idle;
	time_t			c_console_idle;
	long long c_virt_mem;
	long long c_disk;
		// total_disk here means total disk space on the partition containing
		// this slot's execute directory.  Since each slot may have an
		// execute directory on a different partition, we have to store this
		// here in the per-slot data structure, rather than in the machine-wide
		// data structure.
	long long c_total_disk;

	int				c_phys_mem;
	int				c_slot_mem;
	bool			c_allow_fractional_cpus;
	double			c_num_cpus;
    // custom slot resources
    slotres_map_t c_slotres_map;
    slotres_map_t c_slottot_map;
	slotres_constraint_map_t c_slotres_constraint_map;
	slotres_devIds_map_t c_slotres_ids_map; // map of resource tag to vector of Assigned devids
	slotres_props_t c_slotres_props_map; // map if resource tag to aggregate ClassAd of props for custom resource

    // totals
	double			c_num_slot_cpus;

		// These hold the fractions of shared, dynamic resources
		// that are allocated to this CPU.
	double			c_virt_mem_fraction;

	double			c_disk_fraction; // share of execute dir partition
	double			c_slot_disk; // share of execute dir partition
	std::string     c_execute_dir;
	std::string     c_execute_partition_id;  // unique id for partition

	unsigned int	c_type_id;		// The slot type of this resource

		// resource brokenness, set when the resource could not be provisioned
		// or when one of the provisioned resources is not working
	unsigned int    c_broken_code;
	std::string     c_broken_reason;
};	

// a loose bag of resource quantities, for doing resource math
class ResBag {
public:

	ResBag() = default;
	ResBag(const class CpuAttributes & c_attr) 
		: cpus(c_attr.c_num_slot_cpus)
		, disk(c_attr.c_slot_disk)
		, mem(c_attr.c_slot_mem)
		, slots(1)
		, resmap(c_attr.c_slottot_map)
	{}

	ResBag& operator+=(const CpuAttributes& rhs);
	ResBag& operator-=(const CpuAttributes& rhs);

	bool empty() const {return (cpus<=0) && !disk && !mem && resmap.empty();}
	void reset();
	bool underrun(std::string * names) const;
	bool excess(std::string * names) const;
	void clear_underrun(); // reset only the underrun values
	void clear_excess();   // reset only the excess values
	const char * dump(std::string & buf) const;
	void Publish(ClassAd& ad, const char * prefix) const;
	const MachAttributes::slotres_map_t & nfrmap() const { return resmap; }

protected:
	double     cpus{0};
	long long  disk{0};
	int        mem{0};
	int        slots{0};
	MachAttributes::slotres_map_t resmap;
	friend class CpuAttributes;
};

class AvailDiskPartition
{
 public:
	AvailDiskPartition() {
		m_disk_fraction = 1.0;
		m_auto_count = 0;
	}
	double m_disk_fraction; // share of this partition that is not taken yet
	int m_auto_count; // number of slots using "auto" share of this partition
};

// Available machine-wide attributes
class AvailAttributes
{
public:
    typedef MachAttributes::slotres_map_t slotres_map_t;

	AvailAttributes( MachAttributes* map );

	bool decrement( CpuAttributes* cap );
	bool computeRemainder(slotres_map_t & remain_cap, slotres_map_t & remain_cnt);
	bool computeAutoShares( CpuAttributes* cap, slotres_map_t & remain_cap, slotres_map_t & remain_cnt);
	void cat_totals(std::string & buf, const char * execute_partition_id);
	// reduce request as necessary to force a fit, and report the reductions in the unfit string
	void trim_request_to_fit( CpuAttributes::_slot_request & req, const char * execute_partition_id, std::string & unfit );

private:
	int				a_num_cpus;
	int             a_num_cpus_auto_count; // number of slots specifying "auto"
	int				a_phys_mem;
	int             a_phys_mem_auto_count; // number of slots specifying "auto"
	float			a_virt_mem_fraction;
	int             a_virt_mem_auto_count; // number of slots specifying "auto"

    slotres_map_t a_slotres_map;
    slotres_map_t a_autocnt_map;

		// number of slots using "auto" for disk share in each partition
	std::map<std::string,AvailDiskPartition> m_execute_partitions;

	AvailDiskPartition &GetAvailDiskPartition(std::string const &execute_partition_id);
};

#endif /* _RES_ATTRIBUTES_H */
