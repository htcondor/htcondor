################################################################
#
# Copyright (C) 1990-2025, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You may
# obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################

# Librarian Source Files
set(LibrarianSources
  librarian_main.cpp
  librarian.cpp
  readHistory.cpp
  archiveMonitor.cpp
  dbHandler.cpp
  JobAnalysisUtils.cpp
  sha256.c
)

# Temporary build as a one off executable
condor_exe(condor_librarian "${LibrarianSources}" ${C_BIN} "${CONDOR_LIBS}" ON)

# Future: Build as daemon
#condor_daemon(
#	EXE condor_librarian
#	SOURCES "${LibrarianSources}"
#	LIBRARIES "${CONDOR_LIBS}"
#	INSTALL ${C_SBIN}
#)

