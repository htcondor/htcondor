script pre ALL_NODES /dev/null A $JOB $RETRY
pre_skip ALL_NODES 4
script post ALL_NODES /dev/null A $JOB $RETURN
vars ALL_NODES nodename="$(JOB)"
retry ALL_NODES 2 UNLESS-EXIT 5
abort-dag-on ALL_NODES 3 return 4
priority ALL_NODES 10
category ALL_NODES upper
maxjobs upper 1
