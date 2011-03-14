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


if ( NOT PROPER )

    option( WITH_HADOOP "Compiling with support for HADOOP" ON )

	if (WITH_HADOOP)

		set (HADOOP_VER hadoop-0.21.0)
		if ( WINDOWS )
			set ( HADOOP_INSTALL md \"${EXTERNAL_STAGE}/hdfs/conf\" ${CMD_TERM}
								cp -r lib ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								rm -rf ${EXTERNAL_STAGE}/hdfs/lib/native ${CMD_TERM}
								cp -r webapps ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hadoop-common-0.21.0.jar ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hadoop-hdfs-0.21.0.jar ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hdfs/src/java/hdfs-default.xml ${EXTERNAL_STAGE}/hdfs/conf ${CMD_TERM} )
		else()
			set ( HADOOP_INSTALL mkdir -p ${EXTERNAL_STAGE}/hdfs/conf ${CMD_TERM}
								cp -r lib ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp -r webapps ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hadoop-common-0.21.0.jar ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hadoop-hdfs-0.21.0.jar ${EXTERNAL_STAGE}/hdfs ${CMD_TERM}
								cp hdfs/src/java/hdfs-default.xml ${EXTERNAL_STAGE}/hdfs/conf )
		endif()
		# a couple of odd things, this macro requires a configure and build command.  
		ExternalProject_Add(hadoop
							PREFIX ${EXTERNAL_BUILD_PREFIX}/${HADOOP_VER}
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${HADOOP_VER}.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/${HADOOP_VER}.tar.gz
							# PATCH_COMMAND patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/hadoop-0.20.0-patch
							CONFIGURE_COMMAND echo "nothing to configure"
							BUILD_COMMAND echo "nothing to build"
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND cd ${HADOOP_VER} ${CMD_TERM} ${HADOOP_INSTALL} )

		## Set all the config information
		set( HADOOP_TARGET ${HADOOP_TARGET} PARENT_SCOPE )
		set( HAVE_EXT_HADOOP ON PARENT_SCOPE )
		append_var(CONDOR_EXTERNALS hadoop)
		message (STATUS "external configured (HADOOP_FOUND=${EXTERNAL_STAGE}/hdfs)")

		if (WINDOWS)
		  set_property( TARGET hadoop PROPERTY FOLDER "externals" )
		  install ( DIRECTORY ${EXTERNAL_STAGE}/hdfs/ DESTINATION ./hdfs )
		else()
		  install ( DIRECTORY ${EXTERNAL_STAGE}/hdfs DESTINATION ${C_LIBEXEC} )
		endif()

	else()
		message (STATUS "external skipped (hadoop)")
	endif()

else( NOT PROPER )

	message (STATUS "external skipped (hadoop)")

endif( NOT PROPER )

	
