	.space	$TEXT$
	.subspa $CODE$
	.export get_reginfo
	.proc
	.callinfo no_calls
get_reginfo
	.entry
	mfsp    sr0,r22
	stwm    r22,4(arg0)
	mfsp    sr1,r22
	stwm    r22,4(arg0)
	mfsp    sr2,r22
	stwm    r22,4(arg0)
	mfsp    sr3,r22
	stwm    r22,4(arg0)
	mfsp    sr4,r22
	stwm    r22,4(arg0)
	mfsp    sr5,r22
	stwm    r22,4(arg0)
	mfsp    sr6,r22
	stwm    r22,4(arg0)
	mfsp    sr7,r22
	stwm    r22,4(arg0)
	bv,n	r0(rp)			;return
	nop
	.procend

	.end
