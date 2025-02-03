#ifndef _CONDOR_PRETTY_PRINT_H
#define _CONDOR_PRETTY_PRINT_H

class Lbl {
	public:
		explicit Lbl( const char * ll ) : m_lbl( ll ) {};
        operator const char * () const { return m_lbl; }

	private:
    	const char * m_lbl;
};

enum ivfield {
	BlankInvalidField = 0,
	WideInvalidField = 1,
	ShortInvalidField = 2,
	CallInvalidField = 3,   // call the formatter function to generate invalid fields.  only valid when there is a format function.
	FitToName = 4,
	FitToMachine = 8,
};

void ppInitPrintMask( ppOption pps, classad::References & proj,
	const char * & constr, bool no_pr_files );
const CustomFormatFnTable * getCondorStatusPrintFormats();
const char * digest_state_and_activity( char * sa, State st, Activity ac );

class PrettyPrinter {
	public:
		// We can't pass pm or pm_head in the constructor because the List
		// class disallows both copy and assignment for some reason.
		PrettyPrinter(
			const ppOption & _ppStyle,
			const ppOption & _ppTotalStyle,
			const printmask_headerfooter_t & _pmHeadFoot,
			bool _wide_display = false,
			bool _using_print_format = false,
			bool _wantOnlyTotals = false
		) : ppStyle( _ppStyle ), ppTotalStyle( _ppTotalStyle ),
			pmHeadFoot( _pmHeadFoot ), wide_display( _wide_display ),
			using_print_format( _using_print_format ),
			wantOnlyTotals( _wantOnlyTotals ),
			forced_display_width( 0 ),
			targetAd( NULL ),
			width_of_fixed_cols( -1 ),
			ixCol_Name( -1 ),
			ixCol_Machine( -1 ),
			max_totals_subkey( -1 ),
			startdCompact_ixCol_Platform( -1 ),
			startdCompact_ixCol_Slots( -2 ),
			startdCompact_ixCol_FreeCpus( -6 ),
			startdCompact_ixCol_FreeMem( -7 ),
			startdCompact_ixCol_ActCode( -9 ),
			startdCompact_ixCol_MaxSlotMem( -11 ),
			startdCompact_ixCol_JobStarts( -10 ),
			startdCompact_ixCol_MachineSlot( -11 ),
			startdCompact_ixCol_FreeGPUs( -12 ),
			startdCompact_ixCol_TotGPUs( -13 ),
			startdCompact_ixCol_OffGPUs( -14 ),
			startdCompact_ixCol_GPUsProps( -15 ),
			startdCompact_ixCol_GPUsPropsCaps( -16 ),
			startdCompact_ixCol_GPUsPropsMem( -17 ),
			startdCompact_ixCol_GPUsPropsName( -18 ),
			startdCompact_ixCol_GPUsPropsIds( -19 )
		{
			setby.adType = NO_AD;
			setby.argIndex = 0;
			setby.Arg = NULL;
			setby.ppArgIndex = 0;
			setby.ppArg = NULL;
		}

		int getDisplayWidth( bool * is_piped ) const;
		void setPPwidth();
		int setPPstyle( ppOption pps, int arg_index, const char * argv );
		void reportPPconflict(const char * argv, const char * more);

		void ppSetColumnFormat( const char * print, int width, bool truncate, ivfield alt, const char * attr );
		void ppSetColumnFormat( const CustomFormatFn & fmt, const char * print, int width, bool truncate, ivfield alt, const char * attr );
		void ppSetColumnFormat( const char * attr, const Lbl & label, int width, bool truncate, ivfield alt = WideInvalidField );

		void ppSetColumn( const char * attr, const Lbl & label, int width, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, int width, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, const Lbl & label, const char * print, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, const char * print, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, const Lbl & label, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, const CustomFormatFn & fmt, int width, bool truncate, ivfield alt = WideInvalidField );
		void ppSetColumn( const char * attr, const Lbl & label, const CustomFormatFn & fmt, const char * print, int width, bool truncate = true, ivfield alt = WideInvalidField );

		void ppDisplayHeadings( FILE* file, ClassAd *ad, const char * pszExtra );
		void prettyPrintInitMask( classad::References & proj, const char * & constr, bool no_pr_files );

		ppOption prettyPrintHeadings( bool any_ads );

		int ppAdjustNameWidth( void * pv, Formatter * fmt ) const;
		void prettyPrintAd( ppOption pps, ClassAd * ad, int output_index, classad::References * includelist, bool fHashOrder );

		void printCustom( ClassAd * ad );
		void ppInitPrintMask( ppOption pps, classad::References & proj, const char * & constr, bool no_pr_files );

		AdTypes   setMode( int sm, int i, const char * arg );
		AdTypes resetMode( int sm, int i, const char * arg );

		void dumpPPMode( FILE * out ) const;
		const char * adtypeNameFromPPMode() const;

		int set_status_print_mask_from_stream( const char * streamid, bool is_filename, const char ** pconstraint );


		void ppSetAnyNormalCols();
		void ppSetStartdOfflineCols();
		void ppSetStartdAbsentCols();
		void    ppSetStartDaemonCols( int width, const char * & constr );
		void    ppSetStartdNormalCols( int width );
		void           ppSetStateCols( int width );
		void             ppSetRunCols( int width );
		void            ppSetGPUsCols( int width, bool daemon_ad, const char * & constr );
		void          ppSetBrokenCols( int width, bool daemon_ad, const char * & constr );
		void ppSetCollectorNormalCols( int width );
		void  ppSetCkptSrvrNormalCols( int width );
		void   ppSetStorageNormalCols( int width );
		void      ppSetGridNormalCols( int width );
		int     ppSetMasterNormalCols( int width );
		int     ppSetDefragNormalCols( int width );
		int ppSetAccountingNormalCols( int width );
		int ppSetNegotiatorNormalCols( int width );
		int     ppSetScheddNormalCols( int width, int & mach_width );
		int  ppSetSubmitterNormalCols( int width, int & mach_width );
		void          ppSetServerCols( int width, const char * & constr );
		int       ppSetScheddDataCols( int width, const char * & constr );
		int        ppSetScheddRunCols( int width, const char * & constr );
		int    ppSetAnnexInstanceCols( int width, const char * & constr );
		int    ppSetStartdCompactCols( int width, int & mach_width, const char * & constr );

	public:
		AttrListPrintMask           pm;
		std::vector<const char *>	pm_head;
		ppOption					ppStyle;
		ppOption					ppTotalStyle;
		printmask_headerfooter_t	pmHeadFoot;
		bool						wide_display;
		bool						using_print_format;
		bool						wantOnlyTotals;
		int							forced_display_width;
		ClassAd *					targetAd;

	private:
		int							width_of_fixed_cols;
		int							ixCol_Name;
		int							ixCol_Machine;

		struct {
			AdTypes			adType;
			int				argIndex;
			const char *	Arg;
			int				ppArgIndex;
			const char *	ppArg;
		}							setby;

	// For compact mode.
	public:
		int max_totals_subkey;
		signed char startdCompact_ixCol_Platform;
		signed char startdCompact_ixCol_Slots;
		signed char startdCompact_ixCol_FreeCpus;
		signed char startdCompact_ixCol_FreeMem;
		signed char startdCompact_ixCol_ActCode;
		signed char startdCompact_ixCol_MaxSlotMem;
		signed char startdCompact_ixCol_JobStarts;
		signed char startdCompact_ixCol_MachineSlot;
		signed char startdCompact_ixCol_FreeGPUs;
		signed char startdCompact_ixCol_TotGPUs;
		signed char startdCompact_ixCol_OffGPUs;
		signed char startdCompact_ixCol_GPUsProps;
		signed char startdCompact_ixCol_GPUsPropsCaps;
		signed char startdCompact_ixCol_GPUsPropsMem;
		signed char startdCompact_ixCol_GPUsPropsName;
		signed char startdCompact_ixCol_GPUsPropsIds;
};

#define PMODE_AVAIL_CONSTRAINT \
	"State == \"Unclaimed\" && Cpus > 0 && Memory > 0"
#define PMODE_CLAIMED_CONSTRAINT \
	"State == \"Claimed\""
#define PMODE_COD_CONSTRAINT \
	ATTR_NUM_COD_CLAIMS " > 0"
#define PMODE_GPUS_CONSTRAINT \
	"(PartitionableSlot && AssignedGPUs isnt undefined) ?: Gpus"
#define PMODE_BROKEN_CONSTRAINT \
	"size(SlotBrokenReason?:BrokenSlots) > 0"


#define PMODE_STARTD_COMPACT_CONSTRAINT \
	"PartitionableSlot =?= true || DynamicSlot =!= true"
#define PMODE_AVAIL_COMPACT_CONSTRAINT \
	"State == \"Unclaimed\" && Cpus > 0 && Memory > 0"
#define PMODE_CLAIMED_COMPACT_CONSTRAINT \
	"(State == \"Claimed\" && DynamicSlot =!= true) || (NumDynamicSlots isnt undefined && NumDynamicSlots > 0)"
#define PMODE_STATE_COMPACT_CONSTRAINT \
	"PartitionableSlot =?= true || DynamicSlot =!= true"
#define PMODE_SERVER_COMPACT_CONSTRAINT \
	"PartitionableSlot =?= true || DynamicSlot =!= true"
#define PMODE_GPUS_COMPACT_CONSTRAINT \
	"AvailableGPUs isnt undefined && (size(AvailableGPUs) > 0 || (PartitionableSlot && AssignedGPUs isnt undefined))"

#endif /* _CONDOR_PRETTY_PRINT_H */
