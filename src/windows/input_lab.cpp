#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <chrono>
#include <string>
#include "poe2mouse/arena.hpp"
#include "poe2mouse/diagnostics.hpp"
#include <sstream>
#include <cstring>
namespace {
pm::Engine engine;
pm::Diagnostics diagnostics;
pm::Arena arena;
pm::Profile profile{};
std::filesystem::path profile_path;
using Clock=std::chrono::steady_clock;
Clock::time_point last;
bool ticking=false;
constexpr UINT_PTR timer_id=1;
enum : UINT { import_id=100,export_id,reload_id,reset_id,report_id,copy_id,help_id };
void clock_state(HWND w) {
    const bool want=pm::Arena::moving(engine.state());
    if(want && !ticking) {
        last=Clock::now();
        ticking=SetTimer(w,timer_id,16,nullptr)!=0;
        if(!ticking) diagnostics.record(pm::Event::timer_failed);
    } else if(!want && ticking) {
        KillTimer(w,timer_id); ticking=false;
    }
}
void key(HWND w,unsigned code,bool down) {
    if(engine.input(code,down)) {
        clock_state(w); InvalidateRect(w,nullptr,FALSE);
    }
}
void clear(HWND w) {
    engine.reset(); clock_state(w); InvalidateRect(w,nullptr,FALSE);
}
void text(HDC dc,int x,int y,const wchar_t* t) { TextOutW(dc,x,y,t,lstrlenW(t)); }
void draw(HDC dc,RECT area) {
    FillRect(dc,&area,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
    SetBkMode(dc,TRANSPARENT);
    text(dc,20,15,L"PoE2Mouse 0.1.2 | Offline two-character input lab");
    text(dc,20,39,L"WASD: both  |  Arrows: P2  |  L/R mouse: separate attacks (defaults)");
    RECT field{20,70,480,260}; FrameRect(dc,&field,reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
    RECT wall{225,112,275,218}; FillRect(dc,&wall,reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
    for(unsigned p=0;p<2;++p) {
        const auto pos=arena.positions()[p];
        const int x=20+static_cast<int>(pos.x),y=70+static_cast<int>(pos.y);
        const auto& a=engine.state()[p];
        HBRUSH brush=CreateSolidBrush(p==0?RGB(200,105,20):RGB(90,80,190));
        if(brush) {
            HGDIOBJ old=SelectObject(dc,brush);
            Ellipse(dc,x-9,y-9,x+9,y+9);
            SelectObject(dc,old); DeleteObject(brush);
        }
        if(a[4] || a[5] || a[6]) {
            auto old=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Ellipse(dc,x-19,y-19,x+19,y+19);
            SelectObject(dc,old);
        }
        text(dc,x+10,y-12,p==0?L"P1":L"P2");
        const int col=20+static_cast<int>(p)*240;
        text(dc,col,278,p==0?L"PLAYER 1":L"PLAYER 2");
        for(unsigned i=0;i<pm::action_count;++i) {
            std::wstring label(pm::names[i].begin(),pm::names[i].end());
            label+=a[i]?L"  [ACTIVE]":L"  [idle]";
            SetTextColor(dc,a[i]?RGB(0,110,70):RGB(90,90,90));
            text(dc,col,305+static_cast<int>(i)*19,label.c_str());
        }
        SetTextColor(dc,RGB(0,0,0));
    }
    text(dc,20,449,L"Profile: import/export/reload. Help: usage and diagnostics. Offline lab only.");
}
bool load(const std::filesystem::path& path,pm::Profile& result) {
    std::ifstream in(path); std::string error;
    return in && pm::read_profile(in,result,error);
}
// Only a user-initiated operation writes files. Temp file is on the same volume.
bool save(const std::filesystem::path& path,const pm::Profile& p) {
    std::error_code ec;
    const auto full=std::filesystem::absolute(path,ec);
    if(ec) return false;
    wchar_t temp[MAX_PATH]{};
    if(!GetTempFileNameW(full.parent_path().c_str(),L"p2m",0,temp)) return false;
    bool ok=false;
    {
        std::ofstream out{std::filesystem::path(temp),std::ios::binary|std::ios::trunc};
        ok=out && pm::write_profile(out,p);
        out.close(); ok=ok && static_cast<bool>(out);
    }
    if(ok) ok=MoveFileExW(temp,full.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileW(temp);
    return ok;
}
void notify(HWND w,const wchar_t* msg) { MessageBoxW(w,msg,L"PoE2Mouse",MB_OK|MB_ICONINFORMATION); }
std::string make_report() {
    std::ostringstream out;
    SYSTEM_INFO sys{}; GetNativeSystemInfo(&sys);
    MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory);
    out << "PoE2Mouse diagnostic report\n"
        << "app_version: 0.1.2\nprofile_schema: 1\ngame_adapter: none (offline lab)\n"
        << "logical_processors: " << sys.dwNumberOfProcessors << '\n';
    if(GlobalMemoryStatusEx(&memory))
        out << "physical_memory_mib: " << memory.ullTotalPhys/1048576ULL << '\n';
    std::size_t bindings=0;
    for(const auto& b:profile) if(b.target!=pm::Target::none) ++bindings;
    out << "configured_bindings: " << bindings << '\n';
    diagnostics.write(out);
    out << "\nPlease add: game/client version (if relevant), steps, expected result, actual result, reproducibility.\n"
        << "No raw input, profile keys, paths, account names or chat included.\n";
    return out.str();
}
bool copy_report(HWND w,const std::string& report) {
    // Report is generated from ASCII constants and numeric counters only.
    HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,(report.size()+1)*sizeof(wchar_t));
    if(!h) return false;
    auto* dest=static_cast<wchar_t*>(GlobalLock(h));
    if(!dest) { GlobalFree(h); return false; }
    for(std::size_t i=0;i<report.size();++i) dest[i]=static_cast<wchar_t>(static_cast<unsigned char>(report[i]));
    dest[report.size()]=L'\0'; GlobalUnlock(h);
    if(!OpenClipboard(w)) { GlobalFree(h); return false; }
    bool ok=EmptyClipboard()!=0 && SetClipboardData(CF_UNICODETEXT,h)!=nullptr;
    CloseClipboard();
    if(!ok) GlobalFree(h);
    return ok;
}
bool write_report(const std::string& report) {
    // Only explicit user export writes a single, bounded report beside the executable.
    const auto target=profile_path.parent_path()/L"diagnostics.txt";
    wchar_t temp[MAX_PATH]{};
    if(!GetTempFileNameW(target.parent_path().c_str(),L"p2d",0,temp)) return false;
    bool ok=false;
    {
        std::ofstream out{std::filesystem::path(temp),std::ios::binary|std::ios::trunc};
        out << report; out.close(); ok=static_cast<bool>(out);
    }
    if(ok) ok=MoveFileExW(temp,target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) DeleteFileW(temp);
    return ok;
}
void file_command(HWND w,UINT id) {
    if(id==help_id) {
        clear(w);
        notify(w,L"Offline lab only. WASD: both; arrows: P2; left/right mouse: independent attacks; Q/E: skills; Space: dodge. Edit or import lab.profile. For issues, use Help > Copy diagnostic report, then paste it with reproduction steps into GitHub Issues or your support chat. See HOME_TEST.md and FEEDBACK.md.");
        return;
    }
    if(id==report_id || id==copy_id) {
        const auto report=make_report();
        clear(w);
        const bool ok=id==report_id?write_report(report):copy_report(w,report);
        notify(w,ok?(id==report_id?L"Saved diagnostics.txt beside the executable.":L"Diagnostic report copied. Paste into your support chat or issue."):L"Export failed. Try copying the report or use a writable folder.");
        return;
    }
    if(id!=reset_id && id!=reload_id && id!=import_id && id!=export_id) return;
    clear(w);
    if(id==reset_id) { diagnostics.record(pm::Event::arena_reset); arena.reset(); InvalidateRect(w,nullptr,FALSE); return; }
    if(id==reload_id) {
        pm::Profile next{};
        if(load(profile_path,next)) { diagnostics.record(pm::Event::profile_loaded); profile=next; engine.set_profile(profile); notify(w,L"Profile reloaded."); }
        else { diagnostics.record(pm::Event::profile_invalid); notify(w,L"Invalid or missing profile. Previous configuration retained."); }
        return;
    }
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW dlg{}; dlg.lStructSize=sizeof(dlg); dlg.hwndOwner=w;
    dlg.lpstrFilter=L"PoE2Mouse profile (*.profile)\0*.profile\0All files\0*.*\0";
    dlg.lpstrFile=file; dlg.nMaxFile=MAX_PATH; dlg.lpstrDefExt=L"profile";
    dlg.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(id==import_id?OFN_FILEMUSTEXIST:OFN_OVERWRITEPROMPT);
    const bool selected=(id==import_id?GetOpenFileNameW(&dlg):GetSaveFileNameW(&dlg))!=FALSE;
    if(!selected) return;
    if(id==export_id) { const bool ok=save(file,profile); diagnostics.record(ok?pm::Event::profile_exported:pm::Event::profile_write_failed); notify(w,ok?L"Profile exported.":L"Export failed."); return; }
    pm::Profile next{};
    if(!load(file,next)) { diagnostics.record(pm::Event::profile_invalid); notify(w,L"Invalid profile. Previous configuration retained."); return; }
    if(!save(profile_path,next)) { diagnostics.record(pm::Event::profile_write_failed); notify(w,L"Could not save profile. Previous configuration retained."); return; }
    profile=next; engine.set_profile(profile);
    diagnostics.record(pm::Event::profile_imported);
    notify(w,L"Profile imported, saved and applied.");
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_COMMAND: file_command(w,LOWORD(wp)); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: key(w,static_cast<unsigned>(wp),true); break;
    case WM_KEYUP: case WM_SYSKEYUP: key(w,static_cast<unsigned>(wp),false); break;
    case WM_LBUTTONDOWN: SetCapture(w); key(w,VK_LBUTTON,true); return 0;
    case WM_RBUTTONDOWN: SetCapture(w); key(w,VK_RBUTTON,true); return 0;
    case WM_MBUTTONDOWN: SetCapture(w); key(w,VK_MBUTTON,true); return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
        const unsigned code=m==WM_LBUTTONUP?VK_LBUTTON:m==WM_RBUTTONUP?VK_RBUTTON:VK_MBUTTON;
        key(w,code,false);
        if(!(wp&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2))) ReleaseCapture();
        return 0;
    }
    case WM_XBUTTONDOWN: SetCapture(w); key(w,HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,true); return TRUE;
    case WM_XBUTTONUP: key(w,HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,false);
        if(!(LOWORD(wp)&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2))) ReleaseCapture();
        return TRUE;
    case WM_KILLFOCUS: case WM_CANCELMODE: case WM_ENTERMENULOOP: diagnostics.record(pm::Event::focus_reset); clear(w); if(GetCapture()==w) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED:
        if(lp!=reinterpret_cast<LPARAM>(w))
            for(unsigned code:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2}) key(w,code,false);
        return 0;
    case WM_ACTIVATEAPP: if(!wp) clear(w); return 0;
    case WM_TIMER:
        if(wp==timer_id && ticking) {
            auto now=Clock::now(); arena.step(engine.state(),std::chrono::duration<double>(now-last).count());
            last=now; InvalidateRect(w,nullptr,FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(w,&ps);
        RECT area{}; GetClientRect(w,&area); draw(dc,area); EndPaint(w,&ps); return 0;
    }
    case WM_DESTROY: clear(w); PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    wchar_t path[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,path,static_cast<DWORD>(std::size(path)));
    if(n==0 || n>=std::size(path)) return 1;
    profile_path=std::filesystem::path(path).parent_path()/L"lab.profile";
    if(!load(profile_path,profile)) {
        diagnostics.record(pm::Event::startup_profile_failed);
        copy_report(nullptr,make_report());
        notify(nullptr,L"E101: Cannot load lab.profile. Restore the original profile beside the executable. A report was attempted on the clipboard; see FEEDBACK.md."); return 1;
    }
    diagnostics.record(pm::Event::startup_ok);
    engine.set_profile(profile);
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=proc;
    wc.lpszClassName=L"PoE2MouseLab"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&wc)) return 1;
    HMENU menu=CreateMenu(),sub=CreatePopupMenu(),help=CreatePopupMenu();
    if(!menu || !sub || !help) { if(menu) DestroyMenu(menu); if(sub) DestroyMenu(sub); if(help) DestroyMenu(help); return 1; }
    AppendMenuW(sub,MF_STRING,import_id,L"Import profile...");
    AppendMenuW(sub,MF_STRING,export_id,L"Export profile...");
    AppendMenuW(sub,MF_STRING,reload_id,L"Reload profile");
    AppendMenuW(sub,MF_STRING,reset_id,L"Reset arena");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(sub),L"Profile");
    AppendMenuW(help,MF_STRING,help_id,L"How to use / report a problem");
    AppendMenuW(help,MF_STRING,copy_id,L"Copy diagnostic report");
    AppendMenuW(help,MF_STRING,report_id,L"Save diagnostics.txt");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(help),L"Help");
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"PoE2Mouse Input Lab 0.1.2",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,560,nullptr,menu,instance,nullptr);
    if(!w) { DestroyMenu(menu); return 1; }
    ShowWindow(w,show);
    MSG msg{}; BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return result<0?1:static_cast<int>(msg.wParam);
}
