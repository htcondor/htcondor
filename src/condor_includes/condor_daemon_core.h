////////////////////////////////////////////////////////////////////////////////
//
// This file contains the definition for class DaemonCore. This is the
// central structure for every daemon in condor. The daemon core triggers
// preregistered handlers for corresponding events. class Service is the base
// class of the classes that daemon_core can serve. In order to use a class
// with the DaemonCore, it has to be a derived class of Service.
//
// Cai, Weiru
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _CONDOR_DAEMON_CORE_H_
#define _CONDOR_DAEMON_CORE_H_

#if defined(Solaris)
#define __EXTENSIONS__
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#if defined(Solaris)
#undef __EXTENSIONS__
#endif
#include "condor_timer_manager.h"

const	int	 MAXDC	= 5; 

// Users pass this enum type to Register functions to identify what type of
// functions to register.
enum{SIGNAL, REQUEST, TIMER};

// these are for internal use.
typedef	int	  	(*SReqHandler)(Service*, XDR*, struct sockaddr_in*); 
typedef	int	  	(*ReqHandler)(XDR*, struct sockaddr_in*); 
typedef	void	(*SSigHandler)(Service*, int, int, void*);
typedef	void	(*SigHandler)(int, int, void*);
typedef void	(*SFunction)(Service*);
typedef void	(*Function)();

class DaemonCore
{
	public:
		
		DaemonCore(int, int, int);
		~DaemonCore();

		void				Register(Service*, int, void*, int);
		int					Register(Service*, time_t, void*, time_t, int);
		void				Register(Service*, void*);
		void				Delete(int);
		
		int					OpenTcp(char*, u_short = 0);
		int					OpenUdp(char*, u_short = 0);
		
		void				Dump(int, char*);

		void				Driver();

	private:

		struct ReqHandlerEnt
		{
		    int				no;
		    void*			handler;
			Service*		service; 
		};
		struct SigHandlerEnt 
		{
		    int				no;
		    void*			handler;
			Service*		service; 
		};
		struct FunctionEnt
		{
		    void*			func;
			Service*		service; 
		};

		void				DumpSigHandlerTable(int, char* = "");
		void				DumpReqHandlerTable(int, char* = "");
		void				DumpTimerList(int, char* = "");
		void				HandleReq(int, char);
		friend	void		HandleSig(int, int, void*); 
		int					SigHandled(int, int, void*);
		void				Register(Service*, int, SigHandler);
		void				Register(Service*, int, ReqHandler);
		
		int					maxSig;			// max number of signal handlers
		int					nSig;			// number of signal handlers
		SigHandlerEnt*		sigTable;		// signal handler table

		int					maxReq;			// max number of request handlers
		int					nReq;			// number of request handlers
		ReqHandlerEnt*		reqTable;		// request handler table

		int					maxFunc;		// to be executed between select
		int					nFunc;			// to be executed between select
		FunctionEnt*		funcTable;		// to be executed between select

		u_short				ports[32];
		int					nPorts; 
		fd_set				readfds; 
		int					tcpSds[32];		// tcp socket descriptors
		int					nTcp;
		int					udpSds[32];		// udp socket descriptors
		int					nUdp;
};

#endif









