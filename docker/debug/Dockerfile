# Thin debug image: the exact worldserver binary from trinitycore:local + gdb.
# Built FROM the local image so symbols/binary always match the crashing process.
# Rebuild this whenever you rebuild trinitycore:local (see scripts/process-worldserver-crashes.ps1).
FROM trinitycore:local
USER root
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends gdb \
 && rm -rf /var/lib/apt/lists/*
ENTRYPOINT []
CMD ["gdb", "--version"]
