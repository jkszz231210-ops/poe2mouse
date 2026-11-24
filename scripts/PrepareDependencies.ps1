# Explicit user-initiated dependency preparation. No policy bypass, pip, or silent install.
[CmdletBinding()]
param([switch]$DownloadOnly)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$driverUrl = 'https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe'
$driverHash = '89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A'
$sdkUrl = 'https://files.pythonhosted.org/packages/8a/54/0eaddc33f84247963af078f364b37153d09fcd6cdc398f243ec3e8842c56/vgamepad-0.1.0.tar.gz'
$sdkHash = '57F6BD01AEC0C172947517FB782D150EF9B285F7F4D524C317374FA5C24A89DE'
function Assert-Hash([string]$File,[string]$Expected) {
    if ((Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash -ne $Expected) {
        throw 'E311: SHA-256 verification failed. File will not be used.'
    }
}
function Fetch-Verified([string]$Url,[string]$Destination,[string]$Hash) {
    Write-Host "Downloading: $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing -TimeoutSec 90
    Assert-Hash $Destination $Hash
    Write-Host 'SHA-256: PASS' -ForegroundColor Green
}
function Assert-X64Dll([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = New-Object IO.BinaryReader($stream)
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) { throw 'E312: invalid DLL header' }
        $stream.Position = 60
        $offset = $reader.ReadUInt32()
        if ($offset -gt ($stream.Length - 24)) { throw 'E312: invalid PE offset' }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'E312: not an x64 PE file' }
    } finally { $stream.Dispose() }
}
$work = $null
$mutex = $null
$locked = $false
try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'E312: Windows x64 required' }
    $mutex = New-Object Threading.Mutex($false,'Local\PoE2MouseDependencySetup')
    $locked = $mutex.WaitOne(0)
    if (-not $locked) { throw 'E310: another dependency setup is running' }
    Write-Host 'PoE2Mouse 依赖准备：请关闭游戏。公司电脑需获得驱动安装许可。'
    Write-Host '只下载固定版本，校验 SHA-256；不会关闭防护或修改执行策略。'
    $work = Join-Path $root ('.dependency-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null
    $dll = Join-Path $root 'ViGEmClient.dll'
    if (Test-Path -LiteralPath $dll) {
        Assert-X64Dll $dll
        Write-Host '已存在 x64 DLL，保留原文件。来源可信性仍由用户确认。' -ForegroundColor Green
    } else {
        $archive = Join-Path $work 'vgamepad-0.1.0.tar.gz'
        Fetch-Verified $sdkUrl $archive $sdkHash
        $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
        if (-not (Test-Path -LiteralPath $tar)) { throw 'E312: Windows tar.exe not available; use manual extraction instructions' }
        $members = @(& $tar -tf $archive)
        if ($LASTEXITCODE -ne 0) { throw 'E312: archive list failed' }
        $match = @($members | Where-Object { $_ -eq 'vgamepad-0.1.0/vgamepad/win/vigem/client/x64/ViGEmClient.dll' })
        if ($match.Count -ne 1) { throw 'E312: expected x64 SDK member not found uniquely' }
        & $tar -xf $archive -C $work $match[0]
        if ($LASTEXITCODE -ne 0) { throw 'E312: DLL extraction failed' }
        $extracted = Join-Path $work ($match[0] -replace '/', '\')
        Assert-X64Dll $extracted
        # Move never overwrites an existing user-provided DLL.
        [IO.File]::Move($extracted,$dll)
        Write-Host 'DLL 已放入主程序目录。未执行 Python 包代码。' -ForegroundColor Green
    }
    $service = Get-Service -Name 'ViGEmBus' -ErrorAction SilentlyContinue
    if ($null -ne $service) {
        Write-Host '检测到 ViGEmBus 服务，跳过驱动下载与安装；是否可用以主程序复检为准。' -ForegroundColor Green
    } else {
        $installer = Join-Path $work 'ViGEmBus_1.22.0_x64_x86_arm64.exe'
        Fetch-Verified $driverUrl $installer $driverHash
        $sig = Get-AuthenticodeSignature -LiteralPath $installer
        if ($sig.Status -ne 'Valid') { throw ('E313: installer signature is not valid: ' + $sig.Status) }
        Write-Host ('签名有效：' + $sig.SignerCertificate.Subject)
        $deps = Join-Path $root 'dependencies'
        if (-not (Test-Path -LiteralPath $deps)) { New-Item -ItemType Directory -Path $deps | Out-Null }
        if ((Get-Item -LiteralPath $deps).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'E313: dependency directory is a reparse point' }
        $saved = Join-Path $deps 'ViGEmBus_1.22.0_x64_x86_arm64.exe'
        if (Test-Path -LiteralPath $saved) { Assert-Hash $saved $driverHash }
        else { [IO.File]::Move($installer,$saved) }
        Write-Host "安装包已下载并校验，位置：$saved" -ForegroundColor Green
        if (-not $DownloadOnly -and (Read-Host '如已获安装许可，输入 Y 启动官方安装程序；其他输入仅保留下载') -eq 'Y') {
            $proc = Start-Process -FilePath $saved -Wait -PassThru
            Write-Host ('安装程序退出码：' + $proc.ExitCode)
            Write-Host '请遵循官方安装界面与 Windows UAC 提示；如提示重启，请先重启。'
        }
    }
    Write-Host '准备流程结束。回到主程序点击【检查依赖】，通过后再连接设备。' -ForegroundColor Green
} catch {
    Write-Host ('依赖准备失败：' + $_.Exception.Message) -ForegroundColor Red
    Write-Host '请保留错误提示，或按 DEVICE_PROBE.md 手动准备；不要绕过公司权限、执行策略或签名校验。'
} finally {
    if ($null -ne $work -and (Test-Path -LiteralPath $work)) {
        try { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction Stop } catch { Write-Host '临时文件清理失败，请反馈。' }
    }
    if ($locked) { $mutex.ReleaseMutex() }
    if ($null -ne $mutex) { $mutex.Dispose() }
}
[void](Read-Host '按 Enter 关闭此窗口')
