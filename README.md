# OS

## Setup

### 1. Install [QEMU](https://www.qemu.org/)

### 2. setup the docker image

```bash
docker build buildenv -t my-os
```

### 3. Run the docker image

```bash
# Windows
# PowerShell
docker run --rm -it -v ${PWD}:/root/env my-os
# CMD
docker run --rm -it -v %cd%:/root/env my-os

# Linux
docker run --rm -it -v $(pwd):/root/env my-os

# MacOS
docker run --rm -it -v $(pwd):/root/env my-os
```

### 4. Run Makefile _IN THE CONTAINER_

```bash
make build-x86_64
```

### 5. Run the iso

```bash
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso
```
