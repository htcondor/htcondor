
#include "GroupEntry.h"
#include <algorithm>

GroupEntry::GroupEntry():
		name(),
		config_quota(0),
		static_quota(false),
		accept_surplus(false),
		autoregroup(false),
		usage(0),
		submitterAds(NULL),
		priority(0),
		quota(0),
		requested(0),
		currently_requested(0),
		allocated(0),
		subtree_quota(0),
		subtree_requested(0),
		subtree_usage(0),
		rr(false),
		rr_time(0),
		subtree_rr_time(0),
		parent(NULL),
		children(),
		chmap(),
		sort_ad(new ClassAd()),
		sort_key(0)
{
}

GroupEntry::~GroupEntry() {
		for (unsigned long j=0;  j < children.size();  ++j) {
				if (children[j] != NULL) {
						delete children[j];
				}
		}

		if (NULL != submitterAds) {
				submitterAds->Open();
				while (ClassAd* ad = submitterAds->Next()) {
						submitterAds->Remove(ad);
				}
				submitterAds->Close();

				delete submitterAds;
		}

		if (NULL != sort_ad) delete sort_ad;
}

double 
GroupEntry::hgq_fairshare() {
		dprintf(D_FULLDEBUG, "group quotas: fairshare (1): group= %s  quota= %g  requested= %g\n",
						this->name.c_str(), this->quota, this->requested);

		// Allocate whichever is smallest: the requested slots or group quota.
		this->allocated = std::min(this->requested, this->quota);

		// update requested values
		this->requested -= this->allocated;
		this->subtree_requested = this->requested;

		// surplus quota for this group
		double surplus = this->quota - this->allocated;

		dprintf(D_FULLDEBUG, "group quotas: fairshare (2): group= %s  quota= %g  allocated= %g  requested= %g\n",
						this->name.c_str(), this->quota, this->allocated, this->requested);

		// If this is a leaf group, we're finished: return the surplus
		if (this->children.empty()) {
				return surplus;
		}

		// This is an internal group: perform fairshare recursively on children
		for (unsigned long j = 0;  j < this->children.size();  ++j) {
				GroupEntry* child = this->children[j];
				surplus += child->hgq_fairshare();
				if (child->accept_surplus) {
						this->subtree_requested += child->subtree_requested;
				}
		}

		// allocate any available surplus to current node and subtree
		surplus = this->hgq_allocate_surplus(surplus);

		dprintf(D_FULLDEBUG, "group quotas: fairshare (3): group= %s  surplus= %g  subtree_requested= %g\n",
						this->name.c_str(), surplus, this->subtree_requested);

		// return any remaining surplus up the tree
		return surplus;
}

double
GroupEntry::hgq_allocate_surplus(double surplus) {
		dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (1): group= %s  surplus= %g  subtree-requested= %g\n", this->name.c_str(), surplus, this->subtree_requested);

		// Nothing to allocate
		if (surplus <= 0) return 0;

		// If entire subtree requests nothing, halt now
		if (this->subtree_requested <= 0) return surplus;

		// Surplus allocation policy is that a group shares surplus on equal footing with its children.
		// So we load children and their parent (current group) into a single vector for treatment.
		// Convention will be that current group (subtree root) is last element.
		vector<GroupEntry*> groups(this->children);
		groups.push_back(this);

		// This vector will accumulate allocations.
		// We will proceed with recursive allocations after allocations at this level
		// are completed.  This keeps recursive calls to a minimum.
		vector<double> allocated(groups.size(), 0);

		// Temporarily hacking current group to behave like a child that accepts surplus
		// avoids some special cases below.  Somewhere I just made a kitten cry.
		bool save_accept_surplus = this->accept_surplus;
		this->accept_surplus = true;
		double save_subtree_quota = this->subtree_quota;
		this->subtree_quota = this->quota;
		double requested = this->subtree_requested;
		this->subtree_requested = this->requested;

		if (surplus >= requested) {
				// In this scenario we have enough surplus to satisfy all requests.
				// Cornucopia! Give everybody what they asked for.

				dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (2a): direct allocation, group= %s  requested= %g  surplus= %g\n",
								this->name.c_str(), requested, surplus);

				for (unsigned long j = 0;  j < groups.size();  ++j) {
						GroupEntry* grp = groups[j];
						if (grp->accept_surplus && (grp->subtree_requested > 0)) {
								allocated[j] = grp->subtree_requested;
						}
				}

				surplus -= requested;
				requested = 0;
		} else {
				// In this scenario there are more requests than there is surplus.
				// Here groups have to compete based on their quotas.

				dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (2b): quota-based allocation, group= %s  requested= %g  surplus= %g\n",
								this->name.c_str(), requested, surplus);

				vector<double> subtree_requested(groups.size(), 0);
				for (unsigned long j = 0;  j < groups.size();  ++j) {
						GroupEntry* grp = groups[j];
						// By conditioning on accept_surplus here, I don't have to check it below
						if (grp->accept_surplus && (grp->subtree_requested > 0)) {
								subtree_requested[j] = grp->subtree_requested;
						}
				}

				// In this loop we allocate to groups with quota > 0
				hgq_allocate_surplus_loop(true, groups, allocated, subtree_requested, surplus, requested);

				// Any quota left can be allocated to groups with zero quota
				hgq_allocate_surplus_loop(false, groups, allocated, subtree_requested, surplus, requested);

				// There should be no surplus left after the above two rounds
				if (surplus > 0) {
						dprintf(D_ALWAYS, "group quotas: allocate-surplus WARNING: nonzero surplus %g after allocation\n", surplus);
				}
		}

		// We have computed allocations for groups, with results cached in 'allocated'
		// Now we can perform the actual allocations.  Only actual children should
		// be allocated recursively here
		for (unsigned long j = 0;  j < (groups.size()-1);  ++j) {
				if (allocated[j] > 0) {
						double s = groups[j]->hgq_allocate_surplus(allocated[j]);
						if (fabs(s) > 0.00001) {
								dprintf(D_ALWAYS, "group quotas: WARNING: allocate-surplus (3): surplus= %g\n", s);
						}
				}
		}

		// Here is logic for allocating current group
		this->allocated += allocated.back();
		this->requested -= allocated.back();

		dprintf(D_FULLDEBUG, "group quotas: allocate-surplus (4): group %s allocated surplus= %g  allocated= %g  requested= %g\n",
						this->name.c_str(), allocated.back(), this->allocated, this->requested);

		// restore proper group settings
		this->subtree_requested = requested;
		this->accept_surplus = save_accept_surplus;
		this->subtree_quota = save_subtree_quota;

		return surplus;
}

void round_for_precision(double& x) {
		double ref = x;
		x = floor(0.5 + x);
		double err = fabs(x-ref);
		// This error threshold is pretty ad-hoc.  It would be ideal to try and figure out
		// bounds on precision error accumulation based on size of HGQ tree.
		if (err > 0.00001) {
				// If precision errors are not small, I am suspicious.
				dprintf(D_ALWAYS, "group quotas: WARNING: encountered precision error of %g\n", err);
		}
}

double
GroupEntry::hgq_recover_remainders() {
		dprintf(D_FULLDEBUG, "group quotas: recover-remainders (1): group= %s  allocated= %g  requested= %g\n",
						this->name.c_str(), this->allocated, this->requested);

		// recover fractional remainder, which becomes surplus
		double surplus = this->allocated - floor(this->allocated);
		this->allocated -= surplus;
		this->requested += surplus;

		// These should be integer values now, so I get to round to correct any precision errs
		round_for_precision(this->allocated);
		round_for_precision(this->requested);

		this->subtree_requested = this->requested;
		this->subtree_rr_time = (this->requested > 0) ? this->rr_time : DBL_MAX;

		dprintf(D_FULLDEBUG, "group quotas: recover-remainders (2): group= %s  allocated= %g  requested= %g  surplus= %g\n",
						this->name.c_str(), this->allocated, this->requested, surplus);

		// If this is a leaf group, we're finished: return the surplus
		if (this->children.empty()) return surplus;

		// This is an internal group: perform recovery recursively on children
		for (unsigned long j = 0;  j < this->children.size();  ++j) {
				GroupEntry* child = this->children[j];
				surplus += child->hgq_recover_remainders();
				if (child->accept_surplus) {
						this->subtree_requested += child->subtree_requested;
						if (child->subtree_requested > 0)
								this->subtree_rr_time = std::min(this->subtree_rr_time, child->subtree_rr_time);
				}
		}

		// allocate any available surplus to current node and subtree
		surplus = this->hgq_round_robin(surplus);

		dprintf(D_FULLDEBUG, "group quotas: recover-remainder (3): group= %s  surplus= %g  subtree_requested= %g\n",
						this->name.c_str(), surplus, this->subtree_requested);

		// return any remaining surplus up the tree
		return surplus;
}

double
GroupEntry::hgq_round_robin(double surplus) {
		dprintf(D_FULLDEBUG, "group quotas: round-robin (1): group= %s  surplus= %g  subtree-requested= %g\n", this->name.c_str(), surplus, this->subtree_requested);

		// Sanity check -- I expect these to be integer values by the time I get here.
		if (this->subtree_requested != floor(this->subtree_requested)) {
				dprintf(D_ALWAYS, "group quotas: WARNING: forcing group %s requested= %g to integer value %g\n",
								this->name.c_str(), this->subtree_requested, floor(this->subtree_requested));
				this->subtree_requested = floor(this->subtree_requested);
		}

		// Nothing to do if subtree had no requests
		if (this->subtree_requested <= 0) return surplus;

		// round robin has nothing to do without at least one whole slot
		if (surplus < 1) return surplus;

		// Surplus allocation policy is that a group shares surplus on equal footing with its children.
		// So we load children and their parent (current group) into a single vector for treatment.
		// Convention will be that current group (subtree root) is last element.
		vector<GroupEntry*> groups(this->children);
		groups.push_back(this);

		// This vector will accumulate allocations.
		// We will proceed with recursive allocations after allocations at this level
		// are completed.  This keeps recursive calls to a minimum.
		vector<double> allocated(groups.size(), 0);

		// Temporarily hacking current group to behave like a child that accepts surplus
		// avoids some special cases below.  Somewhere I just made a kitten cry.  Even more.
		bool save_accept_surplus = this->accept_surplus;
		this->accept_surplus = true;
		double save_subtree_quota = this->subtree_quota;
		this->subtree_quota = this->quota;
		double save_subtree_rr_time = this->subtree_rr_time;
		this->subtree_rr_time = this->rr_time;
		double requested = this->subtree_requested;
		this->subtree_requested = this->requested;

		double outstanding = 0;
		vector<double> subtree_requested(groups.size(), 0);
		for (unsigned long j = 0;  j < groups.size();  ++j) {
				GroupEntry* grp = groups[j];
				if (grp->accept_surplus && (grp->subtree_requested > 0)) {
						subtree_requested[j] = grp->subtree_requested;
						outstanding += 1;
				}
		}

		// indexes allow indirect sorting
		vector<unsigned long> idx(groups.size());
		for (unsigned long j = 0;  j < idx.size();  ++j) idx[j] = j;

		// order the groups to determine who gets first cut
		ord_by_rr_time ord;
		ord.data = &groups;
		std::sort(idx.begin(), idx.end(), ord);

		while ((surplus >= 1) && (requested > 0)) {
				// max we can fairly allocate per group this round:
				double amax = std::max(double(1), floor(surplus / outstanding));

				dprintf(D_FULLDEBUG, "group quotas: round-robin (2): pass: surplus= %g  requested= %g  outstanding= %g  amax= %g\n",
								surplus, requested, outstanding, amax);

				outstanding = 0;
				double sumalloc = 0;
				for (unsigned long jj = 0;  jj < groups.size();  ++jj) {
						unsigned long j = idx[jj];
						GroupEntry* grp = groups[j];
						if (grp->accept_surplus && (subtree_requested[j] > 0)) {
								double a = std::min(subtree_requested[j], amax);
								allocated[j] += a;
								subtree_requested[j] -= a;
								sumalloc += a;
								surplus -= a;
								requested -= a;
								grp->rr = true;
								if (subtree_requested[j] > 0) outstanding += 1;
								if (surplus < amax) break;
						}
				}

				// a bit of defensive sanity checking -- should not be possible:
				if (sumalloc < 1) {
						dprintf(D_ALWAYS, "group quotas: round-robin (3): WARNING: round robin failed to allocate >= 1 slot this round - halting\n");
						break;
				}
		}

		// We have computed allocations for groups, with results cached in 'allocated'
		// Now we can perform the actual allocations.  Only actual children should
		// be allocated recursively here
		for (unsigned long j = 0;  j < (groups.size()-1);  ++j) {
				if (allocated[j] > 0) {
						double s = groups[j]->hgq_round_robin(allocated[j]);

						// This algorithm does not allocate more than a child has requested.
						// Also, this algorithm is designed to allocate every requested slot,
						// up to the given surplus.  Therefore, I expect these calls to return
						// zero.   If they don't, something is haywire.
						if (s > 0) {
								dprintf(D_ALWAYS, "group quotas: round-robin (4):  WARNING: nonzero surplus %g returned from round robin for group %s\n",
												s, groups[j]->name.c_str());
						}
				}
		}

		// Here is logic for allocating current group
		this->allocated += allocated.back();
		this->requested -= allocated.back();

		dprintf(D_FULLDEBUG, "group quotas: round-robin (5): group %s allocated surplus= %g  allocated= %g  requested= %g\n",
						this->name.c_str(), allocated.back(), this->allocated, this->requested);

		// restore proper group settings
		this->subtree_requested = requested;
		this->accept_surplus = save_accept_surplus;
		this->subtree_quota = save_subtree_quota;
		this->subtree_rr_time = save_subtree_rr_time;

		return surplus;
}


void hgq_allocate_surplus_loop(bool by_quota,
				vector<GroupEntry*>& groups, vector<double>& allocated, vector<double>& subtree_requested,
				double& surplus, double& requested)
{
		int iter = 0;
		while (surplus > 0) {
				iter += 1;

				dprintf(D_FULLDEBUG, "group quotas: allocate-surplus-loop: by_quota= %d  iteration= %d  requested= %g  surplus= %g\n",
								int(by_quota), iter, requested, surplus);

				// Compute the normalizer for outstanding groups
				double Z = 0;
				for (unsigned long j = 0;  j < groups.size();  ++j) {
						GroupEntry* grp = groups[j];
						if (subtree_requested[j] > 0)  Z += (by_quota) ? grp->subtree_quota : 1.0;
				}

				if (Z <= 0) {
						dprintf(D_FULLDEBUG, "group quotas: allocate-surplus-loop: no further outstanding groups at iteration %d - halting.\n", iter);
						break;
				}

				// allocations
				bool never_gt = true;
				double sumalloc = 0;
				for (unsigned long j = 0;  j < groups.size();  ++j) {
						GroupEntry* grp = groups[j];
						if (subtree_requested[j] > 0) {
								double N = (by_quota) ? grp->subtree_quota : 1.0;
								double a = surplus * (N / Z);
								if (a > subtree_requested[j]) {
										a = subtree_requested[j];
										never_gt = false;
								}
								allocated[j] += a;
								subtree_requested[j] -= a;
								sumalloc += a;
						}
				}

				surplus -= sumalloc;
				requested -= sumalloc;

				// Compensate for numeric precision jitter
				// This is part of the convergence guarantee: on each iteration, one of two things happens:
				// either never_gt becomes true, in which case all surplus was allocated, or >= 1 group had its
				// requested drop to zero.  This will move us toward Z becoming zero, which will halt the loop.
				// Note, that in "by-quota" mode, Z can become zero with surplus remaining, which is fine -- it means
				// groups with quota > 0 did not use all the surplus, and any groups with zero quota have the option
				// to use it in "non-by-quota" mode.
				if (never_gt || (surplus < 0)) {
						if (fabs(surplus) > 0.00001) {
								dprintf(D_ALWAYS, "group quotas: allocate-surplus-loop: WARNING: rounding surplus= %g to zero\n", surplus);
						}
						surplus = 0;
				}
		}
}
