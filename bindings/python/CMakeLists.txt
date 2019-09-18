set(PACKAGES      "classad" "htcondor")
set(SETUP_PY_IN   "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.cmake_template")
set(SETUP_PY      "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
set(PACKAGE_FILES "README.rst")

if (WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND)

  set(DEPS "")
  if (WINDOWS)
    if (DEFINED PYTHON_VERSION_STRING)
      list(APPEND DEPS classad_module htcondor)
    endif()
    if (DEFINED PYTHON3_VERSION_STRING)
      list(APPEND DEPS py3classad_module py3htcondor)
    endif()
  else()
    list(APPEND DEPS classad_module htcondor)
  endif()
  
  set(TIMESTAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

  configure_file(${SETUP_PY_IN} ${SETUP_PY})

  # Copy over module directories if __init__.py is missing i.e. out of source build
  foreach(PACKAGE ${PACKAGES})
    set(PKG_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    add_custom_command(OUTPUT "${PKG_DIR}/__init__.py"
      COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}" "${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE}")
    list(APPEND DEPS "${PKG_DIR}/__init__.py")
  endforeach()

  foreach(PKG_FILE ${PACKAGE_FILES})
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PKG_FILE}"
      COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${PKG_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${PKG_FILE}")
    list(APPEND DEPS "${CMAKE_CURRENT_BINARY_DIR}/${PKG_FILE}")
  endforeach()

  add_custom_command(
    OUTPUT ${TIMESTAMP_FILE}
    COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} build
    COMMAND ${CMAKE_COMMAND} ARGS -E touch ${TIMESTAMP_FILE}
    DEPENDS ${DEPS})

  add_custom_target(python_bindings ALL DEPENDS ${TIMESTAMP_FILE})


  
  install(CODE "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} install --root=${CMAKE_INSTALL_PREFIX})")
  
endif()
