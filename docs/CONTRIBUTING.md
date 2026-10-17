# Contributing

1. Read [AGENTS.md](../AGENTS.md) for architecture, conventions and rules.
2. Build with `-DGE_WARNINGS_AS_ERRORS=ON`; run `ctest` in Debug and Release and once with `-DGE_ASM=OFF`.
3. Add tests with every behaviour change; keep them deterministic and display-free.
4. Keep changes focused; do not implement planned features speculatively.
5. Update README.md, PROJECT_STATUS.md and the relevant docs when behaviour or status changes. Describe only what you have run.

The `LICENSE` file is a placeholder pending a decision by the repository owner.
