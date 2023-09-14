
# These are expanded in setup.py.cmake_template
set(PACKAGES      "classad" "htcondor" "htcondor/dags" "htcondor/htchirp" "htcondor/compat_enum" "htcondor/_utils")

# The python sources to copy
set(PYTHON_SOURCES
htcondor/compat_enum/__init__.py
htcondor/compat_enum/LICENSE
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
)

set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(INSTALL_SH    "${CMAKE_CURRENT_SOURCE_DIR}/install.sh")

if (WITH_PYTHON_BINDINGS)
    set(DEPS "")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})

	# Make the target directories for python source tree
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/dags)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/htchirp)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/compat_enum)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/htcondor/_utils)
	file (MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classad)

	# Now make a command to copy each source file to the destination
	foreach(PYTHON_SOURCE_FILE ${PYTHON_SOURCES})
		add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}"
			COMMAND ${CMAKE_COMMAND} ARGS -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_SOURCE_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_SOURCE_FILE}"
    )
	list(APPEND DEPS "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_SOURCE_FILE}")
    endforeach()

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
            DEPENDS ${DEPS} py3classad_module py3htcondor
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
    endif()

endif()

if (NOT WINDOWS AND WANT_PYTHON_WHEELS)
    add_custom_target(bdist_wheel ALL DEPENDS python_bindings wheel_classad_module wheel_htcondor COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} bdist_wheel)
endif()
