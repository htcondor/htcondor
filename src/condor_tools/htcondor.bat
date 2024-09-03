0<0# : ^
'''
@echo off
python "%~f0" %*
goto :EOF
'''

import sys
from htcondor_cli.cli import main

if __name__ == '__main__':
    sys.exit(main())
