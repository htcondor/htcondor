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
