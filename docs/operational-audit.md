# HomePi Operational Audit

Audit date: 2026-06-25 (documentation), implementation follow-up 2026-06-25. Scope: fresh Pi installation path, SSH safety on reboot, and documentation alignment with the current repository.

## Executive summary

| Area | Status | Notes |
|------|--------|-------|
| Fresh Pi install path | **Complete** | `install-operational.sh` installs SSH hardening, events broker, eight native services (incl. paging), and web stack |
| SSH on reboot | **Low risk when hardened** | `homepi-ensure-ssh` installed automatically after preflight; masks `ssh.socket`, repairs key perms on boot |
| Documentation vs code | **Aligned** | Docs and install scripts match the full operational stack |

## 1. Fresh Pi installation path

### Recommended sequence (matches current code)

1. **OS imaging** — Raspberry Pi OS 64-bit, user `homepi`, SSH enabled, public key in `~/.ssh/authorized_keys`
2. **Clone repo** — e.g. `/home/homepi/homepi`
3. **One-command stack install** — `sudo bash scripts/install-operational.sh` (includes SSH hardening + paging)
4. **UI USB assignment** — Settings → Audio Configuration → Save
5. **Post-reboot verification** — `bash scripts/verify-post-reboot.sh`

### What `install-operational.sh` installs

| Component | Installed by script | systemd unit |
|-----------|---------------------|--------------|
| Preflight backups | Yes | — |
| SSH boot hardening | Yes | `homepi-ensure-ssh` |
| apt prerequisites (incl. Mosquitto, NGINX, Avahi) | Yes | `mosquitto`, `nginx`, `avahi-daemon` |
| Monorepo build | Yes | — |
| Core events broker | Yes | `homepi-events` |
| Native services (ordered) | Yes | see below |
| NGINX site + backend | Yes | `nginx`, `homepi-backend` |
| Avahi mDNS alias | Yes | `avahi-homepi-alias` |
| Operational verification | Yes | — |

**Native service install order** (`scripts/install-services.sh`):

1. `homepi-usb-devices`
2. `homepi-nqptp`
3. `homepi-pcm-router`
4. `homepi-metadata`
5. `homepi-hifi-serial`
6. `homepi-shairport-sync` (supervisor + zone template)
7. `homepi-audio-orchestrator`
8. `homepi-audio-paging`

### Standalone install (optional)

| Component | Command |
|-----------|---------|
| SSH hardening only | `sudo bash scripts/install-ensure-ssh.sh` |
| Audio paging only | `sudo bash services/homepi-audio-paging/scripts/install.sh` |

### Install-time safety flags (unchanged)

- `HOMEPI_INSTALL_MODE=1` — skips primary-audio modprobe deploy during install
- `HOMEPI_ALLOW_REBOOT=0` — no automatic reboot during install or post-assignment hook by default

## 2. SSH and service race-condition analysis

### Findings

**No ordering conflict with SSH.** No HomePi systemd unit declares `Before=ssh.service`, `Conflicts=ssh`, or modifies `sshd_config`/firewall during boot.

**SSH starts before HomePi hardening.** On a representative boot:

- `ssh.service` becomes active after `network.target` (~9 s)
- `homepi-ensure-ssh.service` runs after `network-online.target` (~13 s)

SSH is reachable before the hardening oneshot runs. The oneshot does not block SSH startup.

**`ssh.socket` vs `ssh.service` (main Pi OS pitfall).** Raspberry Pi OS may enable socket-activated SSH. If `ssh.socket` is enabled without a reliably enabled `ssh.service`, behavior after reboot can be inconsistent. `scripts/ensure-ssh-access.sh` masks `ssh.socket` and enables/starts `ssh.service`. This is the primary SSH-related hardening step.

**`network-online.target` delays some services, not SSH.** Units that `Wants=`/`After=` `network-online.target` (pcm-router, nqptp, shairport-supervisor, audio-orchestrator, homepi-ensure-ssh) wait for NetworkManager “online” — typically a few seconds after SSH is already listening. This does not remove SSH access.

**USB assignment reboot is opt-in.** `post-assignment-hook.sh` only calls `systemctl reboot` when `HOMEPI_ALLOW_REBOOT=1`. Default is `0`. A controlled reboot after assignments is documented in the fresh Pi runbook; it does not race with SSH unless keys or sshd config were corrupted separately.

**Authorized key permissions.** Strict `sshd` rejects `authorized_keys` if `~/.ssh` is not `700` or keys are not `600`. Preflight warns; `ensure-ssh-access.sh` repairs on boot when the ensure-ssh unit is installed.

### Boot dependency sketch

```text
network.target ──► ssh.service          (SSH listening, ~9s)
       │
       └──► network-online.target ──► homepi-ensure-ssh (oneshot, ~13s)
                                   └──► pcm-router, nqptp, shairport-supervisor, …
```

### Recommendations

1. Keep `HOMEPI_ALLOW_REBOOT=0` until USB/audio assignments are verified stable.
2. After any controlled reboot, run `bash scripts/verify-post-reboot.sh` (includes SSH active/enabled check).
3. Retain serial console or physical access until post-reboot verification passes.

## 3. Documentation gaps corrected in this audit

| Document | Issue | Resolution |
|----------|-------|------------|
| `README.md` | Listed six native services | Updated to full operational stack |
| `scripts/scripts-install-readme.md` | Same; no ensure-ssh or paging steps | Expanded service table, SSH hardening install, paging install |
| `scripts/fresh-pi-runbook.md` | Missing Phase 0, paging, SSH hardening | Added phases and verification rows |
| `docs/architecture/sockets-and-ports.md` | Missing events, metadata, realtime, paging sockets | Updated socket table |
| `docs/architecture/service-status.md` | Referenced `homepi-metadata@N` as primary | Updated to single `homepi-metadata` unit |
| `infra/nginx/infra-nginx-readme.md` | Said `https://homepi.local` | Corrected to `http://` (no TLS in template) |
| `apps/backend/app-backend-readme.md` | “Not Included Yet” contradicted current integration | Removed stale section |
| `services/native/services-native-readme.md` | Empty service inventory | Added full native service list |

### Implementation follow-up (2026-06-25)

| Change | Script |
|--------|--------|
| SSH hardening wired into install | `scripts/install-ensure-ssh.sh` ← called from `install-operational.sh` |
| Paging added to native install order | `scripts/install-services.sh` |
| Uninstall parity | `uninstall-services.sh` + `uninstall-operational.sh` (events, ensure-ssh) |
| Verify checks ensure-ssh + masked socket | `verify-operational.sh` |

`build-native-services.sh` remains a dev subset (four services); production uses per-service `install.sh` scripts.

## 4. Complete operational unit checklist

After `install-operational.sh`, `verify-operational.sh` expects these units **active**:

| Unit | Role |
|------|------|
| `nginx` | Web gateway |
| `homepi-backend` | Node API |
| `homepi-events` | Core event broker |
| `avahi-daemon`, `avahi-homepi-alias` | mDNS |
| `mosquitto` | MQTT (Shairport remote) |
| `homepi-usb-devices` | USB inventory and assignments |
| `homepi-nqptp` | AirPlay 2 timing |
| `homepi-pcm-router` | PCM routing |
| `homepi-metadata` | Metadata + realtime progress |
| `homepi-hifi-serial` | HiFi controller |
| `homepi-shairport-supervisor` | AirPlay zone supervisor |
| `homepi-audio-orchestrator` | AirPlay/PCM lifecycle orchestration |
| `homepi-audio-paging` | Voice paging and chimes |
| `homepi-ensure-ssh` | SSH hardening oneshot (active after boot) |
| `ssh.service` (or `sshd.service`) | Remote access |

## Related docs

- [scripts/fresh-pi-runbook.md](../scripts/fresh-pi-runbook.md)
- [scripts/scripts-install-readme.md](../scripts/scripts-install-readme.md)
- [docs/architecture/sockets-and-ports.md](./architecture/sockets-and-ports.md)
- [docs/architecture/service-status.md](./architecture/service-status.md)
