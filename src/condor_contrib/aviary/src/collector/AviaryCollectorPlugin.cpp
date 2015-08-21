/*
 * Copyright 2009-2012 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include <CollectorPlugin.h>
#include <collector.h>
#include <condor_daemon_core.h>

// local includes
#include "AviaryUtils.h"
#include "AviaryProvider.h"
#include "AviaryCollectorServiceSkeleton.h"
#include "CollectorObject.h"

using namespace std;
using namespace aviary::util;
using namespace aviary::transport;
using namespace aviary::collector;
using namespace aviary::locator;

AviaryProvider* provider = NULL;
CollectorObject* collector = NULL;

struct AviaryCollectorPlugin : public Service, CollectorPlugin
{
    void
    initialize()
    {
        char *tmp;
        string collName;
        
        dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Initializing...\n");
        
        tmp = param("COLLECTOR_NAME");
        if (NULL == tmp) {
            collName = getPoolName();
        } else {
            collName = tmp;
            free(tmp); tmp = NULL;
        }
        
        string log_name("aviary_collector.log");
        string id_name("collector"); id_name+=SEPARATOR; id_name+=getPoolName();
        provider = AviaryProviderFactory::create(log_name, id_name,"COLLECTOR","POOL","services/collector/");
        if (!provider) {
            EXCEPT("Unable to configure AviaryProvider. Exiting...");
        }
        
        collector = CollectorObject::getInstance();
        
        ReliSock *sock = new ReliSock;
        if (!sock) {
            EXCEPT("Failed to allocate transport socket");
        }
        
        if (!sock->assign(provider->getListenerSocket())) {
            EXCEPT("Failed to bind transport socket");
        }
        int index;
        if (-1 == (index =
            daemonCore->Register_Socket((Stream *) sock,
                                        "Aviary Method Socket",
                                        (SocketHandlercpp) ( &AviaryCollectorPlugin::handleTransportSocket ),
                                        "Handler for Aviary Methods.", this))) {
            EXCEPT("Failed to register transport socket");
                                        }
        collector->setMyAddress(daemonCore->publicNetworkIpAddr());
    }

    void
    shutdown()
    {
        dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: shutting down...\n");
        // TODO: implement invalidateAll
        //collector->invalidateAll();
    }
    
    void
    update(int command, const ClassAd &ad)
    {
        string public_addr;
        string cmd_str(getCollectorCommandString(command));
        string param_ignore_str("AVIARY_IGNORE_");
        param_ignore_str.append(cmd_str);
        switch (command) {
            case UPDATE_COLLECTOR_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_COLLECTOR_AD\n");
                // We could receive collector ads from many
                // collectors, but we only maintain our own. So,
                // ignore all others.
                if (ad.LookupString(ATTR_MY_ADDRESS, public_addr)) {                    
                    if (collector->getMyAddress() == public_addr) {
                        if(!collector->update(command,ad)) {
                            dprintf(D_ALWAYS,"AviaryCollectorPlugin: Error on UPDATE_COLLECTOR_AD\n");
                        }
                    }
                }
                else {
                    dprintf(D_ALWAYS,"AviaryCollectorPlugin: Unable to get attribute '%s' from collector ad\n",ATTR_MY_ADDRESS);
                }
                break;
            case UPDATE_MASTER_AD:
            case UPDATE_NEGOTIATOR_AD:
            case UPDATE_SCHEDD_AD:
            case UPDATE_STARTD_AD:
            case UPDATE_SUBMITTOR_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received %s\n",cmd_str.c_str());
                if (param_boolean(param_ignore_str.c_str(), false)) {
                    dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Configured to ignore %s\n",cmd_str.c_str());
                    break;
                }
                if(!collector->update(command,ad)) {
                    dprintf(D_ALWAYS,"AviaryCollectorPlugin: Error on %s\n",cmd_str.c_str());
                }
                break;
            case UPDATE_GRID_AD: // TODO: ignore Grid ads?
            default:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Unsupported command: %s\n",cmd_str.c_str());
        }
    }
    
    void
    invalidate(int command, const ClassAd &ad)
    {        
        string cmd_str = getCollectorCommandString(command);
        switch (command) {
            case INVALIDATE_COLLECTOR_ADS:
            case INVALIDATE_MASTER_ADS:
            case INVALIDATE_NEGOTIATOR_ADS:
            case INVALIDATE_SCHEDD_ADS:
            case INVALIDATE_STARTD_ADS:
            case INVALIDATE_SUBMITTOR_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received %s\n",cmd_str.c_str());
                if(!collector->invalidate(command,ad)) {
                    dprintf(D_ALWAYS,"AviaryCollectorPlugin: Error on %s\n",cmd_str.c_str());
                }
                break;
            case INVALIDATE_GRID_ADS:
            default:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Unsupported command: %s\n",cmd_str.c_str());
        }
    }
    
    int
    handleTransportSocket(Stream *)
    {
        string provider_error;
        if (!provider->processRequest(provider_error)) {
            dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
        }
        return KEEP_STREAM;
    }
};

static AviaryCollectorPlugin instance;
