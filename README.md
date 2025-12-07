## cuda-ssh
GPU-enabled SSH server in Docker (CUDA/OpenCL). Minimal, production-friendly setup to expose SSH on host port 2222 and persist user home data via a named volume.

### Requirements
- Docker (Windows: Docker Desktop with WSL2)
- NVIDIA GPU + drivers
- NVIDIA Container Toolkit
- SSH client

### Quick start (Windows)
- Build and start everything:

```powershell
.\restart.bat
```

- Stop services:

```powershell
.\stop.bat
```

### Build image
```bash
docker build -t cuda-ssh .
```

### Run with docker-compose (recommended)
```bash
docker-compose up -d
```

### Run with Docker CLI
- Basic:

```bash
docker run --gpus all -d -p 2222:22 cuda-ssh
```

- With host volumes mapped to user homes:

```bash
docker run --gpus all -d -p 2222:22 \
  -v /path/on/host/user1:/home/user1 \
  -v /path/on/host/user2:/home/user2 \
  cuda-ssh
```

### CUDA/OpenCL runtime flags
```bash
docker run -d --gpus all -p 2222:22 \
  -e NVIDIA_VISIBLE_DEVICES=all \
  -e NVIDIA_DRIVER_CAPABILITIES=compute,utility \
  cuda-ssh
```

### SSH access
```bash
ssh user1@localhost -p 2222
ssh user2@localhost -p 2222
```

### Add users in a running container (superusers only)
- Enter the container with a sudo-capable account (one from `superusers.txt`):  
  `docker exec -it <container_id> bash`
- Add or update a user and optionally grant sudo:  
  `sudo /root/create_users.sh add alice 'Passw0rd!' --sudo`
- The script updates `AllowUsers` and sends a HUP to `sshd`, so SSH access for the new user becomes available immediately.

### Enter the container
```bash
docker exec -it <container_id> /bin/bash
```

### Persisted data (named volume)
The named volume is fixed: `cuda-ssh_home-data`.

- Create a backup (in the current directory), preserving permissions:

```bash
docker run --rm -v cuda-ssh_home-data:/from -v ${PWD}:/backup \
  alpine sh -c "cd /from && tar -czf /backup/home-data.tar.gz ."
```

- Restore on another machine:

```bash
docker volume create cuda-ssh_home-data
docker run --rm -v cuda-ssh_home-data:/to -v ${PWD}:/backup \
  alpine sh -c "cd /to && tar -xzf /backup/home-data.tar.gz"
```

- Inspect the volume mountpoint (optional):

```bash
docker volume inspect cuda-ssh_home-data --format "{{.Mountpoint}}"
```

- Windows (Docker Desktop, WSL2) physical paths:
  - `\\wsl$\docker-desktop-data\data\docker\volumes\cuda-ssh_home-data\_data`
  - On some versions: `\\wsl$\docker-desktop-data\version-pack-data\community\docker\volumes\cuda-ssh_home-data\_data`