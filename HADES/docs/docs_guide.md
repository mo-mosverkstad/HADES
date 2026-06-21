# HADES Documentation Guide

## Document Index

| Document | Purpose |
|----------|---------|
| `codebase_analysis.md` | Detailed technical analysis of each phase's implementation |
| `compatibility_analysis.md` | API comparison between new HADES and old programming-learning consumers |
| `history.md` | Development timeline and phase summaries |
| `hardware_study.md` | DTEK-V hardware reference and design notes |
| `demos/` | Per-demo documentation (one file per demo) |
| `hardware-reference-sheets/` | I/O device register maps and specs |

## Mandatory Documentation Steps

After completing any new phase or feature implementation in HADES:

1. **Update `codebase_analysis.md`** — Add a section documenting the new code, architecture decisions, and any bugs fixed.

2. **Update `compatibility_analysis.md`** — Analyze whether the new feature closes any gap with the old HADES API. Update the API comparison tables, mark newly-available methods, and adjust the migration checklist for dependent projects (CERBERUS, HYPNOS, HELIOS, etc.).

3. **Add demo documentation** — If a new demo was added, create `demos/demo.NN.name.md`.

4. **Update `history.md`** — Add a phase entry with date and summary.
