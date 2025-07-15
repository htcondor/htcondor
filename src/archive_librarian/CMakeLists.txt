cmake_minimum_required(VERSION 3.14)
project(archive_librarian)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  main.cpp
  librarian.cpp
  readHistory.cpp
  archiveMonitor.cpp
  dbHandler.cpp
  sha256.c
)

add_executable(librarian ${SOURCES})

target_include_directories(librarian PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}  
)

target_link_libraries(librarian
  pthread
  sqlite3
)
