.PHONY: clean

all: cftp_suite cfc_suite

protocol_lib = lib/cftp_protocol.a
transfer_tool_server = bin/server
transfer_tool_client = bin/client

cfcnode = bin/cfcnode
file_finder = bin/file_finder

cftp_suite: $(transfer_tool_server) $(transfer_tool_client)

cfc_suite: $(cfcnode) $(file_finder)


$(protocol_lib):
	cd protocol; make
	mkdir -p lib
	cp protocol/cftp_protocol.a lib/cftp_protocol.a

$(transfer_tool_server): $(protocol_lib)
	cd transfer_tool; make server
	mkdir -p bin
	cp transfer_tool/server bin/cftp_server

$(transfer_tool_client): $(protocol_lib)
	cd transfer_tool; make client
	mkdir -p bin
	cp transfer_tool/client bin/cftp_client

$(cfcnode):
	cd cfc_tool; make cfcnode
	mkdir -p bin
	cp cfc_tool/cfcnode bin/cfcnode

$(file_finder):
	cd cfc_tool; make file_finder
	mkdir -p bin
	cp cfc_tool/file_finder bin/file_finder

clean:
	cd protocol; make clean
	cd transfer_tool; make clean
	cd cfc_tool; make clean
	rm -rf bin/*
	rm -rf lib/*