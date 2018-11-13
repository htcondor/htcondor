set( HTCONDOR_BINDINGS_SOURCES collector.cpp negotiator.cpp config.cpp daemon_and_ad_types.cpp dc_tool.cpp export_headers.h old_boost.h schedd.cpp secman.cpp module_lock.cpp export_compat_classad.cpp enable_deprecation_warnings.cpp claim.cpp startd.cpp bulk_query_iterator.cpp JobEventLog.cpp )

if(WINDOWS)
  if(WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND)
	#only for VS2012 and above
	if(NOT (MSVC_VERSION LESS 1700))
		#only for boost-1.54.0 or boost-1.68
		IF("${BOOST_VER}" MATCHES "boost-1.(54|68).0")
			message(STATUS "===> Generating python-bindings with boost 1.${CMAKE_MATCH_1}")
			set(BOOST_LIB_BOOST_VER "1_${CMAKE_MATCH_1}")
			if ("${CMAKE_MATCH_1}" EQUAL "54") # no bitness neeeded	(32 bit only)
			else() #need bitness
				if(CMAKE_SIZEOF_VOID_P EQUAL 8)
					set(BOOST_LIB_BOOST_VER "x64-1_${CMAKE_MATCH_1}")
				else()
					set(BOOST_LIB_BOOST_VER "x32-1_${CMAKE_MATCH_1}")
				endif()
			endif()
			if(MSVC_VERSION EQUAL 1700)
				set(BOOST_LIB_PY2_VC_VER "python-vc110")
				set(BOOST_LIB_PY3_VC_VER "python3-vc110")
			else()
				set(BOOST_LIB_PY2_VC_VER "python27-vc141")
				set(BOOST_LIB_PY3_VC_VER "python36-vc141")
			endif()

			link_directories(${BOOST_LD})
			include_directories(${BOOST_INCLUDE})

			if (DEFINED PYTHON_VERSION_STRING)
				#we have to do this per-target because python2 and python3 need different ones
				message(STATUS "PYTHON_VERSION_STRING=${PYTHON_VERSION_STRING}")
				message(STATUS "PYTHON_DLL_SUFFIX=${PYTHON_DLL_SUFFIX}")
				message(STATUS "BOOST_LIB_BOOST_VER=${BOOST_LIB_BOOST_VER}")
				message(STATUS "BOOST_LIB_PY2_VC_VER=${BOOST_LIB_PY2_VC_VER}")

				# the boost python libraries for debug vs nondebug are different
				# so for every library, we need two names (3 names if you want add support for python_d)
				#set(PYTHON_LIBRARY_BOOST_ONE ${BOOST_LD}/boost_python-${BOOST_LIB_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib)
				#set(PYTHON_LIBRARY_BOOST_ONE_D ${BOOST_LD}/boost_python-${BOOST_LIB_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib)
				#set(PYTHON_LIBRARY_BOOST_TWO ${BOOST_LD}/libboost_python-${BOOST_LIB_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib)
				#set(PYTHON_LIBRARY_BOOST_TWO_D ${BOOST_LD}/libboost_python-${BOOST_LIB_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib)

				set(THE_BOOST_LIBS
					optimized ${BOOST_LD}/boost_${BOOST_LIB_PY2_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib
						debug ${BOOST_LD}/boost_${BOOST_LIB_PY2_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib
					optimized ${BOOST_LD}/libboost_${BOOST_LIB_PY2_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib
						debug ${BOOST_LD}/libboost_${BOOST_LIB_PY2_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib
					)


				# When you build Debug, the mere act of including <pyconfig.h>
				# will generate a reference to the python27_d library in the object files
				# if you let boost include pyconfig.h, then it defeats this behavior unless
				# you enable python debugging in the boost way.  We don't currently support
				# this option with htcondor builds, so if you end up with link errors to a
				# non-existent python27_d, the problem is in the order that you include headers
				# set(PYTHON_DEBUG_LIBRARY ${BOOST_LD}/python27_d.lib)

				get_filename_component(PYTHON_LIBRARY_FILENAME ${PYTHON_LIBRARIES} NAME)
				string(REGEX REPLACE "([0-9]+[.][0-9]+).*" "\\1" _PYTHON_VERSION ${PYTHON_VERSION_STRING})
				if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
					set( PYCLASSAD_LIB_NAME "pyclassad${_PYTHON_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
				else()
					message(FATAL_ERROR "Can't determine HTCondor version!")
				endif()

				condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h )
				target_link_libraries( pyclassad ${CLASSADS_FOUND} ${THE_BOOST_LIBS} ${PYTHON_LIBRARIES} )
				set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
				target_include_directories( pyclassad PUBLIC ${PYTHON_INCLUDE_DIRS} )

				# Note we do not use condor_shared_lib below because we want a separate install target.
				# the 'optimized' keyword indicates that the library following it is for release builds only
				# the 'debug' keyword indicates that the library following it is for debug builds only
				add_library( classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
				target_link_libraries( classad_module pyclassad ${PCRE_FOUND} ${THE_BOOST_LIBS} ${PYTHON_LIBRARIES} )
				set_target_properties(classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )
				set_target_properties(classad_module PROPERTIES SUFFIX "${PYTHON_DLL_SUFFIX}" )
				target_include_directories( classad_module PUBLIC ${PYTHON_INCLUDE_DIRS} )

				# This is to be used as a CLASSAD_USER_LIBS extension so we can invoke
				# python functions.
				add_library( classad_python_user SHARED classad_python_user.cpp )
				target_link_libraries( classad_python_user condor_utils pyclassad ${CONDOR_WIN_LIBS})
				set_target_properties(classad_python_user PROPERTIES SUFFIX "${PYTHON_DLL_SUFFIX}" )
				target_include_directories( classad_python_user PUBLIC ${PYTHON_INCLUDE_DIRS} )
				install ( TARGETS classad_python_user DESTINATION "${C_LIBEXEC}" )

				add_library( htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} )
				target_link_libraries( htcondor pyclassad condor_utils ${THE_BOOST_LIBS} ${PYTHON_LIBRARIES} ${CONDOR_WIN_LIBS} )
				set_target_properties( htcondor PROPERTIES SUFFIX "${PYTHON_DLL_SUFFIX}" )
				target_include_directories( htcondor PUBLIC ${PYTHON_INCLUDE_DIRS} )

				install ( TARGETS pyclassad DESTINATION ${C_LIB} )
				install ( TARGETS htcondor classad_module DESTINATION ${C_PYTHONARCHLIB} )
			endif(DEFINED PYTHON_VERSION_STRING) # python27 was found

			if (DEFINED PYTHON3_VERSION_STRING) # python36 was found
				#we have to do this per-target because python2 and python3 need different ones
				#include_directories(${PYTHON_INCLUDE_DIRS} ${BOOST_INCLUDE})
				message(STATUS "PYTHON3_VERSION_STRING=${PYTHON3_VERSION_STRING}")
				message(STATUS "PYTHON3_LIBRARIES=${PYTHON3_LIBRARIES}")
				message(STATUS "PYTHON3_INCLUDE_DIRS=${PYTHON3_INCLUDE_DIRS}")
				message(STATUS "PYTHON3_DLL_SUFFIX=${PYTHON3_DLL_SUFFIX}")
				message(STATUS "BOOST_LIB_BOOST_VER=${BOOST_LIB_BOOST_VER}")
				message(STATUS "BOOST_LIB_PY3_VC_VER=${BOOST_LIB_PY3_VC_VER}")

				include_directories(${PYTHON3_INCLUDE_DIRS})

				# When you build Debug, the mere act of including <pyconfig.h>
				# will generate a reference to the python27_d library in the object files
				# if you let boost include pyconfig.h, then it defeats this behavior unless
				# you enable python debugging in the boost way.  We don't currently support
				# this option with htcondor builds, so if you end up with link errors to a
				# non-existent python27_d, the problem is in the order that you include headers
				# set(PYTHON_DEBUG_LIBRARY ${BOOST_LD}/python27_d.lib)

				get_filename_component(PYTHON3_LIBRARY_FILENAME ${PYTHON3_LIBRARIES} NAME)
				string(REGEX REPLACE "([0-9]+[.][0-9]+).*" "\\1" _PYTHON_VERSION ${PYTHON3_VERSION_STRING})
				if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
					set( PY3CLASSAD_LIB_NAME "pyclassad${_PYTHON_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
				else()
					message(FATAL_ERROR "Can't determine HTCondor version!")
				endif()

				set(THE_BOOST_LIBS
					optimized ${BOOST_LD}/boost_${BOOST_LIB_PY3_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib
						debug ${BOOST_LD}/boost_${BOOST_LIB_PY3_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib
					optimized ${BOOST_LD}/libboost_${BOOST_LIB_PY3_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib
						debug ${BOOST_LD}/libboost_${BOOST_LIB_PY3_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib
					)

				condor_shared_lib( py3classad classad.cpp classad_wrapper.h exprtree_wrapper.h )
				target_link_libraries( py3classad ${CLASSADS_FOUND} ${THE_BOOST_LIBS} ${PYTHON3_LIBRARIES} )
				set_target_properties( py3classad PROPERTIES OUTPUT_NAME "${PY3CLASSAD_LIB_NAME}" )
				target_include_directories( py3classad PUBLIC ${PYTHON3_INCLUDE_DIRS} )

				# Note we do not use condor_shared_lib below because we want a separate install target.
				# the 'optimized' keyword indicates that the library following it is for release builds only
				# the 'debug' keyword indicates that the library following it is for debug builds only
				add_library( py3classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
				target_link_libraries( py3classad_module py3classad ${PCRE_FOUND}  ${THE_BOOST_LIBS} ${PYTHON3_LIBRARIES} )
				set_target_properties(py3classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad PDB_NAME py3classad)
				set_target_properties(py3classad_module PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )
				target_include_directories( py3classad_module PUBLIC ${PYTHON3_INCLUDE_DIRS} )

				# This is to be used as a CLASSAD_USER_LIBS extension so we can invoke
				# python functions.
				add_library( classad_python3_user SHARED classad_python_user.cpp )
				target_link_libraries( classad_python3_user condor_utils py3classad ${THE_BOOST_LIBS} ${CONDOR_WIN_LIBS})
				set_target_properties(classad_python3_user PROPERTIES PREFIX "" OUTPUT_NAME classad_python_user PDB_NAME classad_python3_user)
				set_target_properties(classad_python3_user PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )
				target_include_directories( classad_python3_user PUBLIC ${PYTHON3_INCLUDE_DIRS} )
				install ( TARGETS classad_python3_user DESTINATION "${C_LIBEXEC}" )

				add_library( py3htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} )
				target_link_libraries( py3htcondor py3classad condor_utils ${THE_BOOST_LIBS} ${PYTHON3_LIBRARIES} ${CONDOR_WIN_LIBS} )
				set_target_properties( py3htcondor PROPERTIES PREFIX "" OUTPUT_NAME htcondor PDB_NAME py3htcondor)
				set_target_properties( py3htcondor PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )
				target_include_directories( py3htcondor PUBLIC ${PYTHON_INCLUDE3_DIRS} )

				install ( TARGETS py3classad DESTINATION ${C_LIB} )
				install ( TARGETS py3htcondor py3classad_module DESTINATION ${C_PYTHONARCHLIB} )

			endif(DEFINED PYTHON3_VERSION_STRING) # python36 was found

		# more targets are built if we plan on building wheels
		if ( WANT_PYTHON_WHEELS )
			message(STATUS "Setting up python wheels on Windows is not yet supported")
		endif()

            string( COMPARE EQUAL ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} INSOURCEBUILD)
            if (BUILD_TESTING)
              if(NOT INSOURCEBUILD)
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/event_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/event_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/event_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/event_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/job.log DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/job.log COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/job.log ${CMAKE_CURRENT_BINARY_DIR}/tests/job.log )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/log_reader_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/log_reader_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/log_reader_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/log_reader_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/job_queue.log DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/job_queue.log COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/job_queue.log ${CMAKE_CURRENT_BINARY_DIR}/tests/job_queue.log )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_version_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_version_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_version_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_version_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.old.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.old.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_sleep.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_sleep.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_sleep.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_sleep.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_large.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_large.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_large.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_large.ad )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py )
                add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_log.txt DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log.txt COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log.txt ${CMAKE_CURRENT_BINARY_DIR}/tests/test_log.txt )
              endif()
              add_custom_target ( python_bindings_tests ALL DEPENDS tests/log_reader_tests.py tests/event_tests.py tests/job_queue.log tests/job.log tests/classad_tests.py tests/htcondor_version_tests.py tests/htcondor_tests.py tests/delayed_update_tests.py tests/test.ad tests/test_multiple.ad tests/test.old.ad tests/test_multiple.old.ad tests/submit.ad tests/submit_sleep.ad tests/submit_large.ad tests/delayed_submit.ad tests/delayed_update.py tests/test_log.txt ${CMAKE_CURRENT_BINARY_DIR}/test_driver )
              enable_testing()
            endif()
        endif()
    endif(NOT (MSVC_VERSION LESS 1700))
  endif()
else()
  if ( WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND AND Boost_PYTHON_LIBRARY AND NOT SOLARIS )
    configure_file (
      "${PROJECT_SOURCE_DIR}/src/python-bindings/test_driver.in"
      "${CMAKE_CURRENT_BINARY_DIR}/test_driver"
    )

    set ( CMAKE_LIBRARY_PATH_ORIG ${CMAKE_LIBRARY_PATH} )
    set ( CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/lib64 )
    set ( CMAKE_LIBRARY_PATH CMAKE_LIBRARY_PATH_ORIG)

    if ("${PYTHON_VERSION_MAJOR}" MATCHES "3")
      set ( PYTHON_BOOST_LIB boost_python3 )
    else()
      set ( PYTHON_BOOST_LIB boost_python )
    endif()

    include_directories(${PYTHON_INCLUDE_DIRS} ${BOOST_INCLUDE})
    link_directories(${BOOST_LD})

    get_filename_component(PYTHON_LIBRARY_FILENAME ${PYTHON_LIBRARIES} NAME)
    string(REGEX REPLACE "([0-9]+[.][0-9]+).*" "\\1" _PYTHON_VERSION ${PYTHON_VERSION_STRING})
    if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
      set( PYCLASSAD_LIB_NAME "pyclassad${_PYTHON_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
    else()
      message(FATAL_ERROR "Can't determine HTCondor version!")
    endif()
    condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h )
    target_link_libraries( pyclassad ${CLASSADS_FOUND} -l${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
    set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
    set_target_properties( pyclassad PROPERTIES COMPILE_FLAGS "-fPIC")

    # Note we do not use condor_shared_lib below because we want a separate install target.
    add_library( classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
    target_link_libraries( classad_module pyclassad -l${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
    set_target_properties(classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )
    set_target_properties(classad_module PROPERTIES SUFFIX ".so" )
    if ( NOT CMAKE_SKIP_RPATH )
      set_target_properties(classad_module PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
    endif()

    # This is to be used as a CLASSAD_USER_LIBS extension so we can invoke
    # python functions.
    add_library( classad_python_user SHARED classad_python_user.cpp )
    target_link_libraries( classad_python_user condor_utils pyclassad )
    set_target_properties(classad_python_user PROPERTIES SUFFIX ".so" )
    if ( NOT CMAKE_SKIP_RPATH )
      set_target_properties(classad_python_user PROPERTIES INSTALL_RPATH "${CONDOR_RPATH}")
    endif()
    install ( TARGETS classad_python_user DESTINATION "${C_LIBEXEC}" )

    if ( LINUX )
      # This is to be used as a collector plugin module which invoke python functions.
      add_library( collector_python_plugin SHARED collector_plugin.cpp )
      target_link_libraries( collector_python_plugin condor_utils pyclassad )
      set_target_properties( collector_python_plugin PROPERTIES SUFFIX ".so" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties( collector_python_plugin PROPERTIES INSTALL_RPATH "${CONDOR_RPATH}")
      endif()
      install ( TARGETS collector_python_plugin DESTINATION "${C_LIBEXEC}" )
    endif()

    # We'll deprecate log_reader.cpp at some point.
    set_source_files_properties( dc_tool.cpp classad.cpp classad_module.cpp classad_parsers.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp PROPERTIES COMPILE_FLAGS "-Wno-strict-aliasing -Wno-cast-qual -Wno-deprecated -Wno-write-strings")
    add_library( htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp )
    target_link_libraries( htcondor pyclassad condor_utils -l${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
    set_target_properties( htcondor PROPERTIES PREFIX "" )
    set_target_properties( htcondor PROPERTIES SUFFIX ".so" )
    if ( NOT CMAKE_SKIP_RPATH )
      set_target_properties(htcondor PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
    endif()

    install ( TARGETS htcondor classad_module DESTINATION ${C_PYTHONARCHLIB} )

    if ( DARWIN )
        set_target_properties( pyclassad PROPERTIES INSTALL_NAME_DIR ${CMAKE_CURRENT_BINARY_DIR} )
        install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB}/lib${PYCLASSAD_LIB_NAME}.dylib)" )
        install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHONARCHLIB}/classad.so)" )
        install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHONARCHLIB}/htcondor.so)" )
    endif()

    # more targets are built if we plan on building wheels
    if ( WANT_PYTHON_WHEELS )

      # first, check if building in source
      set( PYPI_BUILD_DIR "${CMAKE_BINARY_DIR}/build/packaging/pypi" )
      set( PYPI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/packaging/pypi" )
      string( COMPARE EQUAL ${PYPI_BUILD_DIR} ${PYPI_SOURCE_DIR} INSOURCEBUILD)

      # next, set up the directory structure
      add_custom_target( pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory "${PYPI_BUILD_DIR}" )
      add_custom_target( pypi_classad_dir DEPENDS pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory "${PYPI_BUILD_DIR}/classad" )
      add_custom_target( pypi_htcondor_dir DEPENDS pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory "${PYPI_BUILD_DIR}/htcondor" )
      add_custom_target( pypi_lib_dir DEPENDS pypi_htcondor_dir COMMAND ${CMAKE_COMMAND} -E make_directory "${PYPI_BUILD_DIR}/htcondor/condor_libs" )

      if( NOT INSOURCEBUILD ) # if not building in source, copy over the wheel packaging files
        add_custom_command( TARGET pypi_dir POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${PYPI_SOURCE_DIR}/setup.py ${PYPI_BUILD_DIR}/setup.py )
        add_custom_command( TARGET pypi_dir POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${PYPI_SOURCE_DIR}/setup.cfg ${PYPI_BUILD_DIR}/setup.cfg )
        add_custom_command( TARGET pypi_dir POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${PYPI_SOURCE_DIR}/README.rst ${PYPI_BUILD_DIR}/README.rst )
        add_custom_command( TARGET pypi_classad_dir POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${PYPI_SOURCE_DIR}/classad/__init__.py ${PYPI_BUILD_DIR}/classad/__init__.py )
        add_custom_command( TARGET pypi_htcondor_dir POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${PYPI_SOURCE_DIR}/htcondor/__init__.py ${PYPI_BUILD_DIR}/htcondor/__init__.py )
      endif()

      # the pyclassad library and the classad and htcondor modules should not link libpython
      # instead, the user's python interpreter will look for any such missing symbols
      if ( DARWIN )
        # in macOS, use "-undefined dynamic_lookup" in place of "-lpython" or "-framework Python" (i.e. ${PYTHON_LIBRARIES})
        set( NOLINK_PYTHON_LIB "-undefined dynamic_lookup" )
      else()
        # in Linux, simply omit "-lpython" (i.e. ${PYTHON_LIBRARIES}) to get the same behavior
        set( NOLILNK_PYTHON_LIB "" )
      endif()

      # build libpyclassad *not* linked against libpython
      add_library( _pyclassad SHARED classad.cpp classad_wrapper.h exprtree_wrapper.h )
      add_dependencies( _pyclassad pypi_lib_dir )
      target_link_libraries( _pyclassad ${CLASSADS_FOUND} -l${PYTHON_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      if ( DARWIN )
        set_target_properties( _pyclassad PROPERTIES SUFFIX ".dylib" )
        set_target_properties( _pyclassad PROPERTIES INSTALL_NAME_DIR "${PYPI_BUILD_DIR}/htcondor/condor_libs" )
      else()
        set_target_properties( _pyclassad PROPERTIES SUFFIX ".so" )
      endif()
      set_target_properties( _pyclassad PROPERTIES PREFIX "lib" OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
      set_target_properties( _pyclassad PROPERTIES COMPILE_FLAGS "-fPIC" )
      set_target_properties( _pyclassad PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PYPI_BUILD_DIR}/htcondor/condor_libs" )
      set_target_properties( _pyclassad PROPERTIES INSTALL_RPATH "$ORIGIN" )

      # next, build the classad module *not* linked against libpython
      add_library( _classad_module SHARED _classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
      add_dependencies( _classad_module pypi_classad_dir )
      target_link_libraries( _classad_module _pyclassad -l${PYTHON_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      set_target_properties( _classad_module PROPERTIES PREFIX "" OUTPUT_NAME "_classad" )
      set_target_properties( _classad_module PROPERTIES SUFFIX ".so" )
      set_target_properties( _classad_module PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PYPI_BUILD_DIR}/classad" )
      set_target_properties( _classad_module PROPERTIES INSTALL_RPATH "$ORIGIN/../htcondor/condor_libs" )

      # finally, build the htcondor module *not* linked against libpython
      # See note above about deprecating log_reader.cpp.
      add_library( _htcondor SHARED _htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp )
      add_dependencies( _htcondor pypi_htcondor_dir )
      target_link_libraries( _htcondor _pyclassad condor_utils -l${PYTHON_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      set_target_properties( _htcondor PROPERTIES PREFIX "" )
      set_target_properties( _htcondor PROPERTIES SUFFIX ".so" )
      set_target_properties( _htcondor PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PYPI_BUILD_DIR}/htcondor" )
      set_target_properties( _htcondor PROPERTIES INSTALL_RPATH "$ORIGIN/condor_libs" )

    endif()

    string( COMPARE EQUAL ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} INSOURCEBUILD)
    if (BUILD_TESTING)
      if(NOT INSOURCEBUILD)
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/event_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/event_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/event_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/event_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/job.log DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/job.log COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/job.log ${CMAKE_CURRENT_BINARY_DIR}/tests/job.log )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/log_reader_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/log_reader_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/log_reader_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/log_reader_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/job_queue.log DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/job_queue.log COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/job_queue.log ${CMAKE_CURRENT_BINARY_DIR}/tests/job_queue.log )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_version_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_version_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_version_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_version_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.old.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.old.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiple.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test_multiple.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_sleep.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_sleep.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_sleep.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_sleep.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_large.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_large.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit_large.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit_large.ad )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py )
        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test_log.txt DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log.txt COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log.txt ${CMAKE_CURRENT_BINARY_DIR}/tests/test_log.txt )
      endif()
      add_custom_target ( python_bindings_tests ALL DEPENDS tests/log_reader_tests.py tests/event_tests.py tests/job_queue.log tests/job.log tests/classad_tests.py tests/htcondor_version_tests.py tests/htcondor_tests.py tests/delayed_update_tests.py tests/test.ad tests/test_multiple.ad tests/test.old.ad tests/test_multiple.old.ad tests/submit.ad tests/submit_sleep.ad tests/submit_large.ad tests/delayed_submit.ad tests/delayed_update.py tests/test_log.txt ${CMAKE_CURRENT_BINARY_DIR}/test_driver )
      enable_testing()
      if ( LINUX OR DARWIN )
        if ( ${SYSTEM_NAME} MATCHES "rhel5" )
            message(STATUS "classad_test python bindings tests removed for RedHat 5 OS")
        else()
            add_test(python_classad_tests test_driver tests/classad_tests.py -v)
            add_test(python_htcondor_version_tests test_driver tests/htcondor_version_tests.py -v)
        endif()
      endif ( LINUX OR DARWIN )
    endif (BUILD_TESTING)
  endif ( WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND AND Boost_PYTHON_LIBRARY AND NOT SOLARIS )
endif(WINDOWS)
