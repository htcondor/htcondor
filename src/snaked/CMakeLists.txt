set( SNAKED_SOURCES
    snaked.cpp
    snake.cpp
)

# condor_daemon is broken and requires that its LIBRARIES variable
# not contain any Windows-style paths.
string(REGEX REPLACE "(\\\\)" "/" HACK ${PYTHON3_LIBRARIES})

condor_daemon( EXE condor_snaked
               SOURCES "${SNAKED_SOURCES}"
               LIBRARIES "${CONDOR_LIBS};${HACK}"
               INSTALL "${C_SBIN}"
)

target_include_directories( condor_snaked
     PUBLIC ${PYTHON3_INCLUDE_DIRS}
)
