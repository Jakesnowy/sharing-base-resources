**Status:** ✅ Production-ready, multiplayer-stable, optimized for large guilds.

All P0 fixes (P0-a, P0-b, P0-c) are now properly documented and reflected in the actual code state at `src/dllmain.cpp`.

---

## 🆘 Co-op Troubleshooting Guide - Hosts and Clients

### Quick Decision Tree: Which Configuration Do I Need?

```
┌───────────────────────────────────────────────────────────────┐
│  What's Your Use Case?                                        │
├───────────────────────────────────────────────────────────────┤
│                                                                │
│  A) Casual co-op (2-4 players, same LAN or close network)      │
│     → Use ZERO-COFIG DEFAULT: external_channel = false         │
│     → No IP/port configuration needed!                         │
│     → Just copy DLL and play ✅                                 │
│                                                                │
│  B) Large guild (6+ players, many bases)                       │
│     → Enable external_channel = true                           │
│     → Configure TCP channel as below                           │
│                                                                │
│  C) Dedicated server on separate machine                       │
│     → Enable external_channel = true                            │
│     → Configure TCP channel with server IP                     │
│                                                                │
└───────────────────────────────────────────────────────────────┘
```

---

### Scenario A: Zero-Config Mode (Default - NO Configuration Needed!)

**Best for:** 2-4 players, casual co-op sessions  
**Network Type:** RPC-based (original UE multiplayer behavior)

#### Setup Steps:
1. **Build DLL once** on one machine
2. **Copy DLL to ALL machines:**
   ```bash
   <build-output>/ModIntegratedStorageCpp.dll \
       -> <Pal>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/dlls/main.dll
   ```
3. **Verify `config.txt` on each machine:**
   ```ini
   # This is the DEFAULT - no changes needed!
   external_channel = false  # ← Leave as-is for zero-config mode
   ```
4. **Launch Palworld** and play!

#### What Happens Behind the Scenes:
- Mod uses original UE4SS RPC-based multiplayer behavior
- All three P0 critical fixes still active (P0-a, P0-b, P0-c) ✅
- No IP/port/firewall configuration needed
- Just copy DLL → Play!

---

### Scenario B: Advanced TCP Mode (When You Need It)

**Best for:** Large guilds (6+ players), dedicated servers, edge cases  
**Network Type:** External TCP channel + RPC fallback

#### When to Enable TCP Channel:

Enable `external_channel = true` if you need:
- ✅ Zero buffer saturation under heavy load
- ✅ Dedicated network thread (never blocks game thread)
- ✅ Maximum stability for 6+ concurrent players
- ✅ Multiple bases with many items tracked

#### How to Enable:

1. **Edit `config.txt` on ALL machines:**
   ```ini
   # ADVANCED MODE - Uncomment and configure:
   external_channel = true           # Enable TCP channel
   external_port = 27500             # Must match EXACTLY on all machines
   
   # On CLIENTS only (not server):
   external_server_host = 192.168.1.XXX  # Fill with server's LAN IP
   ```

2. **Server config.txt** (just listen for connections):
   ```ini
   external_channel = true
   external_port = 27500
   # Server doesn't need external_server_host - it listens on all interfaces
   ```

3. **Client config.txt** (must connect to server):
   ```ini
   external_channel = true
   external_port = 27500
   external_server_host = 192.168.1.XXX  # ← Server's IP address
   
   # Example: Find your LAN IP on Windows
   # 1. Open Command Prompt (cmd.exe)
   # 2. Run: ipconfig
   # 3. Look for "IPv4" under your active network adapter
   #    Example: Ethernet adapter vEthernet:
   #             IPv4 Address. . . . . . . . . : 192.168.1.10
   #             Use this IP in external_server_host
   ```

#### Finding Server's LAN IP (Windows):

```bash
# Method 1: Using Command Prompt
cmd.exe /c "ipconfig"
# Look for IPv4 address under your active network adapter
# Example output:
# Ethernet adapter vEthernet:
#   IPv4 Address . . . . . . . . . : 192.168.1.10

# Method 2: Using PowerShell
powershell -Command "Get-NetIPAddress | Where-Object {$_.AddressFamily -eq 'InterNetwork'}"
```

---

### Common Co-op Issues & Solutions

#### Issue 1: "Friends Can't See My Storage"

**Symptoms:** Client joins server but materials don't appear in shared pool.

**Causes & Fixes:**

| Cause | Solution |
|-------|----------|
| Different DLL versions on machines | **VERIFY:** Same DLL on ALL machines (server + clients) |
| `config.txt` missing on some machines | Copy `config.txt` to ALL machines alongside DLL |
| External channel enabled on server but not clients | Enable on ALL machines consistently |
| Firewall blocking connections | See firewall troubleshooting below |

**Quick Fix Steps:**
1. Verify same DLL version on all machines
2. Verify `config.txt` exists and has same content everywhere
3. For zero-config mode: ensure `external_channel = false` on all machines
4. Restart UE4SS mod manager on all machines
5. Re-launch Palworld

---

#### Issue 2: "Server Won't Start Mod"

**Symptoms:** Server launches but mod doesn't load, no "[ISGATE]" logs.

**Causes & Fixes:**

| Cause | Solution |
|-------|----------|
| DLL corrupted during copy | Rebuild DLL and redeploy |
| Wrong DLL location | Verify: `Mods/ModIntegratedStorageCpp/dlls/main.dll` |
| `config.txt` has invalid settings | Check for typos, ensure no commented-out lines with values |

**Quick Fix:**
```bash
# 1. Remove old mod folder completely
<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/ -R

# 2. Re-deploy fresh DLL and config.txt
# (Copy from build output to correct location)
```

---

#### Issue 3: "Clients Can't Connect to Server"

**Symptoms:** Clients show connection timeout, server logs no incoming connections.

**Causes & Fixes:**

| Cause | Solution |
|-------|----------|
| **Firewall blocking TCP port 27500** | Add firewall rule (see below) |
| Wrong IP in client config.txt | Use LAN IP, not public IP |
| Server listening on wrong interface | See server binding options below |
| NAT/Router issues for external connections | Use port forwarding or dedicated server hosting |

**Firewall Configuration (Windows):**

```powershell
# Add firewall rule to allow TCP port 27500
New-NetFirewallRule -DisplayName "ISGATE TCP Channel" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 27500 `
    -Action Allow

# Verify rule was added
Get-NetFirewallRule -DisplayName "ISGATE TCP Channel"
```

**Advanced: Make Server Listen on Specific Interface:**

If server has multiple network interfaces (LAN + WAN), add this to server's config.txt:

```ini
# Optional: Bind to specific interface (useful if you have multiple adapters)
# Uncomment and specify IP of the interface that clients will connect from
# external_listen_interface = 192.168.1.50
```

**Without this line:** Server listens on all interfaces (default behavior - works for LAN).

---

#### Issue 4: "Pool Flickers or Shows Wrong Items"

**Symptoms:** Materials appear/disappear, wrong quantities shown in shared pool.

**This is Expected Behavior During Discovery!**

**What's Happening:**
- The mod performs a discovery reconcile every `reconcile_interval_ms` (default: 8000ms)
- During reconciliation, the pool may temporarily show incorrect states
- After ~8s, the pool stabilizes and shows correct merged storage

**Solutions:**

| Cause | Solution |
|-------|----------|
| Too frequent reconcile interval | Increase in server's config.txt: `reconcile_interval_ms = 15000` |
| Network lag causing missed updates | Keep clients on same network (LAN preferred) |
| Client not stable in camp (P0-a fix should prevent this) | Verify P0-a is active in fork (check code comments) |

**For Large Guilds:** Increase reconcile interval:
```ini
# Server config.txt - for 6+ players with many bases:
reconcile_interval_ms = 30000  # Reduce overhead, pool syncs every 30s
channel_full_sync_ms = 120000  # Full resync every 2 minutes as recovery fallback
```

---

#### Issue 5: "Pal Summoning Fails After 10 Minutes"

**Symptoms:** Works fine initially but fails after extended play.

**This is Expected! The P0-a Fix Should Prevent This.**

**If Still Occurring:**
- Verify P0-a fix is active: check `[FIX P0-a]` in `src/dllmain.cpp` at line 993
- Check UE4SS.log for "[ISGATE]" messages about pool clearing
- If problem persists, this may be a different issue unrelated to storage mod

---

#### Issue 6: "Food Items Disappear After Consumption"

**Symptoms:** Food items from cross-registered chests disappear after being consumed.

**This is Expected! The P0-c Fix Should Prevent This.**

**If Still Occurring:**
- Verify P0-c fix is active: check `[FIX P0-c]` in `src/dllmain.cpp` at lines 427, 511
- Ensure no food boxes are incorrectly registered as storage
- Check for corrupted save files (try new base)

---

### Firewall Configuration Checklist

Before enabling external TCP channel, ensure firewall rules are in place:

#### Windows Firewall Rules Needed:

```powershell
# 1. Allow TCP port 27500 inbound
New-NetFirewallRule -DisplayName "ISGATE TCP Port 27500" `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 27500 `
    -Action Allow `
    -Description "IntegratedStorageCpp external communication channel"

# 2. Verify rule is active
Get-NetFirewallRule | Where-Object { $_.DisplayName -like "*ISGATE*" }
```

#### For Dedicated Servers:

If running dedicated server on separate machine:
1. Configure firewall on **server machine** (allow incoming connections)
2. Add port 27500 to Windows Firewall allow list
3. Consider adding router port forwarding if accessing from outside LAN

---

### Network Configuration Best Practices

#### For Small Guilds (2-4 players):

```ini
# Recommended: Use zero-config mode, no advanced networking needed!
external_channel = false  # ← Default is fine
```

**Why:** Original RPC-based multiplayer with P0 fixes provides excellent stability for small groups.

---

#### For Large Guilds (6+ players) or Dedicated Servers:

```ini
# Recommended: Enable TCP channel for maximum stability
external_channel = true
external_port = 27500

# On server: leave external_server_host empty (listens on all interfaces)
# On clients: set to server's LAN IP
external_server_host = 192.168.1.XXX  # ← Server's IP only!
```

**Network Considerations:**

- **LAN vs WAN:** For best performance, keep all players on same LAN
- **Router Port Forwarding:** Required only for external (non-LAN) connections
- **NAT Traversal:** May cause issues if accessing from outside local network

---

### Testing Checklist Before Going Live

#### Zero-Config Mode Test:
1. ✅ Deploy on localhost with 2-3 players (virtual machine test)
2. ✅ Play for 30+ minutes in multi-base scenario
3. ✅ Verify food consumption works reliably
4. ✅ Verify Pal summoning still works after 10+ minutes

#### TCP Channel Mode Test:
1. ✅ Server starts successfully on separate machine
2. ✅ All clients connect without firewall errors
3. ✅ Materials display correctly from all bases
4. ✅ Play for 30+ minutes with 6+ players
5. ✅ No buffer saturation or paralysis issues

---

### Debugging Tips

#### Enable Verbose Logging:

```ini
# In config.txt on ALL machines:
verbose = true  # Enables [ISGATE] diagnostic logs in UE4SS.log
```

**What to Look For in Logs:**
- Client menu open/request/reply tracking
- Server-side per-chest + discovery tracking  
- Connection attempts and successful replies

#### Common Log Messages:

```
[ISGATE] Client menu opened for player X
[ISGATE] Pool reply sent to client with count: 42
[ISGATE] Discovery reconcile completed, found 120 merged slots
[ISGATE] Connection established from 192.168.1.XXX
```

---

### Migration Guide: Switching Between Modes

#### From Zero-Config to TCP Mode:

1. Edit `config.txt` on **ALL machines**:
   ```ini
   external_channel = true          # Change from false to true
   external_port = 27500             # Verify this matches
   external_server_host = <server-ip>  # Add only on clients
   ```

2. Restart Palworld on all machines (mod needs reload)

3. Test with 3-4 players first before full deployment

#### From TCP Mode to Zero-Config:

1. Edit `config.txt` on **ALL machines**:
   ```ini
   external_channel = false          # Change from true to false
   ```

2. Clear any pending connections (close and restart)

3. Mod will automatically fall back to RPC-based multiplayer

---

### Troubleshooting Summary Table

| Symptom | Likely Cause | Quick Fix |
|---------|--------------|-----------|
| Can't see materials in pool | Different DLL versions | Re-deploy same DLL everywhere |
| Server won't start mod | Corrupted config.txt | Delete folder, redeploy fresh |
| Clients can't connect | Firewall blocking port | Add firewall rule for 27500 |
| Pool flickers during play | Normal discovery behavior | Wait 8s or increase reconcile interval |
| Food items disappear | P0-c fix should prevent this | Verify fork code has `[FIX P0-c]` marker |
| Pal summoning fails after 10min | Should not happen with P0-a fix | Check UE4SS.log for other issues |
| Connection timeout | Wrong server IP in config.txt | Use LAN IP, not public IP |

---

## 📖 Additional Documentation

### For Users: Start Here
- **[`USAGE_GUIDE.md`](<USAGE_GUIDE.md>)** - This document, complete with co-op troubleshooting
- **[`FIXES_README.md`](<FIXES_README.md>)** - User-focused guide with P0 fixes overview
- **[`FORK_README.md`](<FORK_README.md>)** - Quick reference guide

### For Developers: Technical Deep Dives
- **[`CRITICAL_FIXES_SUMMARY.md`](<CRITICAL_FIXES_SUMMARY.md>)** - Implementation details and architecture
- **[`EXTERNAL_COM_CHANNEL_FEASIBILITY.md`](<EXTERNAL_COM_CHANNEL_FEASIBILITY.md>)** - Network design decisions
- **[`DOCS_AUDIT_SUMMARY.md`](<DOCS_AUDIT_SUMMARY.md>)** - Documentation audit verification

---

## 🚀 Build Automation (Optional)

```bash
# PowerShell build script with one-command deployment
.\build.ps1 <path-to-pal-install>

# Example:
.\build.ps1 D:\SteamLibrary\steamapps\common\Palworld
```

---

**End of Usage Guide**