# 编译交接说明 — v3.5 修复（Fix 1–4）

> **本文档面向在另一台电脑上操作的 AI / 开发者。**
> 本机（编写代码的机器）没有安装 RE-UE4SS 源码树和 MSVC，无法编译。
> 代码修改已全部完成并 push 到 GitHub，需要在有编译环境的机器上验证编译。

---

## 一、本次修改总览（v3.4 → v3.5）

| 修复 | 文件/位置 | 改了什么 | 解决的问题 |
|------|-----------|----------|------------|
| **Fix 1** | `dllmain.cpp` `hkEnterCamp` (~line 830-870) | 加入本地玩家过滤：比较 `ctx.Context->GetOuter()` 与本地 PlayerController 的 pawn | 多人共享失灵 + Reliable RPC 风暴（死亡螺旋触发器） |
| **Fix 2** | `dllmain.cpp` `srvCampByIdInner` (~line 505-515) | 用 `g_campIdToCamp` 缓存替代每次请求的 `FindAllOf("PalBaseCampModel")` | 每次客户端请求 O(全部 UObject) → O(1) 哈希查表 |
| **Fix 3** | `dllmain.cpp` reconcile 循环 (~line 543) | 移除 `if (isChest)` 条件，食物箱和箱子一样加入交叉注册 | 建造时无法消耗其他营地的食物（食物箱被发现了但从未注册） |
| **Fix 4a** | `dllmain.cpp` reconcile part (b) (~line 550-587) | `FindAllOf("PalBaseCampModel")` → `PalBaseCampManager.GetBaseCampIds()` + `TryGetModel()` | reconcile 中第一个 FindAllOf 扫描消除 |
| **Fix 4b** | `dllmain.cpp` reconcile 容器发现 (~line 592-624) | `FindAllOf("PalItemContainer")` → 直接读取 `ItemContainerMap_InServer` TMap (偏移 0x98) | reconcile 中第二个 FindAllOf 扫描消除 |

### 新增的全局变量

```cpp
static std::unordered_map<std::wstring, UObject*> g_campIdToCamp; // Fix 2: camp-id hex → camp object
```

### 新增的常量

```cpp
static const uintptr_t OFF_CONT_MGR_MAP = 0x98; // PalItemContainerManager.ItemContainerMap_InServer (TMap)
```

---

## 二、编译方式（二选一）

### 方式 A：GitHub Actions 云端编译（推荐，零本地环境）

本项目已有 `.github/workflows/build.yml`。**只需 push 代码到 `main` 分支即可自动触发编译。**

```powershell
git pull
# （代码已经 commit + push 了，如果还没有就执行下面的）
git add src/dllmain.cpp .gitignore BUILD_TASK_v3.5.md
git commit -m "feat: v3.5 — hkEnterCamp fix + srvCampById cache + food-box registration + native API reconcile"
git push
```

push 后：
1. 打开 GitHub 仓库 → `Actions` 标签
2. 等待 `Build Mod` 工作流完成（绿色 ✓）
3. 下载 `Artifacts` 中的 `ModIntegratedStorageCpp.zip`
4. 解压，把 `ModIntegratedStorageCpp` 文件夹复制到游戏 UE4SS Mods 目录

### 方式 B：本地编译（需要 RE-UE4SS 源码树 + xmake + MSVC）

```powershell
# 首次（自动装环境）：
.\build.ps1 -Bootstrap -UE4SSRoot D:\src\RE-UE4SS -Zip

# 之后每次：
.\build.ps1 -UE4SSRoot D:\src\RE-UE4SS -Zip
```

详见 `打包指南.md`。

---

## 三、需要验证的关键点

编译成功后，请检查以下逻辑是否正确：

### 3.1 Fix 1: hkEnterCamp 本地玩家过滤

**位置**：搜索 `hkEnterCamp`

**逻辑**：
- 旧的 `hkEnterCamp` 忽略了 `ctx.Context`（PalBuilderComponent），导致任何玩家的进营事件都触发
- 新逻辑检查 `ctx.Context->GetOuter()` 是否是本地玩家的 Character，只让本地玩家的事件通过
- 使用 `FindFirstOf("PalPlayerController")` + `K2_GetPawn()` 获取本地玩家 pawn
- 有 `__try/__except` SEH 保护

**验证要点**：确认 `GetOuter()` 返回的是 owning actor（通常是 PalPlayerCharacter），且 `K2_GetPawn` 返回的是同一个 pawn。

### 3.2 Fix 2: srvCampById 缓存

**位置**：搜索 `srvCampByIdInner`

**逻辑**：
- `g_campIdToCamp` 在 reconcile 中每 8 秒重建一次
- `srvCampByIdInner` 改为 O(1) 查表：`g_campIdToCamp.find(hex)`
- 不再每次调用 `FindAllOf`

**验证要点**：`g_campIdToCamp` 的 key 是营地 ID 的 hex 字符串（`hexOf(camp + OFF_CAMP_ID)`），value 是营地对象。确保 `srvCampByIdInner` 中的 hex 格式与填充时一致。

### 3.3 Fix 3: 食物箱交叉注册

**位置**：搜索 `Both chests AND food boxes are cross-registered`

**逻辑**：
- 旧代码：`if (isChest) { g.models.insert(model); ... }` — 只注册箱子
- 新代码：无条件 `g.models.insert(model); g.modelCamp[model] = camp;` — 箱子和食物箱都注册

**安全性**：`OnAvailableConcreteModel_ServerInternal` 注册到的是 `PalBaseCampModuleItemStorage`（物品存储模块），只影响建造材料查询。帕鲁喂食用的是不同模块，不受影响。

### 3.4 Fix 4a: PalBaseCampManager 原生 API

**位置**：搜索 `B4 — enumerate ALL camps via PalBaseCampManager`

**逻辑**：
1. `FindFirstOf("BP_PalBaseCampManager_C")` 获取管理器实例（找不到则回退 `FindFirstOf("PalBaseCampManager")`）
2. `GetFunctionByNameInChain("GetBaseCampIds")` 获取函数指针
3. `ProcessEvent(getIdsFn, &idP)` 调用，`idP.OutIds` 填充为 `TArray<FGuid>`
4. 对每个 GUID 调用 `TryGetModel`，获取营地对象
5. 如果原生 API 失败（管理器/函数未找到），回退到 `FindAllOf`

**参数结构验证**（对照 dump）：
```
GetBaseCampIds:
  struct { RawTArray OutIds; }   // offset 0, 16 bytes (data*, num, max)

TryGetModel:
  struct {
      uint8_t Id[16];   // offset 0x00, BaseCampId (FGuid)
      UObject* Out;     // offset 0x10, OutModel
      bool Ret;          // offset 0x18, ReturnValue
  }
```

**验证要点**：
- `RawTArray` 是 `{ uint8_t* data; int32_t num; int32_t max; }`，在 line 61 定义
- `OutIds.num` 是 FGuid 数量，每个 16 字节，stride = `idP.OutIds.data + ci * 16`
- `FindFirstOf` 用 BP 类名 `"BP_PalBaseCampManager_C"` — UE4SS 的 `FindFirstOf` 使用 `IsA` 检查继承链，原生类名也能匹配 BP 实例

### 3.5 Fix 4b: ItemContainerMap_InServer TMap 直读

**位置**：搜索 `B4 — read ItemContainerMap_InServer`

**逻辑**：
1. `FindFirstOf("BP_PalItemContainerManager_C")` 获取容器管理器
2. 从偏移 `OFF_CONT_MGR_MAP` (0x98) 读取 TMap 的稀疏数组
3. 遍历稀疏数组（stride 0x20, value 在 +0x10），获取每个 `PalItemContainer*`
4. 读取每个容器的 `OwnerMapObjectInstanceId` (OFF_CONT_OWNER = 0xF8)，构建 hex → container 映射
5. 如果管理器未找到或 TMap 无效，回退到 `FindAllOf`

**TMap 布局**（与 MapObjectManager 的 TMap @0x310 完全一致）：
```
TMap<FPalContainerId (16B), UObject* (8B)>
稀疏数组元素 stride = 0x20:
  +0x00: Key (FPalContainerId, 16 bytes)
  +0x10: Value (UObject*, 8 bytes)
  +0x18: HashNextId (int32)
  +0x1C: HashIndex (int32)

TMap 结构体:
  +0x00: uint8_t* elems   (稀疏数组元素缓冲)
  +0x08: int32_t maxIdx    (总槽数，含空洞)
  +0x10: inline allocation bits (备用)
  +0x20: uint32_t* words   (分配位图指针)
```

**验证要点**：
- 位图指针优先读 0x20，为 null 则回退到 inline (0x10)，与现有 MapObjectManager 代码一致
- `guidZero` 跳过 OwnerMapObjectInstanceId 为零的容器（玩家背包等非营地容器）

---

## 四、如果编译报错

### 常见问题

| 报错 | 原因 / 解决 |
|------|-------------|
| `'RawTArray' was not declared` | 不应该出现 — `RawTArray` 在 line 61 定义。检查是否有预处理宏问题 |
| `'GetFunctionByNameInChain' not found` | 不应该出现 — 这是 UE4SS UObject 的标准方法，代码其他地方已大量使用 |
| `'OFF_CONT_MGR_MAP' undeclared` | 检查常量定义区（搜索 `OFF_CAMP_GROUPID`，新常量在它后面） |
| `std::memcpy 未声明` | `<cstring>` 已在 line 31 include |
| SEH 相关编译错误 | `__try/__except` 需要 `/EHa` 或至少 `/EHsc`。RE-UE4SS 的 xmake 规则已配置 |
| `target not found` | 在 RE-UE4SS 根的 xmake.lua 里确保有 `includes("cppmods/ModIntegratedStorageCpp/xmake.lua")` |

### 如果 Fix 4 的原生 API 在运行时崩溃

所有 Fix 4 代码都在 reconcile 的 `__try/__except` SEH 保护内（`srvDiscoverReconcile` wrapper），且有 `FindAllOf` 回退。即使原生 API 偏移在新游戏版本中变了，也只会：
1. 触发 SEH 异常 → 被捕获，保持上一次的有效状态
2. 或 native API 返回空结果 → 自动回退到 `FindAllOf`

**不会导致崩溃或卡死。**

---

## 五、UE4SS Object Dump 关键数据（供验证偏移）

以下是 100MB 的 `UE4SS_ObjectDump.txt` 中提取的关键数据（已从 dump 中移除，不入 git）：

```
PalBaseCampManager:
  Function: GetBaseCampIds (有原生实现 f: 指针)
    - OutIds: ArrayProperty output (TArray<FGuid>)
  Function: TryGetModel (有原生实现 f: 指针)
    - BaseCampId: StructProperty [o: 0x00] (FGuid, 16B)
    - OutModel: ObjectProperty [o: 0x10] (8B)
    - ReturnValue: BoolProperty [o: 0x18] (1B)
  实例类名: BP_PalBaseCampManager_C

PalItemContainerManager:
  Property: ItemContainerMap_InServer [o: 0x98] (MapProperty)
    Key: StructProperty FPalContainerId (16B) [o: 0x00]
    Value: ObjectProperty PalItemContainer* (8B) [o: 0x10]
  实例类名: BP_PalItemContainerManager_C

已有偏移常量:
  OFF_CAMP_MODULES = 0x180  (PalBaseCampModel.ModuleArray)
  OFF_CAMP_GROUPID = 0xE4   (PalBaseCampModel.GroupIdBelongTo)
  OFF_CAMP_ID      = 0x58   (PalBaseCampModel own ID, FGuid)
  OFF_CONT_SLOTS   = 0x70   (PalItemContainer.Slots array)
  OFF_CONT_OWNER   = 0xF8   (PalItemContainer.OwnerMapObjectInstanceId)
```

---

## 六、编译成功后的部署步骤

1. 编译产出 `main.dll`（或从 GitHub Actions 下载 zip）
2. 部署到服务器：`<PalServer>\Pal\Saved\Plugins\UE4SS\Mods\ModIntegratedStorageCpp\dlls\main.dll`
3. 部署到每个客户端：`<游戏>\Pal\Binaries\Win64\UE4SS\Mods\ModIntegratedStorageCpp\dlls\main.dll`
4. 启动服务器 + 2+ 客户端连接
5. 检查 `UE4SS.log`：
   - 应出现 `[ISGATE] === IntegratedStorage 3.5 loaded ===`
   - verbose 模式下应出现 `[ISGATE] SRV discover: chests=... camps=... guilds=... inst=... campIds=...`
   - **不应**出现 `B4 fallback` 日志（如果出现，说明原生 API 未生效，检查管理器类名）
6. 测试场景：
   - 在 A 营地建造，消耗 B 营地箱子里的材料 → 应成功
   - 建造配方中需要食物时，消耗 B 营地食物箱里的食物 → 应成功（Fix 3）
   - 多人同时在不同营地建造 → 不应出现卡顿或共享失效（Fix 1）
