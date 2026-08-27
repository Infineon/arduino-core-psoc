# Dev Container Setup

This repository uses the default dev container configuration in `devcontainer.json`.
It runs as the non-root `vscode` user with the project workspace mounted at
`/home/vscode/myLocalWorkingDir`, the host `/dev` tree bound into the container, and
privileged mode enabled so board tools and USB access can work from inside the
container. Dev Containers updates the `vscode` UID/GID to match the local
workspace owner, which keeps bind-mounted file ownership aligned with the host.

The container image is built from the Dockerfile in the `extras/makers-docker` Git submodule.

## How to use

1. **Initialize submodules** (first time only):
   ```sh
   git submodule update --init --recursive
   ```

2. In VS Code, run "Dev Containers: Reopen in Container" or "Open Folder in Container...".
3. Select the repository root and let the container build.
4. The `postCreateCommand` runs automatically and executes the repo setup scripts.

If you need to rerun the setup manually from the repository root:

```sh
bash .devcontainer/post-create.sh
```

You can also run the individual steps directly:

```sh
bash .devcontainer/scripts/git-setup.sh
bash .devcontainer/scripts/usb-check.sh
bash .devcontainer/scripts/dev-setup.sh
```

## What the setup does

The container bootstrap runs the following scripts in order:

1. **`scripts/git-setup.sh`** — Verifies Git is available and working.
2. **`scripts/usb-set-kitprog3.sh`** — Configures USB permissions for KitProg3 programmers (Cypress/Infineon boards).
3. **`scripts/usb-check.sh`** — Diagnoses USB device visibility (serial ports and USB bus access).
4. **`scripts/dev-setup.sh`** — Runs core setup, checks Arduino CLI, and links the repo as a local Arduino core.

### When to run individual scripts:

- **After changing Git credentials or submodule state:** `bash .devcontainer/scripts/git-setup.sh`
- **When attaching a board or troubleshooting serial/USB access:** `bash .devcontainer/scripts/usb-check.sh`
- **After changing the core, Arduino CLI, or sketchbook configuration:** `bash .devcontainer/scripts/dev-setup.sh`

See the comments in each script for implementation details.

## Configuration Reference

The `devcontainer.json` file contains the following configuration options:

| Option | Value | Purpose |
|--------|-------|---------|
| **name** | `PSOC6 Arduino Core (makers-docker)` | Display name shown in VS Code |
| **dockerFile** | `../extras/makers-docker/Dockerfile.test` | Path to the Dockerfile in the makers-docker submodule used to build the container image |
| **workspaceFolder** | `/home/vscode/myLocalWorkingDir` | Mount point for the repository inside the container |
| **workspaceMount** | Bind source with `consistency=cached` | Mounts the host workspace into the container with optimized performance on macOS/WSL |
| **remoteUser** | `vscode` | User that VS Code connects as when attached to the container |
| **updateRemoteUserUID** | `true` | Automatically updates the `vscode` user's UID/GID to match the host user, ensuring file permissions are aligned |
| **features** | `common-utils:2` with `username=vscode` | Installs common devcontainer utilities for the `vscode` user |
| **privileged** | `true` | Enables privileged mode so the container can access USB devices and board programmers |
| **mounts** | `/dev` → `/dev`, `$HOME/.ssh` → `/home/vscode/.ssh` | Exposes host device files and SSH keys to the container |
| **runArgs** | `--init` | Runs the container with PID 1 init process (proper signal handling) |
| **postCreateCommand** | `bash .devcontainer/post-create.sh` | Runs setup scripts automatically after the container is created |
| **customizations.vscode.extensions** | C/C++, Python, Pylance | VS Code extensions installed in the container |

### Key configuration notes:

- **`remoteUser` + `updateRemoteUserUID`** — Ensures the container user matches the host user, preventing permission issues with bind-mounts.
- **`privileged`** — Required for USB device access (programmers, serial ports). Should only be used with trusted Dockerfiles.
- **`mounts`** — `/dev` binding enables direct access to hardware; SSH key binding supports Git/remote operations without credential re-entry.
- **`workspaceMount`** — `consistency=cached` improves performance on Docker Desktop (macOS/Windows) by reducing sync overhead.

## Notes

- The dev container uses the makers Docker image and includes Git support.
- The container runs as `vscode`; host SSH keys are mounted at `/home/vscode/.ssh`.
- The default container is intentionally privileged and binds `/dev` to support USB serial devices and board programming workflows.
- If `/dev/bus/usb` or `/dev/ttyACM*` devices are not visible, verify the host exposes them and reopen the container after attaching the board.
- On WSL, attach boards with `usbipd attach --wsl --busid <BUSID>` before reopening the container. On native Linux, ensure the host user can access the board and that the relevant USB devices are exposed to the container.
