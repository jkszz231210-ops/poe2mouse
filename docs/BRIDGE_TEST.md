# v0.1.7 独立输入桥试验版（尚未实机验证）
本版本增加 input_bridge.exe，首次接通物理键鼠事件到实际虚拟设备报告的代码路径。仅支持人工一对一映射到指定角色。拒绝 both 与 solo 沙盒动作，不自动施法、不跟随、不镜像、不接管游戏内存。驱动及 DLL 由用户自行确认安装，见 DEVICE_PROBE.md。
 
## 初次测试
1. 解压完整便携包，关闭游戏。按 DEVICE_PROBE.md 先运行设备探测，确认两个 XInput 设备的回读及清理通过。
2. 运行 input_bridge.exe，选择可信的 x64 ViGEmClient.dll（或在 PowerShell 传入 DLL 绝对路径），确认创建临时设备。记下控制台显示的 P1/P2 槽位。
3. 先选择 Arm this window。WASD 控制 P1 左摇杆，方向键控制 P2 左摇杆。Q/E/R/F/T 与鼠标左/右/中键是 P1 的测试按钮；Shift+同样的键是 P2 测试按钮。均为诊断映射，不是经过确认的 PoE2 技能槽。鼠标移动尚未映射瞄准。
4. 如需在 joy.cpl 中观察，选择 Select target (3s)，确认后在三秒内切到手柄属性测试窗口。每次只测试单个映射。切出指定窗口即停止并归零，需要手动重新启用。不屏蔽原生键鼠；窗口选择机制只用于允许重映射的应用。
5. 检查单按、持续按住、双键对角线、组合键先松修饰键、切换窗口、关闭程序。验证另一设备不受单次按键影响。不要把测试映射误认为 PoE2 实际槽位；游戏内是否接受虚拟设备、原生键鼠冲突仍待核实。
 
## 配置
复制 bridge.default.profile 为 bridge.profile；复制 bridge.default.map 为 bridge.map，修改后重启。软件更新不覆盖用户文件。profile 使用 Schema 3；map 每行为 p1/p2、逻辑动作、单个控制器控件。可用名称：lup/ldown/lleft/lright、rup/rdown/rleft/rright、a/b/x/y、lb/rb、lt/rt、dup/ddown/dleft/dright、start/back、lthumb/rthumb。映射缺失、重复配置、共享动作都会拒绝启动。可以独立按键控制肩键/扳机；不自动产生组合输出。
 
## 反馈
复制控制台错误码与两设备 XInput 槽位、描述触发步骤。E300 配置错误；E301 Raw Input 注册失败；E302 窗口焦点监控错误；E303 输出失败；E304 中立恢复失败（尝试关闭设备）。无自动磁盘日志或键鼠历史。正常退出尝试归零和释放设备，异常终止仍需实机验证。此版不宣称已经通过游戏内测试。
