#ifndef __MATCH_AUTH_VERIFY_H_
#define __MATCH_AUTH_VERIFY_H_

namespace classad {
class ClassAd;
}

/**
 * Returns true if the current configuration allows
 */
bool
allow_match_auth(std::string const &the_user, classad::ClassAd *my_match_ad);

#endif
