// User-initiated independent device mapping, no automatic sequences or broadcast.
#include "vigem_backend.hpp"
#include "poe2mouse/physical_keys.hpp"
#include "poe2mouse/diagnostics.hpp"
#include "poe2mouse/bridge_labels.hpp"
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
bool config_ok=false,sdk_ready=false,preview=false;
BridgeOptions options{};
int guide_stage=-1;
HWND preview_label=nullptr,guide_label=nullptr,editor=nullptr;
HWND binding_list=nullptr,key_combo=nullptr,mask_combo=nullptr,role_combo=nullptr,action_combo=nullptr;
unsigned old_key=0,old_mask=0;
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
enum : UINT { self_id=100,external_id,stop_id,attach_id,detach_id,report_id,check_id,help_id,keys_id,driver_id,config_id,guide_id,preview_id,native_id,readback_id };
enum : UINT { edit_list_id=200,edit_role_id,edit_apply_id,edit_new_id,edit_remove_id };
void message(const wchar_t* s) { if(status) SetWindowTextW(status,s); }
bool prepare_sdk(HWND w);
bool load_configuration();
void show_details(HWND owner,const wchar_t* title,const std::wstring& content);
void refresh_preview();
void refresh_bindings();
void next_guide(HWND w);
bool send(std::size_t p,PadReport r,bool force=false) {
    if(!session||!session->added[p]) return false;
    if(!force&&r==sent[p]) return true;
    ++output_updates;
    const auto code=session->api.update(session->client,session->pads[p],r);
    if(code!=success) { ++output_failures;diagnostics.record(Event::bridge_output_failed);result("E303 update",code);return false; }
    sent[p]=r;return true;
}
bool raw_registration(bool enable,bool local=false) {
    RAWINPUTDEVICE devices[2]{};
    const DWORD flags=static_cast<DWORD>(enable?(local?RIDEV_DEVNOTIFY:(RIDEV_INPUTSINK|RIDEV_DEVNOTIFY)):RIDEV_REMOVE);
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
    armed=false;preview=false; countdown=false;target=nullptr;target_pid=0;
    if(self) {KillTimer(self,monitor_timer);KillTimer(self,select_timer);}
    engine.reset();physical_keys.reset();refresh_preview();
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
    if(!distinct_slots(*session,index)) {
        detach();message(L"E206：两个设备槽位无效或重复。请反馈控制台的 P1/P2 编号与 XInput 列表。");return false;
    }
    if(!matches(index[0],{})||!matches(index[1],{})) {
        detach();message(L"E207：设备中立状态回读失败。请反馈控制台 XInput 列表。");return false;
    }
    sent={};assigned=index;diagnostics.record(Event::bridge_attached);
    std::cout<<"P1 XInput slot="<<index[0]<<" P2 XInput slot="<<index[1]<<"\n";
    message(L"两个设备已连接，尚未启用输入。请先测试。");
    return true;
}
bool valid_focus() {
    DWORD pid=0;
    if((!armed&&!preview)||!target||GetForegroundWindow()!=target||!IsWindow(target)) return false;
    GetWindowThreadProcessId(target,&pid);
    return pid==target_pid;
}
void CALLBACK focus_changed(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
    if((armed||preview)&&GetForegroundWindow()!=target) stop(L"已暂停：离开目标窗口。需手动重新启用。");
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
    if(!valid_focus()) { if(armed||preview) stop(L"已暂停：窗口焦点改变。");return; }
    if(k==options.pause_key) {if(down) stop(L"原生键鼠模式：映射已停止。恢复映射需重新启用测试窗口。");return;}
    if(!engine.input(k,down)) return;
    if(preview) {refresh_preview();return;}
    for(std::size_t p=0;p<2;++p) {
        if(!send(p,make_pad_report(engine.state()[p],padmap[p]))) {
            stop(L"E303：输出失败，请先检查设备再启用。");break;
        }
    }
}
void physical(std::uintptr_t device,std::uint32_t token,unsigned key,bool down) {
    if(!armed&&!preview) return;
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
    if(!armed&&!preview) return;
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
    out<<"PoE2Mouse bridge 0.1.12\n"
        <<"preview: "<<preview<<"\nnative_pause_vk: "<<options.pause_key<<"\n"
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
    if(k>=112&&k<=135) return L"F"+std::to_wstring(k-111);
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
        L"第3步：验证键鼠\r\n点击【中文键位 / 改键】确认当前配置，选中一行可直接修改并保存。无需驱动时用离线预览。Windows Win+R 输入 joy.cpl，打开控制器属性。"
        L"回到本软件点击【选择测试窗口】，确认后在三秒内切到属性窗口，松开所有按键。逐项检查两个设备的输入和释放。"
        L"切出目标窗口自动暂停。注意原生键鼠仍会被目标程序收到。\r\n\r\n"
        L"第4步：游戏兼容性\r\n只有设备测试通过后，才考虑游戏原生同屏模式的兼容性验证。当前真实技能映射、输入冲突、原生选敌均未验证。"
        L"不要将诊断配置视为游戏配置。实际游戏使用须符合游戏规则。\r\n\r\n"
        L"第5步：暂停、断开及反馈\r\n战斗映射启用时，默认 F10 可立即暂停虚拟输出。原生键鼠事件一直未被屏蔽。背包、地图、交易前先暂停；游戏是否支持原生键鼠需实测。恢复映射需手动重新启用。\r\n"
        L"【暂停输出】保持设备连接但归零；【断开设备】尝试移除虚拟手柄，软件仍可打开。"
        L"返回游戏单人模式还需游戏侧操作。遇到异常点击【复制诊断】，附版本、实际现象、复现步骤以及控制台错误码。\r\n\r\n"
        L"个人配置：bridge.profile 与 bridge.map；默认文件不会被覆盖。修改后点击【重新读取配置】。"
        L"如配置损坏，先备份个人文件，再移至其他目录，重新读取默认配置。");
}

std::wstring chord_name(unsigned k,unsigned m) {
    std::wstring s;
    if(m&1u) s+=L"Shift+";
    if(m&2u) s+=L"Ctrl+";
    if(m&4u) s+=L"Alt+";
    return s+key_name(k);
}
void refresh_preview() {
    if(!preview_label) return;
    if(!preview) {SetWindowTextW(preview_label,L"离线预览：未启用。无需驱动，使用与实际输入桥相同的配置。");return;}
    std::wostringstream out;
    out<<L"离线预览（不会输出虚拟手柄信号）\r\n";
    for(unsigned p=0;p<2;++p) {
        out<<L"P"<<p+1<<L"：";
        bool any=false;
        for(std::size_t a=0;a<action_count;++a) if(engine.state()[p][a]) {
            if(any) out<<L"、";
            out<<action_zh[a]<<L" ["<<control_zh[static_cast<unsigned>(padmap[p][a])]<<L"]";
            any=true;
        }
        if(!any) out<<L"无按键";
        out<<L"\r\n";
    }
    SetWindowTextW(preview_label,out.str().c_str());
}
bool start_preview(HWND w) {
    stop(L"已暂停。");
    if(!config_ok||GetForegroundWindow()!=w) {message(L"请先修复配置并返回主窗口。");return false;}
    if(!raw_registration(true,true)) {message(L"E301：离线预览输入注册失败。");return false;}
    target=w;GetWindowThreadProcessId(w,&target_pid);preview=true;SetFocus(w);
    if(!SetTimer(w,monitor_timer,50,nullptr)) {stop(L"E302：预览计时器失败。");return false;}
    refresh_preview();message(L"请试按 W、方向键、Q、Shift+Q，并观察 P1/P2；松开后应归零。");
    return true;
}
void combo_add(HWND h,const std::wstring& label,LPARAM data) {
    const LRESULT i=SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    if(i>=0) SendMessageW(h,CB_SETITEMDATA,static_cast<WPARAM>(i),data);
}
LRESULT combo_data(HWND h) {
    const LRESULT i=SendMessageW(h,CB_GETCURSEL,0,0);
    return i<0?-1:SendMessageW(h,CB_GETITEMDATA,static_cast<WPARAM>(i),0);
}
void combo_select(HWND h,LPARAM value) {
    const LRESULT n=SendMessageW(h,CB_GETCOUNT,0,0);
    for(LRESULT i=0;i<n;++i) if(SendMessageW(h,CB_GETITEMDATA,static_cast<WPARAM>(i),0)==value) {
        SendMessageW(h,CB_SETCURSEL,static_cast<WPARAM>(i),0);return;
    }
    SendMessageW(h,CB_SETCURSEL,static_cast<WPARAM>(-1),0);
}
void action_choices() {
    SendMessageW(action_combo,CB_RESETCONTENT,0,0);
    const auto p=combo_data(role_combo);
    if(p<0||p>1) return;
    for(std::size_t a=0;a<action_count;++a) {
        const auto c=static_cast<unsigned>(padmap[static_cast<std::size_t>(p)][a]);
        if(c>0&&c<control_zh.size()&&!solo(static_cast<Action>(a)))
            combo_add(action_combo,std::wstring(action_zh[a])+L" → "+std::wstring(control_zh[c]),static_cast<LPARAM>(a));
    }
    SendMessageW(action_combo,CB_SETCURSEL,0,0);
}
void refresh_bindings() {
    if(!editor||!binding_list) return;
    SendMessageW(binding_list,LB_RESETCONTENT,0,0);
    old_key=old_mask=0;
    if(!config_ok) return;
    for(unsigned p=0;p<2;++p) for(std::size_t a=0;a<action_count;++a)
        for(unsigned m=0;m<layer_count;++m) for(unsigned k=1;k<key_count;++k) {
            const auto bind=current_profile.layers[m][k];
            if(bind.target!=(p==0?Target::p1:Target::p2)||static_cast<std::size_t>(bind.action)!=a) continue;
            const auto c=static_cast<unsigned>(padmap[p][a]);
            const auto line=L"P"+std::to_wstring(p+1)+L"   |   "+chord_name(k,m)+L"   |   "+
                std::wstring(action_zh[a])+L"   |   "+std::wstring(control_zh[c]);
            const auto i=SendMessageW(binding_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str()));
            if(i>=0) SendMessageW(binding_list,LB_SETITEMDATA,static_cast<WPARAM>(i),static_cast<LPARAM>(m*256u+k));
        }
}
void selected_binding() {
    const auto i=SendMessageW(binding_list,LB_GETCURSEL,0,0);
    if(i<0) return;
    const auto n=SendMessageW(binding_list,LB_GETITEMDATA,static_cast<WPARAM>(i),0);
    if(n<0) return;
    old_key=static_cast<unsigned>(n)%256u;old_mask=static_cast<unsigned>(n)/256u;
    if(old_key==0||old_mask>=layer_count) return;
    const auto b=current_profile.layers[old_mask][old_key];
    combo_select(key_combo,old_key);combo_select(mask_combo,old_mask);
    combo_select(role_combo,b.target==Target::p1?0:1);action_choices();
    combo_select(action_combo,static_cast<LPARAM>(b.action));
}
bool save_bindings(const Profile& next) {
    const auto path=folder/L"bridge.profile";
    const auto backup=folder/L"bridge.profile.bak";
    std::error_code ec;
    const bool exists=std::filesystem::exists(path,ec);
    if(ec) return false;
    wchar_t temp[MAX_PATH]{};
    if(!GetTempFileNameW(folder.c_str(),L"pmb",0,temp)) return false;
    const std::filesystem::path tmp(temp);
    bool ok=false;
    {
        std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
        ok=out&&write_profile(out,next);
        out.close();ok=ok&&static_cast<bool>(out);
    }
    if(ok&&exists) {
        std::filesystem::copy_file(path,backup,std::filesystem::copy_options::overwrite_existing,ec);
        ok=!ec;
    }
    if(ok) ok=MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) {std::filesystem::remove(tmp,ec);return false;}
    current_profile=next;engine.set_profile(next);refresh_bindings();return true;
}
LRESULT CALLBACK editor_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_CREATE: {
        const auto inst=reinterpret_cast<const CREATESTRUCTW*>(lp)->hInstance;
        auto child=[&](const wchar_t* cls,const wchar_t* t,DWORD style,int x,int y,int width,int height,UINT id) {
            HWND h=CreateWindowW(cls,t,WS_CHILD|WS_VISIBLE|style,x,y,width,height,w,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),inst,nullptr);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;
        };
        child(L"STATIC",L"角色 | 物理键 | 中文逻辑动作 | 实际手柄控件（诊断映射，游戏技能尚未标定）",0,12,12,730,24,0);
        binding_list=child(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,12,40,725,330,edit_list_id);
        child(L"STATIC",L"物理键",0,12,380,150,20,0);
        child(L"STATIC",L"修饰键",0,175,380,150,20,0);
        child(L"STATIC",L"角色",0,338,380,75,20,0);
        child(L"STATIC",L"逻辑动作 / 手柄控件",0,425,380,300,20,0);
        key_combo=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,12,402,155,250,0);
        mask_combo=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,175,402,155,220,0);
        role_combo=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,338,402,78,100,edit_role_id);
        action_combo=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,425,402,312,260,0);
        child(L"BUTTON",L"保存并立即应用",BS_PUSHBUTTON,12,445,180,34,edit_apply_id);
        child(L"BUTTON",L"新增映射",BS_PUSHBUTTON,202,445,150,34,edit_new_id);
        child(L"BUTTON",L"删除所选映射",BS_PUSHBUTTON,362,445,170,34,edit_remove_id);
        child(L"STATIC",L"选中现有行后改键；新增先点新增映射。不会覆盖冲突键位。鼠标宏仅建议单键映射。",0,12,490,730,38,0);
        for(unsigned k=1;k<255;++k) combo_add(key_combo,key_name(k),k);
        for(unsigned mask=0;mask<layer_count;++mask) {
            std::wstring s;
            if(mask&1u) s+=L"Shift ";
            if(mask&2u) s+=L"Ctrl ";
            if(mask&4u) s+=L"Alt ";
            combo_add(mask_combo,s.empty()?L"无":s,mask);
        }
        combo_add(role_combo,L"P1",0);combo_add(role_combo,L"P2",1);
        combo_select(role_combo,0);combo_select(mask_combo,0);action_choices();return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==edit_list_id&&HIWORD(wp)==LBN_SELCHANGE) selected_binding();
        else if(LOWORD(wp)==edit_role_id&&HIWORD(wp)==CBN_SELCHANGE) action_choices();
        else if(LOWORD(wp)==edit_new_id) {old_key=old_mask=0;SendMessageW(binding_list,LB_SETCURSEL,static_cast<WPARAM>(-1),0);combo_select(key_combo,-1);}
        else if(LOWORD(wp)==edit_apply_id) {
            const auto k=combo_data(key_combo),mask=combo_data(mask_combo),p=combo_data(role_combo),a=combo_data(action_combo);
            if(k<=0||mask<0||p<0||a<0) {MessageBoxW(w,L"请选择物理键、修饰键、角色与动作。",L"改键",MB_OK|MB_ICONWARNING);return 0;}
            Profile next=current_profile;std::string error;
            if(!rebind(next,padmap,options,old_key,old_mask,static_cast<unsigned>(k),static_cast<unsigned>(mask),
                {p==0?Target::p1:Target::p2,static_cast<Action>(a)},error)) {
                MessageBoxW(w,(L"无法保存：键位重复、保留键或配置无效。\r\n"+widen(error)).c_str(),L"改键",MB_OK|MB_ICONWARNING);return 0;
            }
            stop(L"配置已修改，输入保持暂停。");
            if(!save_bindings(next)) MessageBoxW(w,L"E308：保存失败，原配置保持不变。",L"改键",MB_OK|MB_ICONERROR);
            else message(L"配置已保存并立即应用。无需重启；请重新启用预览或设备测试。");
        } else if(LOWORD(wp)==edit_remove_id) {
            if(!old_key) return 0;
            Profile next=current_profile;next.layers[old_mask][old_key]={};std::string error;
            if(!independent_profile(next,padmap,error)) {MessageBoxW(w,L"不能删除最后一项有效绑定。",L"改键",MB_OK|MB_ICONWARNING);return 0;}
            if(MessageBoxW(w,L"删除所选映射？",L"改键",MB_OKCANCEL|MB_ICONQUESTION)==IDOK) {
                stop(L"配置已修改，输入保持暂停。");
                if(!save_bindings(next)) MessageBoxW(w,L"E308：保存失败。",L"改键",MB_OK|MB_ICONERROR);
            }
        }
        return 0;
    case WM_DESTROY:
        editor=nullptr;binding_list=nullptr;key_combo=nullptr;mask_combo=nullptr;role_combo=nullptr;action_combo=nullptr;
        if(self&&IsWindow(self)) {EnableWindow(self,TRUE);SetForegroundWindow(self);}
        return 0;
    default:return DefWindowProcW(w,m,wp,lp);
    }
}
void show_keys(HWND w) {
    stop(L"已暂停，可查看和修改当前键位。");
    if(!config_ok) {show_details(w,L"配置无效",L"E300：配置无效，尚未启用任何映射。\r\n"+widen(config_error));return;}
    if(editor) {SetForegroundWindow(editor);return;}
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=editor_proc;wc.lpszClassName=L"PoE2MouseBindings";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {message(L"E307：键位窗口创建失败。");return;}
    editor=CreateWindowW(wc.lpszClassName,L"当前键位 / 直接改键",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,770,580,w,nullptr,inst,nullptr);
    if(editor) {EnableWindow(w,FALSE);refresh_bindings();SetForegroundWindow(editor);}
    else message(L"E307：键位窗口打开失败。");
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
bool check_dependencies(HWND w) {
    stop(L"已暂停，正在检查依赖。");
    if(!config_ok) {message(L"E300：配置无效。请先重新读取配置。");return false;}
    if(!prepare_sdk(w)) return false;
    if(session&&session->connected) {message(L"SDK 与驱动已连接，两个设备已创建。");return true;}
    if(!session||cleanup_failed) {message(L"E209：请重启并检查设备清理。");return false;}
    const bool ok=session->start();
    const bool closed=session->close();
    cleanup_failed=cleanup_failed||!closed;
    if(ok&&closed) message(L"依赖检查通过：SDK 已加载、驱动可连接。下一步连接两个设备。");
    else message(L"E205：驱动连接失败。请检查官方驱动是否安装，并查看控制台。");
    return ok&&closed;
}


bool device_self_check(HWND w) {
    stop(L"已暂停，准备设备自检。");
    if(!session||!session->connected||session->closed) {message(L"请先连接两个设备。");return false;}
    if(MessageBoxW(w,L"请确认已关闭全部游戏。自检会逐个设备输出固定测试轴值，并验证另一设备保持中立。是否开始？",L"设备输出自检",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return false;
    const bool ok=readback(*session);
    stop(ok?L"设备自检通过：两个设备输出、隔离和归零通过。":L"E208：设备自检失败，请复制诊断和控制台结果。");
    return ok;
}
void next_guide(HWND w) {
    if(guide_stage<0||guide_stage>=5) {
        guide_stage=0;
        SetWindowTextW(guide_label,L"第1/4步：无需游戏或驱动。下一步开启离线预览。默认键位 W、方向键、Q、Shift+Q。");
        return;
    }
    if(guide_stage==0) {
        if(!start_preview(w)) return;
        guide_stage=1;
        SetWindowTextW(guide_label,L"第1/4步：请观察下方两个角色。松键应恢复无按键；确认后点下一步。");
    } else if(guide_stage==1) {
        stop(L"离线预览已停止。");
        guide_stage=2;
        SetWindowTextW(guide_label,L"第2/4步：检查 SDK 与驱动。公司电脑无安装权限可停在此处，不需要游戏。下一步执行检查。");
    } else if(guide_stage==2) {
        if(!check_dependencies(w)) return;
        guide_stage=3;
        SetWindowTextW(guide_label,L"第3/4步：依赖检查通过。下一步创建两个设备并检查独立槽位。");
    } else if(guide_stage==3) {
        if(!attach()) return;
        guide_stage=4;
        SetWindowTextW(guide_label,L"第4/4步：设备连接通过。下一步运行设备输出自检（请先关闭游戏）。");
    } else if(guide_stage==4) {
        if(!device_self_check(w)) return;
        guide_stage=5;
        SetWindowTextW(guide_label,L"引导完成：设备底层自检通过。游戏兼容性仍待验证；可继续人工测试并反馈。");
    }
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
        label(L"首次使用：点新手引导，按照步骤操作。无需游戏即可预览。",15,43,610,25);
        button(L"新手引导 / 下一步",15,77,guide_id);
        button(L"2. 检查依赖",215,77,check_id);
        button(L"3. 连接两个设备",415,77,attach_id);
        button(L"中文键位 / 改键",15,120,keys_id);
        button(L"离线预览（无驱动）",215,120,preview_id);
        button(L"选择测试窗口（3秒）",415,120,external_id);
        button(L"暂停输出",15,163,stop_id);
        button(L"断开设备",215,163,detach_id);
        button(L"复制诊断",415,163,report_id);
        button(L"依赖下载说明",15,206,driver_id);
        button(L"重新读取配置",215,206,config_id);
        button(L"原生键鼠模式（暂停）",415,206,native_id);
        button(L"完整教程",15,249,help_id);
        button(L"真实设备本窗测试",215,249,self_id);
        button(L"设备输出自检",415,249,readback_id);
        guide_label=label(L"引导：点击新手引导开始；可以随时使用离线预览。",15,294,610,55);
        preview_label=label(L"离线预览：未启用。",15,354,610,90);
        status=label(config_ok?L"状态：尚未连接。请从新手教程开始。":L"E300：配置无效；仍可查看帮助和反馈。",15,451,610,58);
        label(L"暂停/原生键鼠：保留设备并归零；断开：释放设备但不退出软件。\r\n"
              L"切出窗口自动暂停。原生界面快捷键使用前请先暂停映射；游戏能否混合输入待验证。\r\n"
              L"问题反馈：点击复制诊断，再附复现步骤。",15,515,610,80);
        focus_hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,focus_changed,0,0,WINEVENT_OUTOFCONTEXT);
        if(!focus_hook) {std::cout<<"E302 focus hook: "<<GetLastError()<<"\n";return -1;}
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==guide_id) next_guide(w);
        else if(LOWORD(wp)==preview_id) start_preview(w);
        else if(LOWORD(wp)==readback_id) device_self_check(w);
        else if(LOWORD(wp)==native_id) stop(L"原生键鼠模式：虚拟输出已暂停。游戏必须自行支持当前输入模式。");
        else if(LOWORD(wp)==help_id) help(w);
        else if(LOWORD(wp)==keys_id) show_keys(w);
        else if(LOWORD(wp)==check_id) check_dependencies(w);
        else if(LOWORD(wp)==driver_id) {stop(L"已暂停。");open_url(w,L"https://github.com/jkszz231210-ops/poe2mouse/blob/dev/bootstrap/docs/DEVICE_PROBE.md");}
        else if(LOWORD(wp)==config_id) {stop(L"已暂停。");load_configuration();refresh_bindings();message(config_ok?L"配置已读取。请查看当前键位并重新启用。":L"E300：配置无效，请查看当前键位中的错误并修复。");}
        else if(LOWORD(wp)==stop_id) stop(L"已手动暂停。");
        else if(LOWORD(wp)==attach_id) {if(!attach()) std::cout<<"Attach unsuccessful; see detailed error above.\n";}
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
        } else if(wp==monitor_timer&&(armed||preview)&&!valid_focus()) stop(L"已暂停：目标窗口失去焦点。");
        return 0;
    case WM_INPUT:
        raw(lp);return DefWindowProcW(w,m,wp,lp);
    case WM_INPUT_DEVICE_CHANGE:
        if((armed||preview) && wp==GIDC_REMOVAL) stop(L"已暂停：输入设备拔出。需重新启用。");
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
    Profile p{};PadMap next{};BridgeOptions next_options{};
    config_error.clear();
    config_ok=load(folder,L"bridge.profile",L"bridge.default.profile",p,read_profile)&&
              load(folder,L"bridge.map",L"bridge.default.map",next,read_pad_map)&&
              independent_profile(p,next,config_error)&&
              load(folder,L"bridge.options",L"bridge.default.options",next_options,read_bridge_options)&&
              bridge_options_ok(p,next_options,config_error);
    if(config_ok) {current_profile=p;padmap=next;options=next_options;engine.set_profile(p);diagnostics.record(Event::profile_loaded);}
    else {engine.reset();diagnostics.record(Event::profile_invalid);std::cout<<"E300: "<<config_error<<"\n";}
    return config_ok;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout<<"PoE2Mouse 0.1.12 independent input bridge\n";
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
    HWND w=CreateWindowW(wc.lpszClassName,L"PoE2Mouse 0.1.12 - 键鼠设备测试",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,660,680,nullptr,nullptr,inst,nullptr);
    if(!w) return 5;
    ShowWindow(w,SW_SHOW);MSG msg{};BOOL result_code;
    while((result_code=GetMessageW(&msg,nullptr,0,0))>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    detach();const bool ok=!cleanup_failed;session=nullptr;
    return result_code<0||!ok?6:0;
}
