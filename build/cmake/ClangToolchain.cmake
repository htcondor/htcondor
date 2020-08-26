# To configure cmake to build with clang, run

# ./configure_uw -DCMAKE_TOOLCHAIN_FILE=build/cmake/ClangToolchain.cmake <usual options>
# this is then read before anything else, which means that these compiler
# definitions are used for feature detection, etc.

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# clang++ is abi compatible with g++'s C++ library, libstdc++, or it can
# use it's own c++ library, libc++.  It can be configured to use either by
# default.  If you want to use the other, and it is installed, you can set
# here either -stdlib=libstdc++ for the gnu one or -stdlib=libc++ for clang

#set(CMAKE_CXX_FLAGS "-stdlib=libstdc++")
#set(CMAKE_CXX_FLAGS "-stdlib=libc++")

