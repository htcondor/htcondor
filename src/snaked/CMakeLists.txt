set( SNAKED_SOURCES
    snaked.cpp
    snake.cpp
)

condor_daemon( EXE condor_snaked
               SOURCES "${SNAKED_SOURCES}"
               LIBRARIES "${CONDOR_LIBS};${PYTHON3_LIBRARIES}"
               INSTALL "${C_SBIN}"
)

target_include_directories( condor_snaked
     PUBLIC ${PYTHON3_INCLUDE_DIRS}
)
