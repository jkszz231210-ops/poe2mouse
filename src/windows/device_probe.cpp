// Explicit opt-in device lifecycle probe, not a game adapter.
#include <windows.h>
#include <Xinput.h>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <string>
namespace {
using Error=std::uint32_t;
constexpr Error success=0x20000000u;
struct Report { WORD buttons{}; BYTE lt{},rt{}; SHORT lx{},ly{},rx{},ry{}; };
static_assert(sizeof(Report)==12);
struct Api {
    HMODULE dll{};
    void* (__cdecl *alloc)(){};
    void (__cdecl *free_client)(void*){};
    Error (__cdecl *connect)(void*){};
    void (__cdecl *disconnect)(void*){};
    void* (__cdecl *alloc_pad)(){};
    void (__cdecl *free_pad)(void*){};
    Error (__cdecl *add)(void*,void*){};
    Error (__cdecl *remove)(void*,void*){};
    Error (__cdecl *update)(void*,void*,Report){};
    Error (__cdecl *user_index)(void*,void*,ULONG*){};
    template<class T> bool bind(T& f,const char* name) {
        f=std::bit_cast<T>(GetProcAddress(dll,name));
        if(!f) std::cout<<"E202 missing symbol: "<<name<<"\n";
        return f!=nullptr;
    }
    bool load(const std::filesystem::path& path) {
        dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dll) { std::cout<<"E201 DLL load failed: "<<GetLastError()<<"\n"; return false; }
        return bind(alloc,"vigem_alloc")&&bind(free_client,"vigem_free")&&
            bind(connect,"vigem_connect")&&bind(disconnect,"vigem_disconnect")&&
            bind(alloc_pad,"vigem_target_x360_alloc")&&bind(free_pad,"vigem_target_free")&&
            bind(add,"vigem_target_add")&&bind(remove,"vigem_target_remove")&&
            bind(update,"vigem_target_x360_update")&&bind(user_index,"vigem_target_x360_get_user_index");
    }
    ~Api(){if(dll) FreeLibrary(dll);}
    Api()=default; Api(const Api&)=delete; Api& operator=(const Api&)=delete;
};
void result(const char* operation,Error code) {
    std::cout<<operation<<": 0x"<<std::hex<<std::uppercase<<code<<std::dec<<"\n";
}
struct Session {
    Api& api; void* client{}; bool connected{};
    bool closed=false;
    std::array<void*,2> pads{}; std::array<bool,2> added{};
    explicit Session(Api& a):api(a){}
    bool start() {
        client=api.alloc();
        if(!client) {std::cout<<"E203 allocation failed\n";return false;}
        auto err=api.connect(client); result("connect",err);
        connected=err==success; return connected;
    }
    bool create(std::size_t i) {
        pads[i]=api.alloc_pad();
        if(!pads[i]) {std::cout<<"E204 target allocation failed\n";return false;}
        auto err=api.add(client,pads[i]); result("add",err);
        if(err!=success) return false;
        added[i]=true;
        err=api.update(client,pads[i],Report{}); result("neutral report",err);
        return err==success;
    }
    bool close() {
        if(closed) return true;
        closed=true; bool ok=true;
        for(std::size_t i=pads.size();i>0;--i) {
            const auto k=i-1;
            if(added[k]) {
                auto neutral=api.update(client,pads[k],Report{}); result("cleanup neutral",neutral);
                auto removed=api.remove(client,pads[k]); result("cleanup remove",removed);
                ok=ok && neutral==success && removed==success;
            }
            if(pads[k]) api.free_pad(pads[k]);
        }
        if(connected) api.disconnect(client);
        if(client) api.free_client(client);
        return ok;
    }
    ~Session() { if(!closed) close(); }
    Session(const Session&)=delete; Session& operator=(const Session&)=delete;
};
bool assigned_slot(Session& s,std::size_t pad,ULONG& slot) {
    for(unsigned retry=0;retry<20;++retry) {
        const auto err=s.api.user_index(s.client,s.pads[pad],&slot);
        if(err==success && slot<XUSER_MAX_COUNT) { result("user_index",err); return true; }
        Sleep(50);
    }
    std::cout<<"E206: SDK did not return a valid XInput index\n"; return false;
}
bool matches(DWORD slot,const Report& expected) {
    for(unsigned retry=0;retry<20;++retry) {
        XINPUT_STATE s{};
        if(XInputGetState(slot,&s)==ERROR_SUCCESS) {
            const auto& g=s.Gamepad;
            if(g.wButtons==expected.buttons && g.bLeftTrigger==expected.lt && g.bRightTrigger==expected.rt &&
               g.sThumbLX==expected.lx && g.sThumbLY==expected.ly && g.sThumbRX==expected.rx && g.sThumbRY==expected.ry) return true;
        }
        Sleep(25);
    }
    return false;
}
bool readback(Session& s) {
    std::array<ULONG,2> index{};
    if(!assigned_slot(s,0,index[0])||!assigned_slot(s,1,index[1])||index[0]==index[1]) {
        std::cout<<"E206: slot assignment unavailable or not distinct\n";return false;
    }
    std::cout<<"Assigned slots: "<<index[0]<<" and "<<index[1]<<"\n";
    if(!matches(index[0],{})||!matches(index[1],{})) {
        std::cout<<"E207: initial neutral readback failed\n";return false;
    }
    // Manual diagnostic sample only, with all games closed. One target at a time.
    for(std::size_t i=0;i<2;++i) {
        Report sample{}; sample.lx=12345; sample.ry=-9876; sample.lt=63;
        auto err=s.api.update(s.client,s.pads[i],sample);result("diagnostic sample",err);
        const bool sample_ok=err==success && matches(index[i],sample) && matches(index[1-i],{});
        const auto neutral=s.api.update(s.client,s.pads[i],{});result("restore neutral",neutral);
        const bool neutral_ok=neutral==success && matches(index[i],{});
        std::cout<<"Device "<<i+1<<": sample="<<(sample_ok?"PASS":"FAIL")<<" neutral="<<(neutral_ok?"PASS":"FAIL")<<"\n";
        if(!sample_ok||!neutral_ok) {std::cout<<"E208: readback mismatch or reset failure\n";return false;}
    }
    return true;
}
void slots(const char* stage) {
    std::cout<<stage<<" XInput slots:";
    for(DWORD i=0;i<XUSER_MAX_COUNT;++i) {
        XINPUT_STATE s{}; auto code=XInputGetState(i,&s);
        std::cout<<" "<<i<<"="<<(code==ERROR_SUCCESS?"connected":code==ERROR_DEVICE_NOT_CONNECTED?"absent":"error");
    }
    std::cout<<"\n";
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout<<"PoE2Mouse 0.1.6 | neutral virtual controller probe\n"
        <<"Close games. This test creates two devices. Optional TEST sends diagnostic samples only.\n";
    if(argc!=2) {
        std::cout<<"Usage: device_probe.exe <absolute path to trusted x64 ViGEmClient.dll>\n";return 2;
    }
    const std::filesystem::path dll(argv[1]);
    if(!dll.is_absolute()||_wcsicmp(dll.filename().c_str(),L"ViGEmClient.dll")!=0) {
        std::cout<<"E200: expected absolute ViGEmClient.dll path\n";return 2;
    }
    std::cout<<"Type YES to create temporary virtual devices: ";
    std::string confirm; if(!std::getline(std::cin,confirm)||confirm!="YES") return 0;
    Api api;
    if(!api.load(dll)) return 3;
    slots("before");
    bool ok=false;
    {
        Session s(api);
        if(s.start()&&s.create(0)&&s.create(1)) {
            Sleep(500); slots("after creation");
            std::cout<<"Open joy.cpl to inspect neutral devices. Close all games, type TEST for readback or press Enter to skip: ";
            std::string line; std::getline(std::cin,line);
            ok=line=="TEST"?readback(s):line.empty();
            if(line.empty()) std::cout<<"Readback skipped; not verified.\n";
        } else std::cout<<"E205: driver connection or device creation failed; see hexadecimal results.\n";
        const bool cleanup=s.close();
        if(!cleanup) std::cout<<"E209: device cleanup returned an error\n";
        ok=ok&&cleanup;
    }
    Sleep(500); slots("after cleanup");
    std::cout<<"Probe "<<(ok?"completed; inspect readback and cleanup results":"failed")<<". No automatic log file.\n";
    return ok?0:4;
}
