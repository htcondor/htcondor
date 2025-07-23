cmake_minimum_required(VERSION 3.14)
project(testbase)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Add Tests Here:

# test_readHistory
add_executable(
  test_readHistory
  tests/test_readHistory.cpp
  src/readHistory.cpp
  src/archiveMonitor.cpp
  src/dbHandler.cpp
  lib/sha256.c
)

# ArchiveMonitor has changed drastically and can be best tested by functional testing
# test_archiveMonitor
# add_executable(
#   test_archiveMonitor
#   tests/test_archiveMonitor.cpp
#   src/archiveMonitor.cpp
#   src/dbHandler.cpp
#   src/dbHandlerTestUtils.cpp
#   lib/sha256.c
# )

#test_dbHandler
add_executable(
  test_dbHandler
  tests/test_dbHandler.cpp 
  src/dbHandler.cpp 
  src/archiveMonitor.cpp
  src/readHistory.cpp
  src/dbHandlerTestUtils.cpp
  lib/sha256.c
)

#test_readEpochHistory
add_executable(
  test_readEpochHistory
  tests/test_readEpochHistory.cpp
  src/readHistory.cpp
  src/archiveMonitor.cpp
  src/dbHandler.cpp
  src/dbHandlerTestUtils.cpp
  lib/sha256.c
)

#test_librarian
add_executable(
  test_librarian 
  tests/test_librarian.cpp
  src/librarian.cpp
  src/readHistory.cpp 
  src/archiveMonitor.cpp
  src/dbHandler.cpp 
  src/dbHandlerTestUtils.cpp
  lib/sha256.c
)

foreach(target IN ITEMS test_readHistory test_dbHandler test_readEpochHistory test_librarian)
    target_link_libraries(${target}
        gtest 
        gtest_main
        pthread
        sqlite3
    )
    target_compile_definitions(${target} PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/history_pool/")
    target_include_directories(${target} PRIVATE include lib)
endforeach()



include(GoogleTest)

# and here?
gtest_discover_tests(test_readHistory)
#gtest_discover_tests(test_archiveMonitor)
gtest_discover_tests(test_dbHandler)
gtest_discover_tests(test_readEpochHistory)
gtest_discover_tests(test_librarian)


# ---- Main driver (non-test) ----
add_executable(
  librarian
  src/main.cpp
  src/librarian.cpp
  src/readHistory.cpp
  src/archiveMonitor.cpp
  src/dbHandler.cpp
  lib/sha256.c
)

target_link_libraries(librarian
  pthread
  sqlite3
)

target_include_directories(librarian PRIVATE include lib)