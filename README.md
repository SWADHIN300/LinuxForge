# LinuXForge 🐧⚡

A full-stack **Linux container simulator** built from scratch — featuring a C-powered container engine, a Node.js bridge, and a Next.js 14 management dashboard styled with Material Design 3.

![Dashboard](./frontend/public/preview.png)

---

## 🗂 Project Structure

```
LinuXForge/
├── src/                    # C source — container engine (namespaces, cgroups, seccomp)
├── include/                # C header files
├── backend/                # Express.js REST API server
├── frontend/               # Next.js 14 + TailwindCSS dashboard
│   └── app/
│       ├── page.js         # Dashboard
│       ├── logs/           # Real-time log streaming (SSE)
│       ├── stats/          # Performance metrics
│       ├── health/         # Container health monitor
│       ├── volumes/        # Volume mount manager
│       ├── stacks/         # Stack deployment manager
│       ├── network/        # Network topology visualizer
│       ├── registry/       # Container image registry
│       ├── security/       # Seccomp security profiles
│       ├── dns/            # Container DNS manager
│       ├── checkpoints/    # CRIU checkpoint manager
│       └── commit/         # Container commit & image pipeline
├── simulatorBridge.js      # Node.js ↔ C engine bridge (JSON protocol)
├── simulatorSocketServer.js# WebSocket server for live stats
├── registry/               # Image layer storage
├── containers/             # Runtime container state (JSON)
├── checkpoints/            # CRIU checkpoint snapshots
├── stacks/                 # Stack definition files
├── security/               # Seccomp profile JSONs
├── logs/                   # Container log files
├── stats/                  # Performance stat snapshots
├── examples/               # Example stack YAML/JSON files
├── mycontainer             # Compiled engine binary (Linux)
├── mycontainer.exe         # Compiled engine binary (Windows cross-compile)
└── Makefile                # Build system
```

---

## 🚀 Features

### Container Engine (C)
- **Linux namespaces** — PID, NET, MNT, UTS, IPC, User isolation
- **cgroups v2** — CPU, memory, and PID limits
- **Seccomp filtering** — per-container syscall allowlists
- **Overlay filesystem** — layered image + writable container layer
- **Network** — virtual ethernet pairs, bridge networking, iptables NAT

### Simulator Bridge (Node.js)
- JSON-RPC protocol between the frontend and the C engine
- Supports: `run`, `stop`, `exec`, `logs`, `commit`, `checkpoint`, `restore`, `network connect/disconnect`
- WebSocket server broadcasting live CPU/memory stats

### Management Dashboard (Next.js 14)
| Page | Description |
|------|-------------|
| **Dashboard** | Overview with network topology, container table, resource summary |
| **Logs Viewer** | Real-time SSE log streaming with level filtering |
| **Performance** | CPU/memory charts with live updates |
| **Health Monitor** | Container health cards with 24h check history |
| **Volume Mounts** | Mount point inspector and log viewer |
| **Stack Manager** | Deploy multi-container stacks with a YAML editor |
| **Network Topology** | Interactive SVG node graph with connection manager |
| **Container Registry** | Image layer explorer with push workflow |
| **Security Profiles** | Seccomp profile editor and container assignment |
| **Container DNS** | DNS record manager with `/etc/hosts` preview |
| **Checkpoints** | CRIU snapshot timeline with restore workflow |
| **Commit Snapshot** | Container-to-image pipeline with version history |

---

## 🛠 Tech Stack

| Layer | Technology |
|-------|-----------|
| Engine | C (Linux syscalls, namespaces, cgroups, seccomp) |
| Bridge | Node.js 20, child_process, WebSocket (ws) |
| API | Express.js, Server-Sent Events |
| Frontend | Next.js 14, React 18, TailwindCSS |
| Design | Material Design 3 dark theme (Stitch tokens) |
| Fonts | Space Grotesk, Inter, JetBrains Mono, Material Symbols |

---

## ⚡ Quick Start

### 1. Build the engine (Linux / WSL)
```bash
make
```

### 2. Start the backend
```bash
node simulatorBridge.js     # Starts C engine bridge
# or
cd backend && npm install && npm start
```

### 3. Start the dashboard
```bash
cd frontend
npm install
npm run dev
# → http://localhost:3000
```

---

## 🔌 API Reference

The Express backend exposes these endpoints (default port **3001**):

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/containers` | List all containers |
| `POST` | `/api/containers/run` | Start a new container |
| `POST` | `/api/containers/:id/stop` | Stop container |
| `POST` | `/api/containers/:id/exec` | Exec command in container |
| `GET` | `/api/containers/:id/logs` | Stream logs (SSE) |
| `GET` | `/api/health` | Container health statuses |
| `GET` | `/api/stats` | Live CPU/mem stats |
| `GET` | `/api/volumes` | Volume mounts |
| `GET` | `/api/networks` | Network topology |
| `POST` | `/api/networks/connect` | Connect containers |
| `GET` | `/api/registry/images` | List registry images |
| `POST` | `/api/commit` | Commit container to image |
| `POST` | `/api/checkpoints` | Create CRIU checkpoint |
| `POST` | `/api/checkpoints/:id/restore` | Restore checkpoint |

---

## 🗺 Roadmap

- [ ] Live WebSocket charts on Performance page
- [ ] Real seccomp profile apply via bridge
- [ ] Multi-node cluster simulation
- [ ] Container log persistence with search
- [ ] Stack YAML import/export

---

## 📄 License

MIT © SWADHIN300
