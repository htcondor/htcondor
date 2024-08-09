
#ifndef _USERREC_H_
#define _USERREC_H_
#if 1
 constexpr bool USERREC_NAME_IS_FULLY_QUALIFIED = true;
 #define ATTR_USERREC_NAME ATTR_USER
#else
 constexpr bool USERREC_NAME_IS_FULLY_QUALIFIED = false;
 #define ATTR_USERREC_NAME ATTR_OWNER
#endif


#endif
