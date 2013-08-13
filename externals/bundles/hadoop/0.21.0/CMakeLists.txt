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
		
		condor_pre_external( HADOOP hadoop-0.21.0 "hdfs" "hdfs/conf/hdfs-default.xml")
	
		if ( WINDOWS )
			set ( HADOOP_INSTALL md \"${HADOOP_INSTALL_LOC}/hdfs/conf\" ${CMD_TERM}
								cp -r lib ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								rm -rf ${HADOOP_INSTALL_LOC}/hdfs/lib/native ${CMD_TERM}
								cp -r webapps ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hadoop-common-0.21.0.jar ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hadoop-hdfs-0.21.0.jar ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hdfs/src/java/hdfs-default.xml ${HADOOP_INSTALL_LOC}/hdfs/conf ${CMD_TERM} )
		else()
			set ( HADOOP_INSTALL mkdir -p ${HADOOP_INSTALL_LOC}/hdfs/conf ${CMD_TERM}
								cp -r lib ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp -r webapps ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hadoop-common-0.21.0.jar ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hadoop-hdfs-0.21.0.jar ${HADOOP_INSTALL_LOC}/hdfs ${CMD_TERM}
								cp hdfs/src/java/hdfs-default.xml ${HADOOP_INSTALL_LOC}/hdfs/conf )
		endif()
		# a couple of odd things, this macro requires a configure and build command.  
		ExternalProject_Add(hadoop
					#-- Download Step ----------
					DOWNLOAD_DIR ${HADOOP_STAGE}/dl
					URL ${EXTERNALS_SOURCE_URL}/${HADOOP_VER}.tar.gz
					# PATCH_COMMAND patch -N -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/hadoop-0.20.0-patch
					CONFIGURE_COMMAND echo "nothing to configure"
					BUILD_COMMAND echo "nothing to build"
					BUILD_IN_SOURCE 1
					#--install Step ----------
					INSTALL_DIR ${HADOOP_INSTALL_LOC}
					INSTALL_COMMAND ${HADOOP_INSTALL} )

		## Set all the config information
		set( HADOOP_TARGET ${HADOOP_TARGET} PARENT_SCOPE )
		set( HAVE_EXT_HADOOP ON PARENT_SCOPE )
		message (STATUS "external configured (HADOOP_FOUND=${HADOOP_INSTALL_LOC}/hdfs)")

		if (WINDOWS)
		  install ( DIRECTORY ${HADOOP_INSTALL_LOC}/hdfs/ DESTINATION ./hdfs )
		else()
		  install ( DIRECTORY ${HADOOP_INSTALL_LOC}/hdfs DESTINATION ${C_LIBEXEC} )
		endif()
		
		condor_post_external( hadoop OFF OFF )

	else()
		message (STATUS "external skipped (hadoop)")
	endif()

else( NOT PROPER )

	message (STATUS "external skipped (hadoop)")

endif( NOT PROPER )

	
