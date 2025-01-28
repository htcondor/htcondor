#include "condor_common.h"
#include "condor_netaddr.h"
#include "internet.h"

condor_netaddr::condor_netaddr() : maskbit_((unsigned int)-1), matchesEverything( false ) {
}

condor_netaddr::condor_netaddr(const condor_sockaddr& base,
		unsigned int maskbit)
: base_(base), maskbit_(maskbit), matchesEverything( false ) {
	set_mask();
}

void condor_netaddr::set_mask() {
	if (base_.is_ipv4()) {
		uint32_t mask = 0xffffffff;
		if (maskbit_ < 32) {
			mask = htonl(~(0xffffffff >> maskbit_));
		}
		mask_ = condor_sockaddr( *(struct in_addr*)&mask );
	} else {
		uint32_t mask[4] = { };
		int curmaskbit = maskbit_;
		for (auto &word: mask) {
			if (curmaskbit <= 0) { 
				break;
			}
			if (curmaskbit >= 32) {
				word = 0xffffffff;
			} else {
				word = htonl(~(0xffffffff >> curmaskbit));
			}
			curmaskbit -= 32;
		}
		mask_ = condor_sockaddr( *(struct in6_addr*)&mask );
	}
}

bool condor_netaddr::match(const condor_sockaddr& target) const {
	if( matchesEverything ) { return true; }

	// An unitialized network matches nothing.
	if (maskbit_ == (unsigned int)-1) {
		return false;
	}

	// An address of the wrong type can't match.
	if (base_.get_aftype() != target.get_aftype()) {
		return false;
	}

	const uint32_t* baseaddr = base_.get_address();
	const uint32_t* targetaddr = target.get_address();
	const uint32_t* maskaddr = mask_.get_address();

	if (!baseaddr || !targetaddr || !maskaddr) {
		return false;
	}
	int addr_len = base_.get_address_len();

	int curmaskbit = maskbit_;
	for (int i = 0; i < addr_len; ++i) {
		if (curmaskbit <= 0) { break; }

		if ((*baseaddr & *maskaddr) != (*targetaddr & *maskaddr)) {
			return false;
		}

		curmaskbit -= 32;
		baseaddr++;
		targetaddr++;
		maskaddr++;
	}

	return true;
}

static int convert_maskaddr_to_maskbit(uint32_t mask_value) {
	int maskbit = 0;

	mask_value = ntohl(mask_value);

	// get rid of 0's from the right-most side
	while (mask_value > 0 && (mask_value & 1) == 0)
		mask_value >>= 1;

	while (mask_value > 0 && (mask_value & 1) == 1) {
		maskbit++;
		mask_value >>= 1;
	}

	if (mask_value != 0) {
		// the mask address is not well formed
		// e.g. 255.129.0.0, ill-formed mask
		// 1111 1111 | 1000 0001 | 0000 0000 | 0000 0000
		return -1;
	}

	return maskbit;
}

bool condor_netaddr::from_net_string(const char* net) {
	if( strcmp( net, "*" ) == 0 || strcmp( net, "*/*" ) == 0 ) {
		matchesEverything = true;
		return true;
	}

	const char* slash = strchr(net, '/');
	const char* net_end = net + strlen(net);
	if (slash) {
		std::string base(net, slash - net);
		if (!base_.from_ip_string(base.c_str()))
			return false;

		// check whether secondary part is number
		char* end_ptr = NULL;
		unsigned long int maskbit = strtoul(slash + 1, &end_ptr, 10);

		// if end_ptr == net_end, the entire secondary part is a number,
		// which means it is the mask bit.
		if (end_ptr == net_end) {
			maskbit_ = maskbit;
		} else {
			// only IPv4 allows secondary part to be address form
			// e.g. 128.105.0.0/255.255.0.0
			if (base_.is_ipv4()) {
				std::string mask(slash + 1, net_end - slash - 1);
				condor_sockaddr maskaddr;

				// convert to numerical representation
				if (!maskaddr.from_ip_string(mask.c_str()) ||
						!maskaddr.is_ipv4())
					return false;

				// convert to mask bit
				const uint32_t* maskaddr_array = maskaddr.get_address();
				maskbit_ = convert_maskaddr_to_maskbit(*maskaddr_array);
				if (maskbit_ == (unsigned int)-1) {
					return false;
				}
			} else {
				return false;
			}
		}
	} else {
		const char * colon = strchr( net, ':' );
		if( colon == NULL ) {
			// IPv4 literal or asterisk.

			in_addr base;
			in_addr mask;
			if( ! is_ipv4_addr_implementation( net, &base, &mask, 1 ) ) {
				return false;
			}

			base_ = condor_sockaddr(base, 0);
			maskbit_ = convert_maskaddr_to_maskbit(*(uint32_t*)&mask);
			if( maskbit_ == (unsigned)-1 ) {
				return false;
			}
		} else {
			// IPv6 literal or asterisk.
			const char * asterisk = strchr( net, '*' );
			if( asterisk == NULL ) {
				if ( base_.from_ip_string( net ) ) {
					maskbit_ = 128;
				} else {
					return false;
				}
			} else {
				//
				// Like IPv4, require that the asterisk be on a boundary.
				// That way, we don't need to guess at how many zeroes the
				// administrator actually wanted.
				//
				const char * rcolon = strrchr( net, ':' );
				if( asterisk - rcolon != 1 ) {
					return false;
				}

				char * safenet = strdup( net );
				assert( safenet != NULL );
				char * safeasterisk = strchr( safenet, '*' );
				assert( safeasterisk != NULL );
				* safeasterisk = ':';

				struct in6_addr in6a;
				int rv = inet_pton( AF_INET6, safenet, & in6a );
				free( safenet );
				if( rv == 1 ) {
					base_ = condor_sockaddr( in6a );
				} else {
					return false;
				}

				maskbit_ = 0;
				for( const char * ptr = net; * ptr != '\0'; ++ptr ) {
					if( * ptr == ':' ) { maskbit_ += 16; }
				}

			}
		}
	}
	set_mask();
	return true;
}
