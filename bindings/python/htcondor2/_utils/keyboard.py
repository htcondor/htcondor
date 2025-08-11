#!/usr/bin/python3

# Copyright 2025 HTCondor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
from typing import Optional

IS_WINDOWS = (os.name == "nt")

if IS_WINDOWS:
    import msvcrt
else:
    import termios
    import atexit
    from select import select

class KBHit():
    def __init__(self, listen: bool = False) -> None:
        """Initialize terminal listening for non-blocking keyboard input (noop for windows)"""
        self.is_listening = IS_WINDOWS
        if not IS_WINDOWS:
            # Save the terminal settings
            self.fd = sys.stdin.fileno()
            self.new_term = termios.tcgetattr(self.fd)
            self.old_term = termios.tcgetattr(self.fd)

            # New terminal setting unbuffered
            self.new_term[3] = (self.new_term[3] & ~termios.ICANON & ~termios.ECHO)

            if listen:
                self.listen_term()

            # Support normal-terminal reset at exit
            atexit.register(self.reset_term)

    def __enter__(self):
        """Enter into the listenting terminal setup"""
        self.listen_term()
        return self

    def __exit__(self, exec_type, exec_value, exec_traceback):
        """Exit back into origianl terminal setup"""
        self.reset_term()

    def listening(self) -> bool:
        """Inform program that object is listening for key presses"""
        return self.is_listening

    def reset_term(self) -> None:
        """Reset to original terminal settings"""
        if not IS_WINDOWS and self.is_listening:
            termios.tcsetattr(self.fd, termios.TCSAFLUSH, self.old_term)
            self.is_listening = False

    def listen_term(self) -> None:
        """Switch into listenting terminal setup"""
        if not IS_WINDOWS and not self.is_listening:
            termios.tcsetattr(self.fd, termios.TCSAFLUSH, self.new_term)
            self.is_listening = True

    def getch(self) -> Optional[str]:
        """Read character from standard input if key has been pressed"""
        if self.__kbhit():
            if IS_WINDOWS:
                return msvcrt.getch().decode('utf-8')
            elif self.is_listening:
                return sys.stdin.read(1)

        return None

    def anyKeyPressed(self) -> bool:
        """Check if 'any' key is pressed"""
        hit = self.__kbhit() == True
        if hit and IS_WINDOWS:
            consume = msvcrt.getch()
        return hit

    def __kbhit(self) -> Optional[bool]:
        """Check if key has been pressed"""
        if IS_WINDOWS:
            return msvcrt.kbhit()
        elif self.is_listening:
            dr,dw,de = select([sys.stdin], [], [], 0)
            return dr != []
        else:
            return None
