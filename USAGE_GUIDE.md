# IntegratedStorageCpp - Comprehensive Usage Guide (JakeSnowy Fork)

**GitHub:** https://github.com/Jakesnowy/sharing-base-resources  
**Base Version:** Sarfflow's IntegratedStorageCpp v4.1.2 with P0 Critical Fixes  
**Fork URL:** https://github.com/Jakesnowy/sharing-base-resources/  

---

## 🎯 Quick Answer: How To Use This Fork

### For Most Users (Zero-Config Mode - RECOMMENDED)

**Just follow these 3 steps:**

1. **Build the DLL once** on any machine
2. **Copy to ALL machines** (server + every client)
3. **Play!** No IP/port/firewall configuration needed!

```bash
# Build (first time only):
git clone https://github.com/Jakesnowy/sharing-base-resources.git
cd sharing-base-resources
xmake f -y -p windows -a x64 --target=ModIntegratedStorageCpp
xmake build -v -t ModIntegratedStorageCpp

# Copy DLL to EVERY machine:
<build-output>/ModIntegratedStorageCpp.dll \
    -> <Palworld-install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/dlls/main.dll

# Play! The default config.txt uses external_channel = false for zero-config mode.
```

### Why Zero-Config Mode Works

This fork defaults to `external_channel = false` because:
- ✅ All three P0 critical fixes still provide major stability improvements
- ✅ Original RPC-based multiplayer works perfectly for 2-4 players
- ✅ No IP/port/firewall configuration required
- ✅ Just copy DLL and play - truly zero configuration

---

## 📖 Complete Documentation Index

### For End Users:
- **[`README.md`](<README.md>)** - Main documentation with quick-start guide
- **[`USAGE_GUIDE.md`](<USAGE_GUIDE.md>)** - This complete usage guide (you are here)
- **[`FIXES_README.md`](<FIXES_README.md>)** - User-focused P0 fixes overview
- **[`FORK_README.md`](<FORK_README.md>)** - Quick reference guide

### For Developers:
- **[`CRITICAL_FIXES_SUMMARY.md`](<CRITICAL_FIXES_SUMMARY.md>)** - Technical implementation details
- **[`COMPREHENSIVE_AUDIT_REPORT.md`](<COMPREHENSIVE_AUDIT_REPORT.md>)** - Full audit results
- **`.github/workflows/build.yml`** - GitHub Actions build configuration

---

## 🚀 Installation & Deployment

### Prerequisites

Before building or deploying, ensure you have:
- ✅ Windows 10/11 (64-bit)
- ✅ Visual Studio 2022 Build Tools with "Desktop development with C++" workload
- ✅ Git for Windows
- ✅ xmake build system (script will install automatically if needed)

### Build from Source (One-Time Setup)

```bash
# Clone your fork
git clone https://github.com/Jakesnowy/sharing-base-resources.git
cd sharing-base-resources

# First-time: Bootstrap environment and build
.\build.ps1 -Bootstrap -UE4SSRoot D:\src\RE-UE4SS -Zip

# Subsequent builds (environment already ready):
.\build.ps1 -UE4SSRoot D:\src\RE-UE4SS -Zip
```

### Manual Build Commands

```bash
# Clone RE-UE4SS SDK (first time only)
git clone https://github.com/UE4SS-RE/RE-UE4SS.git RE-UE4SS
cd RE-UE4SS
xmake f -y --vs2022 --platform=win64
xmake build --only=polyhook_2 --only=cppmods

# Build your fork's DLL
cd ..\sharing-base-resources
xmake f -y -p windows -a x64 --target=ModIntegratedStorageCpp
xmake build -v -t ModIntegratedStorageCpp
```

### Build Output Locations

After successful build, find your DLL at:
1. **Primary:** `dist/ModIntegratedStorageCpp/dlls/main.dll`
2. **Also available in:** `RE-UE4SS\x64-windows_release\ModIntegratedStorageCpp\` (if built there)
3. **GitHub Actions:** See `.github/workflows/build.yml` for automated builds

### Deploy DLL to ALL Machines

**CRITICAL:** The SAME DLL must be on every machine (server + all clients).

```bash
# Location in Palworld installation:
<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/dlls/main.dll

# Copy your built DLL to this location on EVERY machine:
D:\xmake\output\debug\dllmain_fixed\x64-windows_release\ModIntegratedStorageCpp\ModIntegratedStorageCpp.dll \
    -> D:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\Mods\ModIntegratedStorageCpp\dlls\main.dll

# OR copy from build output:
dist/ModIntegratedStorageCpp/dlls/main.dll \
    -> <each-machine>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/dlls/main.dll
```

**Quick Deploy Script:**
```powershell
# Copy to all machines in a directory:
Copy-Item "C:\path\to\build\dist\ModIntegratedStorageCpp\dlls\main.dll" `
    -Destination "D:\AllMachines\*" `
    -Recurse `
    -Force

# Then copy each machine's DLL to Palworld:
$palPath = "D:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\Mods\ModIntegratedStorageCpp\dlls"
Copy-Item "$env:TEMP\isg\main.dll" $palPath -Force
```

---

## ⚙️ Configuration Guide

### Location of Configuration File

Edit this file on **EVERY** machine:
```
<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/config.txt
```

### Default Configuration (Zero-Config Mode)

The default `config.txt` is optimized for zero-config deployment:

```ini
# ============================================================================
#  IntegratedStorage - JakeSnowy Fork v4.1.2-audited (Zero-Config Default)
#  GitHub: https://github.com/Jakesnowy/sharing-base-resources
# ============================================================================

# verbose : Enable detailed [ISGATE] diagnostic logs in UE4SS.log
verbose = true

# reconcile_interval_ms : Server-side discovery reconcile cadence (ms)
reconcile_interval_ms = 8000

# isi_refresh_ms : Remote client refresh frequency (compat only)
isi_refresh_ms = 1500

# ============================================================================
#  EXTERNAL TCP CHANNEL - DISABLED BY DEFAULT FOR ZERO-CONFIG MODE!
# ============================================================================
external_channel = false  # ← Leave as-is for zero-config deployment
external_port = 27500
external_server_host =    # Not used when channel is disabled

# LAYER 2: Delta Sync (Optional performance optimization)
channel_delta = true
channel_full_sync_interval = 3600000

# LAYER 3: Delay Reply (Optional reentrancy protection)
channel_delay_reply = false

# client_inject_enabled : Client-side UI injection switch. MUST stay `true`
#                         on clients or the shared pool will not be visible.
#                         Set to `false` only for developer testing of
#                         server-only cross-registration.
client_inject_enabled = true

# ============================================================================
#  ALL THREE P0 CRITICAL FIXES ARE ACTIVE REGARDLESS OF CHANNEL MODE:
#   - P0-a: Local player filter (prevents flickering from remote events)
#   - P0-b: Camp lookup cache (eliminates FindAllOf blocking)
#   - P0-c: Food box exclusion (fixes food consumption issues)
# ============================================================================
```

### When To Enable TCP Channel

Enable `external_channel = true` **ONLY** if you need:
- Large guilds (6+ concurrent players)
- Dedicated servers on separate machines
- Maximum stability under heavy load
- Zero buffer saturation guarantees

**For most users (2-4 players), keep it disabled!** See the advanced configuration section below.

---

## 🎮 Multiplayer Usage Scenarios

### Scenario A: Local Host/SP Co-op (Most Common)

**Setup:** One player hosts on their machine, friends join as clients.

**Configuration:**
- **Host machine:** Default `config.txt` (`external_channel = false`)
- **Client machines:** Default `config.txt` (`external_channel = false`)
- **No IP/port configuration needed!**

```bash
# Host just builds once and shares DLL to all clients
Host: Build DLL → Copy to host's Palworld Mods folder

# Share DLL with all friends:
Friends copy same DLL to their Palworld Mods folders

# Everyone plays! No additional configuration.
```

**What Happens:**
- Host reads cross-registered containers natively (no external channel needed)
- Clients use original RPC-based multiplayer for item requests
- All P0 fixes active for stability
- **Zero troubleshooting required** ✅

**Note:** The mod must be installed on every client because the shared pool is injected into the client's UI. Host-only deployment is not currently possible with this architecture.

---

### Scenario B: Dedicated Server on Separate Machine

**Setup:** Dedicated server process runs on separate machine, clients connect.

**Configuration Options:**

#### Option 1: Zero-Config Mode (Recommended for Small Guilds)

```ini
# On DEDICATED SERVER:
external_channel = false  # ← Default mode - no IP/port needed!
reconcile_interval_ms = 8000

# On CLIENTS:
external_channel = false  # ← Default mode matches server
```

**Benefits:**
- ✅ No IP/port configuration required
- ✅ Works with original RPC-based multiplayer  
- ✅ All P0 fixes still active for stability

#### Option 2: Advanced TCP Mode (Large Guilds Only)

```ini
# On DEDICATED SERVER (just listen, no IP needed):
external_channel = true           # Enable advanced channel
external_port = 27500             # TCP port (must match on clients)
# external_server_host is NOT needed on server

# On CLIENTS (connect to server's LAN IP):
external_channel = true
external_port = 27500
external_server_host = 192.168.1.XXX  # ← Server's LAN IP address
```

**When to Use:**
- 6+ concurrent players
- Many bases with complex storage layouts
- Need zero buffer saturation guarantees
- Edge cases requiring dedicated network thread

---

### Scenario C: LAN Party / Large Guild Session

**Setup:** Multiple hosts or one host with many clients in same network.

**Configuration:**

```ini
# On HOST (authoritative end):
external_channel = false  # ← Default works perfectly for LAN!
reconcile_interval_ms = 15000  # Slightly longer for larger guilds

# On ALL CLIENTS:
external_channel = false
```

**For Very Large Guilds (6+ players, many bases):**

```ini
# Host config:
external_channel = true
external_port = 27500

# Client configs (each uses their own IP):
external_channel = true
external_port = 27500
external_server_host = <host's LAN IP>
```

---

## 🔧 Advanced Configuration Options

### P0 Critical Fixes (Always Active - No Configuration Needed!)

This fork includes all three P0 critical fixes that work **independently** of the communication channel:

| Fix | What It Does | Impact | Location |
|-----|-------------|--------|----------|
| **P0-a** | Filters remote player events on host-side (only local player events trigger pool refresh) | Eliminates flickering from remote events, prevents RPC storm | `src/dllmain.cpp:994-1017` |
| **P0-b** | Uses cached camp lookup via `g_campIdToCamp` TMap instead of O(N) FindAllOf | Eliminates blocking thread on every request | `src/dllmain.cpp:438,610` |
| **P0-c** | Excludes food boxes from storage cross-registration | Fixes food consumption issues | `src/dllmain.cpp:427,511` |

**These fixes provide major stability improvements regardless of channel mode!**

---

### Server-Side Configuration Tuning

#### Reconcile Interval (`reconcile_interval_ms`)

The server performs a discovery reconcile every N milliseconds to rebuild guild state.

| Use Case | Recommended Value | Reason |
|----------|------------------|--------|
| Single player / Small guild (<3 players) | 8000ms (default) | Fast enough, minimal overhead |
| Medium guild (3-5 players) | 15000ms | Balance between freshness and performance |
| Large guild (6+ players, many bases) | 30000ms | Reduce background overhead |

**Example for large guild:**
```ini
reconcile_interval_ms = 30000
channel_full_sync_interval = 120000  # Full resync every 2 minutes as recovery fallback
```

#### Delta Sync Configuration (`channel_delta`)

Controls whether the channel sends only changed items or full dumps.

| Setting | Behavior | Best For |
|---------|----------|----------|
| `true` (default) | Sends only changed items (delta) | Performance, reduced bandwidth |
| `false` | Always sends full pool dump | Debugging, first requests after reconnect |

**Recommendation:** Keep enabled (`true`) for production use. Disable only for debugging.

---

### Client-Side Configuration

Clients typically don't need to configure anything. The default settings work perfectly.

#### When Clients Need Special Configuration

If clients experience issues, verify these match the server:

```ini
# On client config.txt - must match server's channel mode:
external_channel = false  # OR true if server uses it

# If using external channel, must specify server's LAN IP:
external_server_host = <server's LAN IP>
```

**Note:** Server IP can be found using `ipconfig` on Windows - look for IPv4 address under active network adapter.

---

## 🐛 Troubleshooting Guide

### Quick Diagnostic Checklist

Before reporting issues, verify:

1. ✅ **Same DLL on all machines** - Check file sizes and dates match
2. ✅ **Config.txt exists on all machines** - Missing config breaks mod loading
3. ✅ **Firewall rules allow port 27500** (if using TCP channel)
4. ✅ **All machines use same channel mode** (all false, or all true with correct IPs)
5. ✅ **Verbose logging enabled** (`verbose = true`) for diagnostic output

---

### Issue #1: "Materials Don't Appear in Shared Pool"

**Symptoms:** Client joins server but sees only their own camp's materials.

**Possible Causes & Fixes:**

| Cause | Fix |
|-------|-----|
| Different DLL versions on machines | **VERIFY:** Same DLL everywhere (compare file sizes) |
| Missing `config.txt` on some machines | Copy `config.txt` to ALL machines alongside DLL |
| Mixed channel modes (server: true, clients: false) | Enable/disable channel consistently on all machines |
| Firewall blocking connections | Add firewall rule for port 27500 (if using TCP channel) |

**Quick Fix:**
```bash
# Delete mod folder and redeploy fresh on ALL machines:
<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/ -R

# Redeploy DLL and config.txt from same build output to all machines
```

---

### Issue #2: "Server Won't Start Mod"

**Symptoms:** Server launches but no "[ISGATE]" logs in UE4SS.log.

**Possible Causes & Fixes:**

| Cause | Fix |
|-------|-----|
| DLL corrupted during copy | Rebuild DLL and redeploy fresh |
| Wrong DLL location | Verify: `Mods/ModIntegratedStorageCpp/dlls/main.dll` |
| Invalid config.txt settings | Check for typos, invalid values |
| Missing config.txt | Ensure `config.txt` is in mod folder (zero-config mode requires it) |

**Quick Fix:**
```bash
# Remove old mod folder completely:
<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/ -R

# Redeploy fresh DLL and config.txt from build output
```

---

### Issue #3: "Clients Can't Connect to Server"

**Symptoms:** Clients show connection timeout, server logs no incoming connections.

| Cause | Solution |
|-------|----------|
| **Firewall blocking TCP port 27500** | Add firewall rule (see Firewall Configuration section) |
| Wrong IP in client config.txt | Use LAN IP, not public IP |
| Server listening on wrong interface | See Server Binding Options below |

---

### Issue #4: "Pool Flickers or Shows Wrong Items"

**Symptoms:** Materials appear/disappear, wrong quantities shown.

**This is Often Normal!**

**What's Happening:**
- The mod performs discovery reconcile every `reconcile_interval_ms` (default: 8000ms)
- During reconciliation (~8s), the pool may temporarily show incorrect states
- After reconciliation completes, the pool stabilizes and shows correct merged storage

**Solutions:**

| Cause | Solution |
|-------|----------|
| Too frequent reconcile interval | Increase in server config: `reconcile_interval_ms = 15000` |
| Network lag causing missed updates | Keep clients on same LAN (preferred) |
| Client not stable in camp | P0-a fix should prevent this; verify fork has `[FIX P0-a]` marker |

---

### Issue #5: "Food Items Disappear After Consumption"

**Symptoms:** Food items from cross-registered chests disappear after being consumed.

**This Should NOT Happen!** The P0-c fix addresses this.

**Verification Steps:**
1. Check fork code has `[FIX P0-c]` markers at lines 427 and 511
2. Ensure no food boxes are incorrectly registered as storage
3. Try new base if save file may be corrupted

---

### Issue #6: "Pal Summoning Fails After 10 Minutes"

**Symptoms:** Works fine initially but fails after extended play session.

**This Should NOT HappEN!** The P0-a fix should prevent this.

**Verification Steps:**
1. Verify P0-a fix is active: check `[FIX P0-a]` in `src/dllmain.cpp` at line 994
2. Check UE4SS.log for "[ISGATE]" messages about pool clearing
3. If problem persists, this may be a different issue unrelated to storage mod

---

## 🔥 Firewall Configuration (TCP Channel Only)

### Windows Firewall Rules Needed (When `external_channel = true`)

```powershell
# 1. Add inbound rule for TCP port 27500
New-NetFirewallRule -DisplayName "ISGATE TCP Port 27500" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 27500 `
    -Action Allow `
    -Description "IntegratedStorageCpp external communication channel"

# 2. Verify rule is active
Get-NetFirewallRule | Where-Object { $_.DisplayName -like "*ISGATE*" }

# To remove firewall rule later:
New-NetFirewallRule -DisplayName "ISGATE TCP Port 27500" -WhatIf
```

### For Dedicated Servers

1. Configure firewall on **server machine** (allow incoming connections)
2. Add port 27500 to Windows Firewall allow list
3. Consider router port forwarding if accessing from outside LAN

---

## 🔍 Debugging & Diagnostics

### Enable Verbose Logging

```ini
# In config.txt on ALL machines:
verbose = true  # Enables [ISGATE] diagnostic logs in UE4SS.log
```

**What to Look For:**
```
[ISGATE] Client menu opened for player X
[ISGATE] Pool reply sent with count: 42
[ISGATE] Discovery reconcile completed, found 120 merged slots
[ISGATE] Connection established from 192.168.1.XXX
[ISGATE] [FIX P0-a] hkEnterCamp local player filter active
[ISGATE] [FIX P0-b] Camp lookup cache hit
[ISGATE] [FIX P0-c] Food box excluded from storage
```

### Reading UE4SS Logs

UE4SS.log is typically located at:
```
<UE4-Pal-Install>/Pal/Saved/SaveGames/{YourSave}/UE4SS.log
```

**Common Messages:**
- `[ISGATE] Client menu opened` - Normal, client requesting pool data
- `[ISGATE] Pool reply sent` - Server responding to client request  
- `[ISGATE] Discovery reconcile` - Server rebuilding guild state
- `[ISGATE] Connection established` - TCP channel active (if enabled)
- `[ISGATE] ROLE server=X dedicated=Y` - Mod role detection

---

## 🧪 Testing & Validation

### Pre-Deployment Test

**Minimum Viable Test:**
1. Deploy on localhost (same machine) with 2-3 players via virtual network
2. Play for 30+ minutes in a multi-base scenario
3. Verify no gradual degradation occurs
4. **Test food consumption repeatedly** - this is what P0-c fixes!
5. Test Pal summoning multiple times

### Production Readiness Test

1. Dedicated server on separate machine (or host-SP)
2. 5+ remote clients (casual mode) or large guild (advanced mode with TCP enabled)
3. Multiple bases with different resource specializations
4. Extended session (1+ hour)
5. Stress test: frequent base transitions, item use, construction

---

## 🔄 Migration Guide

### From Zero-Config to TCP Mode

```ini
# Edit config.txt on ALL machines:
external_channel = true          # Change from false to true
external_port = 27500             # Verify this matches
external_server_host = <server-ip>  # Add only on clients

# Restart Palworld on all machines (mod needs reload)
```

### From TCP Mode to Zero-Config

```ini
# Edit config.txt on ALL machines:
external_channel = false          # Change from true to false

# Close and restart Palworld on all machines
# Mod will automatically fall back to RPC-based multiplayer
```

---

## ✅ Validation Checklist

### Pre-deployment:
- [ ] Code compiles without warnings
- [ ] All P0 fixes confirmed in source code comments (`[FIX P0-a]`, `[FIX P0-b]`, `[FIX P0-c]`)
- [ ] TCP channel disabled by default (check `config.txt` shows `external_channel = false`)

### Post-deployment (Single Machine Test):
- [ ] DLL loads correctly in UE4SS
- [ ] No console errors on startup
- [ ] Shared materials display correctly
- [ ] Cross-base construction works
- [ ] **Food consumption works reliably** (key P0-c fix verification!)

### Multiplayer Test:
- [ ] Server starts successfully
- [ ] All clients connect without issues
- [ ] Play for 30+ minutes in multi-base scenario
- [ ] No gradual performance degradation
- [ ] Pal summoning still works after 10+ minutes
- [ ] Other mods remain functional

---

## 🚀 GitHub Actions Build (Optional)

### Setup Required

1. **Create GitHub PAT Token:**
   - Visit https://github.com/settings/tokens
   - Create token with "Public repositories" scope
   - Copy token string (starts with `ghp_...`)

2. **Add to Repository Secrets:**
   - Go to repo Settings → Secrets and variables → Actions
   - Click "New repository secret"
   - Name: `GH_PAT` (exact name, case-sensitive)
   - Value: Paste PAT token

### Build Workflow

The build runs automatically when you push changes to the `main` or `master` branch.

**Output locations:**
1. **GitHub Actions Artifacts tab** (30-day retention)
2. **Local repository:** `dist/ModIntegratedStorageCpp/` folder and zip file
3. **Download URL:** Create release on GitHub Releases tab to publish

---

## 📚 FAQ

### Q: Can I play with friends without configuring anything?

**A:** Yes! Just use the default `config.txt` with `external_channel = false`. This fork's zero-config mode works out-of-the-box with original RPC-based multiplayer.

### Q: Do I need to enable TCP channel for large guilds?

**A:** Not strictly necessary, but recommended for 6+ players or dedicated servers. See advanced configuration section for enabling.

### Q: Why do P0 fixes work regardless of channel mode?

**A:** P0 fixes address fundamental stability issues in the mod's core logic. They operate independently of the communication channel used for item data transfer.

### Q: What happens if I mix channel modes (server: true, clients: false)?

**A:** The server will attempt to use TCP while clients expect RPC-based communication, resulting in connection failures. Keep all machines in same mode.

### Q: Can I switch between zero-config and TCP mode at runtime?

**A:** No. Configuration changes require restarting Palworld on ALL machines (mod needs to reload).

---

## 📖 Additional Resources

- **Main Documentation:** [`README.md`](<README.md>)
- **Quick Reference:** [`FORK_README.md`](<FORK_README.md>)  
- **P0 Fixes Overview:** [`FIXES_README.md`](<FIXES_README.md>)
- **Technical Deep Dive:** [`CRITICAL_FIXES_SUMMARY.md`](<CRITICAL_FIXES_SUMMARY.md>)
- **GitHub Actions Setup:** [`.github/DEPLOYMENT.md`](<.github/DEPLOYMENT.md>)

---

## 📝 Configuration Summary

| Config Key | Default | When to Change |
|-----------|---------|----------------|
| `verbose` | `true` | Set to `false` for production (reduces log spam) |
| `reconcile_interval_ms` | `8000` | Increase for large guilds (>5 players) |
| `external_channel` | `false` | Enable only for advanced networking needs |
| `channel_delta` | `true` | Keep enabled for performance |
| `channel_delay_reply` | `false` | Keep disabled unless debugging edge cases |

---

**End of Usage Guide**

**Last Updated:** 2024  
**Maintained by:** JakeSnowy (https://github.com/Jakesnowy)  
**Base Project:** Sarfflow/IntegratedStorageCpp v4.1.2