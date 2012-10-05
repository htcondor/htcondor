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
#include <string>
using std::string;

class Resource;

typedef int amask_t;

const amask_t A_PUBLIC	= 1;
// no longer used: A_PRIVATE = 2
const amask_t A_STATIC	= 4;
const amask_t A_TIMEOUT	= 8;
const amask_t A_UPDATE	= 16;
const amask_t A_SHARED	= 32;
const amask_t A_SUMMED	= 64;
const amask_t A_EVALUATED = 128; 
const amask_t A_SHARED_SLOT = 256;
/*
  NOTE: We don't want A_EVALUATED in A_ALL, since it's a special bit
  that only applies to the Resource class, and it shouldn't be set
  unless we explicity ask for it.
  Same thing for A_SHARED_SLOT...
*/
const amask_t A_ALL	= (A_UPDATE | A_TIMEOUT | A_STATIC | A_SHARED | A_SUMMED);
/*
  HOWEVER: We do want to include A_EVALUATED and A_SHARED_SLOT in a
  single bitmask since there are multiple places in the startd where
  we really want *everything*, and it's lame to have to keep changing
  all those call sites whenever we add another special-case bit.
*/
const amask_t A_ALL_PUB	= (A_PUBLIC | A_ALL | A_EVALUATED | A_SHARED_SLOT);

#define IS_PUBLIC(mask)		((mask) & A_PUBLIC)
#define IS_STATIC(mask)		((mask) & A_STATIC)
#define IS_TIMEOUT(mask)	((mask) & A_TIMEOUT)
#define IS_UPDATE(mask)		((mask) & A_UPDATE)
#define IS_SHARED(mask)		((mask) & A_SHARED)
#define IS_SUMMED(mask)		((mask) & A_SUMMED)
#define IS_EVALUATED(mask)	((mask) & A_EVALUATED)
#define IS_SHARED_SLOT(mask) ((mask) & A_SHARED_SLOT)
#define IS_ALL(mask)		((mask) & A_ALL)

// This is used as a place-holder value when configuring slot shares of
// system resources.  It is later updated by dividing the remaining resources
// evenly between slots using AUTO_SHARE.
const float AUTO_SHARE = 123;

// The following avoids compiler warnings about == being dangerous
// for floats.
#define IS_AUTO_SHARE(share) ((int)share == (int)AUTO_SHARE)

// This is used as a place-holder value when configuring memory share
// for a slot.  It is later updated by dividing the remaining resources
// evenly between slots using AUTO_MEM.
const int AUTO_MEM = -123;
const int AUTO_RES = -9999;
const int AUTO_CPU = -9999;

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
        int cchValue = value ? strlen(value)+1 : 1;
        int cchAttr  = strlen(pszAttr) + 1;
        int cb = sizeof(struct _AttribValue) + cchValue + cchAttr;
        struct _AttribValue * pav = (struct _AttribValue *)malloc(cb);
        if (pav) {
            pav->cb = cb;
            strcpy((char*)(pav+1), pszAttr);
            pav->pszAttr = (char*)(pav+1);

            pav->vtype = AttribValue_DataType_String;
            pav->value.zstr.ix = sizeof(struct _AttribValue) + cchAttr;
            pav->value.zstr.cb = cchValue;
            if (value)
               strcpy(const_cast<char*>(pav->StringValue()), value);
            else
               (const_cast<char*>(pav->StringValue()))[0] = 0;
        }
        return pav;
    }

} AttribValue;

// Machine-wide attributes.  
class MachAttributes
{
public:
    typedef std::map<string, double> slotres_map_t;

	MachAttributes();
	~MachAttributes();

	void init();
    void init_user_settings(); // read STARTD_PUBLISH_WINREG param and parse it
                               // creating data structure needed in compute and publish
    void init_machine_resources();

	void publish( ClassAd*, amask_t );  // Publish desired info to given CA
	void compute( amask_t );			  // Actually recompute desired stats

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
	void final_idle_dprintf();

		// Functions to return the value of shared attributes
	int				num_cpus()	{ return m_num_cpus; };
	int				num_real_cpus()	{ return m_num_real_cpus; };
	int				phys_mem()	{ return m_phys_mem; };
	unsigned long   virt_mem()	{ return m_virt_mem; };
	float		load()			{ return m_load; };
	float		condor_load()	{ return m_condor_load; };
	time_t		keyboard_idle() { return m_idle; };
	time_t		console_idle()	{ return m_console_idle; };
    const slotres_map_t& machres() const { return m_machres_map; }
    const ClassAd& machattr() const { return m_machres_attr; }

private:

		// Dynamic info
	float			m_load;
	float			m_condor_load;
	float			m_owner_load;
	unsigned long   m_virt_mem;
	time_t			m_idle;
	time_t			m_console_idle;
	int				m_mips;
	int				m_kflops;
	time_t			m_last_benchmark;   // Last time we computed benchmarks
	time_t			m_last_keypress; 	// Last time m_idle decreased
	bool			m_seen_keypress;    // Have we seen our first keypress yet?
	int				m_clock_day;
	int				m_clock_min;
	List<AttribValue> m_lst_dynamic;    // list of user specified dynamic Attributes
#if defined(WIN32)
	char*			m_local_credd;
	time_t			m_last_credd_test;
#endif
		// Static info
	int				m_num_cpus;
	int				m_num_real_cpus;
	int				m_phys_mem;
    slotres_map_t   m_machres_map;
    ClassAd         m_machres_attr;

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
	char*			m_ckptpltfrm;
	List<AttribValue> m_lst_static;     // list of user-specified static attributes

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
	StringList      m_user_specified;
	int             m_user_settings_init;  // set to true when init_user_settings has been called at least once.

	std::string		m_named_chroot;
#if defined ( WIN32 )
	int				m_got_windows_version_info;
	OSVERSIONINFOEX	m_window_version_info;
    char*           m_dot_Net_Versions;
#endif

};	


// CPU-specific attributes.  
class CpuAttributes
{
public:
    typedef MachAttributes::slotres_map_t slotres_map_t;

	friend class AvailAttributes;

	CpuAttributes( MachAttributes*, int slot_type, int num_cpus, 
				   int num_phys_mem, float virt_mem_fraction,
				   float disk_fraction,
                   const slotres_map_t& slotres_map,
				   MyString &execute_dir, MyString &execute_partition_id );

	void attach( Resource* );	// Attach to the given Resource
									   

	void publish( ClassAd*, amask_t );  // Publish desired info to given CA
	void compute( amask_t );			  // Actually recompute desired stats

		// Load average methods
	float condor_load() { return c_condor_load; };
	float owner_load() { return c_owner_load; };
	float total_load() { return c_owner_load + c_condor_load; };
	void set_owner_load( float v ) { c_owner_load = v; };

		// Keyboad activity methods
	void set_keyboard( time_t k ) { c_idle = k; };
	void set_console( time_t k ) { c_console_idle = k; };
	time_t keyboard_idle() { return c_idle; };
	time_t console_idle() { return c_console_idle; };
	int	type() { return c_type; };

	void display( amask_t );
	void dprintf( int, const char*, ... );
	void show_totals( int );

	int num_cpus() { return c_num_cpus; }
	float get_disk() { return c_disk; }
	float get_disk_fraction() { return c_disk_fraction; }
	unsigned long get_total_disk() { return c_total_disk; }
	char const *executeDir() { return c_execute_dir.Value(); }
	char const *executePartitionID() { return c_execute_partition_id.Value(); }
    const slotres_map_t& get_slotres_map() { return c_slotres_map; }
    const MachAttributes* get_mach_attr() { return map; }

	CpuAttributes& operator+=( CpuAttributes& rhs);
	CpuAttributes& operator-=( CpuAttributes& rhs);

private:
	Resource*	 	rip;
	MachAttributes*	map;

		// Dynamic info
	float			c_condor_load;
	float			c_owner_load;
	time_t			c_idle;
	time_t			c_console_idle;
	unsigned long   c_virt_mem;
	unsigned long   c_disk;
		// total_disk here means total disk space on the partition containing
		// this slot's execute directory.  Since each slot may have an
		// execute directory on a different partition, we have to store this
		// here in the per-slot data structure, rather than in the machine-wide
		// data structure.
	unsigned long   c_total_disk;

	int				c_phys_mem;
	int				c_slot_mem;
	int				c_num_cpus;    
    // custom slot resources
    slotres_map_t c_slotres_map;
    slotres_map_t c_slottot_map;

    // totals
	int			  c_num_slot_cpus;

		// These hold the fractions of shared, dynamic resources
		// that are allocated to this CPU.
	double			c_virt_mem_fraction;

	double			c_disk_fraction; // share of execute dir partition
	double			c_slot_disk; // share of execute dir partition
	MyString        c_execute_dir;
	MyString        c_execute_partition_id;  // unique id for partition

	int				c_type;		// The type of this resource
};	

class AvailDiskPartition
{
 public:
	AvailDiskPartition() {
		m_disk_fraction = 1.0;
		m_auto_count = 0;
	}
	float m_disk_fraction; // share of this partition that is not taken yet
	int m_auto_count; // number of slots using "auto" share of this partition
};

// Available machine-wide attributes
class AvailAttributes
{
public:
    typedef MachAttributes::slotres_map_t slotres_map_t;

	AvailAttributes( MachAttributes* map );

	bool decrement( CpuAttributes* cap );
	bool computeAutoShares( CpuAttributes* cap );
	void show_totals( int dprintf_mask, CpuAttributes *cap );

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
	HashTable<MyString,AvailDiskPartition> m_execute_partitions;

	AvailDiskPartition &GetAvailDiskPartition(MyString const &execute_partition_id);
};

#endif /* _RES_ATTRIBUTES_H */
