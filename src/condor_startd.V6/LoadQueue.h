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

#ifndef _LOAD_QUEUE_H
#define _LOAD_QUEUE_H

class Resource;

class LoadQueue
{
public:
	LoadQueue( int queue_size );
	~LoadQueue();
	void	push( ssize_t num, float val );
	void	clear();
	float	avg();
	int		size() const { return q_size; };
	void	display( Resource* rip );

private:
	int			head;	// Index of the next available slot.
	float*	 	buf;	// Actual array to hold values.
	int			q_size;	
};


#endif /* _LOAD_QUEUE_H */
