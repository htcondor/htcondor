/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
