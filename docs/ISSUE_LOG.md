# 问题台账 / 2026-11-24
记录已发现的问题、修复证据和未验证事项。CI 通过不等于实机通过。新增实际问题优先提交 Issue 或更新本表，附版本、步骤、预期、实际、回归验证。
 
| 编号 | 问题 | 状态 / 证据 |
|---|---|---|
| PM-001 | v0.1.4 MSVC C4334 严格构建失败 | 修复 fdc3012，CI 45330149185 通过 |
| PM-002 | v0.1.7 MSVC C4244 转换警告 | v0.1.8 修复，CI 45338189598 通过 |
| PM-003 | v0.1.8 首次提交 Win32 标志类型转换 | a21cdae 修复，CI 45338189598 通过 |
| PM-004 | 左右同名修饰键或两物理设备的同键可能提前释放（代码审查发现，未实机复现） | 0.1.9 添加物理按键当前状态聚合及回归测试，待 CI/实机 |
| PM-005 | 缺少直接 Start/Back/D-pad/右摇杆等测试入口 | 0.1.9 添加人工独立映射，待 CI/实机 |
| PM-006 | 输入桥反馈依赖控制台 | 0.1.9 添加复制报告及限量运行事件，不含原始按键或窗口名称，待实机 |
| PM-007 | 原生键鼠可能与手柄模式冲突 | P0 待确认；不声称已解决 |
| PM-008 | 同屏设备识别、重连及真实技能页 | P0 待实机，记录客户端版本 |
| PM-009 | 原生自动选敌及手动方向调整 | P0 待实机；仅游戏自身选敌 |
| PM-010 | 崩溃、单人模式、市集、性能 | P1 待实机，见 ACCEPTANCE.md |
 
反馈入口：https://github.com/jkszz231210-ops/poe2mouse/issues/new

| PM-011 | 缺少 SDK 或配置错误时主程序直接退出，无法看帮助 | 0.1.10 延迟加载 SDK、配置错误保留界面，待 CI/实机 |
| PM-012 | 主程序只有英文按钮，首次使用流程不明确 | 0.1.10 中文步骤、内置教程、实际键位查看、依赖检查，待 CI/实机 |

| PM-013 | 公司实测：文字教程不足以带领用户完成操作 | 0.1.11 可点击分步引导、无驱动预览，待实机 |
| PM-014 | 公司实测：英文键位含义难懂，沙盒与桥配置不同 | 0.1.11 中文改键表、同配置预览、即时应用，待实机 |
| PM-015 | 原生背包、地图、商城与物品组合键可能冲突 | 0.1.11 默认 F10 显式暂停、I 默认释放、原生快捷键矩阵；实际游戏混合输入仍待验证 |

| PM-016 | 静态审查：离线预览 Raw Input 入口仍检查 armed，导致预览不响应 | 已修正入口检查，待 CI/实机 |

| PM-017 | v0.1.11 Windows CI：新手教程宽字符串拼接错误（C3873/C2001） | 已修正，待新版 CI |

| PM-018 | 公司 Windows 实测 0.1.11：SDK connect/add/neutral/user_index/remove 均返回 0x20000000，但主程序 E206/E207 并清理设备 | 依赖连接与创建/移除接口成功；槽位及回读尚未通过。0.1.12 分离错误码、输出实际槽位及 XInput 状态，延长有限枚举等待，待实机复测；根因未确认 |

| PM-019 | 公司实机 0.1.11：joy.cpl 两个 Xbox 360，XInput 0/1 connected，退出 0..3 absent，SDK index 返回成功但 E206 | 与上游 https://github.com/nefarius/ViGEmClient/issues/31 高度吻合，实际 SDK 数值尚未知。0.1.13 逐个创建并观察唯一稳定新增槽位；SDK index 仅诊断；必须通过逐设备实际回读后才允许真实输入。待实测 |

| PM-020 | 真实设备本窗测试原本不显示实际 XInput 状态，用户必须切换系统控制器属性 | 0.1.14 增加仅本窗启用的实时回读面板、持续不一致保护及反馈计数，待实机 |

| PM-021 | 用户反馈：外部依赖需自行找包、解压与复制，新手引导中断 | 0.1.15 增加显式一键准备、固定下载与校验、驱动安装确认、失败后复检入口；待实机 |

| PM-022 | 0.1.15 公司实测：实际槽位 0/1，SDK 均为 0（PM-019 识别已通过）；E207 两设备初始轴同为 -3356,-1869,-3255,-848，按钮/扳机为零 | 与第三方 https://github.com/boude75/dualsense-xinput 的初始化现象一致，疑似重复零报告被忽略。0.1.16 在用户确认关闭游戏后逐设备尝试小幅非零轴并验证，再恢复严格全零；不使用容差；失败仍释放设备。待实机验证 |

| PM-023 | v0.1.16 Windows CI C4459/C2220：neutral_init 局部 sent 遮蔽主程序全局 sent；Linux 测试通过 | 局部变量更名，保留 /W4 /WX 与严格全零验证；待新 CI |

## 2026-11-24 v0.1.16 实机复测（7c86d44）
- PM-019：通过。实际 XInput 槽位 P1=0、P2=1，SDK 仍均返回 0（W206）；独立样本验证通过，识别替代方案有效。
- PM-022：通过。两个设备初始轴偏移均为 -3356,-1869,-3255,-848；初始化与恢复全零均 PASS，Both devices exact neutral: PASS。
- PM-023：CI 45356904204 已通过。
- PM-024：自检前点击真实设备本窗测试出现通用 E302；自检随后通过。待改进按钮可用状态和错误分流；尚未验证自检后的人工输入。

## 2026-11-24 v0.1.16 人工键鼠实测补充
用户报告 9/9 通过：W→P1 左摇杆上、↑→P2 左摇杆上、Q→P1 A、Shift+Q→P2 A、鼠标左键→P1 RT、Shift+左键→P2 RT、W+↑ 独立同时操作、全部松键归零、F10 暂停。主观反馈灵敏；不是量化延迟测试。短键盘需求：Ctrl+WASD→P2 独立移动，可通过现有 Schema 3 编辑器新增，默认尚未绑定；先按 Ctrl 再按移动键，已按住的键保持原角色至释放。PM-024 的引导/错误提示仍待改进。

| PM-025 | 用户实测 0.1.16：选择测试窗口（3秒）反复 E302，未提供细分状态；本窗此前已通过 | 0.1.17 前置检查、自检状态明确提示、限定目标窗口、选定后最多5秒等待映射键/鼠标释放、连续两次中立再启用，错误细分并进入诊断。实际原因及外窗复测待确认。 |

## v0.1.17 家庭实机补充（2026-11-24）
用户日志：P1/P2 槽位 0/1，W206 仍存在但独立设备识别有效；P1/P2 初始化与严格归零 PASS；Device 1/2 sample=PASS neutral=PASS。自检前 E302-C 正确阻止启用，自检后 Arm: PASS local → Target selected; waiting up to 5 seconds for released inputs → Arm: PASS external → Arm: PASS local。PM-025 的外窗启用流程在此次环境验证通过；目标应用名称与实际外窗输入效果未提供，不据此认定游戏兼容。重复手动自检的易用性仍待优化。


## 2026-11-24 v0.1.18 hybrid controller MVP
- HM-001: no physical controller source in v0.1.17. Add XInput 9.1.0 polling for exactly one physical XInput controller captured before virtual device creation.
- HM-002: add shared left-stick routing and active-role-only buttons/right stick/triggers; Back/View toggles P1/P2.
- HM-003: keyboard P1-target bindings follow active role; explicit P2 modifier-layer chords remain fixed P2. Schema 1 bridge.options remains backward compatible; schema 2 controls role-toggle mask and follow-active.
- Required runtime: one physical XInput device; HidHide manual setup to hide physical pad from game but whitelist input_bridge.exe; verify only two virtual devices visible to game. Game and policy compatibility not claimed.

- CI fix: first hybrid run failed due to a test variable name collision (`live`); renamed to `outside_deadzone`. Latest run 45414343127 passes Windows build/CTest/package and Ubuntu build/CTest. No physical XInput runtime evidence yet.
