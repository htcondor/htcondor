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
