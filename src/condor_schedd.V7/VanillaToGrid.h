#ifndef _VANILLA_TO_GRID_H_
#define _VANILLA_TO_GRID_H_

namespace classad { class ClassAd; }

/* Converts a vanilla job to a grid job

The ad will be modified in place.

*/
class VanillaToGrid 
{
public:
	/*
	ad - Class ad to rewrite.  Only vanilla jobs are tested currently.
	gridresource - where to send the job.  In ATTR_GRID_RESOURCE
		format. 
	*/
	static bool vanillaToGrid(classad::ClassAd * ad, const char * gridresource);
private:
};

/*

Given a grid job created by vanillaToGrid, propogate updates to the grid
job's classad back to the original classad.

orig - The original classad, as was passed to vanillaToGrid.  Note
that vanillaToGrid modifies the classad in place, so you need to keep
your own copy (or be able to rebuild one on the fly).  Changes based on
changes to newgrid will be placed here.

newgrid - The current classad for the transformed job.

return true on success, false on failure.


*/
bool update_job_status( classad::ClassAd & orig, classad::ClassAd & newgrid);

#endif /*#ifndef _VANILLA_TO_GRID_H_*/
