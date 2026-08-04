local projectName = "ModIntegratedStorageCpp"

target(projectName)
    add_rules("ue4ss.mod")
    add_files("src/dllmain.cpp")
    -- Needed for the trampoline detour on the native material collector. polyhook_2 is already
    -- required by UE4SS (deps/xmake.lua); the mod links it directly so PLH::x64Detour resolves.
    add_packages("polyhook_2")
    -- WinSock2 for the external transport channel (TCP) — replaces the debug-RPC carrier that
    -- saturated the PlayerController reliable buffer and froze all native interactions.
    add_links("ws2_32")
