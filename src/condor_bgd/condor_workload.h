#ifndef CONDOR_WORKLOAD_H
#define CONDOR_WORKLOAD_H

class MyString;
class ClassAd;
class StringList;

// This class provides an api which represents a workload from a single
// schedd (or other source) delegated to a classad.
class Workload
{
	public:
		Workload();
		~Workload();

		// This class wons the memory of this pointer and deletes it when
		// this class gets deleted
		void attach(ClassAd *ad);

		// If, however, the user deattaches the ad, the ownership goes back to
		// the caller.
		ClassAd* detach(void);

		// What is an ascii representation for the name of this workload?
		void set_name(MyString name);
		MyString get_name(void);

		// What this workload knows about currently running/needed
		void set_smp_idle(int num);
		int get_smp_idle(void);
		void set_smp_running(int num);
		int get_smp_running(void);

		void set_dual_idle(int num);
		int get_dual_idle(void);
		void set_dual_running(int num);
		int get_dual_running(void);

		void set_vn_idle(int num);
		int get_vn_idle(void);
		void set_vn_running(int num);
		int get_vn_running(void);

		// print it out 
		void dump(int flags);

	private:
		bool m_initialized;
		ClassAd *m_data;
};

#endif
