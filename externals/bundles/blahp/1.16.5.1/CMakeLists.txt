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

# The source tarball for this external was downloaded from this URL:
#   http://jra1mw.cvs.cern.ch:8180/cgi-bin/jra1mw.cgi/org.glite.ce.blahp.tar.gz?view=tar&pathrev=glite-ce-blahp_R_1_16_5_1

if ( LINUX )
	option(WITH_BLAHP "Compiling with support for BLAHP" ON)
endif ( LINUX )

if ( WITH_BLAHP )

	if ( NOT PROPER )

		########################################
		if ( BUILDING_OPENSSL )
			set( BLAHP_DEPENDS ${OPENSSL_REF} ${GLOBUS_REF} )
			set( GLOBUS_M4_FILE globus.m4 )
			set( OPENSSL_FLAG --with-openssl-prefix=${OPENSSL_INSTALL_LOC} )
		else()
			set( BLAHP_DEPENDS ${GLOBUS_REF} )
			set( GLOBUS_M4_FILE globus.m4.openssl )
			set( OPENSSL_FLAG "" )
		endif()


		condor_pre_external( BLAHP blahp-1.16.5.1-p10 "lib;include" "done")
		
		ExternalProject_Add(blahp
						DEPENDS ${BLAHP_DEPENDS}
					    #-- Download Step ----------
						DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/blahp-1.16.5.1.tar.gz ${CMD_TERM}
					    DOWNLOAD_DIR ${BLAHP_STAGE}/dl
						URL http://parrot.cs.wisc.edu/externals/blahp-1.16.5.1.tar.gz
						#--Patch step ----------
						PATCH_COMMAND patch -i ${CMAKE_CURRENT_SOURCE_DIR}/iclassad.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/classad.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/mtrace.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/registry.patch -p0 && 
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/dl.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/add-sge.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/sge-status.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/sge-status-fallback.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/add-condor.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/pbs-completion.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/pbs-qdel.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/pbs-status.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/shared-fs.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/shared-dir.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/config-paths.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/escape-args.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/condor-history.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/submit-l.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/proxy-init.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/ld-library-path.patch -p0 &&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/run-dir.patch -p0 &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/classads.m4 org.glite.ce.blahp/project &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/${GLOBUS_M4_FILE} org.glite.ce.blahp/project/globus.m4 &&
							cp ${CMAKE_CURRENT_SOURCE_DIR}/glite.m4 org.glite.ce.blahp/project.&&
							patch -i ${CMAKE_CURRENT_SOURCE_DIR}/name-change.patch -p0 &&
							mv org.glite.ce.blahp/config/blah.config.template org.glite.ce.blahp/config/batch_gahp.config.template
						#--Configure step ----------
						CONFIGURE_COMMAND cd org.glite.ce.blahp &&
                                                        export CPPFLAGS=-I${BOOST_INCLUDE} &&
							./bootstrap && ./configure --with-classads-prefix=${CLASSADS_INSTALL_LOC}
							${OPENSSL_FLAG}
							--with-globus-prefix=${GLOBUS_INSTALL_LOC}
							--with-globus-nothr-flavor=gcc${BIT_MODE}dbg
							--prefix=${BLAHP_INSTALL_LOC}/opt/glite &&
							sed -i 's/^hardcode_libdir_flag_spec=.*/hardcode_libdir_flag_spec=/' libtool
						#--Build Step ----------
						BUILD_COMMAND cd org.glite.ce.blahp && LD_RUN_PATH='${EXTERNALS_RPATH}' make
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${BLAHP_INSTALL_LOC}
						INSTALL_COMMAND cd org.glite.ce.blahp && make install && touch ${BLAHP_INSTALL_LOC}/done )

		add_dependencies(blahp ${CLASSADS_FOUND})

		### set the install
		install ( DIRECTORY ${BLAHP_INSTALL_LOC}/opt/glite DESTINATION ${C_LIBEXEC} USE_SOURCE_PERMISSIONS )
		install ( CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../../lib ${CMAKE_INSTALL_PREFIX}/${C_LIBEXEC}/glite/lib)" )
		if ( CONDOR_STRIP_PACKAGES AND NOT CONDOR_PACKAGE_BUILD )
			# Strip the libraries manually.
			# Since cmake didn't create the libraries
			# itself, it doesn't know to strip them.
			install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIBEXEC}/glite/bin/. -name [bBt]*[^h]
				COMMAND xargs strip)" )
		endif()

		condor_post_external( blahp OFF OFF )

		set(BLAHP_FOUND ON)

	else ( NOT PROPER )

		find_program( BLAHP_FOUND BLClient HINTS /usr/bin )
		dprint("Scanning for BLClient...(${BLAHP_FOUND})")

	endif ( NOT PROPER )


endif( WITH_BLAHP )

if ( BLAHP_FOUND )
	message( STATUS "external configured (blahp) BLAHP_FOUND=${BLAHP_FOUND}" )
	set( BLAHP_FOUND ${BLAHP_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_BLAHP ON PARENT_SCOPE )
else()
	message ( STATUS "external skipped (blahp)" )
endif ( BLAHP_FOUND )
	
