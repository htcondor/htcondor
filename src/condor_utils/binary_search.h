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



#ifndef _BINARY_SEARCH_H_
#define _BINARY_SEARCH_H_

template <class ObjType>
class BinarySearch
{
  public:
	/** Binary search.  Array must be sorted lowest-to-highest.
		@param array The array to search.
		@param length The length of the array.
		@param key The value to search for.
		@param index Receives the value of index at which the key value
			is found, or else the index at which it should be inserted if it
			is not found.
		@return true if the key is found, false otherwise.
	*/
	static bool Search( const ObjType array[], int length, ObjType key,
				int &index );
};

template <class ObjType>
bool
BinarySearch<ObjType>::Search( const ObjType array[], int length, ObjType key,
			int &index )
{
	int		low = 0;
	int		high = length - 1;

	while ( low <= high ) {
			// Avoid possibility of low + high overflowing.
		int		mid = low + ((high - low) / 2);
		ObjType	midVal = array[mid];

		if ( midVal < key ) {
			low = mid + 1;
		} else if ( midVal > key ) {
			high = mid - 1;
		} else {
				// key found
			index = mid;
			return true;
		}
	}

		// key not found
	index = low;	
	return false;
}

#endif // _BINARY_SEARCH_H_
