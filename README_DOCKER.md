# Docker Quickstart

## 1) Build
```bash
docker build -t ipc-chat:latest .
```

## 2) Option A — Single container (interactive)
```bash
docker run --rm -it --name ipc-chat -v "$PWD":/app ipc-chat:latest /bin/bash
# inside:
./demo_tmux.sh
# or:
./run_server.sh
./run_client.sh 1001 Alice
./run_client.sh 1002 Bob
```

## 2) Option B — docker-compose (multi-container; shares host IPC)
```bash
docker compose up --build -d server
CLIENT_ID=1001 CLIENT_NAME=Alice docker compose run --rm client
CLIENT_ID=1002 CLIENT_NAME=Bob   docker compose run --rm client
docker compose down
```

**Notes**
- System V message queues require shared IPC namespace across processes.
- `ipc: host` is used so containers can talk via SysV message queues.
- Binaries are built by `make` during image build.
