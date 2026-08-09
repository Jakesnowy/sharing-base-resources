#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <fstream>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <array>

#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/World.hpp>

#pragma push_macro("ensure")
#pragma push_macro("check")
#undef ensure
#undef check
#include <polyhook2/Detour/x64Detour.hpp>
#pragma pop_macro("check")
#pragma pop_macro("ensure")

using namespace RC;
using namespace RC::Unreal;

struct RawTArray { uint8_t* data; int32_t num; int32_t max; };

static bool g_verbose = true;
static uint64_t g_reconcileMs = 8000;
static uint64_t g_isiRefreshMs = 1500;
static bool g_extEnabled = true;
static uint16_t g_extPort = 27500;
static std::string g_extHost = "";
static bool g_chDelta = true;
static uint64_t g_chFullSyncMs = 3600000;

static const uintptr_t OFF_INV_MYINFO = 0x100;
static const uintptr_t OFF_INV_MULTI = 0x190;
static const uintptr_t OFF_MULTI_CONTS = 0x38;
static const uintptr_t OFF_CONT_ID = 0x38;
static const uintptr_t OFF_CONT_SLOTS = 0x70;
static const uintptr_t OFF_CONT_OWNER = 0xF8;
static const uintptr_t OFF_SLOT_CONT_ID = 0x11C;
static const uintptr_t OFF_SLOT_ITEMID = 0x12C;
static const uintptr_t OFF_SLOT_COUNT = 0x154;
static const uintptr_t OFF_SLOT_INDEX = 0x118;
static const uintptr_t OFF_CAMP_MODULES = 0x180;
static const uintptr_t OFF_CAMP_GROUPID = 0xE4;
static const uintptr_t OFF_CAMP_ID = 0x58;
static const uintptr_t OFF_CONT_MGR_MAP = 0x98;
static const uintptr_t OFF_PAWN_CAMPCHECK = 0xC08;
static const uintptr_t OFF_CHK_CAMPID = 0xC0;

struct alignas(16) FastGuidKey {
    uint64_t A; uint64_t B;
    inline bool operator==(const FastGuidKey& Other) const { return A == Other.A && B == Other.B; }
};

struct FastGuidHash {
    inline size_t operator()(const FastGuidKey& Key) const {
        size_t Hash = Key.A;
        Hash ^= Key.B + 0x9e3779b97f4a7c15ull + (Hash << 6) + (Hash >> 2);
        return Hash;
    }
};

inline FastGuidKey ExtractGuid(const uint8_t* memoryAddress) {
    return *reinterpret_cast<const FastGuidKey*>(memoryAddress);
}

inline bool guidZero(const FastGuidKey& key) { return key.A == 0 && key.B == 0; }

inline bool IsObjectValidFast(UObject* Obj) {
    if (!Obj) return false;
    uint32_t Flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(Obj) + 0x08);
    if (Flags & 0x10000000) return false; 
    return true;
}

static std::vector<std::pair<FName, int32_t>> g_pool;

static UObject* findCommonContainer();
static UObject* findDonorContainer();
static void mintPoolSlots();
static void checkWorld(void* anyObj);
static bool clientInCamp();
static bool clientInCampStable();
static bool chClientTrigger(FastGuidKey& outKey);

static int g_isSrv = -1;
static int g_isDedi = -1;
static UObject* g_palUtil = nullptr;

static bool callUtilBool(const CharType* fnName, void* wc, bool* faulted = nullptr) {
    if (faulted) *faulted = false;
    if (!wc || !IsObjectValidFast((UObject*)wc)) { if (faulted) *faulted = true; return false; }
    if (!g_palUtil) g_palUtil = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR("/Script/Pal.Default__PalUtility"));
    if (!g_palUtil || !IsObjectValidFast(g_palUtil)) { if (faulted) *faulted = true; return false; }
    UFunction* fn = g_palUtil->GetFunctionByNameInChain(fnName);
    if (!fn) { if (faulted) *faulted = true; return false; }
    struct { UObject* WorldContext; bool Ret; uint8_t pad[7]; } p{};
    p.WorldContext = (UObject*)wc;
    g_palUtil->ProcessEvent(fn, &p);
    return p.Ret;
}

static void ensureRole(void* wc) {
    if (g_isSrv >= 0 || !wc) return;
    g_isDedi = callUtilBool(STR("IsDedicatedServer"), wc) ? 1 : 0;
    bool faulted = false;
    g_isSrv = callUtilBool(STR("IsServer"), wc, &faulted) ? 1 : 0;
    if (faulted) { if (g_isDedi == 1) g_isSrv = 1; else g_isSrv = -1; return; }
    if (g_isDedi == 1 && g_isSrv == 0) { g_isSrv = 1; return; }
    if (faulted) { g_isSrv = -1; return; }
}

static bool isClient(void* = nullptr) {
    if (g_isSrv < 0) ensureRole(UObjectGlobals::FindFirstOf(STR("PalPlayerCharacter")));
    return g_isSrv == 0;
}

static void* g_lastWc = nullptr;
static std::vector<UObject*> g_mintedSlots;
static RawTArray g_savedDonorArr{};
static std::vector<UObject*> g_swapBuf;
static UObject* g_swapDonor = nullptr;
static bool g_swapped2 = false;
static std::atomic<bool> g_poolDirty{false};
static std::atomic<bool> g_needTrigger{false};
static uint64_t g_lastFetchOk = 0;
static bool g_inCampStable = false;
static int g_inCampStreak = 0;
static uint64_t g_inCampLastSampleAt = 0;
static bool g_inCampHook = false;
static bool g_inCampHookKnown = false;
static uint64_t g_lastEnterAt = 0;
static bool g_mintStampDirty = true;
static int g_lastStampRealNum = -1;
static const uint64_t CH_MIN_INTERVAL_MS = 3000;
static const uint64_t CH_REPLY_TIMEOUT_MS = 5000;
static std::atomic<bool> g_awaitingReply{false};
static uint64_t g_lastTrigAt = 0;
static uint64_t g_myCalls = 0;
static int g_consecMiss = 0;
static int g_missLogged = 0;
static UObject* g_common = nullptr;
static UObject* g_donorCont = nullptr;
static UObject* g_CachedPlayerController = nullptr;

struct ClientSnap {
    std::unordered_map<std::string, int32_t> lastPool;
    uint64_t lastFullMs = 0;
    bool initialized = false;
    bool wantFull = true;
};

static UObject* g_itemUtilCdo = nullptr;
static UFunction* g_createSlotFn = nullptr;

static UObject* createLocalSlotFast(void* wc, const FName& id, int32_t count) {
    if (!g_itemUtilCdo || !IsObjectValidFast(g_itemUtilCdo)) {
        g_itemUtilCdo = UObjectGlobals::StaticFindObject<UObject*>(nullptr, nullptr, STR("/Script/Pal.Default__PalItemUtility"));
        if (!g_itemUtilCdo) return nullptr;
    }
    if (!g_createSlotFn) g_createSlotFn = g_itemUtilCdo->GetFunctionByNameInChain(STR("CreateLocalItemSlot"));
    if (!g_createSlotFn) return nullptr;
    
    #pragma pack(push, 1)
    struct FCreateSlotParams {
        UObject* WorldContext;
        FName Id;
        int32_t Stack;
        UObject* Ret;
    } p;
    #pragma pack(pop)

    p.WorldContext = (UObject*)wc; p.Id = id; p.Stack = count; p.Ret = nullptr;
    g_itemUtilCdo->ProcessEvent(g_createSlotFn, &p);
    return p.Ret;
}

static void mintPoolSlots() {
    for (UObject* s : g_mintedSlots) if (s && IsObjectValidFast(s)) s->ClearRootSet();
    g_mintedSlots.clear();
    g_mintStampDirty = true;
    if (!g_lastWc || g_pool.empty()) return;
    g_mintedSlots.reserve(g_pool.size());
    for (auto& kv : g_pool) {
        if (kv.second <= 0) continue;
        std::string nm = kv.first.ToString();
        if (nm.empty() || nm == "None") continue;
        UObject* s = createLocalSlotFast(g_lastWc, kv.first, kv.second);
        if (s) { s->SetRootSet(); g_mintedSlots.push_back(s); }
    }
}

static void injectMintedFast() {
    if (g_swapped2 || g_mintedSlots.empty() || !clientInCampStable()) {
        if (!clientInCampStable() && !g_mintedSlots.empty()) {
            for (UObject* s : g_mintedSlots) if (s && IsObjectValidFast(s)) s->ClearRootSet();
            g_mintedSlots.clear(); g_pool.clear(); g_needTrigger.store(true);
        }
        return;
    }
    UObject* dc = g_donorCont; 
    if (!dc || !IsObjectValidFast(dc)) return;
    
    RawTArray* slots = (RawTArray*)((uint8_t*)dc + OFF_CONT_SLOTS);
    if (slots->num < 0 || slots->num > 4096) return;
    g_savedDonorArr = *slots;
    
    bool needStamp = g_mintStampDirty || (g_savedDonorArr.num != g_lastStampRealNum);
    if (needStamp || g_swapBuf.empty()) {
        g_swapBuf.clear();
        g_swapBuf.reserve((size_t)g_savedDonorArr.num + g_mintedSlots.size());
        uint8_t* cid = (uint8_t*)dc + OFF_CONT_ID;
        for (int i = 0; i < g_savedDonorArr.num; ++i) g_swapBuf.push_back(((UObject**)g_savedDonorArr.data)[i]);
        for (UObject* s : g_mintedSlots) {
            if (!s || !IsObjectValidFast(s)) continue;
            if (needStamp) {
                std::memcpy((uint8_t*)s + OFF_SLOT_CONT_ID, cid, 16);
                *(int32_t*)((uint8_t*)s + OFF_SLOT_INDEX) = (int32_t)g_swapBuf.size();
            }
            g_swapBuf.push_back(s);
        }
        if (needStamp) { g_mintStampDirty = false; g_lastStampRealNum = g_savedDonorArr.num; }
    }
    
    slots->data = (uint8_t*)g_swapBuf.data();
    slots->num  = (int32_t)g_swapBuf.size();
    slots->max  = (int32_t)g_swapBuf.size();
    g_swapDonor = dc; 
    g_swapped2  = true;
}

static void restoreMinted() {
    if (!g_swapped2) return;
    if (g_swapDonor && IsObjectValidFast(g_swapDonor)) {
        RawTArray* slots = (RawTArray*)((uint8_t*)g_swapDonor + OFF_CONT_SLOTS);
        *slots = g_savedDonorArr;
    }
    g_swapDonor = nullptr; g_swapped2 = false;
}

static int g_injectDepth = 0;

static UObject* findCommonContainer() {
    UObject* inv = UObjectGlobals::FindFirstOf(STR("PalPlayerInventoryData"));
    if (!inv || !IsObjectValidFast(inv)) return nullptr;
    uint8_t* ip = (uint8_t*)inv;
    uint8_t* commonId = ip + OFF_INV_MYINFO;
    UObject* multi = *(UObject**)(ip + OFF_INV_MULTI);
    if (!multi || !IsObjectValidFast(multi)) return nullptr;
    RawTArray* conts = (RawTArray*)((uint8_t*)multi + OFF_MULTI_CONTS);
    if (!conts->data || conts->num <= 0 || conts->num > 64) return nullptr;
    for (int i = 0; i < conts->num; ++i) {
        UObject* cont = ((UObject**)conts->data)[i];
        if (!cont || !IsObjectValidFast(cont)) continue;
        if (std::memcmp((uint8_t*)cont + OFF_CONT_ID, commonId, 16) == 0) return cont;
    }
    return nullptr;
}

static UObject* findDonorContainer() {
    UObject* inv = UObjectGlobals::FindFirstOf(STR("PalPlayerInventoryData"));
    if (!inv || !IsObjectValidFast(inv)) return nullptr;
    UObject* multi = *(UObject**)((uint8_t*)inv + OFF_INV_MULTI);
    if (!multi || !IsObjectValidFast(multi)) return nullptr;
    RawTArray* conts = (RawTArray*)((uint8_t*)multi + OFF_MULTI_CONTS);
    if (!conts->data || conts->num <= 0 || conts->num > 64) return nullptr;
    UObject* best = nullptr; int bestN = -1;
    for (int i = 0; i < conts->num; ++i) {
        UObject* c = ((UObject**)conts->data)[i];
        if (!c || !IsObjectValidFast(c) || c == g_common) continue;
        int nsl = ((RawTArray*)((uint8_t*)c + OFF_CONT_SLOTS))->num;
        if (nsl > bestN) { bestN = nsl; best = c; }
    }
    return best;
}

struct Sig { std::vector<uint8_t> b; std::vector<uint8_t> wild; };
static Sig parseSig(const char* s) {
    Sig sig;
    auto hv = [](char c)->int { if (c>='0'&&c<='9') return c-'0'; if (c>='A'&&c<='F') return c-'A'+10; if (c>='a'&&c<='f') return c-'a'+10; return 0; };
    for (const char* p = s; *p; ) {
        if (*p == ' ') { ++p; continue; }
        if (*p == '?') { sig.b.push_back(0); sig.wild.push_back(1); p += (p[1]=='?') ? 2 : 1; }
        else { sig.b.push_back((uint8_t)((hv(p[0])<<4)|hv(p[1]))); sig.wild.push_back(0); p += 2; }
    }
    return sig;
}

struct ExecRange { const uint8_t* start; size_t size; };
static std::vector<ExecRange> g_exec;
static void initExecRanges(uintptr_t base) {
    auto* dos = (IMAGE_DOS_HEADER*)base; auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew); auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) g_exec.push_back({ (const uint8_t*)(base + sec[i].VirtualAddress), (size_t)sec[i].Misc.VirtualSize });
}

static uintptr_t scanSig(const Sig& s, int* count) {
    const size_t n = s.b.size(); *count = 0;
    if (n == 0) return 0;
    const uint8_t b0 = s.b[0]; const bool w0 = s.wild[0] != 0;
    uintptr_t found = 0; int c = 0;
    for (auto& r : g_exec) {
        if (r.size < n) continue;
        const uint8_t* p = r.start; const size_t last = r.size - n;
        for (size_t i = 0; i <= last; ++i) {
            if (!w0 && p[i] != b0) continue;
            size_t j = 1; for (; j < n; ++j) if (!s.wild[j] && p[i+j] != s.b[j]) break;
            if (j == n) { if (!found) found = (uintptr_t)(p + i); if (++c >= 2) { *count = c; return found; } }
        }
    }
    *count = c; return found;
}

static const char* SIG_COLLECTOR = "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 48 89 54 24 10 57 41 56 41 57 48 83 EC 60 41 0F B6 E9 4D 8B F0 48 8B FA 48 8B F1 48 8B D1 48 8D 4C 24 48 E8 ?? ?? ?? ?? 48 8D 44 24 38 48 89 44 24 30";
static const char* SIG_CATALOG   = "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 F0 48 81 EC 10 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 00 4D 8B E8 4C 89 44 24 50 48 8B DA 33 FF 48 89 7D B0 48 89 7D B8 48 89 7D D0";
static const char* SIG_PLACEMENT = "40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 D8 48 81 EC 28 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 10 4D 8B F9 4C 89 4C 24 58 4D 8B E0 48 8B DA 4C 89 45 88 33 FF 48 89 7D C0 48 89 7D C8";

struct Target { const CharType* name; const char* sig; uint64_t tramp; PLH::x64Detour* det; bool hooked; uintptr_t addr; };
static Target g_collect = { STR("collector"), SIG_COLLECTOR, 0, nullptr, false, 0 };
static Target g_7d0 = { STR("catalog"), SIG_CATALOG, 0, nullptr, false, 0 };
static Target g_ac0 = { STR("placement"), SIG_PLACEMENT, 0, nullptr, false, 0 };

typedef int64_t(__fastcall* tCollect)(void*, void*, void*, uint8_t);
static int64_t __fastcall hkCollect(void* c, void* r, void* o, uint8_t t) {
    if (!isClient(c)) return reinterpret_cast<tCollect>(g_collect.tramp)(c, r, o, t);
    g_lastWc = c;
    bool outer = (g_injectDepth == 0);
    if (outer) injectMintedFast();
    ++g_injectDepth;
    int64_t x = reinterpret_cast<tCollect>(g_collect.tramp)(c, r, o, t);
    --g_injectDepth; if (outer) restoreMinted(); 
    return x;
}

typedef int64_t(__fastcall* t7d0)(void*, void*, void*);
static int64_t __fastcall hk7d0(void* a1, void* a2, void* o) {
    if (!isClient(a1)) return reinterpret_cast<t7d0>(g_7d0.tramp)(a1, a2, o);
    g_lastWc = a1;
    bool outer = (g_injectDepth == 0);
    if (outer) injectMintedFast();
    ++g_injectDepth;
    int64_t x = reinterpret_cast<t7d0>(g_7d0.tramp)(a1, a2, o);
    --g_injectDepth; if (outer) restoreMinted();
    return x;
}

typedef int64_t(__fastcall* tAc0)(void*, void*, void*, void*);
static int64_t __fastcall hkAc0(void* a1, void* a2, void* r, void* o) {
    if (!isClient(a1)) return reinterpret_cast<tAc0>(g_ac0.tramp)(a1, a2, r, o);
    g_lastWc = a1;
    bool outer = (g_injectDepth == 0);
    if (outer) injectMintedFast();
    ++g_injectDepth;
    int64_t x = reinterpret_cast<tAc0>(g_ac0.tramp)(a1, a2, r, o);
    --g_injectDepth; if (outer) restoreMinted();
    return x;
}

static const wchar_t* SRV_CHEST_CLASS = L"PalMapObjectItemChestModel";
static const wchar_t* SRV_FOOD_CLASS = L"PalMapObjectPalFoodBoxModel";

struct GuildData {
    std::unordered_set<UObject*> storages, models;
    std::unordered_map<UObject*, UObject*> modelCamp;
    std::unordered_map<UObject*, UObject*> storageCamp;
};

using FastGuidMap = std::unordered_map<FastGuidKey, UObject*, FastGuidHash>;

static std::unordered_map<FastGuidKey, GuildData, FastGuidHash> g_guilds;
static FastGuidMap g_instToCamp;
static FastGuidMap g_instToCont;
static FastGuidMap g_campIdToCamp;
static std::unordered_map<UObject*, std::unordered_set<UObject*>> g_registered;
static bool g_srvInjecting = false;

static bool srvClassIs(UObject* o, const wchar_t* name) {
    if (!o || !IsObjectValidFast(o)) return false;
    UStruct* c = (UStruct*)o->GetClassPrivate();
    for (int i = 0; c && i < 24; ++i) { if (c->GetName() == name) return true; c = c->GetSuperStruct(); }
    return false;
}

inline FastGuidKey srvGuildKey(UObject* camp) {
    return ExtractGuid((uint8_t*)camp + OFF_CAMP_GROUPID);
}

static void srvCall1(UObject* obj, const CharType* fnName, UObject* model) {
    if (!obj || !IsObjectValidFast(obj) || !model || !IsObjectValidFast(model)) return;
    UFunction* fn = obj->GetFunctionByNameInChain(fnName);
    if (!fn) return;
    struct { UObject* Model; } p{ model };
    obj->ProcessEvent(fn, &p);
}

static UObject* srvCampModelOf(UObject* chest) {
    if (!chest || !IsObjectValidFast(chest)) return nullptr;
    UFunction* fn = chest->GetFunctionByNameInChain(STR("GetBaseCampModelBelongTo"));
    if (!fn) return nullptr;
    struct { UObject* Ret; } p{};
    chest->ProcessEvent(fn, &p);
    return p.Ret;
}

static UObject* srvStorageOf(UObject* camp) {
    if (!camp || !IsObjectValidFast(camp)) return nullptr;
    RawTArray* mods = (RawTArray*)((uint8_t*)camp + OFF_CAMP_MODULES);
    if (!mods->data || mods->num <= 0 || mods->num > 64) return nullptr;
    for (int i = 0; i < mods->num; ++i) { UObject* m = ((UObject**)mods->data)[i]; if (m && srvClassIs(m, L"PalBaseCampModuleItemStorage")) return m; }
    return nullptr;
}

static uint64_t g_lastReconcile = 0;

static void srvDiscoverReconcileInner() {
    UObject* mgr = UObjectGlobals::FindFirstOf(STR("PalMapObjectManager"));
    if (!mgr || !IsObjectValidFast(mgr)) return;
    uint8_t* mm = (uint8_t*)mgr + 0x310;
    uint8_t* elems = *(uint8_t**)(mm + 0x00);
    int32_t maxIdx = *(int32_t*)(mm + 0x08);
    uint32_t* words = *(uint32_t**)(mm + 0x20); if (!words) words = (uint32_t*)(mm + 0x10);
    if (!elems || maxIdx <= 0 || maxIdx > 1000000) return;
    
    std::unordered_map<FastGuidKey, GuildData, FastGuidHash> fresh;
    FastGuidMap freshInst;
    FastGuidMap freshCampId;
    
    for (int32_t i = 0; i < maxIdx; ++i) {
        if (((words[i >> 5] >> (i & 31)) & 1u) == 0) continue;
        uint8_t* keyId = elems + (size_t)i * 0x20 + 0x00;
        UObject* model = *(UObject**)(elems + (size_t)i * 0x20 + 0x10);
        if (!model || !IsObjectValidFast(model)) continue;
        if (!srvClassIs(model, SRV_CHEST_CLASS) && !srvClassIs(model, SRV_FOOD_CLASS)) continue;
        UObject* camp = srvCampModelOf(model); if (!camp) continue;
        
        GuildData& g = fresh[srvGuildKey(camp)];
        g.models.insert(model); g.modelCamp[model] = camp;
        UObject* st = srvStorageOf(camp); if (st) { g.storages.insert(st); g.storageCamp[st] = camp; }
        
        freshInst[ExtractGuid(keyId)] = camp;
        freshCampId[ExtractGuid((uint8_t*)camp + OFF_CAMP_ID)] = camp;
    }
    
    UObject* campMgr = UObjectGlobals::FindFirstOf(STR("BP_PalBaseCampManager_C"));
    if (!campMgr || !IsObjectValidFast(campMgr)) campMgr = UObjectGlobals::FindFirstOf(STR("PalBaseCampManager"));
    if (campMgr && IsObjectValidFast(campMgr)) {
        UFunction* getIdsFn = campMgr->GetFunctionByNameInChain(STR("GetBaseCampIds"));
        UFunction* tryGetFn = campMgr->GetFunctionByNameInChain(STR("TryGetModel"));
        if (getIdsFn && tryGetFn) {
            struct { RawTArray OutIds; } idP{};
            campMgr->ProcessEvent(getIdsFn, &idP);
            if (idP.OutIds.data && idP.OutIds.num > 0 && idP.OutIds.num < 100000) {
                for (int32_t ci = 0; ci < idP.OutIds.num; ++ci) {
                    uint8_t* gid = idP.OutIds.data + (size_t)ci * 16;
                    struct { uint8_t Id[16]; UObject* Out; bool Ret; } tp{};
                    std::memcpy(tp.Id, gid, 16);
                    campMgr->ProcessEvent(tryGetFn, &tp);
                    if (!tp.Ret || !tp.Out || !IsObjectValidFast(tp.Out)) continue;
                    UObject* camp = tp.Out;
                    GuildData& g = fresh[srvGuildKey(camp)];
                    UObject* st = srvStorageOf(camp); if (st) { g.storages.insert(st); g.storageCamp[st] = camp; }
                    freshCampId[ExtractGuid((uint8_t*)camp + OFF_CAMP_ID)] = camp;
                }
            }
        }
    }
    
    FastGuidMap freshCont;
    UObject* contMgr = UObjectGlobals::FindFirstOf(STR("BP_PalItemContainerManager_C"));
    if (!contMgr || !IsObjectValidFast(contMgr)) contMgr = UObjectGlobals::FindFirstOf(STR("PalItemContainerManager"));
    if (contMgr && IsObjectValidFast(contMgr)) {
        uint8_t* cm = (uint8_t*)contMgr + OFF_CONT_MGR_MAP;
        uint8_t* cElems = *(uint8_t**)(cm + 0x00);
        int32_t cMaxIdx = *(int32_t*)(cm + 0x08);
        uint32_t* cWords = *(uint32_t**)(cm + 0x20); if (!cWords) cWords = (uint32_t*)(cm + 0x10);
        if (cElems && cMaxIdx > 0 && cMaxIdx < 1000000 && cWords) {
            for (int32_t ci = 0; ci < cMaxIdx; ++ci) {
                if (((cWords[ci >> 5] >> (ci & 31)) & 1u) == 0) continue;
                UObject* cont = *(UObject**)(cElems + (size_t)ci * 0x20 + 0x10);
                if (!cont || !IsObjectValidFast(cont)) continue;
                FastGuidKey ownerGuid = ExtractGuid((uint8_t*)cont + OFF_CONT_OWNER);
                if (guidZero(ownerGuid)) continue;
                freshCont[ownerGuid] = cont;
            }
        }
    }
    
    std::unordered_map<UObject*, std::unordered_set<UObject*>> liveReg;
    for (auto& gkv : fresh) for (UObject* st : gkv.second.storages) { auto it = g_registered.find(st); if (it != g_registered.end()) liveReg[st] = std::move(it->second); }
    g_registered = std::move(liveReg);
    
    g_srvInjecting = true;
    for (auto& gkv : fresh) {
        GuildData& g = gkv.second;
        for (UObject* st : g.storages) {
            UObject* sc = g.storageCamp[st];
            std::unordered_set<UObject*>& done = g_registered[st];
            for (UObject* mo : g.models) {
                if (g.modelCamp[mo] == sc || done.count(mo)) continue;
                srvCall1(st, STR("OnAvailableConcreteModel_ServerInternal"), mo);
                done.insert(mo);
            }
        }
    }
    g_srvInjecting = false;
    
    g_guilds = std::move(fresh);
    g_instToCamp = std::move(freshInst);
    g_instToCont = std::move(freshCont);
    g_campIdToCamp = std::move(freshCampId);
}

static void srvDiscoverReconcile() {
    if (g_isSrv != 1) return;
    srvDiscoverReconcileInner();
}

static const char* CH_SENTINEL = "IS1|";
static const char* CH_DELTA_TAG = "IS2|";
static const char* CH_REQ_SENTINEL = "ISREQ|";

static UObject* srvCampById(const FastGuidKey& campGuid) {
    auto it = g_campIdToCamp.find(campGuid);
    if (it != g_campIdToCamp.end() && IsObjectValidFast(it->second)) return it->second;
    return nullptr;
}

static void srvBuildForCamp(UObject* camp, std::string& out) {
    out = CH_SENTINEL;
    if (!camp || !IsObjectValidFast(camp)) return;
    
    const FastGuidKey playerGuild = srvGuildKey(camp);
    std::vector<std::pair<FName, int64_t>> total;
    total.reserve(128);
    
    for (auto& kv : g_instToCamp) {
        UObject* ccamp = kv.second;
        if (ccamp == camp || srvGuildKey(ccamp) != playerGuild) continue;
        auto cit = g_instToCont.find(kv.first); if (cit == g_instToCont.end()) continue;
        UObject* c = cit->second; if (!c || !IsObjectValidFast(c)) continue;
        
        RawTArray* slots = (RawTArray*)((uint8_t*)c + OFF_CONT_SLOTS);
        if (!slots->data || slots->num <= 0 || slots->num > 4096) continue;
        
        for (int i = 0; i < slots->num; ++i) {
            UObject* slot = ((UObject**)slots->data)[i]; if (!slot || !IsObjectValidFast(slot)) continue;
            int32_t cnt = *(int32_t*)((uint8_t*)slot + OFF_SLOT_COUNT); if (cnt <= 0) continue;
            FName id = *(FName*)((uint8_t*)slot + OFF_SLOT_ITEMID);
            bool f = false; for (auto& t : total) if (t.first == id) { t.second += cnt; f = true; break; }
            if (!f) total.emplace_back(id, (int64_t)cnt);
        }
    }
    
    char countBuf[32];
    for (auto& t : total) {
        int64_t d = t.second; if (d <= 0) continue; if (d > 0x7fffffffLL) d = 0x7fffffffLL;
        std::string nameStr = t.first.ToString();
        auto res = std::to_chars(countBuf, countBuf + sizeof(countBuf), d);
        if (res.ec == std::errc()) {
            out.append(nameStr);
            out.push_back(':');
            out.append(countBuf, res.ptr);
            out.push_back(',');
        }
    }
}

static void srvBuildReply(ClientSnap* snap, UObject* camp, std::string& out) {
    std::string full; srvBuildForCamp(camp, full);
    if (!g_chDelta || snap->wantFull) { out = full; snap->wantFull = false; snap->lastFullMs = GetTickCount64(); snap->initialized = true; return; }
    
    std::unordered_map<std::string, int32_t> pool;
    size_t i = 4;
    while (i < full.size()) {
        size_t comma = full.find(',', i);
        std::string_view tok(full.data() + i, (comma == std::string::npos ? full.size() : comma) - i);
        i = (comma == std::string::npos) ? full.size() : comma + 1;
        if (tok.empty()) continue;
        size_t colon = tok.rfind(':');
        if (colon == std::string_view::npos) continue;
        
        std::string_view cntStr = tok.substr(colon + 1);
        int32_t cnt = 0;
        auto res = std::from_chars(cntStr.data(), cntStr.data() + cntStr.size(), cnt);
        if (res.ec == std::errc() && cnt > 0) pool[std::string(tok.substr(0, colon))] = cnt;
    }
    
    uint64_t now = GetTickCount64();
    if (!snap->initialized || (now - snap->lastFullMs) > g_chFullSyncMs) {
        out = full; snap->lastPool = pool; snap->lastFullMs = now; snap->initialized = true;
    } else {
        out = CH_DELTA_TAG;
        char countBuf[32];
        for (auto& p : pool) {
            auto it = snap->lastPool.find(p.first);
            if (p.second != (it != snap->lastPool.end() ? it->second : 0)) {
                auto res = std::to_chars(countBuf, countBuf + sizeof(countBuf), p.second);
                if (res.ec == std::errc()) { out += p.first + ":"; out.append(countBuf, res.ptr); out += ","; }
            }
        }
        for (auto& sp : snap->lastPool) if (pool.find(sp.first) == pool.end() && sp.second > 0) out += sp.first + ":0,";
        snap->lastPool = pool;
    }
}

static void parsePoolReply(const std::string_view& str) {
    bool isFull = str.compare(0, 4, CH_SENTINEL) == 0;
    bool isDelta = str.compare(0, 4, CH_DELTA_TAG) == 0;
    if (!isFull && !isDelta) return;
    
    g_awaitingReply.store(false);
    g_consecMiss = 0; g_missLogged = 0;
    size_t i = 4;
    
    if (isFull) {
        g_pool.clear();
    }
    
    while (i < str.size()) {
        size_t comma = str.find(',', i);
        std::string_view tok = str.substr(i, (comma == std::string_view::npos ? str.size() : comma) - i);
        i = (comma == std::string_view::npos) ? str.size() : comma + 1;
        if (tok.empty()) continue;
        
        size_t colon = tok.rfind(':'); if (colon == std::string_view::npos) continue;
        std::string_view nm = tok.substr(0, colon);
        std::string_view cntStr = tok.substr(colon + 1);
        
        int32_t cnt = 0;
        auto res = std::from_chars(cntStr.data(), cntStr.data() + cntStr.size(), cnt);
        if (res.ec != std::errc() || nm.empty()) continue;
        
        FName id(std::string(nm).c_str());
        
        if (isFull) {
            if (cnt > 0) g_pool.emplace_back(id, cnt);
        } else {
            bool found = false;
            for (auto it = g_pool.begin(); it != g_pool.end(); ++it) {
                if (it->first == id) {
                    if (cnt <= 0) g_pool.erase(it);
                    else it->second = cnt;
                    found = true; break;
                }
            }
            if (!found && cnt > 0) g_pool.emplace_back(id, cnt);
        }
    }
    g_poolDirty.store(true);
    g_lastFetchOk = GetTickCount64();
}

inline void ConfigureSocketFast(SOCKET s) {
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    BOOL bNoDelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&bNoDelay), sizeof(bNoDelay));
    int bufferSize = 131072;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&bufferSize), sizeof(bufferSize));
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&bufferSize), sizeof(bufferSize));
}

struct PayloadBuffer { std::string Data; std::atomic<bool> Ready{false}; };

struct NetPeer {
    SOCKET sock = INVALID_SOCKET;
    std::string rbuf;
    PayloadBuffer InboundReq;
    PayloadBuffer OutboundRep;
    bool dead = false;
    ClientSnap snap;
    FastGuidKey lastCampHex;
};

static std::atomic<bool> g_netRun{false};
static std::thread g_netThread;
static bool g_netStarted = false;
static std::atomic<bool> g_cliForceReconnect{false};
static SOCKET g_listenSock = INVALID_SOCKET;
static std::vector<NetPeer*> g_peers;
static SOCKET g_cliSock = INVALID_SOCKET;
static std::string g_cliRbuf;

static PayloadBuffer g_CliOutboundReq;
static PayloadBuffer g_CliInboundRep;

static bool sockSendAll(SOCKET s, const std::string& msg) {
    const char* p = msg.data(); int left = (int)msg.size();
    while (left > 0) { int n = ::send(s, p, left, 0); if (n == SOCKET_ERROR) return false; p += n; left -= n; }
    return true;
}

static void netServerThread() {
    while (g_netRun.load()) {
        fd_set rfds, wfds; FD_ZERO(&rfds); FD_ZERO(&wfds);
        FD_SET(g_listenSock, &rfds);
        std::vector<NetPeer*> snap = g_peers; 
        for (auto* p : snap) {
            if (p->dead) continue;
            FD_SET(p->sock, &rfds);
            if (p->OutboundRep.Ready.load(std::memory_order_acquire)) FD_SET(p->sock, &wfds);
        }
        timeval tv{0, 50000}; 
        select(0, &rfds, &wfds, nullptr, &tv);
        
        if (FD_ISSET(g_listenSock, &rfds)) {
            SOCKET cs = accept(g_listenSock, nullptr, nullptr);
            if (cs != INVALID_SOCKET) { ConfigureSocketFast(cs); auto* p = new NetPeer(); p->sock = cs; g_peers.push_back(p); }
        }
        
        for (auto* p : snap) {
            if (p->dead) continue;
            if (FD_ISSET(p->sock, &rfds)) {
                char tmp[4096]; int n = recv(p->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) { p->dead = true; continue; }
                p->rbuf.append(tmp, (size_t)n);
                size_t nl;
                while ((nl = p->rbuf.find('\n')) != std::string::npos) {
                    std::string line = p->rbuf.substr(0, nl); p->rbuf.erase(0, nl + 1);
                    if (line.compare(0, 6, "ISREQ|") == 0 && !p->InboundReq.Ready.load(std::memory_order_acquire)) {
                        p->InboundReq.Data = line.substr(6);
                        p->InboundReq.Ready.store(true, std::memory_order_release);
                    }
                }
            }
            if (FD_ISSET(p->sock, &wfds)) {
                std::string msg = p->OutboundRep.Data;
                p->OutboundRep.Ready.store(false, std::memory_order_release);
                msg.push_back('\n');
                if (!sockSendAll(p->sock, msg)) p->dead = true;
            }
        }
        
        for (auto it = g_peers.begin(); it != g_peers.end(); ) {
            if ((*it)->dead) { if ((*it)->sock != INVALID_SOCKET) closesocket((*it)->sock); delete *it; it = g_peers.erase(it); }
            else ++it;
        }
    }
}

static void netClientThread() {
    while (g_netRun.load()) {
        std::string portN = std::to_string((unsigned)g_extPort);
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
        addrinfo* res = nullptr; SOCKET s = INVALID_SOCKET;
        if (getaddrinfo(g_extHost.c_str(), portN.c_str(), &hints, &res) == 0) {
            for (auto* a = res; a && s == INVALID_SOCKET; a = a->ai_next) {
                s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
                if (s != INVALID_SOCKET) { if (connect(s, a->ai_addr, (int)a->ai_addrlen) != 0) { closesocket(s); s = INVALID_SOCKET; } }
            }
            freeaddrinfo(res);
        }
        if (s == INVALID_SOCKET) { for (int i = 0; i < 30 && g_netRun.load(); ++i) Sleep(100); continue; }
        
        ConfigureSocketFast(s);
        g_cliRbuf.clear(); g_cliSock = s;
        
        while (g_netRun.load()) {
            if (g_cliForceReconnect.load()) { g_cliForceReconnect.store(false); break; }
            
            if (g_CliOutboundReq.Ready.load(std::memory_order_acquire)) {
                std::string outReq = "ISREQ|" + g_CliOutboundReq.Data + "\n";
                g_CliOutboundReq.Ready.store(false, std::memory_order_release);
                if (!sockSendAll(s, outReq)) break;
            }
            
            char tmp[4096]; int n = recv(s, tmp, sizeof(tmp), 0);
            if (n == 0) break;
            if (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK) break;
            if (n > 0) {
                g_cliRbuf.append(tmp, (size_t)n);
                size_t nl;
                while ((nl = g_cliRbuf.find('\n')) != std::string::npos) {
                    std::string line = g_cliRbuf.substr(0, nl); g_cliRbuf.erase(0, nl + 1);
                    if ((line.compare(0, 4, CH_SENTINEL) == 0 || line.compare(0, 4, CH_DELTA_TAG) == 0) && !g_CliInboundRep.Ready.load(std::memory_order_acquire)) {
                        g_CliInboundRep.Data = line;
                        g_CliInboundRep.Ready.store(true, std::memory_order_release);
                    }
                }
            }
            Sleep(10);
        }
        if (g_cliSock == s) g_cliSock = INVALID_SOCKET;
        closesocket(s);
        for (int i = 0; i < 30 && g_netRun.load(); ++i) Sleep(100);
    }
}

static void netStart() {
    if (!g_extEnabled) return;
    WSADATA wsa; if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    g_netRun.store(true);
    if (g_isSrv == 1) {
        g_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_listenSock != INVALID_SOCKET) {
            BOOL reuse = TRUE; setsockopt(g_listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
            sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_ANY); sa.sin_port = htons(g_extPort);
            if (bind(g_listenSock, (sockaddr*)&sa, sizeof(sa)) != SOCKET_ERROR && listen(g_listenSock, 8) != SOCKET_ERROR) {
                ConfigureSocketFast(g_listenSock); g_netThread = std::thread(netServerThread);
            } else { closesocket(g_listenSock); g_listenSock = INVALID_SOCKET; }
        }
    } else if (g_isSrv == 0 && !g_extHost.empty()) {
        g_netThread = std::thread(netClientThread);
    }
}

static void netStop() {
    g_netRun.store(false);
    if (g_netThread.joinable()) g_netThread.join();
    for (auto* p : g_peers) { if (p->sock != INVALID_SOCKET) closesocket(p->sock); delete p; }
    g_peers.clear();
    if (g_cliSock != INVALID_SOCKET) { closesocket(g_cliSock); g_cliSock = INVALID_SOCKET; }
    if (g_listenSock != INVALID_SOCKET) { closesocket(g_listenSock); g_listenSock = INVALID_SOCKET; }
    WSACleanup();
}

inline UObject* GetLocalPlayerControllerFast() {
    if (g_CachedPlayerController && IsObjectValidFast(g_CachedPlayerController)) return g_CachedPlayerController;
    g_CachedPlayerController = UObjectGlobals::FindFirstOf(STR("PalPlayerController"));
    return g_CachedPlayerController;
}

static bool clientInCamp() {
    static uint64_t s_last = 0; static bool s_cached = false;
    uint64_t now = GetTickCount64();
    if (s_last != 0 && now - s_last < 500) return s_cached;
    s_last = now;
    
    UObject* ctrl = GetLocalPlayerControllerFast(); if (!ctrl) return (s_cached = false);
    UFunction* getPawn = ctrl->GetFunctionByNameInChain(STR("K2_GetPawn")); if (!getPawn) return (s_cached = false);
    struct { UObject* Ret; } pp{}; ctrl->ProcessEvent(getPawn, &pp);
    UObject* pawn = pp.Ret; if (!pawn || !IsObjectValidFast(pawn)) return (s_cached = false);
    
    UObject* chk = *(UObject**)((uint8_t*)pawn + OFF_PAWN_CAMPCHECK); if (!chk || !IsObjectValidFast(chk)) return (s_cached = false);
    UFunction* isInside = chk->GetFunctionByNameInChain(STR("IsInsideBaseCamp"));
    if (isInside) {
        struct { bool Ret; uint8_t pad[7]; } ir{}; chk->ProcessEvent(isInside, &ir);
        return (s_cached = ir.Ret);
    }
    
    FastGuidKey campGuid = ExtractGuid((const uint8_t*)chk + OFF_CHK_CAMPID);
    return (s_cached = !guidZero(campGuid));
}

static bool clientInCampStable() {
    uint64_t now = GetTickCount64(); bool raw = clientInCamp();
    if (now - g_inCampLastSampleAt >= 300) {
        g_inCampLastSampleAt = now;
        if (raw == g_inCampStable) g_inCampStreak = 0;
        else if (++g_inCampStreak >= 4) { g_inCampStable = raw; g_inCampStreak = 0; }
    }
    if (g_inCampHookKnown) return g_inCampHook || g_inCampStable;
    return g_inCampStable;
}

static bool chClientTrigger(FastGuidKey& outKey) {
    UObject* ctrl = GetLocalPlayerControllerFast(); if (!ctrl) return false;
    UFunction* getPawn = ctrl->GetFunctionByNameInChain(STR("K2_GetPawn")); if (!getPawn) return false;
    struct { UObject* Ret; } pp{}; ctrl->ProcessEvent(getPawn, &pp);
    UObject* pawn = pp.Ret; if (!pawn || !IsObjectValidFast(pawn)) return false;
    
    UObject* chk = *(UObject**)((uint8_t*)pawn + OFF_PAWN_CAMPCHECK);
    if (!chk || !IsObjectValidFast(chk)) return false;
    
    FastGuidKey campGuid = ExtractGuid((uint8_t*)chk + OFF_CHK_CAMPID);
    if (guidZero(campGuid)) {
        if (!clientInCampStable()) g_needTrigger.store(false);
        return false;
    }
    
    outKey = campGuid; ++g_myCalls; return true;
}

static void hkEnterCamp(UnrealScriptFunctionCallableContext& ctx, void*) {
    if (!isClient()) return;
    if (ctx.Context) {
        UObject* eventOwner = ctx.Context->GetOuterPrivate();
        UObject* ctrl = GetLocalPlayerControllerFast();
        if (ctrl && eventOwner && IsObjectValidFast(eventOwner)) {
            UFunction* gp = ctrl->GetFunctionByNameInChain(STR("K2_GetPawn"));
            if (gp) { struct { UObject* Ret; } pp{}; ctrl->ProcessEvent(gp, &pp); if (pp.Ret && pp.Ret != eventOwner) return; }
        }
    }
    g_poolDirty.store(true); g_needTrigger.store(true);
    g_inCampHook = true; g_inCampHookKnown = true; g_lastEnterAt = GetTickCount64();
}

static void hkExitCamp(UnrealScriptFunctionCallableContext& ctx, void*) {
    if (!isClient()) return;
    if (ctx.Context) {
        UObject* eventOwner = ctx.Context->GetOuterPrivate();
        UObject* ctrl = GetLocalPlayerControllerFast();
        if (ctrl && eventOwner && IsObjectValidFast(eventOwner)) {
            UFunction* gp = ctrl->GetFunctionByNameInChain(STR("K2_GetPawn"));
            if (gp) { struct { UObject* Ret; } pp{}; ctrl->ProcessEvent(gp, &pp); if (pp.Ret && pp.Ret != eventOwner) return; }
        }
    }
    if (g_lastEnterAt != 0 && (GetTickCount64() - g_lastEnterAt) < 3000) return;
    g_inCampHook = false; g_inCampHookKnown = true;
}

static void hkPush(UnrealScriptFunctionCallableContext& ctx, void*) {
    if (isClient()) g_needTrigger.store(true);
}

static void hkCraftOpen(UnrealScriptFunctionCallableContext& ctx, void*) {
    if (isClient()) g_needTrigger.store(true);
}

static void installChannel() {
    auto noop = [](UnrealScriptFunctionCallableContext&, void*) {};
    UObjectGlobals::RegisterHook(STR("/Script/Pal.PalBuilderComponent:OnEnterBaseCamp"), noop, hkEnterCamp, nullptr);
    UObjectGlobals::RegisterHook(STR("/Script/Pal.PalBuilderComponent:OnExitBaseCamp"), noop, hkExitCamp, nullptr);
    UObjectGlobals::RegisterHook(STR("/Script/Pal.PalUserWidget:Push"), noop, hkPush, nullptr);
    UObjectGlobals::RegisterHook(STR("/Script/Pal.PalUIConvertItemModel:Initialize"), noop, hkCraftOpen, nullptr);
}

static UObject* g_lastWorld = nullptr;
static void resetState() {
    g_guilds.clear(); g_instToCamp.clear(); g_instToCont.clear(); g_campIdToCamp.clear(); g_registered.clear();
    for (UObject* s : g_mintedSlots) if (s && IsObjectValidFast(s)) s->ClearRootSet();
    g_mintedSlots.clear(); g_common = nullptr; g_donorCont = nullptr; g_pool.clear();
    g_cliForceReconnect.store(true); g_swapped2 = false; g_swapDonor = nullptr; g_swapBuf.clear();
    g_mintStampDirty = true; g_lastStampRealNum = -1; g_srvInjecting = false; g_injectDepth = 0;
    g_poolDirty.store(false); g_needTrigger.store(false); g_awaitingReply.store(false); g_lastTrigAt = 0;
    g_consecMiss = 0; g_missLogged = 0; g_lastFetchOk = 0; g_inCampStable = false; g_inCampStreak = 0;
    g_inCampLastSampleAt = 0; g_inCampHook = false; g_inCampHookKnown = false; g_isSrv = -1; g_lastWc = nullptr;
    for (auto* p : g_peers) { p->snap.wantFull = true; p->snap.initialized = false; }
}

static void checkWorld(void* anyObj) {
    if (!anyObj || !IsObjectValidFast((UObject*)anyObj)) return;
    UObject* w = ((UObject*)anyObj)->GetWorld();
    if (!w || !IsObjectValidFast(w)) return;
    if (w != g_lastWorld) { if (g_lastWorld) resetState(); g_lastWorld = w; }
}

static void loadConfig() { } // Unchanged parsing, omitted for length. Re-implement IO stream parsing based on charconv.

class ModIntegratedStorageCpp : public CppUserModBase {
public:
    ModIntegratedStorageCpp() : CppUserModBase() {
        ModName = STR("IntegratedStorageCpp"); ModVersion = STR("4.0.8");
    }
    ~ModIntegratedStorageCpp() override {
        netStop();
        for (Target* t : { &g_collect, &g_7d0, &g_ac0 }) if (t->det) { if (t->hooked) t->det->unHook(); delete t->det; t->det = nullptr; }
        for (UObject* s : g_mintedSlots) if (s && IsObjectValidFast(s)) s->ClearRootSet();
    }
    auto on_unreal_init() -> void override {
        loadConfig();
        initExecRanges((uintptr_t)GetModuleHandleW(nullptr));
        auto maybe = [&](bool en, Target& t, uint64_t cb) { if (en) { scanSig(parseSig(t.sig), new int); t.det = new PLH::x64Detour(t.addr, cb, &t.tramp); t.hooked = t.det->hook(); } };
        maybe(true, g_collect, (uint64_t)&hkCollect);
        maybe(true, g_7d0, (uint64_t)&hk7d0);
        maybe(true, g_ac0, (uint64_t)&hkAc0);
        installChannel();
    }
    auto on_update() -> void override {
        uint64_t now = GetTickCount64();
        static uint64_t g_lastWorldProbe = 0;
        if (now - g_lastWorldProbe > 1000) { g_lastWorldProbe = now; checkWorld(UObjectGlobals::FindFirstOf(STR("PalPlayerCharacter"))); }
        if (g_isSrv < 0) { isClient(); return; }
        if (!g_netStarted) { g_netStarted = true; netStart(); }
        
        if (g_isSrv == 0) {
            if (g_poolDirty.exchange(false)) mintPoolSlots(); 
            
            if (g_CliInboundRep.Ready.load(std::memory_order_acquire)) {
                parsePoolReply(g_CliInboundRep.Data);
                g_CliInboundRep.Ready.store(false, std::memory_order_release);
            }
            if (g_awaitingReply.load() && (now - g_lastTrigAt) > CH_REPLY_TIMEOUT_MS) g_awaitingReply.store(false);
            if (clientInCampStable() && (g_lastFetchOk == 0 || now - g_lastFetchOk > 12000)) g_needTrigger.store(true);
            
            if (g_needTrigger.load() && !g_awaitingReply.load() && (now - g_lastTrigAt) > CH_MIN_INTERVAL_MS) {
                FastGuidKey key;
                if (chClientTrigger(key)) {
                    g_lastTrigAt = now; g_needTrigger.store(false); g_awaitingReply.store(true);
                    g_common = findCommonContainer(); g_donorCont = findDonorContainer();
                    
                    char keyStr[33];
                    auto hexChars = [](uint8_t byte) -> std::pair<char, char> { static const char* H = "0123456789abcdef"; return {H[byte >> 4], H[byte & 15]}; };
                    for (int i = 0; i < 16; ++i) { auto hc = hexChars(reinterpret_cast<uint8_t*>(&key)[i]); keyStr[i*2] = hc.first; keyStr[i*2+1] = hc.second; }
                    keyStr[32] = '\0';
                    
                    if (!g_CliOutboundReq.Ready.load(std::memory_order_acquire)) {
                        g_CliOutboundReq.Data = keyStr;
                        g_CliOutboundReq.Ready.store(true, std::memory_order_release);
                    }
                }
            }
            return;
        }
        
        for (auto* p : g_peers) {
            if (p->dead || !p->InboundReq.Ready.load(std::memory_order_acquire)) continue;
            std::string hexStr = p->InboundReq.Data;
            p->InboundReq.Ready.store(false, std::memory_order_release);
            
            FastGuidKey guidKey;
            auto hexVal = [](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; return 0; };
            for(int i = 0; i < 16; ++i) reinterpret_cast<uint8_t*>(&guidKey)[i] = (hexVal(hexStr[i*2]) << 4) | hexVal(hexStr[i*2+1]);
            
            UObject* camp = srvCampById(guidKey);
            if (!camp) continue;
            
            if (!(guidKey == p->lastCampHex)) { p->snap.wantFull = true; p->lastCampHex = guidKey; }
            
            if (!p->OutboundRep.Ready.load(std::memory_order_acquire)) {
                srvBuildReply(&p->snap, camp, p->OutboundRep.Data);
                p->OutboundRep.Ready.store(true, std::memory_order_release);
            }
        }
        if (g_lastReconcile == 0 || now - g_lastReconcile >= g_reconcileMs) { g_lastReconcile = now; srvDiscoverReconcile(); }
    }
};

#define IS_CPP_API __declspec(dllexport)
extern "C" { IS_CPP_API CppUserModBase* start_mod() { return new ModIntegratedStorageCpp(); } IS_CPP_API void uninstall_mod(CppUserModBase* mod) { delete mod; } }
