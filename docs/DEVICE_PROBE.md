# Windows 设备验证 v0.1.6
device_probe.exe 是独立的原生 C++ 设备生命周期测试。创建两个 Xbox 360 虚拟设备、查询 XInput 槽位、退出时释放设备。可选 TEST 模式在关闭游戏时逐设备发送固定诊断样本并验证回读，不接受游戏按键。不捕获键鼠，不输出任何攻击、移动或其他游戏动作，不连接游戏。不会自动安装驱动或写日志。请先关闭游戏。
 
## 依赖（仅设备测试需要）
- 已安装的官方 ViGEmBus 驱动：https://github.com/nefarius/ViGEmBus/releases 。该项目已停止维护，非长期绑定方案。仅在你明确选择安装时使用。
- 可信的 x64 ViGEmClient.dll，需自行提供绝对路径。官方 SDK：https://github.com/nefarius/ViGEmClient 。也可从已验证来源的 vgamepad 0.1.3 源码发行包取得 win/vigem/client/x64/ViGEmClient.dll；只提取 DLL，不运行其安装脚本，不需要 Python 常驻。来源：https://github.com/yannbouteiller/vgamepad 。不要从随机 DLL 下载站获取文件。此版本不分发第三方 DLL。
 
## 测试
在 PowerShell 进入发行目录，执行：
```powershell
.\device_probe.exe "D:\你的实际DLL目录\ViGEmClient.dll"
```
输入 YES 确认。观察 connect、add、neutral report 和 XInput 槽位。成功后在 Windows Win+R 中运行 joy.cpl，检查新增的设备。返回命令行按 Enter 跳过回读并释放，或在关闭所有游戏后输入 TEST：程序查询 SDK 分配的真实 XInput 槽位，逐个验证固定轴值、另一设备中立、恢复中立及清理。确认新增设备消失；驱动不会被自动卸载。正常退出或部分创建失败均会尝试清理。
 
## 反馈
复制整个控制台输出（请勿附私人路径），说明物理手柄是否已连接、驱动版本、是否出现两个新增设备、退出后是否消失。E200 参数错误，E201 DLL 加载失败，E202 SDK 符号缺失，E203/E204 分配失败，E205 连接/创建设备失败；E206 槽位分配异常；E207 初始中立回读失败；E208 样本或中立回读失败；E209 清理异常。十六进制 SDK 返回码有助于定位。尚未在 Windows 实机执行，CI 只负责编译，无驱动安装。软件仍不具备 PoE2 游戏内双控。

## 0.1.15 一键准备
主界面按钮启动随包 PrepareDependencies.ps1（Windows PowerShell，遵守执行策略）。明确确认后下载缺失组件：vgamepad 0.1.0 PyPI sdist，SHA256 57f6bd01aec0c172947517fb782d150ef9b285f7f4d524c317374fa5c24a89de，仅提取 x64 DLL，不运行 Python 包。ViGEmBus 1.22.0 官方安装包 SHA256 89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A（Microsoft winget manifest）；要求 Authenticode Valid，用户输入 Y 才启动交互式安装。已存在 SDK 不覆盖，已检测到服务不重装。临时目录仅删除本次创建的目录，已校验的驱动安装包保留于 dependencies。无网络、校验失败、策略限制或安装失败需反馈；不关闭防护、不修改签名设置。手动流程仍可用。
来源：https://pypi.org/project/vgamepad/0.1.0/#files ; https://github.com/microsoft/winget-pkgs/blob/master/manifests/v/ViGEm/ViGEmBus/1.22.0/ViGEm.ViGEmBus.installer.yaml
