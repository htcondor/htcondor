#ifndef YOUR_STRING_H
#define YOUR_STRING_H

// This is a simple wrapper class to enable char *'s
// that we don't manage to be put into HashTables

// HashTable needs operator==, which we define to be
// case-insensitive for ClassAds

class YourString {
	public:
		YourString() : s(0) {}
		YourString(const char *str) : s(str) {}
		bool operator==(const YourString &rhs) {
			return (strcasecmp(s,rhs.s) == 0);
		}
		const char *s; // Someone else owns this
};
#endif
