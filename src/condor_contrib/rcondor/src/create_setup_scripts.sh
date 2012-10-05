#!/bin/bash
basedir="$1"

cat > $basedir/rcondor_setup.sh << EOF
export PATH="$basedir/bin:$PATH"
EOF

cat > $basedir/rcondor_setup.csh << EOF
setenv PATH "$basedir/bin:$PATH"
EOF

