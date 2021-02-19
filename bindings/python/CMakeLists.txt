set(PACKAGES      "classad" "htcondor" "htcondor/dags" "htcondor/htchirp" "htcondor/compat_enum")
set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(INSTALL_SH    "${CMAKE_CURRENT_SOURCE_DIR}/install.sh")

if (WITH_PYTHON_BINDINGS)
    set(DEPS "")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})

    # Copy over module directories if __init__.py is missing, i.e. out of
    # source build.  This is broken (see the remove-file hack below)
    foreach(PACKAGE ${PACKAGES})
        set(PKG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
        add_custom_command(OUTPUT "${PKG_DIR}/__init__.py"
            COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}"
        )
        list(APPEND DEPS "${PKG_DIR}/__init__.py")
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

    if (PYTHONLIBS_FOUND)
        set(TIMESTAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")
        if (WINDOWS) # build scaffolding for both Python 2 and 3 on Windows
            add_custom_command(
                OUTPUT ${TIMESTAMP_FILE}
                COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
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

		# This is an awful hack, but it's easier than fixing than this file.
		add_custom_command(TARGET python_bindings POST_BUILD
			COMMAND ${CMAKE_COMMAND} ARGS -E remove -f "${CMAKE_CURRENT_BINARY_DIR}/*/__init__.py"
			)

        if (WINDOWS) # Windows can call setup.py as normal
            install(CODE "execute_process(
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=${CMAKE_INSTALL_PREFIX} --install-lib=${C_PYTHONARCHLIB}
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

    if (NOT WINDOWS AND PYTHON3LIBS_FOUND AND NOT ${SYSTEM_NAME} MATCHES "fc27")
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
