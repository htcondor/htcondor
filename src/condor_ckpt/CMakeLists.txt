 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
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
 ############################################################### 

##############################################################
## Abandon all hope ye who enter here:
## this be the mystical workings of the infamous
## standard universe.  No moral platitudes apply, so
## batton your hatches, find yourself a cup of ale,
## because you are about to loose your mind.
## YARRR!
##############################################################
if( STD_UNIVERSE )

	###########################################################################
	# NOTE: all glibc ripping which existed here has moved to syscall_lib
	# where it is actually bound in.
	###########################################################################
	set( CMAKE_BUILD_TYPE Debug )
	
	###########################################################################
	# define the actual targets, and copiler options
	###########################################################################
	add_definitions( ${STD_U_C_FLAGS} -DFILE_TABLE -DIN_CKPT_LIB)
	include_directories(${CONDOR_SOURCE_DIR}/src/condor_ckpt)
	condor_selective_glob("*file*;signals*;machdep.LINUX.cpp;tmp_*;syscall_*;fake_*;condor_error*;eprintf*;shared_*;malloc-user*;gto*;maps*" CommonCkptSrcs )

	# we create a local lib, but it's never actually used
	# instead condorsyscall will rip out the target refs.
	#condor_shared_lib( ckpt "${CommonCkptSrcs};${CompressSrcs}")
	add_library(ckpt STATIC EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/image.cpp;${CommonCkptSrcs})
	add_dependencies( ckpt syscall_numbers )

	if (DOES_COMPRESS_CKPT)

		add_library(zckpt STATIC EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/zimage.cpp;${CMAKE_CURRENT_SOURCE_DIR}/malloc-condor.c;${CommonCkptSrcs})

		add_dependencies( zckpt syscall_numbers )
	endif(DOES_COMPRESS_CKPT)

endif(STD_UNIVERSE)
