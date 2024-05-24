# We'll be deprecating event.cpp shortly.
set( HTCONDOR_BINDINGS_SOURCES
    bulk_query_iterator.cpp
    claim.cpp
    collector.cpp
    config.cpp
    daemon_and_ad_types.cpp
    credd.cpp
    daemon_location.cpp
    dc_tool.cpp
    enable_deprecation_warnings.cpp
    event.cpp
    exception_utils.cpp
    export_compat_classad.cpp
    export_headers.h
    JobEventLog.cpp
    module_lock.cpp
    negotiator.cpp
    schedd.cpp
    secman.cpp
    startd.cpp
)

if(WINDOWS)
  if(WITH_PYTHON_BINDINGS AND ( PYTHONLIBS_FOUND OR PYTHON3LIBS_FOUND ))
    # Modern compilers complain about using the deprecated std::auto_ptr.
    # Prevent boost from using them.
    add_definitions ( -DBOOST_NO_AUTO_PTR )

	#only for VS2012 and above
	if(NOT (MSVC_VERSION LESS 1700))
		#only for boost-1.54.0 or boost-1.68
		IF("${BOOST_VER}" MATCHES "boost-1.(54|68|78).0")
			message(STATUS "===> Generating python-bindings with boost 1.${CMAKE_MATCH_1}")
			set(BOOST_LIB_BOOST_VER "1_${CMAKE_MATCH_1}")
			if ("${CMAKE_MATCH_1}" EQUAL "78")
				# disable python2
				unset (PYTHON_VERSION_STRING)
			endif()
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
				set(BOOST_LIB_PY3_VC_VER "${PYTHON3_LIB_BASENAME}-${BOOST_DLL_VCVER}")
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

				condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h exception_utils.cpp classad_exceptions.cpp )
				target_link_libraries( pyclassad ${CLASSADS_FOUND} ${THE_BOOST_LIBS} ${PYTHON_LIBRARIES} )
				set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
				target_include_directories( pyclassad PUBLIC ${PYTHON_INCLUDE_DIRS} )

				# Note we do not use condor_shared_lib below because we want a separate install target.
				# the 'optimized' keyword indicates that the library following it is for release builds only
				# the 'debug' keyword indicates that the library following it is for debug builds only
				add_library( classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
				target_link_libraries( classad_module pyclassad ${THE_BOOST_LIBS} ${PYTHON_LIBRARIES} )
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
				install ( TARGETS htcondor DESTINATION ${C_PYTHONARCHLIB}/htcondor )
				install ( TARGETS classad_module DESTINATION ${C_PYTHONARCHLIB}/classad )
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
				#  optimized ${BOOST_LD}/libboost_${BOOST_LIB_PY3_VC_VER}-mt-${BOOST_LIB_BOOST_VER}.lib
				#      debug ${BOOST_LD}/libboost_${BOOST_LIB_PY3_VC_VER}-mt-gd-${BOOST_LIB_BOOST_VER}.lib
					)

				condor_shared_lib( py3classad classad.cpp classad_wrapper.h exprtree_wrapper.h exception_utils.cpp classad_exceptions.cpp )
				target_link_libraries( py3classad ${CLASSADS_FOUND} ${THE_BOOST_LIBS} ${PYTHON3_LIBRARIES} )
				set_target_properties( py3classad PROPERTIES OUTPUT_NAME "${PY3CLASSAD_LIB_NAME}" )
				target_include_directories( py3classad PUBLIC ${PYTHON3_INCLUDE_DIRS} )

				# Note we do not use condor_shared_lib below because we want a separate install target.
				# the 'optimized' keyword indicates that the library following it is for release builds only
				# the 'debug' keyword indicates that the library following it is for debug builds only
				add_library( py3classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
				target_link_libraries( py3classad_module py3classad ${THE_BOOST_LIBS} ${PYTHON3_LIBRARIES} )
				set_target_properties(py3classad_module PROPERTIES PREFIX "" OUTPUT_NAME py3classad PDB_NAME py3classad)
				set_target_properties(py3classad_module PROPERTIES PREFIX "" RUNTIME_OUTPUT_NAME classad PDB_NAME py3classad)
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
				set_target_properties( py3htcondor PROPERTIES PREFIX "" OUTPUT_NAME py3htcondor PDB_NAME py3htcondor)
				set_target_properties( py3htcondor PROPERTIES PREFIX "" RUNTIME_OUTPUT_NAME htcondor PDB_NAME py3htcondor)
				set_target_properties( py3htcondor PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )
				target_include_directories( py3htcondor PUBLIC ${PYTHON3_INCLUDE_DIRS} )


				get_filename_component(PLD ${PYTHON3_LIBRARIES} "DIRECTORY")
				message(STATUS "PLD=${PLD}")
				set(PLFN "${PLD}/python3.lib")

				add_library(                  py3classad2_impl SHARED classad2_impl.cpp )
				target_include_directories(   py3classad2_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
				target_link_libraries(        py3classad2_impl ${CLASSADS_FOUND} condor_utils ${PLFN} ${CONDOR_WIN_LIBS} )
				set_target_properties(        py3classad2_impl PROPERTIES PREFIX "" OUTPUT_NAME classad2_impl )
				set_target_properties(        py3classad2_impl PROPERTIES PREFIX "" RUNTIME_OUTPUT_NAME classad2_impl PDB_NAME py3classad2 )
				set_target_properties(        py3classad2_impl PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )

				add_library(                  py3htcondor2_impl SHARED htcondor2_impl.cpp )
				target_include_directories(   py3htcondor2_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
				target_link_libraries(        py3htcondor2_impl ${CLASSADS_FOUND} condor_utils ${PLFN} ${CONDOR_WIN_LIBS} )
				set_target_properties(        py3htcondor2_impl PROPERTIES PREFIX "" OUTPUT_NAME htcondor2_impl )
				set_target_properties(        py3htcondor2_impl PROPERTIES PREFIX "" RUNTIME_OUTPUT_NAME htcondor2_impl PDB_NAME py3htcondor2 )
				set_target_properties(        py3htcondor2_impl PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )

				add_library(                  py3classad3_impl SHARED classad3_impl.cpp )
				target_include_directories(   py3classad3_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
				target_link_libraries(        py3classad3_impl ${CLASSADS_FOUND} condor_utils ${PLFN} ${CONDOR_WIN_LIBS} )
				set_target_properties(        py3classad3_impl PROPERTIES PREFIX "" OUTPUT_NAME classad3_impl )
				set_target_properties(        py3classad3_impl PROPERTIES PREFIX "" RUNTIME_OUTPUT_NAME classad3_impl PDB_NAME py3classad3 )
				set_target_properties(        py3classad3_impl PROPERTIES SUFFIX "${PYTHON3_DLL_SUFFIX}" )

				install ( TARGETS py3classad DESTINATION ${C_LIB} )
				install ( TARGETS py3htcondor DESTINATION ${C_PYTHONARCHLIB}/htcondor )
				install ( TARGETS py3htcondor2_impl DESTINATION ${C_PYTHONARCHLIB}/htcondor2 )
				install ( TARGETS py3classad_module DESTINATION ${C_PYTHONARCHLIB}/classad )
				install ( TARGETS py3classad2_impl DESTINATION ${C_PYTHONARCHLIB}/classad2 )
				install ( TARGETS py3classad3_impl DESTINATION ${C_PYTHONARCHLIB}/classad3 )

			endif(DEFINED PYTHON3_VERSION_STRING) # python3x was found

		# more targets are built if we plan on building wheels
		if ( WANT_PYTHON_WHEELS )
			message(STATUS "Setting up python wheels on Windows is not yet supported")
		endif()
      endif()
    endif(NOT (MSVC_VERSION LESS 1700))
  endif()
else()
  if ( WITH_PYTHON_BINDINGS AND ( PYTHONLIBS_FOUND OR PYTHON3LIBS_FOUND ) )
    configure_file (
      "${PROJECT_SOURCE_DIR}/src/python-bindings/test_driver.in"
      "${CMAKE_CURRENT_BINARY_DIR}/test_driver"
    )

    # Modern compilers complain about using the deprecated std::auto_ptr.
    # Prevent boost from using them, except on Debian 9, where a compile
    # time error occurs
    if ( NOT ${PLATFORM} MATCHES "Debian.*9" )
      add_definitions ( -DBOOST_NO_AUTO_PTR )
    endif()

    # In clang 15 on macOS, support for std::unary_function is removed
    # (since it's gone in C++17). The older boost versions don't
    # detect this automatically, but setting _HAS_AUTO_PTR_ETC=0 will
    # cause them to avoid using std::unary_function.
    if (APPLE)
      add_definitions ( -D_HAS_AUTO_PTR_ETC=0 )
    endif()

    if (APPLE)
        # This is probably a sign that something else needs fixing.
        # Note that you can NOT set this to DARWIN, because CMake's parser
        # is terrible.  (It will silently dereference the quoted string
        # "DARWIN" as the variable in some situations.)
        set(SYSTEM_NAME "darwin")
    endif()

    if (DEFINED PYTHON_VERSION_STRING AND PYTHONLIBS_FOUND)
      if (NOT DEFINED PYTHON_BOOST_LIB )
        set ( PYTHON_BOOST_LIB boost_python )
        if (DEFINED SYSTEM_NAME)
          if (${SYSTEM_NAME} MATCHES "Debian" OR ${SYSTEM_NAME} MATCHES "Ubuntu")
            set ( PYTHON_BOOST_LIB "boost_python-py${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}" )
          endif()
          if (${SYSTEM_NAME} MATCHES "rhel8" OR ${SYSTEM_NAME} MATCHES "centos8" OR ${SYSTEM_NAME} MATCHES "rocky8" OR ${SYSTEM_NAME} MATCHES "almalinux8")
            set ( PYTHON_BOOST_LIB "boost_python${PYTHON_VERSION_MAJOR}" )
          endif()
          if (${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7" OR ${SYSTEM_NAME} MATCHES "amzn2" OR ${SYSTEM_NAME} MATCHES "fc3[0-9]" OR ${SYSTEM_NAME} MATCHES "Debian.*10" OR WANT_PYTHON_WHEELS)
            set ( PYTHON_BOOST_LIB "boost_python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}" )
          endif()
          if (${SYSTEM_NAME} MATCHES "fc2[0-9]" )
            set ( PYTHON_BOOST_LIB "boost_python" )
          endif()
          if (${SYSTEM_NAME} MATCHES "darwin")
              set( PYTHON_BOOST_LIB "boost_python${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}" )
          endif()
        endif()
      endif()
      include_directories(${BOOST_INCLUDE})
      link_directories(${BOOST_LD})

      get_filename_component(PYTHON_LIBRARY_FILENAME ${PYTHON_LIBRARIES} NAME)
      string(REGEX REPLACE "([0-9]+[.][0-9]+).*" "\\1" _PYTHON_VERSION ${PYTHON_VERSION_STRING})
      if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
        set( PYCLASSAD_LIB_NAME "pyclassad${_PYTHON_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
      else()
        message(FATAL_ERROR "Can't determine HTCondor version!")
      endif()
      condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h exception_utils.cpp classad_exceptions.cpp )
      target_include_directories( pyclassad PUBLIC ${PYTHON_INCLUDE_DIRS} )
      target_link_libraries( pyclassad ${CLASSADS_FOUND} ${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
      set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
      set_target_properties( pyclassad PROPERTIES COMPILE_FLAGS "-fPIC")

      # Note we do not use condor_shared_lib below because we want a separate install target.
      add_library( classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
      target_include_directories( classad_module PUBLIC ${PYTHON_INCLUDE_DIRS} )
      target_link_libraries( classad_module pyclassad ${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
      set_target_properties(classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )
      set_target_properties(classad_module PROPERTIES SUFFIX "${PYTHON_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(classad_module PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
      endif()

      # This is to be used as a CLASSAD_USER_LIBS extension so we can invoke
      # python functions.
      add_library( classad_python_user SHARED classad_python_user.cpp )
      target_include_directories( classad_python_user PUBLIC ${PYTHON_INCLUDE_DIRS} )
      target_link_libraries( classad_python_user condor_utils pyclassad )
      set_target_properties(classad_python_user PROPERTIES SUFFIX "${PYTHON_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(classad_python_user PROPERTIES INSTALL_RPATH "${CONDOR_RPATH}")
      endif()
      install ( TARGETS classad_python_user DESTINATION "${C_LIBEXEC}" )

      # See note below about deprecating log_reader.cpp.
      set_source_files_properties( dc_tool.cpp classad.cpp classad_module.cpp classad_parsers.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp PROPERTIES COMPILE_FLAGS "-Wno-strict-aliasing -Wno-cast-qual -Wno-deprecated -Wno-write-strings")
      # We should probably deprecate log_reader.cpp (shortly after we deprecate event.cpp).
      add_library( htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp )
      target_include_directories( htcondor PUBLIC ${PYTHON_INCLUDE_DIRS} )
      target_link_libraries( htcondor pyclassad condor_utils ${PYTHON_BOOST_LIB} ${PYTHON_LIBRARIES} )
      set_target_properties( htcondor PROPERTIES PREFIX "" )
      set_target_properties( htcondor PROPERTIES SUFFIX "${PYTHON_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(htcondor PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
      endif()

      install ( TARGETS htcondor DESTINATION ${C_PYTHONARCHLIB}/htcondor )
      install ( TARGETS classad_module DESTINATION ${C_PYTHONARCHLIB}/classad )

      if ( APPLE )
          set_target_properties( pyclassad PROPERTIES INSTALL_NAME_DIR ${CMAKE_CURRENT_BINARY_DIR} )

          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB}/lib${PYCLASSAD_LIB_NAME}.dylib)" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHONARCHLIB}/classad/classad${PYTHON_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHONARCHLIB}/htcondor/htcondor${PYTHON_MODULE_SUFFIX})" )

          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad2/classad2_impl${PYTHON3_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/htcondor2/htcondor2_impl${PYTHON3_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad3/classad3_impl${PYTHON3_MODULE_SUFFIX})" )
      endif()
    endif()

    if (APPLE)
        # This is probably a sign that something else needs fixing.
        # Note that you can NOT set this to DARWIN, because CMake's parser
        # is terrible.  (It will silently dereference the quoted string
        # "DARWIN" as the variable in some situations.)
        set(SYSTEM_NAME "darwin")
    endif()

    if (DEFINED PYTHON3_VERSION_STRING AND PYTHON3LIBS_FOUND AND NOT ${SYSTEM_NAME} MATCHES "fc27")
      if (NOT DEFINED PYTHON3_BOOST_LIB)
        if ( WANT_PYTHON_WHEELS )
          set ( PYTHON3_BOOST_LIB "boost_python${PYTHON3_VERSION_MAJOR}${PYTHON3_VERSION_MINOR}" )
        elseif (${SYSTEM_NAME} MATCHES "rhel8" OR ${SYSTEM_NAME} MATCHES "centos8" OR ${SYSTEM_NAME} MATCHES "rocky8" OR ${SYSTEM_NAME} MATCHES "almalinux8")
          set ( PYTHON3_BOOST_LIB "boost_python${PYTHON3_VERSION_MAJOR}" )
        elseif (${SYSTEM_NAME} MATCHES "Ubuntu.*18")
          set ( PYTHON3_BOOST_LIB "boost_python-py${PYTHON3_VERSION_MAJOR}${PYTHON3_VERSION_MINOR}" )
        elseif (${SYSTEM_NAME} MATCHES "openSUSE")
          set ( PYTHON3_BOOST_LIB "boost_python${PYTHON3_VERSION_MAJOR}" )
        else ()
          set ( PYTHON3_BOOST_LIB "boost_python${PYTHON3_VERSION_MAJOR}${PYTHON3_VERSION_MINOR}" )
        endif()
      endif()
      include_directories(${BOOST_INCLUDE})
      link_directories(${BOOST_LD})

      string(REGEX REPLACE "([0-9]+[.][0-9]+).*" "\\1" _PYTHON3_VERSION ${PYTHON3_VERSION_STRING})
      if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
          set( PY3CLASSAD_LIB_NAME "pyclassad${_PYTHON3_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
      else()
        message(FATAL_ERROR "Can't determine HTCondor version!")
      endif()
      condor_shared_lib( py3classad classad.cpp classad_wrapper.h exprtree_wrapper.h exception_utils.cpp classad_exceptions.cpp )
      target_include_directories( py3classad PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries( py3classad ${CLASSADS_FOUND} ${PYTHON3_BOOST_LIB} ${PYTHON3_LIBRARIES} )
      set_target_properties( py3classad PROPERTIES OUTPUT_NAME "${PY3CLASSAD_LIB_NAME}" )
      set_target_properties( py3classad PROPERTIES COMPILE_FLAGS "-fPIC")

      # Note we do not use condor_shared_lib below because we want a separate install target.
      add_library( py3classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
      target_include_directories( py3classad_module PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries( py3classad_module py3classad ${PYTHON3_BOOST_LIB} ${PYTHON3_LIBRARIES} )
      set_target_properties(py3classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )
      set_target_properties(py3classad_module PROPERTIES SUFFIX "${PYTHON3_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(py3classad_module PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
      endif()

      # This is to be used as a CLASSAD_USER_LIBS extension so we can invoke
      # python functions.
      add_library( classad_python3_user SHARED classad_python_user.cpp )
      target_include_directories( classad_python3_user PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries( classad_python3_user condor_utils py3classad )
      set_target_properties( classad_python3_user PROPERTIES OUTPUT_NAME classad_python_user )
      set_target_properties( classad_python3_user PROPERTIES SUFFIX "${PYTHON3_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties( classad_python3_user PROPERTIES INSTALL_RPATH "${CONDOR_RPATH}" )
      endif()
      install ( TARGETS classad_python3_user DESTINATION "${C_LIBEXEC}" )
      add_custom_command( TARGET classad_python3_user POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E create_symlink "libclassad_python_user${PYTHON3_MODULE_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/libclassad_python3_user.so"
          COMMENT "Creating libclassad_python3_user.so symlink" )
      install ( FILES "${CMAKE_CURRENT_BINARY_DIR}/libclassad_python3_user.so" DESTINATION "${C_LIBEXEC}" )

      # See note below about deprecating log_reader.cpp.
      set_source_files_properties( dc_tool.cpp classad.cpp classad_module.cpp classad_parsers.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp PROPERTIES COMPILE_FLAGS "-Wno-strict-aliasing -Wno-cast-qual -Wno-deprecated -Wno-write-strings")
      # We should probably deprecate log_reader.cpp (shortly after we deprecate event.cpp).
      add_library( py3htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp )
      target_include_directories( py3htcondor PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries( py3htcondor py3classad condor_utils ${PYTHON3_BOOST_LIB} ${PYTHON3_LIBRARIES} )
      set_target_properties( py3htcondor PROPERTIES PREFIX "" OUTPUT_NAME htcondor )
      set_target_properties( py3htcondor PROPERTIES SUFFIX "${PYTHON3_MODULE_SUFFIX}" )
      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(py3htcondor PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
      endif()


      set( WHEEL_BINARY_DIR "${CMAKE_BINARY_DIR}/bindings/python" )

      set( MVI_SUFFIX ".abi3.so" )
      if (APPLE)
        set( MVI_SUFFIX ".abi3.dylib" )
        set( MAGIC_LINKER_FLAG "-undefined dynamic_lookup" )
      endif ()


      add_library(                  py3classad2_impl SHARED classad2_impl.cpp )
      target_include_directories(   py3classad2_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries(        py3classad2_impl classad condor_utils ${MAGIC_LINKER_FLAG} )
      set_target_properties(        py3classad2_impl PROPERTIES SUFFIX ${MVI_SUFFIX} )
      set_target_properties(        py3classad2_impl PROPERTIES PREFIX "" OUTPUT_NAME classad2_impl )
      set_target_properties(        py3classad2_impl PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${WHEEL_BINARY_DIR}/classad2" )

      add_library(                  py3htcondor2_impl SHARED htcondor2_impl.cpp )
      target_include_directories(   py3htcondor2_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries(        py3htcondor2_impl classad condor_utils ${MAGIC_LINKER_FLAG} )
      set_target_properties(        py3htcondor2_impl PROPERTIES SUFFIX ${MVI_SUFFIX} )
      set_target_properties(        py3htcondor2_impl PROPERTIES PREFIX "" OUTPUT_NAME htcondor2_impl )
      set_target_properties(        py3htcondor2_impl PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${WHEEL_BINARY_DIR}/htcondor2" )


      add_library(                  py3classad3_impl SHARED classad3_impl.cpp )
      target_include_directories(   py3classad3_impl PUBLIC ${PYTHON3_INCLUDE_DIRS} )
      target_link_libraries(        py3classad3_impl classad condor_utils ${PYTHON3_LIBRARIES} )
      set_target_properties(        py3classad3_impl PROPERTIES PREFIX "" OUTPUT_NAME classad3_impl )
      set_target_properties(        py3classad3_impl PROPERTIES SUFFIX "${PYTHON3_MODULE_SUFFIX}" )


      if ( NOT CMAKE_SKIP_RPATH )
        set_target_properties(py3classad2_impl PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
        set_target_properties(py3htcondor2_impl PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
        set_target_properties(py3classad3_impl PROPERTIES INSTALL_RPATH "${PYTHON_RPATH}")
      endif()


      install ( TARGETS py3htcondor DESTINATION ${C_PYTHON3ARCHLIB}/htcondor )
      install ( TARGETS py3classad_module DESTINATION ${C_PYTHON3ARCHLIB}/classad )
      # These are now built in the right place (for the wheels).
      # install ( TARGETS py3htcondor2_impl DESTINATION ${C_PYTHON3ARCHLIB}/htcondor2 )
      # install ( TARGETS py3classad2_impl DESTINATION ${C_PYTHON3ARCHLIB}/classad2 )
      install ( TARGETS py3classad3_impl DESTINATION ${C_PYTHON3ARCHLIB}/classad3 )


      if ( APPLE )
          set_target_properties( py3classad PROPERTIES INSTALL_NAME_DIR ${CMAKE_CURRENT_BINARY_DIR} )

          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB}/lib${PY3CLASSAD_LIB_NAME}.dylib)" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad/classad${PYTHON3_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/htcondor/htcondor${PYTHON3_MODULE_SUFFIX})" )

          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad2/classad2_impl${PYTHON3_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/htcondor2/htcondor2_impl${PYTHON3_MODULE_SUFFIX})" )
          install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad/3classad3_impl${PYTHON3_MODULE_SUFFIX})" )
      endif()
    endif()


    # more targets are built if we plan on building wheels
    if ( WANT_PYTHON_WHEELS )
      include_directories(${PYTHON3_INCLUDE_DIRS} ${BOOST_INCLUDE})

      # the pyclassad library and the classad and htcondor modules should not link libpython
      # instead, the user's python interpreter will look for any such missing symbols
      if ( APPLE )
        # in macOS, use "-undefined dynamic_lookup" in place of "-lpython" or "-framework Python" (i.e. ${PYTHON_LIBRARIES})
        set( NOLINK_PYTHON_LIB "-undefined dynamic_lookup" )
      else()
        # in Linux, simply omit "-lpython" (i.e. ${PYTHON_LIBRARIES}) to get the same behavior
        set( NOLILNK_PYTHON_LIB "" )
      endif()

      # build libpyclassad *not* linked against libpython
      add_library( wheel_pyclassad SHARED classad.cpp classad_wrapper.h exprtree_wrapper.h exception_utils.cpp classad_exceptions.cpp )
      target_link_libraries( wheel_pyclassad ${CLASSADS_FOUND} ${PYTHON3_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      if ( APPLE )
        set_target_properties( wheel_pyclassad PROPERTIES SUFFIX ".dylib" )
        set_target_properties( wheel_pyclassad PROPERTIES INSTALL_NAME_DIR "${WHEEL_BINARY_DIR}/htcondor/htcondor_libs" )
      else()
        set_target_properties( wheel_pyclassad PROPERTIES SUFFIX ".so" )
        set_target_properties( wheel_pyclassad PROPERTIES INSTALL_RPATH "$ORIGIN" )
      endif()
      set_target_properties( wheel_pyclassad PROPERTIES PREFIX "lib" OUTPUT_NAME "${PYCLASSAD_LIB_NAME}_wheel" )
      set_target_properties( wheel_pyclassad PROPERTIES COMPILE_FLAGS "-fPIC" )

      # next, build the classad module *not* linked against libpython
      add_library( wheel_classad_module SHARED classad_module.cpp classad_module_impl.cpp classad_parsers.cpp )
      target_link_libraries( wheel_classad_module wheel_pyclassad ${PYTHON3_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      set_target_properties( wheel_classad_module PROPERTIES PREFIX "" OUTPUT_NAME "classad" SUFFIX ".so")
      set_target_properties( wheel_classad_module PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${WHEEL_BINARY_DIR}/classad" )
      if ( NOT APPLE )
        set_target_properties( wheel_classad_module PROPERTIES INSTALL_RPATH "$ORIGIN/../htcondor/htcondor_libs" )
      endif()

      # finally, build the htcondor module *not* linked against libpython
      # See note above about deprecating log_reader.cpp.
      add_library( wheel_htcondor SHARED htcondor.cpp ${HTCONDOR_BINDINGS_SOURCES} log_reader.cpp )
      target_link_libraries( wheel_htcondor wheel_pyclassad condor_utils ${PYTHON3_BOOST_LIB} ${NOLINK_PYTHON_LIB} )
      set_target_properties( wheel_htcondor PROPERTIES PREFIX "" OUTPUT_NAME "htcondor" SUFFIX ".so")
      set_target_properties( wheel_htcondor PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${WHEEL_BINARY_DIR}/htcondor" )
      if ( NOT APPLE )
        set_target_properties( wheel_htcondor PROPERTIES INSTALL_RPATH "$ORIGIN/htcondor_libs" )
      endif()

    endif()
  endif ( WITH_PYTHON_BINDINGS AND ( PYTHONLIBS_FOUND OR PYTHON3LIBS_FOUND ) )
endif(WINDOWS)
