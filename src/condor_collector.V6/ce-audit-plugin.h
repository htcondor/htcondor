#ifndef   _CE_AUDIT_PLUGIN_H
#define   _CE_AUDIT_PLUGIN_H

#include "CollectorPlugin.h"

class CEAuditPlugin : public CollectorPlugin {
	public:
		         CEAuditPlugin();
		virtual ~CEAuditPlugin();

		virtual void initialize();
		virtual void shutdown();
		virtual void update(int command, const ClassAd& ad);
		virtual void invalidate(int command, const ClassAd& ad);

	private:
		int maxJobSeconds;
		std::map< std::pair< std::string, std::string >,
		          std::pair< time_t, std::map< std::string, std::string > > >
		          runningMasters;

		void stopJob(const ClassAd& ad);
		void startJob(const ClassAd& ad);
};

#endif /* _CE_AUDIT_PLUGIN_H */
