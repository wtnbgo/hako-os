# hako documentation

[日本語](../ja/README.md)

hako is an environment for running Debian trixie (arm64) on Qualcomm-based handheld game consoles that can boot via OpenABL,
and for putting your own apps on them. The only supported device right now is the **Retroid Pocket Mini (V1, SM8250)**.

## Reading order

| # | Document | Contents |
|---|---|---|
| 1 | [1-device-setup.md](1-device-setup.md) | From backup → OpenABL → writing the dev image to SD and booting it (Retroid Pocket Mini only) |
| 2 | [2-development-and-samples.md](2-development-and-samples.md) | Day-to-day steps for installing and running apps on the device, and how to use each sample (SDL3 / LÖVE / raylib / Siv3D / Kirikiri Z) |
| 3 | [3-packaging-an-app.md](3-packaging-an-app.md) | Turning what you made into a .deb, and bundling it into a single image |
| 4 | [4-new-boards.md](4-new-boards.md) | Preparing an image for another device of the same family (OpenABL + Qualcomm) |
| - | [reference.md](reference.md) | Details on structure, packages, configuration and troubleshooting |

## Working with an AI agent

This environment is designed on the assumption that you work together with an AI coding agent (Claude Code, etc.).
The agent handles building, transferring to the device, checking logs and taking screenshots; the human operates the device and confirms the results.

- [AGENTS.md](../../AGENTS.md) at the repository root (Claude Code reads it via CLAUDE.md) contains a map and the rules
  the agent must follow. With that and docs/, an agent can start working.
- Each document includes examples of how to ask an agent. Paste them as-is, or adapt them to your situation.
- A rough division of roles:

| Leave to the agent | Done by the human |
|---|---|
| Building, packaging, transferring to and installing on the device | Writing SD cards, plugging/unplugging cables, power operations |
| Investigating logs, configuration and source; drafting fixes | Checking buttons, touch, sound and appearance |
| Taking screenshots (grim) and checking their content | Deciding whether to write to the bootloader or partitions |
| Updating documentation, committing | Deciding on publishing or distribution |

- If you ask the agent to "for checks on the device that need human operation, explain the steps and wait for a reply before starting",
  tests won't proceed on their own while you aren't looking at the screen (this is also written in AGENTS.md).

## Terminology

| Term | Meaning |
|---|---|
| OpenABL | A replacement for the bootloader (ABL) of Qualcomm devices. Makes it possible to boot Linux from SD, etc. |
| ESP | The FAT partition at the start of the SD card. Holds the kernel, initrd, dtb and boot entries |
| ELBE | A tool that assembles images from Debian packages. Builds inside a VM (initvm) |
| bsp | Board Support Package. A package bundling device-specific pieces (`rpmini-bsp`) |
| `.app` | A small configuration file that registers an app with the launcher |
| dev image | The development image. Includes ssh and debugging tools; root is read-write |
