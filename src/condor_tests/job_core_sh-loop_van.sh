#!/bin/csh
set i = 1
while ($i <= $1)
	echo -n $i " "
	sleep 1
	@ i = $i + 1
end
echo ""
echo "Normal End-of-Job"
