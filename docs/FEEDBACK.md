# 使用说明与问题反馈 / v0.1.3
当前仅为离线双角色沙盒，不是游戏适配器。无需安装驱动。解压到 D 盘可写目录，双击 input_lab.exe。

## 1. 使用
- WASD：沙盒两角色移动；方向键：P2 独立移动；鼠标左右键：分别攻击；Q/E：独立技能；空格：闪避状态。
- Profile > Import profile / Export profile / Reload profile：导入、导出和重新加载键位。配置使用 lab.profile 文件，修改后可 Reload。先导出原配置作为备份。
- Help > How to use / report a problem：内置说明。
- Help > Copy diagnostic report：将报告复制到剪贴板。
- Help > Save diagnostics.txt：保存一个报告到程序目录，每次覆盖同名报告。默认不写日志，不上传，不记录原始键鼠输入、聊天、用户名和路径。只保留最近 64 条运行事件及分类计数（内存），退出即丢失。进程崩溃时内存事件无法恢复。

## 2. 出问题怎么办
能打开程序时：先保留当前现场，不重启，复制报告，附上简短的操作步骤和预期/实际结果。也可把 diagnostics.txt 发给 ChatGPT 或附在 GitHub Issue。公开提交前检查内容，不附账号、密码、私人截图。

打不开时：确认已完整解压且 lab.default.profile 与 exe 同目录。E101 表示配置读取失败，先备份并移走无效的 lab.profile，程序会回退到 lab.default.profile。程序会询问是否保存诊断报告；选择“是”才写入 diagnostics.txt。若未成功，反馈错误编号、截图、版本、是否能复现即可。E102：导入或重载配置无效；E103：配置写入失败；E104：诊断报告导出失败。异常退出时可附 Windows 可靠性监视器中对应应用的错误代码（先去除隐私信息）。没有崩溃转储或自动上传功能。

## 3. 反馈模板
版本 / 提交号：
环境（Windows 版本、电脑、鼠标，可选）：
问题分类（无法运行 / 卡键 / 配置 / 操作不顺手 / 性能 / 其他）：
复现步骤：
期望结果：
实际结果：
复现频率：
诊断报告（可选）：

不顺手的问题也值得反馈：说清楚哪个动作、原本想怎么按、实际要怎么按，最好附一个真实战斗场景（不用写长篇）。
反馈入口：https://github.com/jkszz231210-ops/poe2mouse/issues/new

## 4. 设备与升级
Help > Check input devices 可查看当前 Windows 枚举的键盘、鼠标、HID 数量和四个 XInput 槽位是否连接。报告不记录设备序列号或按键。控制器只需在检测时接入，不要求实际游玩使用。此功能仅查询设备，不创建虚拟手柄。新版压缩包仅含 lab.default.profile，已有 lab.profile 优先读取，不会被解压覆盖。Schema 1 仍可读取；Schema 2 的默认 Shift/Ctrl 分别临时接管 P1/P2 共享移动，技能和独立键不受影响。旧配置要尝试新键位请先导出备份，再导入默认配置。
