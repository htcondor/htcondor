all: random.condor half.condor sum.condor

random.condor: random.c
	condor_compile gcc -o random.condor random.c

half.condor: half.c
	condor_compile gcc -o half.condor half.c

sum.condor: sum.c
	condor_compile gcc -o sum.condor sum.c

clean:
	rm -f *.out *.err *.log diamond.dag.* *.condor *~ 