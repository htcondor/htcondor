#ifndef _CONDOR_EVENT_NOTIFICATION_H
#define _CONDOR_EVENT_NOTIFICATION_H


// The shadow had no complaints about the event.
#define GENERIC_EVENT_RV_OK          0

// The event ad was missing an attribute expected for its type.
#define GENERIC_EVENT_RV_INCOMPLETE -1

// The event ad didn't include ATTR_EVENT_TYPE.
#define GENERIC_EVENT_RV_NO_ETYPE   -2

// The shadow didn't recognize the event type.
#define GENERIC_EVENT_RV_UNKNOWN    -3

// Like RV_UNKNOWN, except for attributes other than the event type.
#define GENERIC_EVENT_RV_CONFUSED   -4

// An attribute's value was not in the expected format.  (For
// example, "Contents" is expected to be base64-encoded.)
#define GENERIC_EVENT_RV_INVALID    -5


#endif /* _CONDOR_EVENT_NOTIFICATION_H */
