#!/usr/bin/env bash

# do NOT set -e, because we want to let the user hit the container shell
# even if something in here goes wrong

# if there is no directory where we expect to find the HTCondor git repo,
# clone it (see Dockerfile for default environment variable values)
if [[ "$HTCONDOR_CHECK_FOR_REPO" = true && ! -d "$HTCONDOR_REPO_DIR" ]]; then
  >&2 echo "HTCondor source repository NOT found in container, cloning from $HTCONDOR_GIT_URL@$HTCONDOR_CLONE_BRANCH"
  git clone \
    --branch "$HTCONDOR_BRANCH" \
    --depth 1 \
    "$HTCONDOR_GIT_URL" \
    "$HTCONDOR_REPO_DIR"
fi

# execute whatever command the user gave; see Dockerfile for the default CMD
exec "$@"
