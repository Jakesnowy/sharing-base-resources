# IntegratedStorageCpp Audit Report — v4.1.2-audited

**Audit date:** 2026-08-13  
**Target:** `Jakesnowy/sharing-base-resources` fork of Sarfflow's IntegratedStorageCpp  
**Goal:** Identify correctness, performance, and deployment issues; make the mod work out-of-the-box for single-player and co-op with minimal configuration.

---

## Executive Summary

The fork contains valuable fixes (P0-a, P0-b, P0-c) and an optional external TCP channel, but it shipped with a **critical bug that prevented the three UI hooks from ever being installed**. The signature-scan helper discarded the found address and created detours at address `0`, so the client-side injection (`collector`, `catalog`, `placement`) silently failed. This audit repairs that bug and hardens the surrounding code.

**Verdict after fixes:** Suitable for single-player and small-to-medium co-op. For larger dedicated servers, the optional TCP channel can be enabled, but the mod still **must be installed on every client** for the shared pool to be visible.

---

## Critical Bug Fixed

### Signature scan discarded the result
**Location:** `src/dllmain.cpp`, `ModIntegratedStorageCpp::on_unreal_init`

**Original code:**
```cpp
auto maybe = [&](bool en, Target& t, uint64_t cb) {
    if (en) {
        scanSig(parseSig(t.sig), new int);          // result thrown away
        t.det = new PLH::x64Detour(t.addr, cb, &t.tramp); // t.addr was still 0
        t.hooked = t.det->hook();
    }
};
```

**Problems:**
- `t.addr` was never assigned, so the detour was created at `0x0`.
- `new int` leaked on every hook attempt.
- No validation that the signature matched exactly once.

**Fix:**
```cpp
int count = 0;
t.addr = scanSig(parseSig(t.sig), &count);
if (!t.addr) { /* log error, skip */ return; }
if (count != 1) { /* warn about multiple matches */ }
t.det = new PLH::x64Detour(t.addr, cb, &t.tramp);
t.hooked = t.det->hook();
```

This is the single most important change; without it the mod's client-side UI injection cannot function.

---

## Additional Fixes & Hardening

| Area | Change | Rationale |
|------|--------|-----------|
| **External channel default** | `g_extEnabled` now defaults to `false` in code (matching `config.txt`) | Previously the code defaulted to `true` and only the config file set it false. If `config.txt` was missing, the mod would try to open sockets unnecessarily. |
| **Role detection** | Added `g_roleMutex`, `g_lastRoleCheck` cooldown (2 s) | `ensureRole` was called every frame until success, each call running `ProcessEvent`. Now it retries at most every 2 seconds and is thread-safe. |
| **Camp-state caching** | Added `g_campMutex` around `clientInCamp` statics | The function used function-static locals that are not thread-safe. |
| **Minted-slot lifecycle** | `mintPoolSlots` now rebuilds if any existing slot became invalid | Prevents stale UObject pointers from being injected into a container. |
| **Container injection safety** | `injectMintedFast` / `restoreMinted` validate `slots->data` and saved array before restoring | Reduces risk of corrupting a container if the donor array was reallocated. |
| **Class-check cache** | `srvClassIs` caches `PalMapObjectItemChestModel` and `PalBaseCampModuleItemStorage` class pointers, invalidated on world change | Avoids walking the inheritance chain on every chest during reconcile. |
| **Network startup** | `netStart()` now returns `bool`; `g_netStarted` is only set on success | Failed binds no longer permanently disable retry. |
| **Logging** | Added `Output::send` startup / role / hook / network logs | Makes deployment troubleshooting possible via `UE4SS.log`. |
| **Dead code** | Removed unused `g_isiRefreshMs` | The config key `isi_refresh_ms` was never read. |
| **ws2_32 link** | Added `ws2_32` to `src/CMakeLists.txt` | The TCP channel uses Winsock2; the CMake build was missing this link dependency. |

---

## Performance Optimization: Per-Camp Item Totals

**Location:** `srvDiscoverReconcileInner` / `srvBuildForCamp`

Previously, every client request caused the server to scan every chest instance in the world, read its container slots, and sum items by guild. For large servers this was `O(chests)` per request.

**Change:** During reconcile, the server now builds `g_campTotals`: a map from camp GUID to aggregated item counts. `srvBuildForCamp` then sums only camps in the same guild, making replies `O(camps)`.

This is especially helpful for the use case the user cares about: **dedicated larger servers** with many bases.

---

## Deployment Reality Check: Host-Only Is Not Currently Possible

The user's ideal goal is "only installed on the host." This is **not achievable with the current architecture**, and the documentation previously implied otherwise.

Why:
- The server-side cross-registration (`OnAvailableConcreteModel_ServerInternal`) tells each camp's storage module about chests in *other* camps of the same guild.
- Palworld's native replication does **not** automatically merge those remote chests into the client's crafting/building UI.
- Therefore the mod injects a virtual "shared pool" on the **client** via three signature-scanned UI hooks (`collector`, `catalog`, `placement`).
- Without the mod on the client, the client simply has no code to display the shared items.

**Added:** `client_inject_enabled = true` config option. Setting it to `false` disables client-side injection and leaves only server-side cross-registration. This is exposed as a **developer/testing option** so users can verify whether a future game update enables native replication, but it is not a usable end-user mode today.

**Recommendation:** Continue installing the same DLL on every machine (host/server + all clients). The default config is zero-config (`external_channel = false`), so end users only need to copy files.

---

## Configuration Changes

`dist/ModIntegratedStorageCpp/config.txt` was updated:
- Header now reads `v4.1.2-audited`.
- Added `client_inject_enabled = true` with a clear explanation that it must remain enabled on clients.
- Added a prominent note that the mod must be installed on every client.

---

## Build / CI

The GitHub workflow (`.github/workflows/build.yml`) was already repaired in a prior commit to use the working CMake-based build instead of the broken xmake conversion. `src/CMakeLists.txt` now links `ws2_32` so the CMake CI produces a working DLL.

---

## Remaining Risks & Future Work

| Risk | Mitigation | Future Work |
|------|------------|-------------|
| Hardcoded UObject offsets can break with game updates | Pin RE-UE4SS commit and document the target game version | Use RE-UE4SS reflection helpers where possible |
| Client-side injection is fragile by nature | Validate donor container before/after swap | Investigate hooking container-specific functions instead of a global donor |
| TCP channel has no authentication | Disabled by default; only use on trusted LAN | Add a shared secret / token if WAN use is desired |
| `external_channel = true` requires manual IP config | Documented in `USAGE_GUIDE.md` | Add LAN discovery / UPnP if host-only remains impossible |
| Host-only deployment | Not supported today | Would require deeper engine-level container merging on the server |

---

## Validation Checklist After Audit

- [ ] `src/dllmain.cpp` compiles with both CMake CI and local `build.ps1`.
- [ ] `[ISGATE] === IntegratedStorageCpp v4.1.2-audited loaded ===` appears in `UE4SS.log`.
- [ ] `[ISGATE] Hooked ... OK` appears for `collector`, `catalog`, `placement`.
- [ ] Single-player: shared materials display, food consumption works.
- [ ] Co-op (same DLL on all machines, default config): shared pool works for 2+ players.
- [ ] Optional TCP channel: server binds, clients connect, deltas sync.

---

## Files Changed

- `src/dllmain.cpp` — critical bug fix, hardening, performance optimization, logging
- `src/CMakeLists.txt` — added `ws2_32`
- `dist/ModIntegratedStorageCpp/config.txt` — added `client_inject_enabled`, updated version header
- `.github/workflows/build.yml` — restored working CMake CI (prior change)
- `AUDIT_v4.1.2-audited.md` — this report
