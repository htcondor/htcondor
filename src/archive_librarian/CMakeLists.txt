cmake_minimum_required(VERSION 3.14)
project(testbase)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add debug flags for better debugging
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address")

# Main driver executable
add_executable(
  myHistoryList
  main.cpp
  librarian.cpp
  readHistory.cpp
  archiveMonitor.cpp
  dbHandler.cpp
  JobAnalysisUtils.cpp
  sha256.c
)

target_link_libraries(myHistoryList
  pthread
  sqlite3
)

# Custom clean target to remove executable and database files
add_custom_target(clean-all
  COMMAND ${CMAKE_COMMAND} -E remove -f myHistoryList
  COMMAND ${CMAKE_COMMAND} -E remove -f tester_output.db
  COMMAND ${CMAKE_COMMAND} -E remove -f tester_output.db-wal  
  COMMAND ${CMAKE_COMMAND} -E remove -f tester_output.db-shm
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Cleaning executable and database files"
)