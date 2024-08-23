
# These are expanded in setup.py.cmake_template
set(PACKAGES      "classad3" "classad2" "classad" "htcondor2" "htcondor" "htcondor/dags" "htcondor/htchirp" "htcondor/_utils")

# The python sources to copy
set(PYTHON_SOURCES
htcondor/_wrap.py
htcondor/__init__.py
htcondor/_lock.py
htcondor/personal.py
htcondor/htchirp/__init__.py
htcondor/htchirp/htchirp.py
htcondor/dags/writer.py
htcondor/dags/exceptions.py
htcondor/dags/__init__.py
htcondor/dags/formatter.py
htcondor/dags/dag.py
htcondor/dags/walk_order.py
htcondor/dags/edges.py
htcondor/dags/utils.py
htcondor/dags/rescue.py
htcondor/dags/node.py
htcondor/_deprecation.py
htcondor/_job_status.py

classad/__init__.py

# Private python utilities
htcondor/_utils/__init__.py
htcondor/_utils/ansi.py

# Version 2.
htcondor2/_negotiator.py
htcondor2/_collector.py
htcondor2/_startd.py
htcondor2/_credd.py
htcondor2/_cred_check.py
htcondor2/_cred_type.py
htcondor2/_subsystem_type.py
htcondor2/_daemon_type.py
htcondor2/_ad_type.py
htcondor2/_daemon_command.py
htcondor2/_drain_type.py
htcondor2/_completion_type.py
htcondor2/_common_imports.py
htcondor2/_schedd.py
htcondor2/_query_opt.py
htcondor2/_job_action.py
htcondor2/_transaction_flag.py
htcondor2/_submit.py
htcondor2/_submit_method.py
htcondor2/_submit_result.py
htcondor2/_job_event.py
htcondor2/_job_event_log.py
htcondor2/_job_event_type.py
htcondor2/_job_status.py
htcondor2/_param.py
htcondor2/_remote_param.py
htcondor2/_file_transfer_event_type.py
htcondor2/_logging.py
htcondor2/_log_level.py
htcondor2/_loose_functions.py
htcondor2/__init__.py

# This can't be the best way to do this.
htcondor2/personal.py
htcondor2/htchirp/__init__.py
htcondor2/htchirp/htchirp.py
htcondor2/dags/writer.py
htcondor2/dags/exceptions.py
htcondor2/dags/__init__.py
htcondor2/dags/formatter.py
htcondor2/dags/dag.py
htcondor2/dags/walk_order.py
htcondor2/dags/edges.py
htcondor2/dags/utils.py
htcondor2/dags/rescue.py
htcondor2/dags/node.py

classad2/__init__.py
classad2/_class_ad.py
classad2/_value.py
classad2/_expr_tree.py
classad2/_parser_type.py

# Version 3
classad3/__init__.py
classad3/ClassAd.py
classad3/Expression.py

)

set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(INSTALL_SH    "${CMAKE_CURRENT_SOURCE_DIR}/install.sh")

if (WITH_PYTHON_BINDINGS)
    set(DEPS "")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})

	# Make the target directories for python source tree
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor2)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/dags)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/htchirp)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/_utils)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classad)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classad2)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classad3)

	# Now make a command to copy each source file to the destination
	if (NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")

		foreach(PYTHON_SOURCE_FILE ${PYTHON_SOURCES})
			add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}"
				COMMAND ${CMAKE_COMMAND} ARGS -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_SOURCE_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}"
				DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_SOURCE_FILE}"
			)
			list(APPEND DEPS "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}")
		endforeach()
	endif()

    if (WINDOWS AND DEFINED PYTHON_VERSION_STRING)
        list(APPEND DEPS classad_module htcondor)
    endif()

    if (WINDOWS AND DEFINED PYTHON3_VERSION_STRING)
        list(APPEND DEPS py3classad_module py3htcondor)
        if (NOT DEFINED PYTHON_VERSION_STRING) # make sure Windows has a Python executable even if building Python 3 only
            set(PYTHON_EXECUTABLE ${PYTHON3_EXECUTABLE})
        endif()
    endif()

    if (PYTHONLIBS_FOUND OR (WINDOWS AND PYTHON3LIBS_FOUND))
        set(TIMESTAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")
        if (WINDOWS) # build scaffolding for both Python 2 and 3 on Windows
            add_custom_command(
                OUTPUT ${TIMESTAMP_FILE}
                COMMAND "${PYTHON_EXECUTABLE}" ${SETUP_PY} build
                COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP_FILE}
                DEPENDS ${DEPS}
            )
        else() # build scaffolding for Python 2 only on non-Windows
            add_custom_command(
                OUTPUT ${TIMESTAMP_FILE}
                COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
        	    COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP_FILE}
                DEPENDS ${DEPS} classad_module htcondor
            )
        endif()
        add_custom_target(python_bindings ALL DEPENDS ${TIMESTAMP_FILE})

        if (WINDOWS) # Windows can call setup.py as normal
            install(CODE "execute_process(
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMAND \"${PYTHON_EXECUTABLE}\" ${SETUP_PY} install --root=${CMAKE_INSTALL_PREFIX} --install-lib=${C_PYTHONARCHLIB}
                ERROR_VARIABLE _err
                RESULT_VARIABLE _ret)
                if (NOT \${_ret} EQUAL \"0\")
                    message( FATAL_ERROR \"\${_err}\" )
                endif()
            ")
        else() # non-Windows should call a script that invokes setup.py
            install(CODE "execute_process(
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMAND ${INSTALL_SH} ${PYTHON_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHONARCHLIB}
                ERROR_VARIABLE _err
                RESULT_VARIABLE _ret)
                if (NOT \${_ret} EQUAL \"0\")
                    message( FATAL_ERROR \"\${_err}\" )
                endif()
            ")
        endif()
    endif()

    # build Python 3 scaffolding on non-Windows
    if (DARWIN)
        # This is probably a sign that something else needs fixing.
        # Note that value shoud NOT be the same as the variable name.
        set(SYSTEM_NAME "darwin")
    endif()

    if (NOT WINDOWS AND PYTHON3LIBS_FOUND)
        set(TIMESTAMP3_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp3")
        add_custom_command(
            OUTPUT ${TIMESTAMP3_FILE}
            COMMAND ${PYTHON3_EXECUTABLE} ${SETUP_PY} build
            COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP3_FILE}
            DEPENDS ${DEPS} py3classad_module py3htcondor py3classad3_impl py3htcondor2_impl py3classad2_impl
        )

        add_custom_target(python3_bindings ALL DEPENDS ${TIMESTAMP3_FILE})
        install(CODE "execute_process(
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMAND ${INSTALL_SH} ${PYTHON3_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHON3ARCHLIB}
                ERROR_VARIABLE _err
                RESULT_VARIABLE _ret)
                if (NOT \${_ret} EQUAL \"0\")
                    message( FATAL_ERROR \"\${_err}\" )
                endif()
        ")
        if (APPLE)
            set( MVI_SUFFIX ".abi3.so" )
            install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/classad2/classad2_impl${MVI_SUFFIX})" )
            install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_PYTHON3ARCHLIB}/htcondor2/htcondor2_impl${MVI_SUFFIX})" )
        endif()
    endif()

endif()

if (NOT WINDOWS AND WANT_PYTHON_WHEELS)
    add_custom_target(bdist_wheel ALL DEPENDS python_bindings wheel_classad_module wheel_htcondor COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} bdist_wheel)
endif()
