# IntegratedStorage for Palworld（幻兽帕鲁）

**作者：Sarfflow** · 适配 Palworld 1.0 · 一个基于 [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) 的 C++ Mod

在你的公会里**任意地点**建造和制作——直接使用存放在公会**任意**基地营地的材料，而不局限于你当前所在的营地。原版 Palworld 只允许营地使用自己的本地仓库；本 Mod 把同公会所有营地的仓库汇集成一个池，让整个公会的材料在每个营地都可用，包括全新空营地。

- 建造与制作菜单会显示每种材料的**真实公会汇总数量**。
- 你公会确实缺少的配方仍然保持灰显/红显，与原版一致。
- 服务器保持权威性——它从材料实际所在位置真实消耗。
- 原生 `ItemStackInfo` **绝不修改**，因此原生的快速堆叠（Quick Stack）和物品回收装置（Item Retrieval Device）照常工作。

支持单人（host）、监听/主机会话，以及带远程客户端的专用服务器。同一个 DLL 会分发到每个端，运行时根据角色自行判定。

> 本仓库是**源码**。大多数玩家直接安装预编译的 Windows 二进制文件（从获取 Mod 的地方）。源码提供给想阅读、自行编译，或**移植到其他平台**的人（见[移植](#移植到其他平台例如-macos-arm64)）。

---

## 安装（Windows，预编译版）

本 Mod 是标准的 UE4SS C++ Mod。目录结构（见 [`dist/`](dist/)）：

```
<UE4SS Mods 目录>/
└── ModIntegratedStorageCpp/
    ├── enabled.txt          （空文件——存在即启用本 Mod）
    ├── config.txt           （可选；见下文）
    └── dlls/
        └── main.dll         （编译后的 Mod——你需要自行编译，见下文）
```

在服务器和**每个**客户端上安装**相同**的 Mod 文件夹。在专用服务器上，Mod 负责权威的跨注册与消耗；在远程客户端上，它负责显示和客户端侧的制造/建造判定。

### config.txt

所有键都是可选的；缺失时各自回落到内置默认值。见
[`dist/ModIntegratedStorageCpp/config.txt`](dist/ModIntegratedStorageCpp/config.txt)。

| 键 | 默认值 | 含义 |
|---|---|---|
| `verbose` | `true` | 在 `UE4SS.log` 中输出详细的 `[ISGATE]` 诊断信息。功能正常后可设为 `false`。 |
| `reconcile_interval_ms` | `8000` | （服务器/主机）重新扫描公会箱子并重新应用合并的频率。最小 500。 |
| `isi_refresh_ms` | `1500` | 保留/仅为配置兼容而保留。 |

---

## 工作原理

所有逻辑都在加载时通过**唯一的 AOB 特征码**定位，因此能抵御游戏更新导致的地址偏移。三种角色，同一个二进制文件：

- **服务器（权威端）**——每隔 `reconcile_interval_ms` 执行一次*发现式对账（discovery reconcile）*：从地图对象管理器枚举每个公会箱子，以及每个基地营地（包括空营地），然后把每个公会箱子的容器交叉注册到同公会每个营地的存储模块中。这让原生的建造/制造流程能够**消耗**跨营地材料。在主机/单人权威端上，原生材料收集器随后读取已合并的容器，因此显示自动正确。

- **远程客户端**——看不到远处营地的容器，因此通过**铸造**本地物品槽位并临时追加到一个备用库存容器（"cont5"）来**显示**公会总量，**仅在原生材料扫描期间**生效（三个 AOB 定位的 detour）。逐物品的池数据通过一个自定义传输通道（见下）传输，绝不走 `ItemStackInfo`。

- **传输通道**——按需驱动且事件驱动：客户端通过 `OnEnterBaseCamp`/`OnExitBaseCamp` 钩子追踪当前营地（无轮询），在顶层 tick 中触发一个轻量请求，服务器解析该客户端的营地、读取（公会 − 自身）的容器真实内容，并通过引擎 RPC 回复（`Debug_CheatCommand_ToServer` / `Debug_ReceiveCheatCommand_ToClient`，携带一个小的 `key:count,…` 字符串）。任何逐帧路径都不使用 `FindAllOf`。

核心实现集中在单个文件：[`src/dllmain.cpp`](src/dllmain.cpp)。代码注释非常详尽——建议从文件顶部的架构说明块，以及 `on_update` / `install` 方法开始阅读。

---

## 编译（Windows）

本 Mod 在 UE4SS custom-mods 源码树内编译；它**不是**独立项目。

**依赖（均由 UE4SS 源码树提供）：**
- UE4SS C++ Mod SDK（`Mod/CppUserModBase.hpp`、`Unreal/*`、`DynamicOutput/*`）。
- **PolyHook2**（`polyhook2/Detour/x64Detour.hpp`）——用于 trampoline detour。UE4SS 已内置；本 Mod 通过 `add_packages("polyhook_2")` 链接。

**步骤：**
1. 克隆并编译 [RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS)，配置其 xmake 构建。
2. 将本 Mod 放入源码树作为一个 C++ mod 目标，例如把 `src/` 复制到 `cppmods/ModIntegratedStorageCpp/`（[`src/xmake.lua`](src/xmake.lua) 中的目标名为 `ModIntegratedStorageCpp`），并按 RE-UE4SS 注册其自身 cppmods 的方式注册。
3. `xmake build ModIntegratedStorageCpp`。
4. 把生成的 DLL 复制到 `.../Mods/ModIntegratedStorageCpp/dlls/main.dll`。

编译目标平台为 Win64 / MSVC。

---

## 移植到其他平台（例如 macOS arm64）

macOS 版 UE4SS 从 `ModName/dlls/main.dylib` 加载 C++ Mod，并导出相同的 API（`CppUserModBase`、`start_mod` 等），因此 Mod 的*结构*可以直接沿用。但移植并非简单重编译——以下是按难度递增排列的、诚实的可移植性对照：

1. **纯 UE4SS 反射部分（可干净移植）：** 角色判定（`IsServer`/`IsDedicatedServer`）、进入/离开营地钩子、传输请求/回复 RPC，以及服务端的发现式对账 + 容器交叉注册。这些都走 UE4SS 反射 API，不使用任何平台相关技巧。**这构成了整个服务端部分和传输通道，也就是大部分价值所在。**

2. **编译器/系统适配（机械性工作）：**
   - `<Windows.h>` + `GetModuleFileNameW`/`MAX_PATH`（仅用于在 DLL 旁查找 `config.txt`）→ 通过 `dladdr` 定位模块路径，或干脆去掉配置解析、硬编码默认值。
   - `GetTickCount64()` → `std::chrono::steady_clock`。
   - SEH（`__try/__except`、`__try/__finally`）用于保护容器读取，并*保证*即使原生扫描出错也能恢复借用的容器。clang 不支持 SEH——在正常 + C++ 异常路径上使用 RAII 作用域守卫（在析构函数中恢复）；硬件故障的兜底属于防御性额外措施，在健康的客户端上可以省略。

3. **字符宽度（微妙——请细读）：** 代码通过 `wchar_t*` 读取 UE 字符串/`FName`/`FGuid` 字节。在 Windows 上 `wchar_t` 是 16 位，与 UE 的 UTF-16 `TCHAR` 一致。在 macOS/Linux 上 **`wchar_t` 是 32 位**，会读错每一个字符串。对这些读取请改用 UE 的 `TCHAR` / `char16_t` / `RC::CharType` 类型定义（以及 `FString`/`FName` API），而非原始 `wchar_t`。

4. **显示注入部分（需要重新逆向）：** 三个材料扫描 detour 及其结构体偏移（`OFF_*`）是从 **Windows x86-64** 版 Palworld 逆向得到的。AOB 字节特征码是 x86-64 操作码，**不会**匹配 arm64 二进制文件，且结构体偏移可能不同。如果你的 macOS 客户端是 arm64 原生游戏进程，这一半需要在该二进制文件上重新定位。注意：客户端侧的*判定*（制造是否被允许）绑定到这条显示路径——因此没有它，远程客户端可以请求，但本地菜单可能仍会拒绝。服务端权威的消耗本身并不依赖它。

如果你已经把反射 + 传输部分编译通过，并在逆向部分需要帮助，请开一个 issue 并附上你的 `UE4SS.log`（客户端和服务器）以及你看到的现象。

---

## 许可证

[MIT](LICENSE)。可自由使用；如能注明出处则不胜感激。
