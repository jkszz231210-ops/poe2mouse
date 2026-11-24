// User-initiated independent device mapping, no automatic sequences or broadcast.
#include "vigem_backend.hpp"
#include "poe2mouse/physical_keys.hpp"
#include "poe2mouse/diagnostics.hpp"
#include <sstream>
#include <algorithm>
#include <commdlg.h>
#include <fstream>
#include <iterator>
#include <string>
#include <shellapi.h>
namespace {
using namespace pm;
using namespace pm::win;
Engine engine;
Profile current_profile{};
std::filesystem::path folder,sdk_path;
Api* sdk=nullptr;
bool config_ok=false,sdk_ready=false;
std::string config_error;
HWND details=nullptr;
PhysicalKeys physical_keys;
Diagnostics diagnostics;
std::array<ULONG,2> assigned{XUSER_MAX_COUNT,XUSER_MAX_COUNT};
std::uint64_t output_updates=0,output_failures=0;
PadMap padmap{};
Session* session=nullptr;
std::array<PadReport,2> sent{};
HWND self=nullptr,target=nullptr,status=nullptr;
DWORD target_pid=0;
HWINEVENTHOOK focus_hook=nullptr;
bool armed=false,countdown=false,raw_registered=false,cleanup_failed=false;
constexpr UINT_PTR monitor_timer=1,select_timer=2;
enum : UINT { self_id=100,external_id,stop_id,attach_id,detach_id,report_id,check_id,help_id,keys_id,driver_id,config_id };
void message(const wchar_t* s) { if(status) SetWindowTextW(status,s); }
bool prepare_sdk(HWND w);
bool load_configuration();
void show_details(HWND owner,const wchar_t* title,const std::wstring& content);
bool send(std::size_t p,PadReport r,bool force=false) {
    if(!session||!session->added[p]) return false;
    if(!force&&r==sent[p]) return true;
    ++output_updates;
    const auto code=session->api.update(session->client,session->pads[p],r);
    if(code!=success) { ++output_failures;diagnostics.record(Event::bridge_output_failed);result("E303 update",code);return false; }
    sent[p]=r;return true;
}
bool raw_registration(bool enable) {
    RAWINPUTDEVICE devices[2]{};
    const DWORD flags=static_cast<DWORD>(enable?(RIDEV_INPUTSINK|RIDEV_DEVNOTIFY):RIDEV_REMOVE);
    devices[0]={1,6,flags,enable?self:nullptr};
    devices[1]={1,2,flags,enable?self:nullptr};
    if(!RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))) {
        diagnostics.record(Event::bridge_raw_failed);
        std::cout<<"E301 Raw Input registration: "<<GetLastError()<<"\n";return false;
    }
    raw_registered=enable;return true;
}
void stop(const wchar_t* reason) {
    if(armed) diagnostics.record(Event::bridge_paused);
    armed=false; countdown=false;target=nullptr;target_pid=0;
    if(self) {KillTimer(self,monitor_timer);KillTimer(self,select_timer);}
    engine.reset();physical_keys.reset();
    const bool unregistered=!raw_registered||raw_registration(false);
    bool ok=true;
    for(std::size_t i=0;i<2;++i) if(session&&session->added[i]) {
        const bool neutral=send(i,{},true);ok=ok&&neutral;
    }
    if(!ok) {
        std::cout<<"E304: neutral output failed; closing device session\n";
        if(session) {const bool closed=session->close();cleanup_failed=cleanup_failed||!closed;}
        sent={};
        message(L"E304：归零失败，已尝试释放设备。请检查诊断报告。");
    } else if(!unregistered) message(L"E301：注销输入订阅失败，已停止输出。");
    else message(reason);
}
void detach() {
    stop(L"已暂停。");
    if(session) {
        const bool ok=session->close();
        cleanup_failed=cleanup_failed||!ok;
        sent={};assigned={XUSER_MAX_COUNT,XUSER_MAX_COUNT};
        diagnostics.record(ok?Event::bridge_detached:Event::bridge_cleanup_failed);
        message(ok&&!cleanup_failed?L"设备已断开；原生键鼠可用。游戏侧单人模式需自行切换。":L"E209：设备清理失败，请检查 Windows 游戏控制器。");
    }
}
bool attach() {
    if(!session||cleanup_failed) {message(L"E209：清理异常，请关闭软件并检查设备。");return false;}
    if(!config_ok) {message(L"E300：配置无效，请先检查配置。");return false;}
    if(!prepare_sdk(self)) return false;
    if(session->connected&&!session->closed) return true;
    if(!session->start()||!session->create(0)||!session->create(1)) {
        detach();message(L"E205：创建设备失败。检查驱动及控制台。");return false;
    }
    std::array<ULONG,2> index{};
    if(!assigned_slot(*session,0,index[0])||!assigned_slot(*session,1,index[1])||index[0]==index[1]||
        !matches(index[0],{})||!matches(index[1],{})) {
        detach();message(L"E206/E207：设备回读失败。请复制诊断并附控制台。");return false;
    }
    sent={};assigned=index;diagnostics.record(Event::bridge_attached);
    std::cout<<"P1 XInput slot="<<index[0]<<" P2 XInput slot="<<index[1]<<"\n";
    message(L"两个设备已连接，尚未启用输入。请先测试。");
    return true;
}
bool valid_focus() {
    DWORD pid=0;
    if(!armed||!target||GetForegroundWindow()!=target||!IsWindow(target)) return false;
    GetWindowThreadProcessId(target,&pid);
    return pid==target_pid;
}
void CALLBACK focus_changed(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
    if(armed&&GetForegroundWindow()!=target) stop(L"已暂停：离开目标窗口。需手动重新启用。");
}
bool arm(HWND w) {
    stop(L"已暂停。");
    if(!config_ok||!session||!session->connected||session->closed||cleanup_failed||!w||!IsWindow(w)||w==GetDesktopWindow()) return false;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);
    if(!pid||GetForegroundWindow()!=w||!focus_hook) return false;
    // A held key at arm time must be released before the next session.
    for(int k=1;k<256;++k) if(GetAsyncKeyState(k)&0x8000) {message(L"请松开全部按键和鼠标按钮后重试。");return false;}
    if(!raw_registration(true)) return false;
    target=w;target_pid=pid;armed=true;
    diagnostics.record(Event::bridge_armed);
    if(!SetTimer(self,monitor_timer,50,nullptr)) {
        stop(L"E302：无法启动焦点监控。");return false;
    }
    message(w==self?L"已启用本窗口测试。切换窗口会自动暂停。":L"已启用指定窗口。返回本软件即暂停。");
    return true;
}
void input(unsigned k,bool down) {
    if(!valid_focus()) { if(armed) stop(L"已暂停：窗口焦点改变。");return; }
    if(!engine.input(k,down)) return;
    for(std::size_t p=0;p<2;++p) {
        if(!send(p,make_pad_report(engine.state()[p],padmap[p]))) {
            stop(L"E303：输出失败，请先检查设备再启用。");break;
        }
    }
}
void physical(std::uintptr_t device,std::uint32_t token,unsigned key,bool down) {
    if(!armed) return;
    const auto change=physical_keys.input(device,token,key,down);
    if(change==PhysicalKeys::Change::overflow) {
        diagnostics.record(Event::bridge_raw_failed);
        stop(L"E305：按键状态容量异常，已暂停。");
        return;
    }
    if(change==PhysicalKeys::Change::pressed||change==PhysicalKeys::Change::released)
        input(key,change==PhysicalKeys::Change::pressed);
}
void raw(LPARAM l) {
    if(!armed) return;
    if(!valid_focus()) {stop(L"已暂停：窗口焦点改变。");return;}
    RAWINPUT r{};UINT size=sizeof(r);
    const UINT n=GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&r,&size,sizeof(RAWINPUTHEADER));
    if(n==static_cast<UINT>(-1)||n<sizeof(RAWINPUTHEADER)) {
        diagnostics.record(Event::bridge_raw_failed);stop(L"E301：读取输入失败，已暂停。");return;
    }
    const auto device=reinterpret_cast<std::uintptr_t>(r.header.hDevice);
    if(r.header.dwType==RIM_TYPEKEYBOARD) {
        const auto& k=r.data.keyboard;
        unsigned key=k.VKey;
        if(key==VK_LSHIFT||key==VK_RSHIFT) key=VK_SHIFT;
        if(key==VK_LCONTROL||key==VK_RCONTROL) key=VK_CONTROL;
        if(key==VK_LMENU||key==VK_RMENU) key=VK_MENU;
        const auto token=(static_cast<std::uint32_t>(k.VKey)<<16)|(static_cast<std::uint32_t>(k.MakeCode)<<3)|static_cast<std::uint32_t>(k.Flags&(RI_KEY_E0|RI_KEY_E1));
        if(key!=255) physical(device,token,key,(k.Flags&RI_KEY_BREAK)==0);
    } else if(r.header.dwType==RIM_TYPEMOUSE) {
        const auto flags=r.data.mouse.usButtonFlags;
        constexpr std::array<std::array<unsigned,3>,5> buttons{{
            {RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_LEFT_BUTTON_UP,VK_LBUTTON},
            {RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_UP,VK_RBUTTON},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_UP,VK_MBUTTON},
            {RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_4_UP,VK_XBUTTON1},
            {RI_MOUSE_BUTTON_5_DOWN,RI_MOUSE_BUTTON_5_UP,VK_XBUTTON2}}};
        for(const auto& b:buttons) {
            const auto token=0x80000000u|b[2];
            if(flags&b[0]) physical(device,token,b[2],true);
            if(flags&b[1]) physical(device,token,b[2],false);
        }
    }
}
void copy_diagnostics(HWND w) {
    std::ostringstream out;
    out<<"PoE2Mouse bridge 0.1.10\n"
        <<"config_ok: "<<config_ok<<"\nsdk_ready: "<<sdk_ready<<"\n"
        <<"armed: "<<armed<<"\nconnected: "<<(session&&session->connected)<<"\n"
        <<"p1_xinput_slot: "<<assigned[0]<<"\np2_xinput_slot: "<<assigned[1]<<"\n"
        <<"slot 4 means detached/unknown\noutput_updates: "<<output_updates<<"\noutput_failures: "<<output_failures<<"\n"
        <<"cleanup_failed: "<<cleanup_failed<<"\nraw_registered: "<<raw_registered<<"\n";
    diagnostics.write(out);
    const auto report=out.str();
    const std::wstring wide(report.begin(),report.end());
    const auto bytes=(wide.size()+1)*sizeof(wchar_t);
    HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(!mem) {message(L"E306：无法分配报告内存。");return;}
    auto* dest=static_cast<wchar_t*>(GlobalLock(mem));
    if(!dest) {GlobalFree(mem);message(L"E306：无法锁定报告内存。");return;}
    std::copy(wide.cbegin(),wide.cend(),dest);dest[wide.size()]=L'\0';GlobalUnlock(mem);
    if(!OpenClipboard(w)) {GlobalFree(mem);message(L"E306：剪贴板不可用。");return;}
    const bool ok=EmptyClipboard()!=0&&SetClipboardData(CF_UNICODETEXT,mem)!=nullptr;
    CloseClipboard();
    if(!ok) GlobalFree(mem);
    message(ok?L"报告已复制，请粘贴到反馈对话或 GitHub Issue。":L"E306：复制失败。");
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(),s.end()); }
std::wstring key_name(unsigned k) {
    if(k==1) return L"鼠标左键";
    if(k==2) return L"鼠标右键";
    if(k==4) return L"鼠标中键";
    if(k==5) return L"鼠标侧键1";
    if(k==6) return L"鼠标侧键2";
    if(k==16) return L"Shift";
    if(k==17) return L"Ctrl";
    if(k==18) return L"Alt";
    if(k>=96&&k<=105) return L"小键盘"+std::to_wstring(k-96);
    if(k>=65&&k<=90) return std::wstring(1,static_cast<wchar_t>(k));
    if(k>=48&&k<=57) return std::wstring(1,static_cast<wchar_t>(k));
    if(k==32) return L"空格";
    if(k==13) return L"Enter";
    if(k==8) return L"Backspace";
    if(k==37) return L"左方向键";
    if(k==38) return L"上方向键";
    if(k==39) return L"右方向键";
    if(k==40) return L"下方向键";
    return L"VK "+std::to_wstring(k);
}
void open_url(HWND w,const wchar_t* url) {
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(w,L"open",url,nullptr,nullptr,SW_SHOWNORMAL))<=32)
        message(L"E307：无法打开链接，请查看随包 QUICK_START.md。");
}
LRESULT CALLBACK details_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    if(m==WM_CREATE) {
        const auto* cs=reinterpret_cast<const CREATESTRUCTW*>(lp);
        HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",static_cast<const wchar_t*>(cs->lpCreateParams),
            WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,0,0,0,0,w,nullptr,cs->hInstance,nullptr);
        if(!edit) return -1;
        SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
        SetFocus(edit);
        return 0;
    }
    if(m==WM_SIZE) { HWND edit=GetWindow(w,GW_CHILD); if(edit) MoveWindow(edit,0,0,LOWORD(lp),HIWORD(lp),TRUE);return 0; }
    if(m==WM_DESTROY) {details=nullptr;return 0;}
    return DefWindowProcW(w,m,wp,lp);
}
void show_details(HWND owner,const wchar_t* title,const std::wstring& content) {
    if(details) DestroyWindow(details);
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=details_proc;wc.lpszClassName=L"PoE2MouseDetails";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {message(L"E307：帮助窗口创建失败。");return;}
    details=CreateWindowW(wc.lpszClassName,title,WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,650,540,owner,nullptr,inst,const_cast<wchar_t*>(content.c_str()));
    if(!details) message(L"E307：帮助窗口打开失败。");
}
void help(HWND w) {
    stop(L"已暂停，可查看帮助。");
    show_details(w,L"PoE2Mouse 新手教程",
        L"这是工程测试版，不是已验证可游玩的正式版。\r\n\r\n"
        L"第1步：准备环境\r\n关闭游戏。点击【检查依赖】。需要官方 ViGEmBus 驱动与可信 x64 ViGEmClient.dll；本软件不会安装驱动或自动下载文件。"
        L"可通过【依赖下载说明】打开官方来源说明。驱动已停止维护，仅用于当前原型。\r\n\r\n"
        L"第2步：连接设备\r\n点击【连接两个设备】，确认两个独立 XInput 槽位。设备创建后默认暂停。\r\n\r\n"
        L"第3步：验证键鼠\r\n点击【查看当前键位】确认自己的配置。Windows Win+R 输入 joy.cpl，打开控制器属性。"
        L"回到本软件点击【选择测试窗口】，确认后在三秒内切到属性窗口，松开所有按键。逐项检查两个设备的输入和释放。"
        L"切出目标窗口自动暂停。注意原生键鼠仍会被目标程序收到。\r\n\r\n"
        L"第4步：游戏兼容性\r\n只有设备测试通过后，才考虑游戏原生同屏模式的兼容性验证。当前真实技能映射、输入冲突、原生选敌均未验证。"
        L"不要将诊断配置视为游戏配置。实际游戏使用须符合游戏规则。\r\n\r\n"
        L"第5步：暂停、断开及反馈\r\n【暂停输出】保持设备连接但归零；【断开设备】尝试移除虚拟手柄，软件仍可打开。"
        L"返回游戏单人模式还需游戏侧操作。遇到异常点击【复制诊断】，附版本、实际现象、复现步骤以及控制台错误码。\r\n\r\n"
        L"个人配置：bridge.profile 与 bridge.map；默认文件不会被覆盖。修改后点击【重新读取配置】。"
        L"如配置损坏，先备份个人文件，再移至其他目录，重新读取默认配置。");
}
void show_keys(HWND w) {
    stop(L"已暂停，可查看当前键位。");
    if(!config_ok) {show_details(w,L"配置无效",L"E300：配置无效，尚未启用任何映射。\r\n"+widen(config_error));return;}
    std::wostringstream out;
    out<<L"当前实际载入的键位（诊断映射，不是已验证的游戏技能栏）\r\n"
       <<L"物理键 → 角色 / 逻辑动作 → 虚拟手柄控件\r\n\r\n";
    for(std::size_t m=0;m<layer_count;++m) for(unsigned k=1;k<key_count;++k) {
        const auto b=current_profile.layers[m][k];
        if(b.target==Target::none) continue;
        const auto p=b.target==Target::p1?0u:1u;
        if(m&1u) out<<L"Shift+";
        if(m&2u) out<<L"Ctrl+";
        if(m&4u) out<<L"Alt+";
        const auto a=static_cast<std::size_t>(b.action);
        const auto c=static_cast<unsigned>(padmap[p][a]);
        out<<key_name(k)<<L" → P"<<p+1<<L" / "<<widen(std::string(names[a]))<<L" → "<<widen(std::string(control_names[c]))<<L"\r\n";
    }
    show_details(w,L"当前键位",out.str());
}
bool prepare_sdk(HWND w) {
    if(sdk_ready) return true;
    if(!sdk) return false;
    std::error_code ec;
    if(sdk_path.empty()||!std::filesystem::is_regular_file(sdk_path,ec)) {
        wchar_t file[MAX_PATH]{};
        OPENFILENAMEW dlg{};dlg.lStructSize=sizeof(dlg);dlg.hwndOwner=w;dlg.lpstrFile=file;dlg.nMaxFile=MAX_PATH;
        dlg.lpstrFilter=L"ViGEmClient.dll\0ViGEmClient.dll\0";dlg.lpstrTitle=L"选择可信的 x64 ViGEmClient.dll";
        dlg.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&dlg)) {message(L"尚未选择 SDK。请查看新手教程或依赖说明。");return false;}
        sdk_path=file;
    }
    if(!sdk_path.is_absolute()||_wcsicmp(sdk_path.filename().c_str(),L"ViGEmClient.dll")!=0) {
        sdk_path.clear();message(L"E200：请选择可信的 ViGEmClient.dll 绝对路径。");return false;
    }
    if(MessageBoxW(w,L"仅加载你确认来源可信的 x64 SDK DLL。不会自动安装驱动。是否继续？",L"加载 SDK",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return false;
    if(!sdk->load(sdk_path)) {sdk_path.clear();message(L"E201/E202：DLL 加载失败或缺少接口。请检查 x64 版本及控制台。");return false;}
    sdk_ready=true;return true;
}
void check_dependencies(HWND w) {
    stop(L"已暂停，正在检查依赖。");
    if(!config_ok) {message(L"E300：配置无效。请先重新读取配置。");return;}
    if(!prepare_sdk(w)) return;
    if(session&&session->connected) {message(L"SDK 与驱动已连接，两个设备已创建。");return;}
    if(!session||cleanup_failed) {message(L"E209：请重启并检查设备清理。");return;}
    const bool ok=session->start();
    const bool closed=session->close();
    cleanup_failed=cleanup_failed||!closed;
    if(ok&&closed) message(L"依赖检查通过：SDK 已加载、驱动可连接。下一步连接两个设备。");
    else message(L"E205：驱动连接失败。请检查官方驱动是否安装，并查看控制台。");
}

LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_CREATE: {
        self=w;
        const auto inst=reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(w,GWLP_HINSTANCE));
        auto label=[&](const wchar_t* t,int x,int y,int width,int height) {
            HWND h=CreateWindowW(L"STATIC",t,WS_VISIBLE|WS_CHILD,x,y,width,height,w,nullptr,inst,nullptr);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;
        };
        auto button=[&](const wchar_t* t,int x,int y,UINT id) {
            HWND h=CreateWindowW(L"BUTTON",t,WS_VISIBLE|WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,x,y,190,34,w,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),inst,nullptr);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
        };
        label(L"PoE2Mouse 工程测试版 | 尚未验证游戏内兼容性",15,12,610,24);
        label(L"首次使用：先看教程 → 检查依赖 → 连接设备 → 验证键鼠",15,43,610,25);
        button(L"1. 新手教程",15,77,help_id);
        button(L"2. 检查依赖",215,77,check_id);
        button(L"3. 连接两个设备",415,77,attach_id);
        button(L"查看当前键位",15,120,keys_id);
        button(L"本窗口测试",215,120,self_id);
        button(L"选择测试窗口（3秒）",415,120,external_id);
        button(L"暂停输出",15,163,stop_id);
        button(L"断开设备",215,163,detach_id);
        button(L"复制诊断",415,163,report_id);
        button(L"依赖下载说明",15,206,driver_id);
        button(L"重新读取配置",215,206,config_id);
        status=label(config_ok?L"状态：尚未连接。请从新手教程开始。":L"E300：配置无效；仍可查看帮助和反馈。",15,258,610,65);
        label(L"暂停：保留设备并归零；断开：尝试移除设备但不退出软件。\r\n"
              L"切出目标窗口会自动暂停。原生键鼠不屏蔽；真实游戏技能映射待验证。\r\n"
              L"问题反馈：点击复制诊断，再附复现步骤。",15,335,610,95);
        focus_hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,focus_changed,0,0,WINEVENT_OUTOFCONTEXT);
        if(!focus_hook) {std::cout<<"E302 focus hook: "<<GetLastError()<<"\n";return -1;}
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==help_id) help(w);
        else if(LOWORD(wp)==keys_id) show_keys(w);
        else if(LOWORD(wp)==check_id) check_dependencies(w);
        else if(LOWORD(wp)==driver_id) {stop(L"已暂停。");open_url(w,L"https://github.com/jkszz231210-ops/poe2mouse/blob/dev/bootstrap/docs/DEVICE_PROBE.md");}
        else if(LOWORD(wp)==config_id) {stop(L"已暂停。");load_configuration();message(config_ok?L"配置已读取。请查看当前键位并重新启用。":L"E300：配置无效，请查看当前键位中的错误并修复。");}
        else if(LOWORD(wp)==stop_id) stop(L"已手动暂停。");
        else if(LOWORD(wp)==attach_id) {if(!attach()) std::cout<<"E205: Connect unsuccessful\n";}
        else if(LOWORD(wp)==detach_id) detach();
        else if(LOWORD(wp)==report_id) {stop(L"已暂停以生成报告。");copy_diagnostics(w);}
        else if(LOWORD(wp)==self_id) {if(!arm(w)) std::cout<<"E302: Cannot arm this window; check connection, focus and released keys.\n";}
        else if(LOWORD(wp)==external_id) {
            stop(L"已暂停。");
            if(!session||!session->connected||session->closed) {message(L"请先连接设备。");return 0;}
            if(MessageBoxW(w,L"仅选择允许独立重映射的应用。本版不屏蔽原生键鼠，未验证 PoE2 游戏兼容性。松开所有按键，确认后在三秒内切到测试窗口。",L"选择目标",MB_OKCANCEL|MB_ICONWARNING)!=IDOK) return 0;
            countdown=SetTimer(w,select_timer,3000,nullptr)!=0;
            message(countdown?L"请在三秒内切换目标窗口。":L"E302：选择计时器失败。");
        }
        return 0;
    case WM_TIMER:
        if(wp==select_timer&&countdown) {
            KillTimer(w,select_timer);countdown=false;
            if(!arm(GetForegroundWindow())) std::cout<<"E302: Could not arm selected window; check connection, focus and released keys.\n";
        } else if(wp==monitor_timer&&armed&&!valid_focus()) stop(L"已暂停：目标窗口失去焦点。");
        return 0;
    case WM_INPUT:
        raw(lp);return DefWindowProcW(w,m,wp,lp);
    case WM_INPUT_DEVICE_CHANGE:
        if(armed && wp==GIDC_REMOVAL) stop(L"已暂停：输入设备拔出。需重新启用。");
        return 0;
    case WM_POWERBROADCAST:
        if(wp==PBT_APMSUSPEND) stop(L"已暂停：系统挂起。");
        return TRUE;
    case WM_DESTROY:
        detach();
        if(focus_hook) {UnhookWinEvent(focus_hook);focus_hook=nullptr;}
        PostQuitMessage(0);return 0;
    default:return DefWindowProcW(w,m,wp,lp);
    }
}
template<class T,class Reader> bool load(const std::filesystem::path& dir,const wchar_t* user,const wchar_t* defaults,T& out,Reader reader) {
    std::error_code ec;
    const auto path=dir/user;
    const bool exists=std::filesystem::exists(path,ec);
    if(ec) {config_error="configuration path error";return false;}
    std::ifstream in(exists?path:dir/defaults);
    std::string error;
    if(!in||!reader(in,out,error)) {config_error=error.empty()?"configuration file missing or unreadable":error;std::cout<<"E300 configuration: "<<config_error<<"\n";return false;}
    return true;
}
bool load_configuration() {
    Profile p{};PadMap next{};
    config_error.clear();
    config_ok=load(folder,L"bridge.profile",L"bridge.default.profile",p,read_profile)&&
              load(folder,L"bridge.map",L"bridge.default.map",next,read_pad_map)&&
              independent_profile(p,next,config_error);
    if(config_ok) {current_profile=p;padmap=next;engine.set_profile(p);diagnostics.record(Event::profile_loaded);}
    else {engine.reset();diagnostics.record(Event::profile_invalid);std::cout<<"E300: "<<config_error<<"\n";}
    return config_ok;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout<<"PoE2Mouse 0.1.10 independent input bridge\n";
    wchar_t path[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,path,static_cast<DWORD>(std::size(path)));
    if(n==0||n>=std::size(path)) return 1;
    folder=std::filesystem::path(path).parent_path();
    load_configuration();diagnostics.record(Event::startup_ok);
    if(argc==2) sdk_path=argv[1];
    else sdk_path=folder/L"ViGEmClient.dll";
    Api api;sdk=&api;
    Session s(api);
    session=&s;
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=proc;wc.lpszClassName=L"PoE2MouseBridge";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)) return 5;
    HWND w=CreateWindowW(wc.lpszClassName,L"PoE2Mouse 0.1.10 - 键鼠设备测试",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,660,500,nullptr,nullptr,inst,nullptr);
    if(!w) return 5;
    ShowWindow(w,SW_SHOW);MSG msg{};BOOL result_code;
    while((result_code=GetMessageW(&msg,nullptr,0,0))>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    detach();const bool ok=!cleanup_failed;session=nullptr;
    return result_code<0||!ok?6:0;
}
