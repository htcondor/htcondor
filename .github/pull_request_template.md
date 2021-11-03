# HTCondor Pull Request Template

## Checklist:

- [] Verify that (GitHub thinks) the merge is clean. If it isn't, and you're confident you can resolve the conflicts, do so. Otherwise, send it back to the original developer.
- [] Verify that the jira ticket has a target version number and that it is correct
- [] Verify that the jira ticket is in review status
- [] Verify that the Jira ticket is mentioned at the beginning of the title. Edit it, if not
- [] Check for correctness of change
- [] Verify that the destination of the pull request matches the target version of the ticket
- [] Check for documentation, if needed
- [] Check for version history, if needed
- [] Check batlab dashboard for successful build and test for either the pull request or a workspace build by the developer that has the jira ticket as a comment.
- [] Check that commit message references the jira ticket (HTCONDOR-xxx)

## After the above
- Hit the merge button if the pull request is approved and it is not a security patch (security changes require 2 additional review)
- If the pull request is approved, take the ticket out of review state
- Assign JIRA Ticket back to the developer
