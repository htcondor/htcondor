#include <iostream>
#include <string>
#include <list>

#include <qpid/console/ConsoleListener.h>
#include <qpid/console/SessionManager.h>

using namespace std;
using namespace qpid::console;

class SimpleConsole : public ConsoleListener {
   private:
      SessionManager* sessionManager;
      Broker* broker;

   public:
      SimpleConsole();
      ~SimpleConsole();

      list<string> findAbsentNodes();
      void init(string host,
				int port,
				string user,
				string passwd);
};

SimpleConsole::SimpleConsole():
	sessionManager(NULL),
	broker(NULL)
{ }

SimpleConsole::~SimpleConsole()
{
   if (broker) {
	   cout << "Deleting broker object" << endl;
	   sessionManager->delBroker(broker); broker = NULL;
	   cout << "Deleted broker object" << endl;
   }

   if (sessionManager) {
	   cout << "Deleting session manager object" << endl;
	   delete sessionManager; sessionManager = NULL;
	   cout << "Deleted session manager object" << endl;
   }
}

void
SimpleConsole::init(string host, int port, string user, string passwd)
{
	qpid::client::ConnectionSettings connectionSettings;
	SessionManager::Settings sessionManagerSettings;

	sessionManagerSettings.rcvObjects = false;
	sessionManagerSettings.rcvEvents = false;
	sessionManagerSettings.rcvHeartbeats = false;
	sessionManagerSettings.userBindings = false;

	connectionSettings.host = host;
	connectionSettings.port = port;
	connectionSettings.username = user;
	connectionSettings.password = passwd;

	sessionManager = new SessionManager(this, sessionManagerSettings);
	assert(sessionManager);

	broker = sessionManager->addBroker(connectionSettings);
	assert(broker);
}

list<string>
SimpleConsole::findAbsentNodes()
{
	list<string> missing;
	Object::Vector objs;

	try {
		sessionManager->getObjects(objs, "Node");
	} catch(...) {
		cout << "Failed to get roster of Nodes" << endl;
		return missing;
	}

	cout << "Roster contains " << objs.size() << " nodes" << endl;

	for (Object::Vector::const_iterator obj = objs.begin();
		 objs.end() != obj;
		 obj++) {
		cout << " . " << obj->attrString("name") << endl;
		missing.push_back(obj->attrString("name"));
	}

	try {
		sessionManager->getObjects(objs, "master");
	} catch(...) {
		cout << "Failed to get list of present Nodes" << endl;
		return missing;
	}

	cout << objs.size() << " nodes present" << endl;

	for (Object::Vector::const_iterator obj = objs.begin();
		 objs.end() != obj;
		 obj++) {
		missing.remove(obj->attrString("Name"));
	}

	cout << "Found " << missing.size() << " missing nodes" << endl;

	return missing;
}


int
main(int argc, char **argv)
{
	SimpleConsole console;
	console.init(argv[1], 5672, "guest", "guest");
	sleep(10);

	list<string> missing = console.findAbsentNodes();
	for (list<string>::const_iterator i = missing.begin();
		 missing.end() != i;
		 i++) {
		cout << "Missing: " << (*i) << endl;
	}

	return 0;
}
