/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
//
// 	These are the condor system parameters. Their values are dependent on 
//  the platform on which condor is running
//
enum SysParamName {
			Arch            = 0,		// system architecture
			OpSys              ,        // operating system
			LoadAvg			   ,        // load average
			SwapSpace          ,		// swap space
			PhyMem             ,		// physical memory
			CpuType            ,		// cpu type
				};
enum SysType
{
			Integer				,       // Integer
			Float				,       // float , real
			Char				,       // character
			String				,       // strings
			Long				,       // long
			Other						// any other type
};

// the base virtual platform
class BaseOs
{
public:
	virtual	int	readSysParam(const SysParamName,char*& buffer,int& size,
														SysType& type){};
	virtual void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type){};
};

class SunOs		: public BaseOs
{
public:
	SunOs();                      // do required initialization
private:
	int  	readSysParam(const SysParamName,char*& buffer,int& size,
													SysType& type) ;
	void	disSysParam(const SysParamName sys, const char*buffer, 
								const int size, const SysType type);
} ;

/* Solaris specific change ..dhaval 6/27 */

class solaris		: public BaseOs
{
public:
	solaris();                      // do required initialization
private:
	int  	readSysParam(const SysParamName,char*& buffer,int& size,
													SysType& type) ;
	void	disSysParam(const SysParamName sys, const char*buffer, 
								const int size, const SysType type);
} ;

class Osf1	: public BaseOs 
{
public:
	Osf1();                      // do required initialisation
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
														SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type);
};


class Ultrix	: public BaseOs 
{
public:
	Ultrix();                      // do required initialisation
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
														SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type);
};

class Aix	: public BaseOs 
{
public:
	Aix();                      // do required initialisation
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
														SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type);
};

class Hpux	: public BaseOs 
{
public:
	Hpux();                      // do required initialisation
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
														SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type);
};

class Linux	: public BaseOs 
{
public:
	Linux();                      // do required initialisation
	float get_load_avg();
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
				SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
				const int size, const SysType type);
};


class Irix	: public BaseOs 
{
public:
	Irix();                      // do required initialisation
private:
	int  	readSysParam(const SysParamName, char*& buffer, int& size, 
														SysType& type);
	void	disSysParam(const SysParamName sys, const char*buffer, 
									const int size, const SysType type);
};

extern  BaseOs*     platform ;
