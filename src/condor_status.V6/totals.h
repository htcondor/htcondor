#ifndef __TOTALS_H__
#define __TOTALS_H__

class Totals
{
  public:
	Totals ();
	~Totals ();

	void setSummarySize (int);
	void *getTotals (char *, char *);
	void *getTotals (int);

  private:
	enum {_descSize = 32};	// max size of string "arch/os"
	enum {_maxDescs = 32};	// max # of different "arch/os" combos in a pool

	// private internal structure
	typedef struct {
		char	archOs[_descSize];
		void	*data;
	} _totalNode;

	int			dataSize;
	int			descsUsed;	
	_totalNode	totalNodes[_maxDescs];
};

// these are the structures used by the various modes for totals
typedef struct {
	int machines;
	int condor;
} NormalTotals;


typedef struct {
	int machines;
	int avail;
	int condor;
	int owner;
	int mips;
	int mflops;
} ServerTotals;


typedef struct {
	int machines;
	int avail;
} AvailTotals;


typedef struct {
	int machines;
	int mips;
	int mflops;
	float avgloadavg;
} RunTotals;


typedef struct {
	int machines;
	int totaljobs;
	int idlejobs;
	int runningjobs;
	int unexpandedjobs;
} SubmittorsTotals;
	

#endif//__TOTALS_H__
