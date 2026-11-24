// User-initiated independent device mapping, no automatic sequences or broadcast.
#include "vigem_backend.hpp"
#include <commdlg.h>
#include <fstream>
#include <iterator>
#include <string>
namespace {
using namespace pm;
using namespace pm::win;
Engine engine;
PadMap padmap{};
Session* session=nullptr;
std::array<PadReport,2> sent{};
HWND self=nullptr,target=nullptr,status=nullptr;
DWORD target_pid=0;
HWINEVENTHOOK focus_hook=nullptr;
bool armed=false,countdown=false,raw_registered=false,cleanup_failed=false;
constexpr UINT_PTR monitor_timer=1,select_timer=2;
enum : UINT { self_id=100,external_id,stop_id,attach_id,detach_id };
void message(const wchar_t* s) { if(status) SetWindowTextW(status,s); }
bool send(std::size_t p,PadReport r,bool force=false) {
    if(!session||!session->added[p]) return false;
    if(!force&&r==sent[p]) return true;
    const auto code=session->api.update(session->client,session->pads[p],r);
    if(code!=success) { result("E303 update",code);return false; }
    sent[p]=r;return true;
}
bool raw_registration(bool enable) {
    RAWINPUTDEVICE devices[2]{};
    devices[0]={1,6,enable?RIDEV_INPUTSINK|RIDEV_DEVNOTIFY:RIDEV_REMOVE,enable?self:nullptr};
    devices[1]={1,2,enable?RIDEV_INPUTSINK|RIDEV_DEVNOTIFY:RIDEV_REMOVE,enable?self:nullptr};
    if(!RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))) {
        std::cout<<"E301 Raw Input registration: "<<GetLastError()<<"\n";return false;
    }
    raw_registered=enable;return true;
}
void stop(const wchar_t* reason) {
    armed=false; countdown=false;target=nullptr;target_pid=0;
    if(self) {KillTimer(self,monitor_timer);KillTimer(self,select_timer);}
    engine.reset();
    const bool unregistered=!raw_registered||raw_registration(false);
    bool ok=true;
    for(std::size_t i=0;i<2;++i) if(session&&session->added[i]) {
        const bool neutral=send(i,{},true);ok=ok&&neutral;
    }
    if(!ok) {
        std::cout<<"E304: neutral output failed; closing device session\n";
        if(session) {const bool closed=session->close();cleanup_failed=cleanup_failed||!closed;}
        sent={};
        message(L"E304: Reset failed. Devices closed. Restart to retry.");
    } else if(!unregistered) message(L"E301: Input subscription removal failed. Output is paused.");
    else message(reason);
}
void detach() {
    stop(L"Paused.");
    if(session) {
        const bool ok=session->close();
        cleanup_failed=cleanup_failed||!ok;
        sent={};
        message(ok&&!cleanup_failed?L"Devices detached. Native keyboard/mouse remains available.":L"E209: Cleanup failed; check joy.cpl before continuing.");
    }
}
bool attach() {
    if(!session||cleanup_failed) return false;
    if(session->connected&&!session->closed) return true;
    if(!session->start()||!session->create(0)||!session->create(1)) {
        detach();message(L"E205: Device creation failed. Check console.");return false;
    }
    std::array<ULONG,2> index{};
    if(!assigned_slot(*session,0,index[0])||!assigned_slot(*session,1,index[1])||index[0]==index[1]||
        !matches(index[0],{})||!matches(index[1],{})) {
        detach();message(L"E206/E207: Device readback failed. Check console.");return false;
    }
    sent={};
    std::cout<<"P1 XInput slot="<<index[0]<<" P2 XInput slot="<<index[1]<<"\n";
    message(L"Two devices connected, paused. Arm explicitly to test.");
    return true;
}
bool valid_focus() {
    DWORD pid=0;
    if(!armed||!target||GetForegroundWindow()!=target||!IsWindow(target)) return false;
    GetWindowThreadProcessId(target,&pid);
    return pid==target_pid;
}
void CALLBACK focus_changed(HWINEVENTHOOK,DWORD,HWND w,LONG,LONG,DWORD,DWORD) {
    if(armed&&w!=target) stop(L"Paused: focus left target. Re-arm manually.");
}
bool arm(HWND w) {
    stop(L"Paused.");
    if(!session||!session->connected||session->closed||cleanup_failed||!w||!IsWindow(w)||w==GetDesktopWindow()) return false;
    DWORD pid=0;GetWindowThreadProcessId(w,&pid);
    if(!pid||GetForegroundWindow()!=w||!focus_hook) return false;
    // A held key at arm time must be released before the next session.
    for(int k=1;k<256;++k) if(GetAsyncKeyState(k)&0x8000) {message(L"Release all keys/buttons before arming.");return false;}
    if(!raw_registration(true)) return false;
    target=w;target_pid=pid;armed=true;
    if(!SetTimer(self,monitor_timer,50,nullptr)) {
        stop(L"E302: Focus monitor unavailable.");return false;
    }
    message(w==self?L"Armed: this test window. Switch windows to release.":L"Armed: selected window only. Return here to pause.");
    return true;
}
void input(unsigned k,bool down) {
    if(!valid_focus()) { if(armed) stop(L"Paused: focus changed.");return; }
    if(!engine.input(k,down)) return;
    for(std::size_t p=0;p<2;++p) {
        if(!send(p,make_pad_report(engine.state()[p],padmap[p]))) {
            stop(L"E303: Output failed. Re-arm only after checking device.");break;
        }
    }
}
void raw(LPARAM l) {
    if(!armed) return;
    if(!valid_focus()) {stop(L"Paused: focus changed.");return;}
    RAWINPUT r{};UINT size=sizeof(r);
    const UINT n=GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&r,&size,sizeof(RAWINPUTHEADER));
    if(n==static_cast<UINT>(-1)||n<sizeof(RAWINPUTHEADER)) return;
    if(r.header.dwType==RIM_TYPEKEYBOARD) {
        const auto& k=r.data.keyboard;
        unsigned key=k.VKey;
        if(key==VK_LSHIFT||key==VK_RSHIFT) key=VK_SHIFT;
        if(key==VK_LCONTROL||key==VK_RCONTROL) key=VK_CONTROL;
        if(key==VK_LMENU||key==VK_RMENU) key=VK_MENU;
        if(key!=255) input(key,(k.Flags&RI_KEY_BREAK)==0);
    } else if(r.header.dwType==RIM_TYPEMOUSE) {
        const auto flags=r.data.mouse.usButtonFlags;
        constexpr std::array<std::array<unsigned,3>,5> buttons{{
            {RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_LEFT_BUTTON_UP,VK_LBUTTON},
            {RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_UP,VK_RBUTTON},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_UP,VK_MBUTTON},
            {RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_4_UP,VK_XBUTTON1},
            {RI_MOUSE_BUTTON_5_DOWN,RI_MOUSE_BUTTON_5_UP,VK_XBUTTON2}}};
        for(const auto& b:buttons) {
            if(flags&b[0]) input(b[2],true);
            if(flags&b[1]) input(b[2],false);
        }
    }
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_CREATE: {
        self=w;
        const auto inst=reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(w,GWLP_HINSTANCE));
        CreateWindowW(L"STATIC",L"Independent device bridge | no automatic actions or shared inputs",WS_VISIBLE|WS_CHILD,15,15,625,25,w,nullptr,inst,nullptr);
        CreateWindowW(L"BUTTON",L"Arm this window",WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,15,55,160,35,w,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(self_id)),inst,nullptr);
        CreateWindowW(L"BUTTON",L"Select target (3s)",WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,185,55,180,35,w,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(external_id)),inst,nullptr);
        CreateWindowW(L"BUTTON",L"Stop / release",WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,375,55,160,35,w,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(stop_id)),inst,nullptr);
        CreateWindowW(L"BUTTON",L"Connect devices",WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,15,105,160,35,w,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(attach_id)),inst,nullptr);
        CreateWindowW(L"BUTTON",L"Disconnect devices",WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,185,105,180,35,w,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(detach_id)),inst,nullptr);
        status=CreateWindowW(L"STATIC",L"Devices detached. Connect devices to test.",WS_VISIBLE|WS_CHILD,15,155,625,65,w,nullptr,inst,nullptr);
        CreateWindowW(L"STATIC",L"Default: WASD / arrows, QERTF / Shift+QERTF, mouse buttons.\r\nChange bridge.profile and bridge.map, then restart.\r\nNo mouse aiming or game-specific skill mapping yet.\r\nSwitch to another window to disarm. Keyboard is not suppressed.",WS_VISIBLE|WS_CHILD,15,225,625,95,w,nullptr,inst,nullptr);
        focus_hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,focus_changed,0,0,WINEVENT_OUTOFCONTEXT);
        if(!focus_hook) {std::cout<<"E302 focus hook: "<<GetLastError()<<"\n";return -1;}
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(wp)==stop_id) stop(L"Paused by user.");
        else if(LOWORD(wp)==attach_id) {if(!attach()) std::cout<<"E205: Connect unsuccessful\n";}
        else if(LOWORD(wp)==detach_id) detach();
        else if(LOWORD(wp)==self_id) {if(!arm(w)) message(L"E302: Cannot arm this window.");}
        else if(LOWORD(wp)==external_id) {
            stop(L"Paused.");
            if(!session||!session->connected||session->closed) {message(L"Connect devices first.");return 0;}
            if(MessageBoxW(w,L"Only select an application that permits independent remapping. No keyboard suppression or game-specific mapping. Release all keys, click OK, then select target window within 3 seconds.",L"Select target",MB_OKCANCEL|MB_ICONWARNING)!=IDOK) return 0;
            countdown=SetTimer(w,select_timer,3000,nullptr)!=0;
            message(countdown?L"Select target now (3 seconds).":L"E302: Selection timer failed.");
        }
        return 0;
    case WM_TIMER:
        if(wp==select_timer&&countdown) {
            KillTimer(w,select_timer);countdown=false;
            if(!arm(GetForegroundWindow())) message(L"E302: Could not arm selected window.");
        } else if(wp==monitor_timer&&armed&&!valid_focus()) stop(L"Paused: target lost.");
        return 0;
    case WM_INPUT:
        raw(lp);return DefWindowProcW(w,m,wp,lp);
    case WM_INPUT_DEVICE_CHANGE:
        if(armed && wp==GIDC_REMOVAL) stop(L"Paused: an input device disconnected. Re-arm manually.");
        return 0;
    case WM_POWERBROADCAST:
        if(wp==PBT_APMSUSPEND) stop(L"Paused: suspend.");
        return TRUE;
    case WM_DESTROY:
        detach();
        if(focus_hook) {UnhookWinEvent(focus_hook);focus_hook=nullptr;}
        PostQuitMessage(0);return 0;
    default:return DefWindowProcW(w,m,wp,lp);
    }
}
template<class T,class Reader> bool load(const std::filesystem::path& folder,const wchar_t* user,const wchar_t* defaults,T& out,Reader reader) {
    std::error_code ec;
    const auto path=folder/user;
    const bool exists=std::filesystem::exists(path,ec);
    if(ec) return false;
    std::ifstream in(exists?path:folder/defaults);
    std::string error;
    if(!in||!reader(in,out,error)) {std::cout<<"E300 configuration: "<<error<<"\n";return false;}
    return true;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout<<"PoE2Mouse 0.1.8 independent input bridge\n";
    wchar_t path[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,path,static_cast<DWORD>(std::size(path)));
    if(n==0||n>=std::size(path)) return 1;
    const auto folder=std::filesystem::path(path).parent_path();
    Profile p{};std::string error;
    if(!load(folder,L"bridge.profile",L"bridge.default.profile",p,read_profile)||
       !load(folder,L"bridge.map",L"bridge.default.map",padmap,read_pad_map)||
       !independent_profile(p,padmap,error)) {
        std::cout<<"E300: "<<error<<"\n";return 2;
    }
    engine.set_profile(p);
    wchar_t file[MAX_PATH]{};
    std::filesystem::path dll;
    if(argc==2) dll=argv[1];
    else if(std::filesystem::is_regular_file(folder/L"ViGEmClient.dll")) dll=folder/L"ViGEmClient.dll";
    else {
        OPENFILENAMEW dlg{};dlg.lStructSize=sizeof(dlg);dlg.lpstrFile=file;dlg.nMaxFile=MAX_PATH;
        dlg.lpstrFilter=L"ViGEmClient.dll\0ViGEmClient.dll\0";dlg.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&dlg)) return 0;
        dll=file;
    }
    if(!dll.is_absolute()||_wcsicmp(dll.filename().c_str(),L"ViGEmClient.dll")!=0) {std::cout<<"E200: expected absolute ViGEmClient.dll path\n";return 2;}
    if(MessageBoxW(nullptr,L"Load trusted SDK? Devices remain disconnected until you click Connect devices. Test with games closed first.",L"PoE2Mouse",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return 0;
    Api api;if(!api.load(dll)) return 3;
    Session s(api);
    session=&s;
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=proc;wc.lpszClassName=L"PoE2MouseBridge";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)) return 5;
    HWND w=CreateWindowW(wc.lpszClassName,L"PoE2Mouse 0.1.8 - independent input bridge",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,675,400,nullptr,nullptr,inst,nullptr);
    if(!w) return 5;
    ShowWindow(w,SW_SHOW);MSG msg{};BOOL result_code;
    while((result_code=GetMessageW(&msg,nullptr,0,0))>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    detach();const bool ok=!cleanup_failed;session=nullptr;
    return result_code<0||!ok?6:0;
}
