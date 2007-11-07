/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
