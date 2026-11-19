# Installing your own build on an M-VAVE FM-1 (macOS)

How to build this branch on a Mac and put it on an M-VAVE FM-1. It works from the official
M-VAVE firmware, from a released SLOOP or from an earlier build of your own. Your projects,
user presets, samples and settings are kept.

> Custom firmware is installed at your own risk. The way back to the official firmware is at
> the end of this page.

## What you need

- A Mac with this repo, Python 3 and the project's `.venv` (Pillow, `mido`, `python-rtmidi`).
- Colima (or Docker Desktop), the JieLi toolchain in `~/.jieli/toolchain` and the three AC79 SDK
  files in `~/fw-AC79_AIoT_SDK`. Setting these up once is covered in [BUILDING.md](BUILDING.md)
  and in [the one-time setup below](#one-time-setup-if-it-is-gone).
- **Chrome or Edge** if you use the browser installer. Safari and Firefox have no Web MIDI.
- A USB **data** cable plugged straight into the Mac, with no hub. A charge-only cable shows no
  MIDI port at all.

## 1. Before you install: a backup

If the FM-1 already runs SLOOP, open the [web editor](https://isod89.github.io/sloop-fm1/webapp/editor/)
in Chrome, press **Connect**, and save a backup. An install keeps your data, but the backup is
your copy if anything goes wrong. On the official firmware there is nothing to back up.

## 2. Build

Start the Linux VM the build runs in, then build. Run these from the repo folder:

```bash
colima start
```

```bash
PYTHON=.venv/bin/python ./build.sh
```

The last lines name the package, for example:

```
package  .../build/felucca.fwsc  610076 B, identity FM-1_900
```

`build/felucca.fwsc` is the file you install. When you're done building, `colima stop` frees the
VM's memory.

### The SLOOP JR (kid) build

The same command with the kid flag builds SLOOP JR (see `docs/KID-MODE-*.md`):

```bash
FELUCCA_KID=1 PYTHON=.venv/bin/python ./build.sh
```

Two things to know first:

- **It's early.** It first ran on a real FM-1 on 2026-11-19, and mission 1 had bugs that the host
  tests missed (fixed on 2026-11-20). Later missions have been checked only in the host tests. Put the
  normal build on first and check that it plays. Then try SLOOP JR, and keep the normal
  `.fwsc` to hand so you can go back.
- **It only just fits.** With the picture splash the image overflows flash (by 1,016 bytes on
  2026-11-20). The build script then retries by itself with the text logo
  (`FELUCCA_LOGO_TEXT=1`), and that fits with about 3.5 KB to spare. So SLOOP JR starts with a
  text logo, and there's very little room left for more kid features.

Both builds write the same file (`build/felucca.fwsc`) with the same identity (`FM-1_900`).
Copy the one you want to keep before you build the other:

```bash
cp build/felucca.fwsc ~/Desktop/sloop-jr.fwsc
```

## 3. Install

Close every other app that uses MIDI first: Ableton Live, M-UPGRADE, other editor tabs. Then
choose **A** or **B**.

### A. Browser installer, served from your Mac

Make a local copy of the installer site with your build in it, and serve it:

```bash
.venv/bin/python web/make_site.py build/felucca.fwsc dev /tmp/sloop-site
```

```bash
python3 -m http.server 8766 --bind 127.0.0.1 --directory /tmp/sloop-site
```

1. Open <http://localhost:8766/webapp/installer/> in **Chrome or Edge**. It has to be
   `localhost`: Web MIDI needs a secure page, and a file opened from disk doesn't count.
2. Plug the FM-1 in and switch it on.
3. Press **INSTALL** and allow MIDI access (with SysEx) when Chrome asks.
4. Wait for **Done**. The FM-1 restarts on the SLOOP logo (a text logo for SLOOP JR).

Stop the server with Ctrl+C, then delete the temporary site:

```bash
rm -rf /tmp/sloop-site
```

The public installer at <https://isod89.github.io/sloop-fm1/> installs the official SLOOP
release, not your build.

### B. Command line

```bash
.venv/bin/python tools/fm1_install.py build/felucca.fwsc
```

It shows the FM-1 it found and asks before writing. It then starts the update loader, writes the
package, waits for the restart and checks the installed identity. To see what is on the FM-1
without installing anything:

```bash
.venv/bin/python tools/fm1_install.py --info
```

If it says `FM-1 not found`, check the cable, switch the FM-1 on, and close other MIDI apps. If
only `IAC Driver Bus 1` is listed under MIDI inputs, the Mac can't see the FM-1 at all.

## 4. After the install

- **Unplug the FM-1 and plug it back in once.** Only then does macOS find its USB audio input
  ("Felucca"). MIDI works without this step.
- Play a busy pattern for a minute: all four tracks, drums, a GRAIN or SAMPLE sound. Listen for
  clicks or dropouts. The host tests can't hear the device, so this is the real check of a new
  build.

## If something goes wrong

| What happens | What to do |
| --- | --- |
| The install stopped half-way (cable out, Mac asleep) | The FM-1 waits in update mode. Run the same install again and it finishes. |
| The FM-1 no longer starts SLOOP | Hold **OCT−** alone while switching on. It shows *SLOOP USB RESCUE*. Then install again. |
| The FM-1 doesn't start at all | Recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter). |
| The installer can't find the FM-1 | Chrome or Edge only, a data cable, no hub, MIDI access allowed, other MIDI apps closed. Reload the page. |

## Going back to the official firmware

1. Save a backup with the editor.
2. Get **M-UPGRADE** from m-vave.com → Downloads → PC Software, and the official firmware from
   PC Firmware → FM-1 → **V16** (`FM-1.fwsc`).
3. Close every other MIDI app, open M-UPGRADE and install `FM-1.fwsc`.

If M-UPGRADE doesn't list the FM-1, open Clipper's installer page
(<https://isod89.github.io/clipper-fm1/webapp/installer/>) and use **Return to the official
firmware** with the same `FM-1.fwsc`. You don't need to install Clipper for this.

To go back to an official SLOOP release instead, install its `.fwsc` from the
[releases page](https://github.com/isod89/sloop-fm1/releases), or use the public installer.

## One-time setup, if it is gone

These are only needed if the toolchain, the SDK files, the VM or the venv have been removed.

```bash
tools/get_toolchain.sh
```

The build needs only three files of the AC79 SDK, so a sparse clone fetches just those, in
seconds:

```bash
git clone --depth 1 --filter=blob:none --no-checkout --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
```

```bash
cd ~/fw-AC79_AIoT_SDK && git sparse-checkout set --no-cone /cpu/wl82/tools/uboot.boot /cpu/wl82/tools/cfg_tool.bin /cpu/wl82/tools/cfg/eq_cfg_hw.bin && git checkout
```

A new Colima VM. Apple's built-in virtualisation plus Rosetta runs the Linux x86-64 toolchain:

```bash
colima start --vm-type vz --vz-rosetta --cpu 4 --memory 6 --disk 30
```

The Python packages:

```bash
python3 -m venv .venv && .venv/bin/pip install Pillow mido python-rtmidi
```

If the build stops on `docker-credential-desktop: executable file not found`, the build image
needs fetching again, and `~/.docker/config.json` still points at Docker Desktop's login helper
(`"credsStore": "desktop"`). Fetch the public image once without it:

```bash
mkdir -p /tmp/sloop-dockercfg && echo '{}' > /tmp/sloop-dockercfg/config.json && DOCKER_CONFIG=/tmp/sloop-dockercfg DOCKER_HOST=unix://$HOME/.colima/default/docker.sock docker pull --platform linux/amd64 debian:bookworm-slim && rm -rf /tmp/sloop-dockercfg
```
