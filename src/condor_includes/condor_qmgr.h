#if !defined(_LIBQMGR_H)
#define _LIBQMGR_H

#include "condor_xdr.h"
#include "proc.h"

#define QMGMT_CMD	1111

typedef struct {
	int		fd;
	XDR		xdr;
	int		count;
	char	*rendevous_file;
} Qmgr_connection;

typedef int (*scan_func)(int, int);


#if defined(__cplusplus)
extern "C" {
#endif

int InitializeConnection(char *, char *);
int NewCluster();
int NewProc( int );
int DestroyProc(int, int);
int DestroyCluster(int);
int DestroyClusterByConstraint(const char*); 
int SetAttribute(int, int, char *, char *);
int CloseConnection();

int GetAttributeFloat(int, int, char *, float *);
int GetAttributeInt(int, int, char *, int *);
int GetAttributeString(int, int, char *, char *);
int GetAttributeExpr(int, int, char *, char *);
int DeleteAttribute(int, int, char *);

int GetNextJob(int, int, int *, int *);
int FirstAttribute(int, int, char *);
int NextAttribute(int, int, char *);

Qmgr_connection *ConnectQ(char *qmgr_location);
int DisconnectQ(Qmgr_connection *);
int WalkJobQueue(scan_func);

float rusage_to_float(struct rusage);
int float_to_rusage(float, struct rusage *);

/* These are here for compatibility with old code which uses the PROC
   structure to ease porting.  Use of these functions is discouraged! */
int SaveProc(PROC *);
int GetProc(int, int, PROC *);

#if defined(__cplusplus)
}
#endif

#define SetAttributeInt(cl, pr, name, val){\
										   char buf[100]; \
										   sprintf(buf, "%d", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeFloat(cl, pr, name, val){\
										   char buf[100]; \
										   sprintf(buf, "%f", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeString(cl, pr, name, val){\
										   char buf[1000]; \
										   sprintf(buf, "\"%s\"", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeExpr(cl, pr, name, val) SetAttribute(cl, pr, name, val);
#endif
