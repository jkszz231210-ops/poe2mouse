#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "poe2mouse/input_core.hpp"

namespace {
pm::Engine engine;
void key(HWND w, unsigned code, bool down) {
    if (engine.input(code,down)) InvalidateRect(w,nullptr,FALSE);
}
void clear(HWND w) {
    engine.reset();
    InvalidateRect(w,nullptr,FALSE);
}
void draw(HDC dc, RECT area) {
    FillRect(dc,&area,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
    SetBkMode(dc,TRANSPARENT);
    const wchar_t* title=L"PoE2Mouse | Local input lab (no game output)";
    TextOutW(dc,20,20,title,lstrlenW(title));
    for (unsigned p=0;p<2;++p) {
        int x=20+static_cast<int>(p)*260;
        const wchar_t* label=p==0?L"PLAYER 1":L"PLAYER 2";
        TextOutW(dc,x,65,label,lstrlenW(label));
        for (unsigned a=0;a<pm::action_count;++a) {
            std::wstring s(pm::names[a].begin(),pm::names[a].end());
            s += engine.state()[p][a] ? L"  [ACTIVE]" : L"  [idle]";
            SetTextColor(dc,engine.state()[p][a]?RGB(0,110,70):RGB(90,90,90));
            TextOutW(dc,x,95+static_cast<int>(a)*24,s.c_str(),static_cast<int>(s.size()));
        }
        SetTextColor(dc,RGB(0,0,0));
    }
    const wchar_t* hint=L"Edit lab.profile beside the executable; restart to load.";
    TextOutW(dc,20,285,hint,lstrlenW(hint));
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
    switch(m) {
    case WM_KEYDOWN: case WM_SYSKEYDOWN: key(w,static_cast<unsigned>(wp),true); break;
    case WM_KEYUP: case WM_SYSKEYUP: key(w,static_cast<unsigned>(wp),false); break;
    case WM_LBUTTONDOWN: SetCapture(w); key(w,VK_LBUTTON,true); return 0;
    case WM_RBUTTONDOWN: SetCapture(w); key(w,VK_RBUTTON,true); return 0;
    case WM_MBUTTONDOWN: SetCapture(w); key(w,VK_MBUTTON,true); return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
        const unsigned code=m==WM_LBUTTONUP?VK_LBUTTON:m==WM_RBUTTONUP?VK_RBUTTON:VK_MBUTTON;
        key(w,code,false);
        if (!(wp & (MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2))) ReleaseCapture();
        return 0;
    }
    case WM_XBUTTONDOWN: SetCapture(w); key(w,HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,true); return TRUE;
    case WM_XBUTTONUP:
        key(w,HIWORD(wp)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,false);
        if (!(LOWORD(wp)&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON|MK_XBUTTON1|MK_XBUTTON2))) ReleaseCapture();
        return TRUE;
    case WM_KILLFOCUS: case WM_CANCELMODE: clear(w); if(GetCapture()==w) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED: if(lp!=reinterpret_cast<LPARAM>(w)) clear(w); return 0;
    case WM_ACTIVATEAPP: if(!wp) clear(w); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(w,&ps);
        RECT area{}; GetClientRect(w,&area); draw(dc,area); EndPaint(w,&ps); return 0;
    }
    case WM_DESTROY: engine.reset(); PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(w,m,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    wchar_t path[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,path,static_cast<DWORD>(std::size(path)));
    if(n==0 || n>=std::size(path)) return 1;
    const auto profile=std::filesystem::path(path).parent_path()/L"lab.profile";
    std::ifstream in(profile);
    pm::Profile p{}; std::string error;
    if(!in || !pm::read_profile(in,p,error)) {
        std::wstring message=L"Cannot load lab.profile. ";
        message.append(error.begin(),error.end());
        MessageBoxW(nullptr,message.c_str(),L"PoE2Mouse",MB_OK|MB_ICONERROR); return 1;
    }
    engine.set_profile(p);
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=proc;
    wc.lpszClassName=L"PoE2MouseLab"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&wc)) return 1;
    HWND w=CreateWindowExW(0,wc.lpszClassName,L"PoE2Mouse Input Lab",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,580,380,nullptr,nullptr,instance,nullptr);
    if(!w) return 1;
    ShowWindow(w,show);
    MSG msg{};
    BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return result<0?1:static_cast<int>(msg.wParam);
}
