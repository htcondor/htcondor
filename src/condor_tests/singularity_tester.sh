#!/bin/bash

# ignore leading - options, except for --version
while [[ "$1" =~ "-".* ]]
do
	if [ "$1" == "--version" ]
	then
		echo "3.0.0"
		exit 0
	fi

	shift
done

#  Have test command always return true
if [ "$1" = "test" ]
then
	echo 0
	exit 0
fi

# Just run the command without a container
if [ "$1" = "exec" ]
then
	# Shift off the exec
	shift

	while [[ "$1" =~ "-".* ]]
	do
		case $1 in
		 -S)
			 # scratch dir has an argument
			 shift
			 shift
			 ;;
		 -B)
			 # scratch dir has an argument
			 shift
			 shift
			 ;;
		 --pwd)
			 shift
			 shift
			 ;;
		 --no-home)
			 shift
			 ;;
		 -C)
			 shift
			 ;;
		 -*)
			 echo "Unknown argument at $@"
			 exit 1
			 break
			 ;;
		esac
	done

	# Shift off the container name
	shift

	# special case for test job
	if [ "$1" = "/exit_37" ]
	then
		exit 37
	fi

	# And run the command
	eval "$@"
	exit $?
fi

echo "Unknown/unsupported singularity command: $@"
exit 0
