# mycontainer — Container Simulator

A Linux container simulator written in C that demonstrates container concepts including image registries, container commits, and container-to-container networking.

## Features

### Existing (Stubs / Runtime Metadata)
- **PID Namespace Isolation** (`CLONE_NEWPID`)
- **UTS Namespace** (`CLONE_NEWUTS`)
- **Mount Namespace** (`CLONE_NEWNS`)
- **chroot** into Alpine rootfs
- **Cgroups** (CPU + memory limits)
- **Rootless / Privileged Runtime Modes**
- **CPU Pinning Metadata** (`--cpuset=...`)

### New Features
1. **Container Registry** — Local image store with push, pull, list, delete, inspect
2. **Container Commit** — Capture container state changes into new images via OverlayFS layers
3. **Container-to-Container Networking** — Linux bridge, veth pairs, IPv4 + IPv6 assignment, iptables rules
4. **Built-in Image Builder** — Build generic or Node.js-aware images from a directory context
5. **Image Signing + Verification** — Digest + shared-secret signature workflow
6. **Socket Bridge for Node.js** — Optional Unix socket bridge with direct-exec fallback

## Prerequisites

- **Linux** (Ubuntu 20.04+ recommended, or WSL2 on Windows)
- **GCC** (`apt install build-essential`)
- **iptables** (for networking features)
- **Root/sudo** (for network and cgroup operations)

## Build

```bash
cd mycontainer
make
```

## Usage

### Container Run
```bash
./mycontainer run --name=web-01 /bin/sh
./mycontainer run --name=web-01 --image=alpine:3.18 /bin/sh
./mycontainer run --name=web-01 --privileged --cpuset=0,1 /bin/sh
```

### Image Registry
```bash
./mycontainer image push alpine:3.18 rootfs.tar.gz    # Push image
./mycontainer image build ./examples/node-app demo:node --node
./mycontainer image ls                                  # List images
./mycontainer image inspect alpine:3.18                 # Inspect image
./mycontainer image sign alpine:3.18 --key=mysecret     # Sign image
./mycontainer image verify alpine:3.18 --key=mysecret   # Verify image
./mycontainer image pull alpine:3.18                    # Pull image
./mycontainer image rm alpine:3.18                      # Delete image
```

### Container Commit
```bash
./mycontainer commit <container_id> myapp:v2 --description="Added nginx"
./mycontainer commit ls                                 # List containers
./mycontainer commit history                            # Show commit history
```

### Networking
```bash
sudo ./mycontainer network init                         # Create bridge
sudo ./mycontainer network ls                           # List topology
sudo ./mycontainer network connect web-01 db-01         # Connect containers
sudo ./mycontainer network disconnect web-01 db-01      # Disconnect
sudo ./mycontainer network inspect web-01               # Inspect
sudo ./mycontainer network destroy                      # Tear down
```

### JSON Output
Append `--json` to any command for machine-readable output:
```bash
./mycontainer image ls --json
./mycontainer image build ./examples/node-app demo:node --node --json
./mycontainer image verify demo:node --json
./mycontainer commit history --json
./mycontainer network ls --json
```

## Testing

```bash
make test-registry      # Test image registry (no root needed)
make test-commit        # Test commit (no root needed)
make test-logs          # Smoke-test logs against a fresh container
make test-stats         # Smoke-test stats against a fresh container
make test-health        # Run a real healthcheck against a fresh container
make test-checkpoint    # Checkpoint + restore a fresh container
make test-stack         # Bring the sample stack up and down
make test-dns           # Inspect generated DNS entries
make test-network       # Exercise networking state flow (Linux/WSL; sudo for real bridge setup)
make test-all           # Run all non-root smoke tests
make demo-full          # Full end-to-end demo
```

### Stack File Format

Stack files are JSON with a `"containers"` array (the key `"services"` is also
accepted for Docker Compose familiarity):

```json
{
  "name": "my-stack",
  "version": "1.0",
  "containers": [
    {
      "name": "web",
      "image": "alpine:3.18",
      "env": ["PORT=80"],
      "volumes": [],
      "network": true,
      "healthcheck": "echo ok",
      "restart": "always"
    },
    {
      "name": "db",
      "image": "alpine:3.18",
      "restart": "on-failure"
    }
  ],
  "network": {
    "connect_all": true
  }
}
```

## Project Structure

```
mycontainer/
├── include/
│   ├── container.h      # Container run declarations
│   ├── cgroups.h        # Cgroup declarations
│   ├── fs.h             # Filesystem declarations
│   ├── registry.h       # Image registry API
│   ├── commit.h         # Container commit API
│   ├── network.h        # Networking API
│   └── utils.h          # Shared utilities
├── src/
│   ├── main.c           # CLI entry + command router
│   ├── container.c      # Container run logic
│   ├── cgroups.c        # Cgroup setup
│   ├── fs.c             # Filesystem/chroot setup
│   ├── registry.c       # Image storage system
│   ├── commit.c         # Container commit logic
│   ├── network.c        # Bridge + veth networking
│   └── utils.c          # File I/O, JSON, helpers
├── registry/            # Auto-created image store
├── containers/          # Runtime container state
├── network/             # Network state
├── simulatorBridge.js   # Node.js integration bridge
├── simulatorSocketServer.js
├── Makefile
└── README.md
```

## Node.js Integration

Use `simulatorBridge.js` to call the C binary from Node.js:

```javascript
const { registry, commit, containers, network } = require('./simulatorBridge');

// List images
const images = await registry.list();

// Build a Node.js image from a local directory
await registry.build('./examples/node-app', 'demo', 'node', { node: true });

// Commit the first available container
const containerList = await commit.containers();
if (containerList.length > 0) {
  await commit.create(containerList[0].id, 'myapp', 'v2', 'Added nginx config');
}

// Start a container with privileged metadata + cpu pinning
await containers.run({
  name: 'web-01',
  image: 'alpine:3.18',
  privileged: true,
  cpuset: '0,1',
  command: ['/bin/sh'],
});

// Get network topology
const topology = await network.topology();
```

To reuse a long-lived bridge instead of spawning a fresh process per request on Linux/WSL:

```bash
node simulatorSocketServer.js
```

## Backend API

The optional Express backend now ships with working route modules for logs, stats,
health, volumes, stacks, security, DNS, exports, and checkpoints.

```bash
cd backend
npm install
npm start
```

Once started, `GET /api/healthz` should return `{"ok":true}`.

## Architecture

### Container Networking
```
Host Bridge (mycontainer0) — 172.18.0.1/24, fd42:18::1/64
      │
      ├── veth_<id1> ←→ eth0 (container 1: 172.18.0.2, fd42:18::2)
      │
      └── veth_<id2> ←→ eth0 (container 2: 172.18.0.3, fd42:18::3)
```

### OverlayFS Layers
```
merged (rootfs/)
  ├── lower/   (read-only base image)
  ├── upper/   (writable layer — changes go here)
  └── work/    (overlayfs internal)
```

### Commit Flow
```
Container upper/ → tar → merge with base → new rootfs.tar.gz → registry
```

## Notes

- **Network commands require `sudo`** — `network init`, `connect`, `disconnect`,
  and `destroy` create Linux bridges, toggle IPv6 forwarding, and manage iptables
  rules, all of which need root.  Run with `sudo ./mycontainer network init`.
  Without root the binary prints warnings and degrades gracefully.
- **Security profiles** — passing `--security=<profile>` (e.g. `--security=default`)
  makes the binary look for `security/<profile>.json` in the working directory.
  Create the profile before starting the container:
  ```bash
  mkdir -p security
  # default.json and strict.json ship with this repo
  ls security/
  ```
- **Health checks must be set at `run` time** — use `--healthcheck='<cmd>'` when
  starting the container.  Health checks cannot be added retroactively:
  ```bash
  ./mycontainer run --name=web --image=alpine:3.18 --healthcheck='echo ok' /bin/sh
  ./mycontainer health <id> --run --json
  ```
- OverlayFS needs kernel support: `grep overlay /proc/filesystems`
- Container run is still a stub — the runtime flags are recorded and exposed, but actual namespace/cgroup enforcement still needs implementation
- All JSON handling is built-in (no external dependencies)

## License

MIT
