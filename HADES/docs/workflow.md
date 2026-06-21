# HADES Development Workflow

## Implementation Cycle

For each new phase or feature:

```
1. Implement C++ code (src/hardware/)
2. Add pybind11 bindings (src/bridge/bindings.cpp)
3. Write demo (src/demos/)
4. Build and test: make engine && make test
5. Run demo: make demo-XX
6. Document (see below)
7. Git commit and push
```

## Documentation Workflow (Mandatory)

After every implementation is complete and tested:

### Step 1: Update `docs/codebase_analysis.md`
- Add a new section for the phase/feature
- Document architecture, key code paths, and any bugs found and fixed

### Step 2: Update `docs/compatibility_analysis.md`
- Check which APIs from the old HADES (`programming-learning/HADES/`) are now covered
- Move newly-implemented methods from the "Missing" tables to the "Present in both" table
- Update the effort estimates and migration checklist
- If a dependent project (CERBERUS, HYPNOS, HELIOS, HYPERION, IronGuard, HYPNOS-Net) can now be migrated, note it

### Step 3: Demo documentation
- Create `docs/demos/demo.NN.feature-name.md` if a new demo was added

### Step 4: Update `docs/history.md`
- Add date + one-line summary of the phase

## Why Compatibility Analysis Matters

The new HADES is a rewrite. Multiple projects in `programming-learning/` embed copies of the old HADES and depend on its API. The compatibility analysis tracks:
- What old APIs are still missing (blocking migration)
- What has been ported (enabling migration)
- Per-project readiness status

This ensures we know at all times which projects can switch to the new HADES and what remains to be done.
