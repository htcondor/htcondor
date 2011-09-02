.PHONY: tests clean

all: tests nodes


CPPFLAGS = -g -I../chimera -I../protocol
LDFLAGS = -L../chimera
LIBS = chimera pthread crypto db_cxx

LINKER_LIBS = $(addprefix -l, ${LIBS})

%.h.gch : %.h
	${CXX} -c $(CPPFLAGS) $< -o $@

%.o : %.cpp
	${CXX} -c $(CPPFLAGS) $< -o $@

HEADERS = filemanager.h io_access.h keys.h posix_io_access.h cfcdatabase.h \
	  node_control.h
PC_HEADERS = $(HEADERS:.h=.h.gch)

CNODE_SRC = filemanager.cpp posix_io_access.cpp keys.cpp cfcdatabase.cpp \
            node_control.cpp
CNODE_OBJ = $(CNODE_SRC:.cpp=.o)

TESTS_SRC = filemanager_test.cpp db_test.cpp node_test.cpp
TESTS_OBJ = $(TESTS_SRC:.cpp=.o)

${CNODE_OBJ} : ${PC_HEADERS}
${TESTS_OBJ} : ${CNODE_OBJ}


tests: filemanager_test db_test 

nodes: cfcnode file_finder

filemanager_test: filemanager_test.o
	${CXX} ${LDFLAGS}  $<  filemanager.o posix_io_access.o keys.o ${LINKER_LIBS} -o $@

db_test: db_test.o
	${CXX} ${LDFLAGS}  $< cfcdatabase.o  ${LINKER_LIBS} -o $@

cfcnode: cfcnode.o
	${CXX} ${LDFLAGS}  $< ${CNODE_OBJ}  ${LINKER_LIBS} -o $@

file_finder: file_finder.o
	${CXX} ${LDFLAGS}  $< ${CNODE_OBJ}  ${LINKER_LIBS} -o $@

clean:
	rm -rf ${PC_HEADERS}
	rm -rf ${CNODE_OBJ} ${TESTS_OBJ}
	rm -rf filemanager_test.o db_test.o cfcnode.o file_finder.o
	rm -rf filemanager_test db_test cfcnode file_finder