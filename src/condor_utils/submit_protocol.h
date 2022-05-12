#include "condor_qmgr.h"
#include "submit_utils.h"

// Abstract the schedd's queue protocol so we can NOT call it when simulating
//
class AbstractScheddQ {
public:
	virtual ~AbstractScheddQ() {}
	virtual int get_NewCluster() = 0;
	virtual int get_NewProc(int cluster_id) = 0;
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL) = 0;
	virtual int get_Capabilities(ClassAd& reply) = 0;
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 ) = 0;
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 ) = 0;
	virtual int send_SpoolFile(char const *filename) = 0;
	virtual int send_SpoolFileBytes(char const *filename) = 0;
	virtual bool disconnect(bool commit_transaction, CondorError & errstack) = 0;
	virtual int  get_type() = 0;
	virtual bool has_late_materialize(int &ver) = 0;
	virtual bool allows_late_materialize() = 0;
	virtual bool has_extended_submit_commands(ClassAd &cmds) = 0;
	virtual bool has_send_jobset(int &ver) = 0;
	virtual int set_Factory(int cluster, int qnum, const char * filename, const char * text) = 0;
	virtual int send_Itemdata(int cluster, SubmitForeachArgs & o) = 0;
	virtual int send_Jobset(int cluster, const ClassAd * jobset_ad) = 0;

	// helper function used as 3rd argument to SendMaterializeData.
	// it treats pv as a pointer to SubmitForeachArgs, calls next() on it and then formats the
	// resulting rowdata for SendMaterializeData to use 
	static int next_rowdata(void* pv /*SubmitForeachArgs*/, std::string & rowdata);

protected:
	AbstractScheddQ() {}
};

enum {
	AbstractQ_TYPE_NONE = 0,
	AbstractQ_TYPE_SCHEDD_RPC = 1,
	AbstractQ_TYPE_SIM,	 // used by submit -dry-run. see condor_submit.V6 directory
	AbstractQ_TYPE_FILE,
};

class ActualScheddQ : public AbstractScheddQ {
public:
	ActualScheddQ() : qmgr(NULL)
		, tried_to_get_capabilities(false), has_late(false), allows_late(false), late_ver(0)
		, has_jobsets(false), use_jobsets(false), jobsets_ver(0)
	{}
	virtual ~ActualScheddQ();
	virtual int get_NewCluster();
	virtual int get_NewProc(int cluster_id);
	virtual int destroy_Cluster(int cluster_id, const char *reason = NULL);
	virtual int get_Capabilities(ClassAd& reply);
	virtual int set_Attribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 );
	virtual int set_AttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 );
	virtual int send_SpoolFile(char const *filename);
	virtual int send_SpoolFileBytes(char const *filename);
	virtual bool disconnect(bool commit_transaction, CondorError & errstack);
	virtual int  get_type() { return AbstractQ_TYPE_SCHEDD_RPC; }
	virtual bool has_late_materialize(int &ver); // version check for late materialize
	virtual bool allows_late_materialize(); // capabilities check ffor late materialize enabled.
	virtual bool has_extended_submit_commands(ClassAd &cmds); // capabilities check for extended submit commands
	virtual bool has_send_jobset(int &ver);
	virtual int set_Factory(int cluster, int qnum, const char * filename, const char * text);
	virtual int send_Itemdata(int cluster, SubmitForeachArgs & o);
	virtual int send_Jobset(int cluster, const ClassAd * jobset_ad);

	bool Connect(DCSchedd & MySchedd, CondorError & errstack);
private:
	struct _Qmgr_connection * qmgr;
	ClassAd capabilities;
	bool tried_to_get_capabilities;
	bool has_late; // set in Connect based on the version in DCSchedd
	bool allows_late;
	char late_ver;
	bool has_jobsets;
	bool use_jobsets;
	char jobsets_ver;
	int init_capabilities();
};