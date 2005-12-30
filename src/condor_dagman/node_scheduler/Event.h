/*
 *	This file is licensed under the terms of the Apache License Version 2.0 
 *	http://www.apache.org/licenses. This notice must appear in modified or not 
 *	redistributions of this file.
 *
 *	Redistributions of this Software, with or without modification, must
 *	reproduce the Apache License in: (1) the Software, or (2) the Documentation
 *	or some other similar material which is provided with the Software (if any).
 *
 *	Copyright 2005 Argonne National Laboratory. All rights reserved.
 */

#ifndef GM_EVENT_H
#define GM_EVENT_H

/*
 *	FILE CONTENT:
 *	Definition of the Event structure used by the discrete time
 *	event simulator class Simulator.
 *
 *	AUTHOR:
 *	Grzegorz Malewicz
 *
 *	HISTORY:
 *	8/9/05	--	coding and testing finished
 */


enum EventType { REQUEST, COMPLETE };

struct Event	// could be a union, but ...
{
	EventType	type;
	int			bulkSize;	// used when type is REQUEST
	int			node;		// used when type is COMPLETE
};

typedef Event *  EventPtr;

#endif
