#include "condor_common.h"
#include "condor_netaddr.h"
#include "internet.h"

condor_netaddr::condor_netaddr() : maskbit_((unsigned int)-1) {
}

condor_netaddr::condor_netaddr(const condor_sockaddr& base,
		unsigned int maskbit)
: base_(base), maskbit_(maskbit) {
}

bool condor_netaddr::match(const condor_sockaddr& target) const {
	if (maskbit_ == (unsigned int)-1) {
		return false; // uninitialized
	}

	// check if address type is same
	//
	// what is correct policy for matching between IPv4 address and
	// IPv4-mapped-IPv6 address?
	if (base_.get_aftype() != target.get_aftype()) {
		return false;
	}

	const uint32_t* baseaddr = base_.get_address();
	const uint32_t* targetaddr = target.get_address();

	if (!baseaddr || !targetaddr) {
		return false;
	}
	int addr_len = base_.get_address_len();

	int curmaskbit = maskbit_;
	for (int i = 0; i < addr_len; ++i) {
		if (curmaskbit <= 0) { break; }
		uint32_t mask;
		if (curmaskbit >= 32) {
			mask = 0xffffffff;
		} else {
			mask = htonl(~(0xffffffff >> curmaskbit));
		}

		if ((*baseaddr & mask) != (*targetaddr & mask)) {
			return false;
		}

		curmaskbit -= 32;
		baseaddr++;
		targetaddr++;
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
				if (maskbit_ == (unsigned int)-1)
					return false;
			}
		}
	} else {
		// if there is no slash ('/'), it should be IPv4 and
		// wildcard format
		//
		// e.g. 128.105.*

		in_addr base;
		in_addr mask;
		if (!is_ipv4_addr_implementation(net, &base, &mask, 1))
			return false;

		base_ = condor_sockaddr(base, 0);
		maskbit_ = convert_maskaddr_to_maskbit(*(uint32_t*)&mask);
		if (maskbit_ == -1)
			return false;
	}
	return true;
}
