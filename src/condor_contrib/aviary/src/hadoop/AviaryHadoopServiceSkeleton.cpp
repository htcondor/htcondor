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
#include "condor_attributes.h"

#include <time.h>

// local includes
#include "AviaryHadoopServiceSkeleton.h"
#include <AviaryHadoop_StartTaskTracker.h>
#include <AviaryHadoop_StartTaskTrackerResponse.h>
#include <AviaryHadoop_StartDataNode.h>
#include <AviaryHadoop_StartDataNodeResponse.h>
#include <AviaryHadoop_GetTaskTracker.h>
#include <AviaryHadoop_GetTaskTrackerResponse.h>
#include <AviaryHadoop_StopJobTracker.h>
#include <AviaryHadoop_StopJobTrackerResponse.h>
#include <AviaryHadoop_GetJobTracker.h>
#include <AviaryHadoop_GetJobTrackerResponse.h>
#include <AviaryHadoop_StopTaskTracker.h>
#include <AviaryHadoop_StopTaskTrackerResponse.h>
#include <AviaryHadoop_StartNameNode.h>
#include <AviaryHadoop_StartNameNodeResponse.h>
#include <AviaryHadoop_GetDataNode.h>
#include <AviaryHadoop_GetDataNodeResponse.h>
#include <AviaryHadoop_StopNameNode.h>
#include <AviaryHadoop_StopNameNodeResponse.h>
#include <AviaryHadoop_GetNameNode.h>
#include <AviaryHadoop_GetNameNodeResponse.h>
#include <AviaryHadoop_StartJobTracker.h>
#include <AviaryHadoop_StartJobTrackerResponse.h>
#include <AviaryHadoop_StopDataNode.h>
#include <AviaryHadoop_StopDataNodeResponse.h>
#include "HadoopObject.h"

using namespace std;
using namespace wso2wsf;
using namespace AviaryHadoop;
using namespace AviaryCommon;
using namespace aviary::codec;
using namespace aviary::hadoop;
using namespace compat_classad;

extern bool qmgmt_all_users_trusted;

AviaryCommon::Status * setFailResponse()
{
    HadoopObject * ho = HadoopObject::getInstance();
    string szErr;
    
    ho->getLastError(szErr);
    return (new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),szErr));
}

AviaryCommon::Status * setOKResponse()
{
    string szNoErr;
    return (new AviaryCommon::Status(new AviaryCommon::StatusCodeType("OK"),szNoErr));
}

HadoopID * setHadoopID(const tHadoopRef & ref)
{
    HadoopID * pID = new HadoopID;
    
    pID->setId(ref.id);
    pID->setIpc(ref.ipcid);
    pID->setHttp(ref.http);
    
    return pID;
}


HadoopStartResponse* start (tHadoopInit & hInit, HadoopStart* pHS)
{
    HadoopObject * ho = HadoopObject::getInstance();
    HadoopStartResponse* hresp = new HadoopStartResponse;
    
    if( pHS->getRef()->isIdNil() && pHS->isBin_fileNil() )
    {
        hInit.unmanaged = true;
        hInit.idref.id ="0.0";
        hInit.count = 1;
    }
    else
    {
        hInit.idref.id = pHS->getRef()->getId();
        hInit.idref.tarball = pHS->getBin_file();
        hInit.unmanaged = false;
        
        // if ClusterId comes in without ProcId set it to zero
        if ( hInit.idref.id.length() && !strstr( hInit.idref.id.c_str(), ".") )
        {
           hInit.idref.id +=".0";
        }
        
        hInit.count = pHS->getCount();
    }

    hInit.idref.ipcid = pHS->getRef()->getIpc();
    hInit.idref.http = pHS->getRef()->getHttp();
    hInit.owner = pHS->getOwner();
    hInit.description = pHS->getDescription();

    qmgmt_all_users_trusted = true;

    if ( !ho->start( hInit ) )
    {
    hresp->setStatus( setFailResponse() );
    }
    else
    {
    hInit.idref.id = hInit.newcluster;
    
    hresp->setRef(setHadoopID(hInit.idref));
    hresp->setStatus( setOKResponse());
    }
    qmgmt_all_users_trusted = false;
    
    return hresp;
}


HadoopStopResponse* stop ( const vector<HadoopID *> * refs )
{
    HadoopObject * ho = HadoopObject::getInstance();
    HadoopStopResponse* hresp = new HadoopStopResponse;
    bool bOK = false;
    unsigned int iSize=(refs)?refs->size():0;
    
    for (unsigned int iCtr=0; iCtr<iSize; iCtr++)
    {
    tHadoopRef refId;
    HadoopStopResult * hResult = new HadoopStopResult;
    
    refId.id = (*refs)[iCtr]->getId();
    refId.ipcid = (*refs)[iCtr]->getIpc(); 
    
    // so we create a new one to store the return results
    hResult->setRef(setHadoopID(refId));
    
    if ( !ho->stop( refId ) )
    {
        hResult->setStatus( setFailResponse() ) ;    
    }
    else
    {
        bOK = true;
        hResult->setStatus(setOKResponse());
    }
    
    hresp->addResults(hResult);
    }
    
    // check complete status.
    if (bOK)
    {
        hresp->setStatus(setOKResponse());
    }
    else
    {
        string szErr = "One or more stop operations failed, check results";
        hresp->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),szErr));
    }
    
    return hresp;
}

HadoopQueryResponse* query (tHadoopType qType, vector<HadoopID*>* refs)
{
    HadoopObject * ho = HadoopObject::getInstance();
    HadoopQueryResponse* hresp = new HadoopQueryResponse;
    bool bOK = true;
    unsigned int iSize=(refs)?refs->size():0;
    unsigned int iCtr=0;

    do
    {
    std::vector<tHadoopJobStatus> hStatus;
    tHadoopRef refId;
    
    refId.type = qType;
    if (refs)
    {   
        refId.id = (*refs)[iCtr]->getId();
        refId.ipcid = (*refs)[iCtr]->getIpc();
        refId.http = (*refs)[iCtr]->getHttp();
    }
    
    if ( !ho->query( refId, hStatus ) )
    {
        HadoopQueryResult * hResult = new HadoopQueryResult;
        bOK = false; 
        hResult->setRef(setHadoopID(refId));
        hResult->setStatus( setFailResponse() ) ;  
        hresp->addResults(hResult);
    }
    else
    {
        // loop through the return results. for the query
        for (unsigned int jCtr=0; jCtr<hStatus.size(); jCtr++)
        {
            HadoopQueryResult * hResult = new HadoopQueryResult;
        
            hResult->setRef(setHadoopID(hStatus[jCtr].idref));
            hResult->setParent(setHadoopID(hStatus[jCtr].idparent));
            hResult->setBin_file(hStatus[jCtr].idref.tarball);
            hResult->setOwner(hStatus[jCtr].owner);
            hResult->setDescription(hStatus[jCtr].description);
            hResult->setSubmitted(hStatus[jCtr].qdate);
            hResult->setUptime(hStatus[jCtr].uptime);
            hResult->setState(new HadoopStateType(hStatus[jCtr].state));
            hResult->setStatus( setOKResponse() );
            
            hresp->addResults(hResult);
        }
    }
    
    iCtr++;
        
    } while (iCtr<iSize);
    
    // check complete status.
    if (bOK)
    {
        hresp->setStatus(setOKResponse());
    }
    else
    {
        string szErr = "One or more query operations failed, check results";
        hresp->setStatus(new AviaryCommon::Status(new AviaryCommon::StatusCodeType("FAIL"),szErr));
    }

    return hresp;
}

StartNameNodeResponse* AviaryHadoopServiceSkeleton::startNameNode(MessageContext* /*outCtx*/ , StartNameNode* _startNameNode)
{
    
    StartNameNodeResponse* response = new StartNameNodeResponse;
    HadoopStartResponse* hresp = new HadoopStartResponse;
    HadoopObject * ho = HadoopObject::getInstance();
    tHadoopInit hInit;
    hInit.unmanaged = false;
    
    // we have an unmanaged element
    if (!_startNameNode->getStartNameNode()->isUnmanagedNil()) 
    {
        hInit.unmanaged = true;
        hInit.idref.http = _startNameNode->getStartNameNode()->getUnmanaged()->getHttp();
        hInit.idref.ipcid = _startNameNode->getStartNameNode()->getUnmanaged()->getIpc();
    }
    else
    {
        hInit.idref.tarball = _startNameNode->getStartNameNode()->getBin_file();
    }
    
    // setup the initialization struct
    hInit.idref.type = NAME_NODE;
    hInit.count = 1;
    hInit.owner = _startNameNode->getStartNameNode()->getOwner();
    hInit.description = _startNameNode->getStartNameNode()->getDescription();
    

    qmgmt_all_users_trusted = true;
    
    if ( !ho->start( hInit ) )
    {
        hresp->setStatus( setFailResponse() );
    }
    else
    {
        hInit.idref.id = hInit.newcluster;
        hresp->setRef(setHadoopID(hInit.idref));
        hresp->setStatus( setOKResponse());
    }
    
    qmgmt_all_users_trusted = false;
    
    response->setStartNameNodeResponse(hresp);
    return response;
}

StopNameNodeResponse* AviaryHadoopServiceSkeleton::stopNameNode(MessageContext* /*outCtx*/ , StopNameNode* _stopNameNode)
{
    
    StopNameNodeResponse* response = new StopNameNodeResponse;
    HadoopStopResponse* hresp;
    
    hresp = stop (_stopNameNode->getStopNameNode()->getRefs() );
    
    response->setStopNameNodeResponse(hresp);
    return response;
}

GetNameNodeResponse* AviaryHadoopServiceSkeleton::getNameNode(MessageContext* /*outCtx*/ , GetNameNode* _getNameNode)
{
    
    GetNameNodeResponse* response = new GetNameNodeResponse;
    HadoopQueryResponse* hresp; 
    
    hresp = query(NAME_NODE, _getNameNode->getGetNameNode()->getRefs());
    
    response->setGetNameNodeResponse(hresp);
    return response;
}

StartDataNodeResponse* AviaryHadoopServiceSkeleton::startDataNode(MessageContext* /*outCtx*/ , StartDataNode* _startDataNode)
{
    StartDataNodeResponse* response = new StartDataNodeResponse;
    HadoopStartResponse* hresp;
    
    tHadoopInit hInit;
    hInit.idref.type = DATA_NODE;
    
    hresp= start(hInit,_startDataNode->getStartDataNode());
    
    response->setStartDataNodeResponse(hresp);
    return response;
}

StopDataNodeResponse* AviaryHadoopServiceSkeleton::stopDataNode(MessageContext* /*outCtx*/ , StopDataNode* _stopDataNode)
{
    StopDataNodeResponse* response = new StopDataNodeResponse;
    HadoopStopResponse* hresp;
   
    hresp = stop ( _stopDataNode->getStopDataNode()->getRefs() );
    
    response->setStopDataNodeResponse(hresp);
    return response;
}

GetDataNodeResponse* AviaryHadoopServiceSkeleton::getDataNode(MessageContext* /*outCtx*/ , GetDataNode* _getDataNode)
{
    GetDataNodeResponse* response = new GetDataNodeResponse;
    HadoopQueryResponse* hresp;
    
    hresp = query(DATA_NODE, _getDataNode->getGetDataNode()->getRefs());
    
    response->setGetDataNodeResponse(hresp);
    return response;
}

StartJobTrackerResponse* AviaryHadoopServiceSkeleton::startJobTracker(MessageContext* /*outCtx*/ , StartJobTracker* _startJobTracker)
{
    StartJobTrackerResponse* response = new StartJobTrackerResponse;
    HadoopStartResponse* hresp ;
    tHadoopInit hInit;
    
    hInit.idref.type = JOB_TRACKER;
    hresp = start( hInit, _startJobTracker->getStartJobTracker() );
    
    response->setStartJobTrackerResponse(hresp);
    return response;
}

StopJobTrackerResponse* AviaryHadoopServiceSkeleton::stopJobTracker(MessageContext* /*outCtx*/ , StopJobTracker* _stopJobTracker)
{
    StopJobTrackerResponse* response = new StopJobTrackerResponse;
    HadoopStopResponse* hresp;
 
    hresp =  stop (  _stopJobTracker->getStopJobTracker()->getRefs() );
    
    response->setStopJobTrackerResponse(hresp);
    return response;
}

GetJobTrackerResponse* AviaryHadoopServiceSkeleton::getJobTracker(MessageContext* /*outCtx*/ , GetJobTracker* _getJobTracker)
{
    GetJobTrackerResponse* response = new GetJobTrackerResponse;
    HadoopQueryResponse* hresp;
    
    hresp = query( JOB_TRACKER, _getJobTracker->getGetJobTracker()->getRefs() );
    
    response->setGetJobTrackerResponse(hresp);
    return response;
}

StartTaskTrackerResponse* AviaryHadoopServiceSkeleton::startTaskTracker(MessageContext* /*outCtx*/ , StartTaskTracker* _startTaskTracker)
{
    
    StartTaskTrackerResponse* response = new StartTaskTrackerResponse;
    HadoopStartResponse* hresp;
    
    tHadoopInit hInit;
    hInit.idref.type = TASK_TRACKER;
    
    hresp = start(hInit,_startTaskTracker->getStartTaskTracker());
    
    response->setStartTaskTrackerResponse(hresp);
    return response;
}

StopTaskTrackerResponse* AviaryHadoopServiceSkeleton::stopTaskTracker(MessageContext* /*outCtx*/ , StopTaskTracker* _stopTaskTracker)
{
    StopTaskTrackerResponse* response = new StopTaskTrackerResponse;
    HadoopStopResponse* hresp;
   
    hresp = stop (  _stopTaskTracker->getStopTaskTracker()->getRefs() );
    
    response->setStopTaskTrackerResponse(hresp);
    return response;
}

GetTaskTrackerResponse* AviaryHadoopServiceSkeleton::getTaskTracker(MessageContext* /*outCtx*/ , GetTaskTracker* _getTaskTracker)
{
    GetTaskTrackerResponse* response = new GetTaskTrackerResponse;
    HadoopQueryResponse* hresp;
    
    hresp = query( TASK_TRACKER, _getTaskTracker->getGetTaskTracker()->getRefs() );
    
    response->setGetTaskTrackerResponse(hresp);
    return response;
}
