<Insert PR description here, and leave checklist below for code review.>

# HTCondor Pull Request Checklist for internal reviewers

- [ ] Verify that (GitHub thinks) the merge is clean. If it isn't, and you're confident you can resolve the conflicts, do so. Otherwise, send it back to the original developer.
- [ ] Verify that the related Jira ticket exists and has a target version number and that it is correct.
- [ ] Verify that the Jira ticket is in review status and is assigned to the reviewer.
- [ ] Verify that the Jira ticket (HTCONDOR-xxx) is mentioned at the beginning of the title. Edit it, if not
- [ ] Verify that the branch destination of the PR matches the target version of the ticket
- [ ] Check for correctness of change
- [ ] Check for regression test(s) of new features and bugfixes (if the feature doesn't require root)
- [ ] Check for documentation, if needed  (documentation [build logs](https://app.readthedocs.org/projects/htcondor/builds/))
- [ ] Check for version history, if needed
- [ ] Check BaTLab dashboard for successful build (https://batlab.chtc.wisc.edu/results/workspace.php) and test for either the PR or a workspace build by the developer that has the Jira ticket as a comment.
- [ ] Check that each commit message references the Jira ticket (HTCONDOR-xxx)

## After the above
- Hit the merge button if the pull request is approved and it is not a security patch (security changes require 2 additional reviews)
- If the pull request is approved, take the ticket out of review state
- Assign JIRA Ticket back to the developer
