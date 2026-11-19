* The schedd does very little scheduling, if any; it mostly empties a queue
  of shadow starts, which is filled by various pieces of event-drive code,
  mostly in the negotiation protocol.  The only thinking it could be said to
  do is in how it sorts the priorec array.

* The schedd keeps track of "matches" -- claims -- in "match records" --
  `match_rec` objects -- and of shadows -- in "shadow records -- `shadow_rec`
  objects.

  Match records are conjured into existence when the schedd receives a match,
  either from the negotiator or from direct attach.  They are destroyed by
  delMrec() calling unlinkMrec(); the former is called from all over, in any
  situation where the schedd knows it will or can no longer deal with a given
  match.  Every match record corresponds to a claim, and there is a map from
  claim ID to match records that is complete and always up-to-date.

  Note that the map of matches by job ID is updated when the match assigned
  to a job, not when the shadow is started, and this happens _before_
  StartJob() is called; this must be undone for common file transfer to
  function properly, because of assumptions elsewhere in the code.

  Shadow records are conjured into existence when the schedd decides which
  job to start on a match.  They are not registered in a table until the
  corresponding shadow has started, so any code that might possibly execute
  between when the shadow record is made and when the shadow is actually
  started must clean that particular shadow record up.  Once the shadow has
  started, no code may clean a shadow record up (call `delete_shadow_rec()`)
  except for the reaper (childExit() and subsidiaries).

  Note that because of bad design, both unlinkMrec() and delete_shadow_rec()
  are _not_ [in] the destructors for their respective classes and must be
  called explicitly.  This is obviously dangerous, and compromises the simpler
  ways to ensure safety.

  This lack of a single owning table for shadows has caused the common files
  code no end of grief, because it operates not only asynchronously (like the
  the shadow start queue) but also concurrently, in that a shadow which hasn't
  started yet (either because it's in the queue or because of command_data_slot)
  may never be started because of a problem (a failure in command_data_slot or
  if its match record vanishes while in command_data_slot or in the shadow
  start queue).  It seems like it would easiest to tie the lifetime of shadow
  recs to their match records, but we rather want to keep the shadow record
  around until the shadow dies, so we can't truncate shadow lifetimes in
  unlinkMrec(), even if it were safe (in the sense of not multiple free'ing)
  to do so.

  Specifically: in start_command_data_slot(), command_data_slot_callback(),
  and call_StartJobFailure(), we can't rely on the shadow record pointer in
  the match record after the timer or callback fires: it may have been reset to
  NULL, or the match record itself may have been deleted.  We avoid the latter
  by passing along the claim ID (as a unique and tracked identifier), but
  there's no equivalent for shadow records.

  Note that the actual problem causing jobs to stay blocked or idle forever
  is not unregistering the shadow catalogs; actually deleting the shadow
  records just prevents a memory leak.

* checkBlockedJob() verifies -- as best we can -- that a blocked job in
  the queue won't be blocked forever.  Specifically, it checks for either a
  corresponding transfer shadow or that the job is holding onto match and
  that there's a transfer shadow registered for each of its catalogs.

  This is not wholly sufficient; an idle job with an entry in matchesByJobID
  but which is not already running may have become lost in the shuffle;
  presently, we can't even readily inspect the queue of shadow starts to see
  if it's gotten that far; see previous commentary about a lack of table of
  all shadow records.

* unregister_shadow_catalogs() is an asynchronous / detatched failure mode
  for blocked jobs, in the sense of who's responsible for deleting the
  shadow record.  It's also responsible for deleting the match record,
  because once the jobs are unblocked, we won't be using the matches again.

  In general:

    //
    // Our invariants about blocked jobs follow:
    // (a) A blocked job must either be a prompting job or
    //     have a match; and
    // (b) for each common file catalog, there is corresponding
    //     shadow record.
    //
    // Because of (b), when we unregister a catalog, we must
    // unblock blocked jobs.  As an optimization, we could try to
    // re-use the match held by the blocked job.  However, since
    // we just unconditionally delete the match record, we must
    // also delete its shadow record; definitionally, no blocked
    // job has a live shadow, so we can't rely on the reaper.  (If
    // we wanted to confuse people, we could move the blocked job
    // to the shadow start queue and then fail to start the job
    // because its match evaporated; this would excuse this code
    // from the need to clean up the shadow rec, but that would be
    // its only benefit.)
    //
    // We don't have to worry about (a) here because if this shadow
    // is responsible for a catalog, we just took care of unblocking
    // its prompting job above.

  If we actually unblocked a job:

	//
	// The job is now idle and holding a claimed resource,
	// but we can't start a shadow for it.  We can't call
	// mark_serial_job_running() because we're not starting
	// a shadow, and if we call addRunnableJob(), we'll
	// skip StartJob(mrec, job_id) [which is normally
	// responsible for calling addRunnableJob() via
	// start_std()].
	//
	// If we delete this match record, we must also delete
	// this shadow record; nobody else will, because no
	// other code will ever see the shadow record.  We
	// don't want to leak memory, but we need to delete
	// the shadow record to maintain our invariants, too,
	// most notably about entries in catalogToShadowMap.
	//
