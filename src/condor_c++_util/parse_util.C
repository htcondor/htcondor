#include "condor_common.h"
#include "parse_util.h"

void
breakStringByDelimiter( StringList &toAppendTo, const MyString &strToBreak,
						char cDelimiter )
{
	int iStartPos = 0;
	int iDelimPos;
	int iLength = strToBreak.Length();
	while (iStartPos < iLength)
	{
		iDelimPos = strToBreak.FindChar(cDelimiter, iStartPos);
		if (iDelimPos == -1)
		{
			toAppendTo.append(strToBreak.Substr(iStartPos, strToBreak.Length()-1).GetCStr());
			break;
		}	

		toAppendTo.append(strToBreak.Substr(iStartPos, iDelimPos-1).GetCStr());
		iStartPos = iDelimPos+1;
	} 
}
