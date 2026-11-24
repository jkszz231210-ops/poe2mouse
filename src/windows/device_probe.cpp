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
            bind(update,"vigem_target_x360_update");
    }
    ~Api(){if(dll) FreeLibrary(dll);}
    Api()=default; Api(const Api&)=delete; Api& operator=(const Api&)=delete;
};
void result(const char* operation,Error code) {
    std::cout<<operation<<": 0x"<<std::hex<<std::uppercase<<code<<std::dec<<"\n";
}
struct Session {
    Api& api; void* client{}; bool connected{};
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
    ~Session() {
        for(std::size_t i=pads.size();i>0;--i) {
            const auto k=i-1;
            if(added[k]) {
                result("cleanup neutral",api.update(client,pads[k],Report{}));
                result("cleanup remove",api.remove(client,pads[k]));
            }
            if(pads[k]) api.free_pad(pads[k]);
        }
        if(connected) api.disconnect(client);
        if(client) api.free_client(client);
    }
    Session(const Session&)=delete; Session& operator=(const Session&)=delete;
};
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
    std::cout<<"PoE2Mouse 0.1.5 | neutral virtual controller probe\n"
        <<"Close games. This test creates two neutral devices and sends no gameplay input.\n";
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
            std::cout<<"Open joy.cpl to inspect the two neutral devices. Press Enter to remove them.\n";
            std::string line; std::getline(std::cin,line); ok=true;
        } else std::cout<<"E205: driver connection or device creation failed; see hexadecimal results.\n";
    }
    Sleep(500); slots("after cleanup");
    std::cout<<"Probe "<<(ok?"created both devices; inspect cleanup and slot results":"failed")<<". No automatic log file.\n";
    return ok?0:4;
}
