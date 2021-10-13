/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef _HIBERNATER_H_
#define _HIBERNATER_H_

#include <vector>

/***************************************************************
 * Base Hibernator class
 ***************************************************************/

class HibernatorBase
{

public:

	struct StateLookup;

	/* The following are the standard sleep states: doHibernate()
	   makes the machine to enter the given sleep state, while
	   getStates() returns a mask describing the sleep states
	   supported on the current machine, if any (read: NONE)
	*/

	enum SLEEP_STATE {
		NONE = 0,	 /* No sleep states supported */
		S1   = 0x01, /* Sleep */
		S2   = 0x02, /* (not used) */
		S3   = 0x04, /* Suspend to RAM */
		S4   = 0x08, /* Hibernate */
		S5   = 0x10  /* Shutdown (i.e. soft-off) */
	};

	/** @name Instantiation.
		*/
	//@{

	/// Constructor
	HibernatorBase( void ) noexcept;

	/// Destructor
	virtual ~HibernatorBase( void ) noexcept;

	/// Initializer
	virtual bool initialize( void ) = 0;
	bool isInitialized( void ) const { return m_initialized; };

	//@}

	/** @name Configuration.
		*/
	//@{

	/// Specify hibernation method
	virtual bool setMethod( const char * ) { return true; };
	
	/// Update configuration
	virtual bool update( void ){ return true; };

	//@}

	/** @name Power management.
		Basic checks to determined if we want to use the power
		management capabilities of the OS.
		*/
	//@{

	/** Signal the OS to enter hibernation.
		@return true if the machine will enter hibernation; otherwise, false.
		@param State of hibernation to enter.
		@param Actual state entered
		@param Should the computer be forced into the hibernation state?
		*/
	bool switchToState ( SLEEP_STATE state,
						 SLEEP_STATE &new_state,
						 bool force = true ) const;

	//@}

	/** @name State management.
		Basic management call for supported OS hibernation states.
		*/
	//@{

	/** Retrieve the hibernation states supported by the OS.
		@return a mask of the states supported by the OS.
		@see SLEEP_STATE
		*/
	unsigned short getStates( void ) const;

	/** Is the given state supported?
		@return true / false */
	bool isStateSupported ( SLEEP_STATE state ) const;

    /** Set the supported sleep states
        */
    void setStates ( unsigned short states );

    /** Add to the supported sleep states
        @see SLEEP_STATE
        */
    void addState ( SLEEP_STATE state );
	void addState ( const char *state );

	/** Get the name of the hibernation method used */
	virtual const char* getMethod( void ) const;

	//@}

	/** We use this to create hibernation objects so we don't need to
		deal with the differences between OSs at the invocation
		level.
		@return if the OS is supported a valid HibernatorBase*;
        otherwise NULL.
	*/
# if  0
	static HibernatorBase* createHibernator( void );
# endif


	/** @name Conversion functions.
		These can be used to convert between sleep states
		(i.e. SLEEP_STATE) and their string representation
		and vice versa
	*/
	//@{

	static bool isStateValid( SLEEP_STATE state );

	static SLEEP_STATE intToSleepState ( int x );
	static int sleepStateToInt ( SLEEP_STATE state );
	static char const* sleepStateToString ( SLEEP_STATE state );
	static SLEEP_STATE stringToSleepState ( char const *name );

	static bool maskToStates( unsigned mask, std::vector<SLEEP_STATE> &states );
	static bool statesToString( const std::vector<SLEEP_STATE> &states,
								std::string &string );
	static bool maskToString( unsigned mask, std::string &str );

	static bool stringToStates( const char *s,
								std::vector<SLEEP_STATE> &states );
	static bool statesToMask( const std::vector<SLEEP_STATE> &states,
							  unsigned &mask );
	static bool stringToMask( const char *str,
							  unsigned &mask );

	//@}

protected:

	/* Override this to enter the given sleep state on a
	   particular OS */
	virtual SLEEP_STATE enterStateStandBy ( bool force ) const = 0;
	virtual SLEEP_STATE enterStateSuspend ( bool force ) const = 0;
	virtual SLEEP_STATE enterStateHibernate ( bool force ) const = 0;
	virtual SLEEP_STATE enterStatePowerOff ( bool force ) const = 0;

	static const HibernatorBase::StateLookup& Lookup ( int n );
	static const HibernatorBase::StateLookup& Lookup ( SLEEP_STATE state );
	static const HibernatorBase::StateLookup& Lookup ( const char *name );

	void setStateMask( unsigned _states ) { m_states = _states; };
	void setInitialized( bool i ) { m_initialized = i; };

private:

	/* OS agnostic sleep state representation */
	unsigned short  m_states;
	bool			m_initialized;
};

#endif // _HIBERNATER_H_
