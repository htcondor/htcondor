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

template <class T>
T MIN( T* array, int size ) {
	int		i;
	T		 answer;

	answer = array[0];
	for( i=1; i<size; i++ ) {
		if( array[i] < answer ) {
			answer = array[i];
		}
	}
	return answer;
}

template <class T>
T MAX( T* array, int size ) {
	int		i;
	T		 answer;

	answer = array[0];
	for( i=1; i<size; i++ ) {
		if( array[i] > answer ) {
			answer = array[i];
		}
	}
	return answer;
}

template <class T>
T AVG( T* array, int size ) {
	int		i;
	T		 answer;

	answer = 0.0;
	for( i=0; i<size; i++ ) {
		answer += array[i];
	}
	return answer / size;
}
