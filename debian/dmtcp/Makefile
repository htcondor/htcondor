DMTCP_BIN = /scratch/psilord/local/bin
DMTCP_LIB = /scratch/psilord/local/lib/dmtcp

DMTCP_FILES = $(DMTCP_BIN)/dmtcp_checkpoint \
	$(DMTCP_BIN)/dmtcp_command \
	$(DMTCP_BIN)/dmtcp_coordinator \
	$(DMTCP_BIN)/dmtcp_restart \
	$(DMTCP_LIB)/dmtcphijack.so \
	$(DMTCP_LIB)/libmtcp.so \
	$(DMTCP_LIB)/mtcp_restart

all: refresh foo

foo: foo.c
	gcc -Wall -g foo.c -o foo

submit:
	condor_submit job.sub
	condor_submit system_job.sub

analyze:
	@echo "Jobs which started from initial state more than once!"
	@grep -c '                                        -' shim_dmtcp*log | sed -e 's/:/ /g' | awk '{print $$2 " " $$1}' | sort -n | egrep -v '^1 '
	@echo "Which job output files don't match the template?"
	@for i in `echo output_file.*.out`; do if ! `/usr/bin/cmp $$i out.template`; then echo "$$i corrupt!"; diff $$i out.template; fi; done

refresh: $(DMTCP_FILES)
	cp $(DMTCP_FILES) .
	touch refresh

clean:
	rm -f *.log *.out *.err *.dmtcp jassertlog.* dmtcpConTable.* foo dmtcp_restart_script.sh dmtcpPid* output_file core.* dmtcp_checkpoint dmtcp_command dmtcp_coordinator dmtcp_restart dmtcphijack.so libmtcp.so mtcp_restart refresh proc-maps* backtrace* dmtcpShmidList* dmtcpShmidMap* dmtcp_restart_script*

