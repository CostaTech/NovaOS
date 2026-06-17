# NovaOS Roadmap

## Core goals

- Boot on QEMU, VirtualBox and real PC live USB
- Keep keyboard and mouse drivers separated
- Use framebuffer graphics later, keep VGA text fallback stable
- Add RAM filesystem and initrd for default apps
- Add C++ desktop and app manager
- Add NovaC compiler/runtime after the kernel is stable

## Rule

One feature, one build, one test. If real hardware breaks, revert only that feature.
