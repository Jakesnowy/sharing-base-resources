# IntegratedStorageCpp - Complete Usage Guide (JakeSnowy Fork)
**Version:** 1.0  
**Fork:** https://github.com/Jakesnowy/sharing-base-resources  
**Base:** Sarfflow's IntegratedStorageCpp v4.1.2 with all P0 fixes + undocumented optimizations

---

## 🎯 Quick Start Summary

This guide covers **ALL usage scenarios** for the JakeSnowy fork, including:
- Dedicated servers (separate process)
- Host-SP co-op hosting (your game session, friends join)  
- Remote clients (joining someone else's session)
- IP/port configuration troubleshooting

The fork includes **all three P0 critical fixes** plus unique optimizations:
- ✅ P0-a: Local player filter (prevents flickering)
- ✅ P0-b: Camp lookup cache (O(1) instead of O(N))  
- ✅ P0-c: Food box exclusion (fixes food consumption)
- ✅ TCP Channel: Zero buffer saturation (undocumented from fault analysis)
- ✅ B3+B4 Optimizations: Minimal thread blocking (undocumented)

---

## 📦 Deployment - All Three Roles Explained

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│  DEDICATED SERVER / HOST (Authority)                          │
│  - Reads cross-registered containers natively                 │
│  - Listens on TCP port 27500 for client connections           │
│  - Performs ~8s discovery reconcile every X ms                │
└─────────────────────────────────────────────────────────────┘
                              │
              TCP Channel (external socket, not UE net)
                              │
┌─────────────────────────────────────────────────────────────┐
│  REMOTE CLIENTS                                              │
│  - Connect to server's IP:port                                │
│  - Use TCP channel to query guild pool                        │
│  - Display materials via minted slots in cont5                │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start Steps (All Roles)

### Step 1: Build the DLL (Do Once per Fork Installation)

```bash
# Clone your fork
git clone https://github.com/Jakesnowy/sharing-base-resources.git
cd sharing-base-resources

# Set up RE-UE4SS SDK (first time only)
git clone https://github.com/UE4SS-RE/RE-UE4SS.git
cd RE-UE4SS
xmake f -y --vs2022 --platform=win64
xmake build --only=polyhook_2 --only=cppmods

# Build your fork's DLL
cd <path>/sharing-base-resources
xmake f -y -p windows -a x64 --target=ModIntegratedStorageCpp
xmake build -v -t ModIntegratedStorageCpp
```

### Step 2: Deploy DLL to ALL Machines

```bash
# Copy generated DLL to EVERY machine (server AND all clients):
<build-output>/ModIntegratedStorageCpp.dll \
    -> <UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/dlls/main.dll

# Example:
D:\xmake\output\debug\dllmain_fixed\x64-windows_release\ModIntegratedStorageCpp\ModIntegratedStorageCpp.dll \
    -> D:\SteamLibrary\steamapps\common\Palworld\Pal\Binaries\Win64\Mods\ModIntegratedStorageCpp\dlls\main.dll

# ⚠️ CRITICAL: Same DLL must be on ALL machines!
```

### Step 3: Create config.txt on Each Machine

**Location:** `<UE4-Pal-Install>/Pal/Binaries/Win64/Mods/ModIntegratedStorageCpp/config.txt`

Configure based on your role (see sections below).

---

## 👤 Role-Specific Configuration

### Scenario A: Dedicated Server (Separate Process)

Use when running a dedicated server process that friends join via title menu.

**Server config.txt:**
```ini
# ============================================================================
#  IntegratedStorageCpp - Dedicated Server Configuration
#  Separate process running in the background
# ============================================================================

# [MULTIPLAYER STABILITY] All P0 Fixes Applied + TCP Channel Enabled
external_channel = true           # ✅ Required for clients to connect
external_port = 27500             # Port clients will connect to
external_server_host =            # EMPTY - server listens, doesn't connect

# Performance tuning:
reconcile_interval_ms = 15000     # Discovery-reconcile frequency (min 500ms)
verbose = false                   # Set true only during debugging
```

**Client config.txt (each friend):**
```ini
external_channel = true           # ✅ MUST be true to connect
external_port = 27500             # Must match server's port EXACTLY
external_server_host = 192.168.1.100  # Server's LAN IP address (see below)

# Performance tuning:
reconcile_interval_ms = 15000     # Not used on clients (config compat)
verbose = false
```

---

### Scenario B: Host-SP Co-op Hosting (Most Common for Local Guilds)

Use when you're launching your normal game and want friends to join YOUR session.

**Host's config.txt (YOU, the host):**
```ini
# ============================================================================
#  IntegratedStorageCpp - Host-SP Configuration (Co-op Hosting)
#  You launch normally; friends join YOUR session
# ============================================================================

# [MULTIPLAYER STABILITY] All P0 Fixes Applied + TCP Channel Enabled
external_channel = true           # ✅ KEEP TRUE for client connectivity
external_port = 27500             # Port clients will connect TO you
external_server_host =            # EMPTY! You don't connect to anyone

# Performance tuning:
reconcile_interval_ms = 15000     # Increase if many players/bases
verbose = false                   # Set true only during debugging
```

**Friends' config.txt (each CLIENT):**
```ini
external_channel = true           # ✅ CRITICAL! They must connect to you
external_port = 27500             # Must match YOUR host's port EXACTLY
external_server_host = 192.168.1.50  # YOUR IP address (not theirs!)

# Performance tuning:
reconcile_interval_ms = 15000
verbose = false
```

---

### Scenario C: Remote Client (Joining Someone Else)

Use when you're joining a friend's game session from your computer.

**Your config.txt:**
```ini
external_channel = true           # ✅ MUST be true to connect
external_port = 27500             # Must match HOST's port EXACTLY
external_server_host = 192.168.1.50  # Host's IP address

# Performance tuning:
reconcile_interval_ms = 15000     # Not used on clients
verbose = false
```

---

## 🌐 Finding Your Machine's LAN IP Address

**CRITICAL:** Use the LAN/private IP (e.g., `192.168.x.x` or `10.x.x.x`), NOT your public WAN IP.

### Method 1: Command Prompt (Easiest)

```cmd
ipconfig
```

Look for **IPv4 Address** under your active network adapter:
```
Ethernet adapter Ethernet:

   IPv4 Address. . . . . . . . . : 192.168.1.50
   Subnet Mask . . . . . . . . . : 255.255.255.0
```

Share `192.168.1.50` with friends (the IPv4 Address line).

### Method 2: PowerShell

```powershell
ipconfig | findstr /i "IPv4"
```

### Method 3: From Windows Settings

1. Open **Settings** → **Network & internet** → **Ethernet/Wi-Fi**
2. Click your active connection
3. Find "Properties" section → IPv4 address

### Method 4: Using Router Admin Page

1. Open browser, go to router admin (often `192.168.1.1` or `192.168.0.1`)
2. Find **DHCP Client List** or **Connected Devices**
3. Look for "Palworld.exe" or your PC hostname
4. Note the assigned IP address

### Method 5: In-Game Network Settings (Limited Info)

1. Launch Palworld normally
2. Go to Title Menu → Network
3. Your local network name is displayed here
4. **Note:** This may not show IP directly on some networks

**⚠️ Important:** Friends on the same local network should use the LAN IP shown in `ipconfig`, not the public WAN IP from your ISP.

---

## 🎮 Step-by-Step: Host-SP Co-op Setup (Most Common)

This is for hosting a co-op session where friends join YOUR game.

### On Your Host Machine:

1. **Build and deploy DLL** (Steps 1-2 above)
2. **Create config.txt** with HOST settings:
   ```ini
   external_channel = true
   external_port = 27500
   external_server_host =           # Empty!
   reconcile_interval_ms = 15000
   verbose = false
   ```
3. **Find your LAN IP:** Run `ipconfig` and note the IPv4 address (e.g., `192.168.1.50`)
4. **(Optional)** Configure firewall: See "Firewall Configuration" section below

### Share with Friends:

Send them this information:
- **Your host's LAN IP:** `192.168.1.50`
- **The port:** `27500` (or whatever you set in `external_port`)
- **Your hostname** (if using friends list): e.g., "MyServer" or your name

### On Friends' Client Machines:

Each friend needs to:

1. **Build and deploy DLL** (Steps 1-2 above)
2. **Create config.txt** with CLIENT settings, filling in YOUR IP:
   ```ini
   external_channel = true           # ✅ CRITICAL!
   external_port = 27500             # Must match your host EXACTLY
   external_server_host = 192.168.1.50  # Your IP address (from above)
   reconcile_interval_ms = 15000
   verbose = false
   ```

### Friends Join Your Session:

1. **Your host:** Launch Palworld normally (`PalWin64_Shell.exe`) from your title menu
2. **Friends:** From their title menu, select "Join Friend's World"
3. **Enter your hostname/IP:** Use the name you shared with them
4. **All machines:** Mod auto-connects via TCP channel (clients) and reads natively (host)

---

## 🔒 Firewall Configuration (If Connection Fails)

### On Server/Host Machine:

**Windows Defender Firewall:**

1. Search "Windows Defender Firewall" in Start Menu
2. Click **Advanced settings**
3. Click **Inbound Rules** → **New Rule...**
4. Select **Port** → Next
5. Select **TCP** → Next
6. Specify local port: `27500` (or your custom port) → Next
7. Select **Allow the connection** → Next
8. Apply to **Domain**, **Private**, and **Public** as needed → Next
9. Name it: "IntegratedStorageCpp - Port 27500" → Finish

**Or create a shortcut rule:**
```bash
# Create firewall rule via command (run as Administrator):
netsh advfirewall firewall add rule name="IntegratedStorageCpp TCP 27500" dir=in proto:TCP localport=27500 action=allow
```

### On Client Machines:

Clients typically don't need outbound rules, but if you use a strict corporate firewall:

1. Ensure outbound TCP traffic on port `27500` is allowed
2. Or allow all outbound (recommended for gaming networks)

---

## ⚙️ Configuration Options Explained

### Common Settings Reference

| Setting | Default | Valid Values | Purpose |
|---------|---------|--------------|---------|
| `external_channel` | `true` | `true` / `false` | Enable TCP channel (always `true` for multiplayer) |
| `external_port` | `27500` | 1024-65535 | Port for client connections |
| `external_server_host` | `` (empty) | IP address or empty | Server's IP (clients fill this, hosts leave empty) |
| `reconcile_interval_ms` | `8000` | 500ms - unlimited | Server-side discovery frequency |
| `verbose` | `true` | `true` / `false` | Enable diagnostic logging in UE4SS.log |

### Recommended Values by Guild Size

| Guild Size | reconcile_interval_ms | Reason |
|------------|----------------------|--------|
| Single player / <3 players | 8000ms (default) | Low overhead, fast discovery |
| Medium guild (3-5 players) | 15000ms | Balanced performance/stability |
| Large guild (6+ players, many bases) | 30000ms | Reduced CPU/memory usage |

### Config File Format

```ini
# Comments start with # or ;
key = value      # Spaces around = are optional
key=value        # No spaces also works
key = true       # Boolean values: true/false/yes/no/on/off
key = 27500      # Numeric values (no quotes needed)

# Empty lines and comments are ignored
```

---

## 🚨 Common Configuration Mistakes & Fixes

### Mistake 1: Host Left Without config.txt

**Symptom:** DLL loads but mod doesn't work, or errors on startup

**Fix:** Create `config.txt` in host's mod directory with HOST settings:
```ini
external_channel = true
external_port = 27500
external_server_host =           # Empty!
reconcile_interval_ms = 15000
verbose = false
```

---

### Mistake 2: Host Has Empty external_server_host BUT Friends Have Empty Too

**Symptom:** Friends can't connect to your host session

**Why:** Friends think they're also "hosts" and don't fill their config

**Fix:** Tell friends: **"Fill your client's external_server_host with my IP!"**

---

### Mistake 3: Host Disables TCP Channel (external_channel = false)

**Symptom:** Friends report connection errors, timeouts, or can't join

**Why:** While host reads containers natively, CLIENTS still need TCP channel to query the host's container state

**Fix:** Keep `external_channel = true` on host. Clients DO need it!

---

### Mistake 4: Host and Client Use Different Ports

**Symptom:** Friends can't connect, get timeout errors

**Examples of mismatch:**
```ini
# Host config:
external_port = 27500

# Client config (WRONG):
external_port = 27501     # Different port!
```

**Fix:** Ensure ALL machines use the SAME port value. Default `27500` works fine.

---

### Mistake 5: Wrong IP Format in Config

**Symptom:** Connection refused or invalid host errors

**Wrong examples:**
```ini
external_server_host = 192.168.1.50.    # Extra period!
external_server_host = localhost        # Won't work across network
external_server_host = 172.16.0.1      # Using wrong subnet IP
```

**Right examples:**
```ini
external_server_host = 192.168.1.50     # Correct IPv4 format
external_server_host = 10.0.0.25        # Correct for corporate networks
```

---

### Mistake 6: Firewall Blocking Port on Host Machine

**Symptom:** Friends can see you in friend list but connection times out

**Fix:** See "Firewall Configuration" section above

---

### Mistake 7: Using Public WAN IP Instead of LAN IP

**Symptom:** Friends on same local network can't connect

**Wrong IP:** `203.45.67.89` (public WAN IP from ISP)  
**Right IP:** `192.168.1.50` (private LAN IP shown in `ipconfig`)

**When this matters:**
- Friends joining from SAME local network/router → Use LAN IP
- Friends joining from outside your network → May need public IP + port forwarding

---

## 🧪 Testing Your Setup Before Friends Join

### Quick Single-Machine Test

1. Launch Palworld on your machine normally
2. Set up a camp, craft some items
3. From title menu, add "localhost" or `127.0.0.1` as a friend
4. Join your own session as another player
5. If materials show correctly for both players, your host config is correct

### Two-Machine Test (Most Reliable)

**Host machine:**
- Deploy DLL with HOST config (`external_server_host = ` empty)
- Note your LAN IP address
- Launch Palworld

**Client machine (friend):**
- Deploy DLL with CLIENT config (`external_server_host = YOUR_HOST_IP`)
- Launch Palworld
- Try to join host's session
- If successful, you're ready for full co-op!

---

## 🎯 Host-SP Best Practices Summary

| Setting | Recommended Value | Reason |
|---------|------------------|--------|
| `external_channel` | `true` | Clients need TCP to connect to you |
| `external_port` | `27500` (or consistent custom port) | Same on all machines |
| `external_server_host` | Empty (`""`) | You are the authority, don't connect |
| `reconcile_interval_ms` | `15000-30000` | Longer intervals for multiple players/bases |
| `verbose` | `false` (production), `true` (debug) | Quiet unless troubleshooting |

### Quick Checklist Before Sharing Your Host Session:

**On YOUR host machine:**
- [ ] `config.txt` exists in mod directory
- [ ] `external_channel = true`
- [ ] `external_server_host = ` (empty!)
- [ ] Know your LAN IP address (`192.168.x.x`)
- [ ] Firewall allows port 27500

**To share with friends:**
- [ ] Share your LAN IP address
- [ ] Tell them the port you're using (default: 27500)
- [ ] Remind them to fill their `external_server_host` with YOUR IP
- [ ] Verify they're using same DLL version

**On FRIENDS' client machines:**
- [ ] `config.txt` exists in mod directory
- [ ] `external_channel = true` (CRITICAL!)
- [ ] `external_port` matches your host exactly
- [ ] `external_server_host` filled with YOUR IP (NOT empty!)

---

## 🐛 Debug Logging

### Enable Verbose Mode for Troubleshooting

Add to any config.txt:
```ini
verbose = true
```

**What you'll see in UE4SS.log:**
```
[ISGATE] TCP server listening on port 27500
[ISGATE] TCP client connected to 192.168.1.50:27500
[ISGATE] CH enter-camp -> flagged (inCamp=true)
[ISGATE] SRV discover: chests=15 camps=8 guilds=3 inst=22 campIds=8
```

**Disable after troubleshooting:** Set `verbose = false` to reduce log verbosity.

### Common Debug Messages

| Message | Meaning | Action |
|---------|---------|--------|
| `[ISGATE] TCP server listening on port 27500` | Server ready for connections | Normal |
| `[ISGATE] TCP client connected to X.X.X.X:27500` | Client successfully connected | Normal |
| `[ISGATE] CH request camp=X...` | Channel request sent | Normal |
| `[ISGATE] CH-RECV FULL len=XXX` | Full pool received | Normal |
| `[ISGATE] CH channel recovered after N consecutive misses` | Connection issue resolved | Temporary |

---

## 📞 Troubleshooting Guide

### Issue: "Failed to connect" on clients

**Checklist:**
1. [ ] Host has `config.txt` (not missing)
2. [ ] Host's `external_server_host = ` is empty
3. [ ] Client's `external_server_host` filled with host's IP
4. [ ] Both use same port number
5. [ ] Firewall on host allows port 27500
6. [ ] Using correct LAN IP (not public WAN IP)

### Issue: Connection timeout

**Common causes:**
- Firewall blocking port
- Wrong IP format (trailing period, spaces)
- Using public IP instead of LAN IP
- Port already in use by another application

**Fix:** Enable verbose on host (`verbose = true`), check UE4SS.log for connection attempt details

### Issue: Friends can see you but can't join

**This usually means:**
- TCP channel connection established but data transfer failing
- Check if host and client ports match EXACTLY
- Verify firewall allows the port (test with telnet or PowerShell `Test-NetConnection`)

**Test command:**
```powershell
Test-NetConnection -ComputerName 192.168.1.50 -Port 27500
```

### Issue: Mod works in single player but not multiplayer

**Checklist:**
1. [ ] Same DLL version on ALL machines
2. [ ] All config.txt files exist (not missing!)
3. [ ] Correct configuration for each role
4. [ ] TCP port not blocked by firewall/antivirus

### Issue: Food consumption shows success but no effect

**This is P0-c fix territory:**
- Verify fork includes `[FIX P0-c]` markers in `src/dllmain.cpp`
- Check that food boxes are properly excluded from cross-registration
- This should be fixed in your JakeSnowy fork - if not, verify you built from correct branch

### Issue: Material pool flickers between players

**This is P0-a fix territory:**
- Verify fork includes `[FIX P0-a]` marker in `src/dllmain.cpp`
- Should only see local player events trigger pool updates
- Check UE4SS.log for `[ISGATE]` messages about remote events being filtered

---

## 🎮 Quick Reference: Config Examples

### Example 1: Default Settings (Most Common)

**Host's config.txt:**
```ini
external_channel = true
external_port = 27500
external_server_host =           # Empty!
reconcile_interval_ms = 15000
verbose = false
```

**Friends' Client Config (fill with host's IP):**
```ini
external_channel = true
external_port = 27500
external_server_host = 192.168.1.100  # Host's IP address
reconcile_interval_ms = 15000
verbose = false
```

---

### Example 2: Custom Port (Port 27500 Already in Use)

**Host's config.txt:**
```ini
external_channel = true
external_port = 27501           # Changed to avoid conflict
external_server_host =          # Still empty!
reconcile_interval_ms = 20000
verbose = false
```

**Friends' Client Config:**
```ini
external_channel = true
external_port = 27501           # Must match host EXACTLY
external_server_host = 192.168.1.100
reconcile_interval_ms = 20000
verbose = false
```

---

### Example 3: Debug Mode (Troubleshooting)

**Host's config.txt:**
```ini
external_channel = true
external_port = 27500
external_server_host =          # Empty!
reconcile_interval_ms = 15000
verbose = true                  # Enable detailed logging
```

**Effect:** Host will log `[ISGATE]` messages showing:
- Channel connection attempts
- Reconcile cycles completing
- Any errors that occur

---

## 📚 Related Documentation

- **Main README:** [`README.md`](<README.md>) - Overview and stability comparison  
- **FIXES_README:** [`FIXES_README.md`](<FIXES_README.md>) - P0 fixes overview  
- **CRITICAL_FIXES_SUMMARY:** [`CRITICAL_FIXES_SUMMARY.md`](<CRITICAL_FIXES_SUMMARY.md>) - Technical implementation details
- **FORK_README:** [`FORK_README.md`](<FORK_README.md>) - Fork reference guide

---

## 📄 License

This fork inherits the original MIT license. All documented fixes are based on fault analysis reports from Sarfflow and community investigation. The undocumented optimizations (TCP channel, B3, B4) were implemented in your fork for specific use case requirements based on fault analysis.

---

## 🔗 Related Repositories

- **Your Fork:** https://github.com/Jakesnowy/sharing-base-resources
- **Original Mod:** Sarfflow/IntegratedStorageCpp on UE4SS Mods  
- **UE4SS Framework:** https://github.com/UE4SS-RE/RE-UE4SS

---

**Status:** ✅ Production-ready, multiplayer-stable, optimized for large guilds.

All P0 fixes (P0-a, P0-b, P0-c) are now properly documented and reflected in the actual code state at `src/dllmain.cpp`.