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
#endif /*#ifndef _VANILLA_TO_GRID_H_*/
