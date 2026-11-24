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
#include "vigem_backend.hpp"
namespace {
using namespace pm::win;
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
    std::cout<<"PoE2Mouse 0.1.10 | neutral virtual controller probe\n"
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
