set(PACKAGES      "classad" "htcondor")
set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(INSTALL_SH    "${CMAKE_CURRENT_SOURCE_DIR}/install.sh")

if (WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND)

  configure_file(${SETUP_PY_IN} ${SETUP_PY})

  # Copy over module directories if __init__.py is missing i.e. out of source build
  set(DEPS "")
  foreach(PACKAGE ${PACKAGES})
    set(PKG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    add_custom_command(OUTPUT "${PKG_DIR}/__init__.py"
      COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    list(APPEND DEPS "${PKG_DIR}/__init__.py")
  endforeach()

  if (WINDOWS AND DEFINED PYTHON3_VERSION_STRING)
    # Windows can put both Python 2 and 3 libraries in the same folder (C_PYTHONARCHLIB)
    list(APPEND DEPS py3classad_module py3htcondor)
  endif()
  
  if (DEFINED PYTHON_VERSION_STRING)
    set(TIMESTAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")   
    add_custom_command(
      OUTPUT ${TIMESTAMP_FILE}
      COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
      COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP_FILE}
      DEPENDS ${DEPS} classad_module htcondor)

    add_custom_target(python_bindings ALL DEPENDS ${TIMESTAMP_FILE})
    if (WINDOWS)
      install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=${CMAKE_INSTALL_PREFIX} --install-lib=${C_PYTHONARCHLIB})")
    else()
      install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${INSTALL_SH} ${PYTHON_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHONARCHLIB})")
    endif()
  endif()
  
  if (NOT WINDOWS AND DEFINED PYTHON3_VERSION_STRING AND NOT ${SYSTEM_NAME} MATCHES "fc27")
    set(TIMESTAMP3_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp3")   
    add_custom_command(
      OUTPUT ${TIMESTAMP3_FILE}
      COMMAND ${PYTHON3_EXECUTABLE} ${SETUP_PY} build
      COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP3_FILE}
      DEPENDS ${DEPS} py3classad_module py3htcondor)
    
    add_custom_target(python3_bindings ALL DEPENDS ${TIMESTAMP3_FILE})
    install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${INSTALL_SH} ${PYTHON3_EXECUTABLE} ${SETUP_PY} ${CMAKE_INSTALL_PREFIX} ${C_PYTHON3ARCHLIB})")
  endif()

  
endif()

if (WANT_PYTHON_WHEELS)
  add_custom_target(bdist_wheel ALL DEPENDS python_bindings wheel_classad_module wheel_htcondor COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} bdist_wheel)
endif()
