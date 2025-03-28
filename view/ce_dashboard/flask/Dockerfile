ARG PYTHON_VERSION=3.9.13
FROM python:${PYTHON_VERSION}-slim AS base

# Prevents Python from writing pyc files.
ENV PYTHONDONTWRITEBYTECODE=1

# Keeps Python from buffering stdout and stderr to avoid situations where
# the application crashes without emitting any logs due to buffering.
ENV PYTHONUNBUFFERED=1

WORKDIR /app

# Create a non-privileged user that the app will run under.
# See https://docs.docker.com/go/dockerfile-user-best-practices/
ARG UID=10001
RUN adduser \
    --disabled-password \
    --gecos "" \
    --home "/nonexistent" \
    --shell "/sbin/nologin" \
    --no-create-home \
    --uid "${UID}" \
    appuser

# Download dependencies as a separate step to take advantage of Docker's caching.
# Leverage a cache mount to /root/.cache/pip to speed up subsequent builds.
# Leverage a bind mount to requirements.txt to avoid having to copy them into
# into this layer.
RUN --mount=type=cache,target=/root/.cache/pip \
    --mount=type=bind,source=requirements.txt,target=requirements.txt \
    pip install --upgrade pip && \
    pip install --root-user-action=ignore -r requirements.txt && \
    mkdir /etc/condor && \
    touch /etc/condor/condor_config

# Switch to the non-privileged user to run the application.
USER appuser

# Copy the source code into the container.
COPY . .

# Expose the port that the application listens on.
EXPOSE 5000

# Run the gunicorn server, using number of workers based on the number of CPU cores,
# defaulting to 2xCPU + 1.  Or use the value of the GUNICORN_CPUS environment variable if set.
# Setting GUNICORN_CPUS may be needed for containers running in a Kubernetes cluster.
# We launch the app with a bash script so we can use exec to avoid having a shell process
# as the parent process of the application; this way signals are properly handled.
ENV GUNICORN_CPUS=
COPY --chmod=755 <<EOT ./start_ce_dash_with_gunicorn.sh
#!/usr/bin/env bash
set -e
exec gunicorn -w $\{GUNICORN_CPUS:-$(expr $(nproc) \\* 2 + 1)\} 'app:app' --bind=0.0.0.0:5000
EOT

CMD ["./start_ce_dash_with_gunicorn.sh"]
# End of Dockerfile
