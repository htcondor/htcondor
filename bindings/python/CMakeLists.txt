set(PACKAGES      "classad" "htcondor")
set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(INSTALL_SH    "${CMAKE_CURRENT_SOURCE_DIR}/install.sh")

if (WITH_PYTHON_BINDINGS)

  configure_file(${SETUP_PY_IN} ${SETUP_PY})

  # Copy over module directories if __init__.py is missing i.e. out of source build
  set(DEPS "")
  foreach(PACKAGE ${PACKAGES})
    set(PKG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    add_custom_command(OUTPUT "${PKG_DIR}/__init__.py"
      COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    list(APPEND DEPS "${PKG_DIR}/__init__.py")
  endforeach()

  # Copy the scripts directory over.
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts"
    COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_SOURCE_DIR}/src/python-bindings/scripts" "${CMAKE_CURRENT_BINARY_DIR}/scripts")
  list(APPEND DEPS "${CMAKE_CURRENT_BINARY_DIR}/scripts")

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
	DEPENDS ${DEPS})
    else() # build scaffolding for Python 2 only on non-Windows
      add_custom_command(
	OUTPUT ${TIMESTAMP_FILE}
	COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
	COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP_FILE}
	DEPENDS ${DEPS} classad_module htcondor)
    endif()

    add_custom_target(python_bindings ALL DEPENDS ${TIMESTAMP_FILE})
    if (WINDOWS) # Windows can call setup.py as normal
      install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=${CMAKE_INSTALL_PREFIX} --install-lib=${C_PYTHONARCHLIB} --install-scripts=${C_BIN})")
    else() # non-Windows should call a script that invokes setup.py
      install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${INSTALL_SH} ${PYTHON_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHONARCHLIB} ${C_BIN})")
    endif()
  endif()

  # build Python 3 scaffolding on non-Windows
  if (NOT WINDOWS AND PYTHON3LIBS_FOUND AND NOT ${SYSTEM_NAME} MATCHES "fc27")
    set(TIMESTAMP3_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp3")
    add_custom_command(
      OUTPUT ${TIMESTAMP3_FILE}
      COMMAND ${PYTHON3_EXECUTABLE} ${SETUP_PY} build
      COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP3_FILE}
      DEPENDS ${DEPS} py3classad_module py3htcondor)

    add_custom_target(python3_bindings ALL DEPENDS ${TIMESTAMP3_FILE})
    # Note that if you build with both Python 2 and Python 3, the scripts
    # from this step will overwrite the scripts from the Python 2 install.
    install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${INSTALL_SH} ${PYTHON3_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHON3ARCHLIB} ${C_BIN})")
  endif()

endif()

if (NOT WINDOWS AND WANT_PYTHON_WHEELS)
  add_custom_target(bdist_wheel ALL DEPENDS python_bindings wheel_classad_module wheel_htcondor COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} bdist_wheel)
endif()
