FROM debian:stable-slim

RUN apt-get update && apt-get install -y --no-install-recommends         build-essential make tmux ca-certificates         && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app
RUN make
CMD ["/bin/bash"]
