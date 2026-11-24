#pragma once
#include <windows.h>
#include <Xinput.h>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include "poe2mouse/pad_output.hpp"
#include "poe2mouse/slot_identity.hpp"
namespace pm::win {
using Error=std::uint32_t;
constexpr Error success=0x20000000u;
using Report=pm::PadReport;
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
        if(dll) {FreeLibrary(dll);dll=nullptr;}
        dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dll) { std::cout<<"E201 DLL load failed: "<<GetLastError()<<"\n"; return false; }
        const bool ok=bind(alloc,"vigem_alloc")&&bind(free_client,"vigem_free")&&
            bind(connect,"vigem_connect")&&bind(disconnect,"vigem_disconnect")&&
            bind(alloc_pad,"vigem_target_x360_alloc")&&bind(free_pad,"vigem_target_free")&&
            bind(add,"vigem_target_add")&&bind(remove,"vigem_target_remove")&&
            bind(update,"vigem_target_x360_update")&&bind(user_index,"vigem_target_x360_get_user_index");
        if(!ok) {FreeLibrary(dll);dll=nullptr;}
        return ok;
    }
    ~Api(){if(dll) FreeLibrary(dll);}
    Api()=default; Api(const Api&)=delete; Api& operator=(const Api&)=delete;
};
inline void result(const char* operation,Error code) {
    std::cout<<operation<<": 0x"<<std::hex<<std::uppercase<<code<<std::dec<<"\n";
}
struct Session {
    Api& api; void* client{}; bool connected{};
    bool closed=false,last_cleanup_ok=true;
    std::array<ULONG,2> slots{XUSER_MAX_COUNT,XUSER_MAX_COUNT};
    std::array<void*,2> pads{}; std::array<bool,2> added{};
    explicit Session(Api& a):api(a){}
    bool start() {
        if(client||connected) return false;
        closed=false;
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
        if(closed) return last_cleanup_ok;
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
        client=nullptr;connected=false;pads={};added={};slots.fill(XUSER_MAX_COUNT);last_cleanup_ok=ok;
        return ok;
    }
    ~Session() { if(!closed) close(); }
    Session(const Session&)=delete; Session& operator=(const Session&)=delete;
};
inline void slot_snapshot() {
    for(DWORD i=0;i<XUSER_MAX_COUNT;++i) {
        XINPUT_STATE s{};const auto code=XInputGetState(i,&s);
        const auto& g=s.Gamepad;
        std::cout<<"XInput slot "<<i<<": code="<<code;
        if(code==ERROR_SUCCESS) std::cout<<" buttons="<<g.wButtons<<" lt="<<static_cast<unsigned>(g.bLeftTrigger)
          <<" rt="<<static_cast<unsigned>(g.bRightTrigger)<<" lx="<<g.sThumbLX<<" ly="<<g.sThumbLY
          <<" rx="<<g.sThumbRX<<" ry="<<g.sThumbRY;
        std::cout<<"\n";
    }
}
inline bool connected_mask(unsigned& mask) {
    mask=0;
    for(DWORD i=0;i<XUSER_MAX_COUNT;++i) {
        XINPUT_STATE state{};const auto code=XInputGetState(i,&state);
        if(code==ERROR_SUCCESS) mask|=1u<<i;
        else if(code!=ERROR_DEVICE_NOT_CONNECTED) return false;
    }
    return true;
}
inline bool create_identified(Session& s,std::size_t p) {
    unsigned before{};
    if(!connected_mask(before)) {std::cout<<"E206: baseline XInput enumeration failed\n";return false;}
    if(!s.create(p)) return false;
    int candidate=-1;unsigned stable=0;
    for(unsigned attempt=0;attempt<50;++attempt) {
        unsigned after{};
        const int next=connected_mask(after)?pm::new_slot(before,after):-1;
        if(next>=0&&next==candidate) ++stable;
        else {candidate=next;stable=next>=0?1u:0u;}
        if(stable>=3) {
            s.slots[p]=static_cast<ULONG>(candidate);
            ULONG sdk_index=XUSER_MAX_COUNT;
            const auto code=s.api.user_index(s.client,s.pads[p],&sdk_index);
            std::cout<<"P"<<p+1<<" observed XInput slot="<<s.slots[p]<<" SDK index="<<sdk_index<<"\n";
            if(code!=success||sdk_index!=s.slots[p]) std::cout<<"W206: SDK index mismatch; independent readback required\n";
            return true;
        }
        Sleep(100);
    }
    std::cout<<"E206: no unique stable new XInput slot\n";slot_snapshot();return false;
}
inline bool distinct_slots(Session& s,std::array<ULONG,2>& index) {
    index=s.slots;
    unsigned mask{};
    return index[0]<XUSER_MAX_COUNT&&index[1]<XUSER_MAX_COUNT&&index[0]!=index[1]&&
        connected_mask(mask)&&(mask&(1u<<index[0]))&&(mask&(1u<<index[1]));
}
inline bool matches(DWORD slot,const Report& expected) {
    for(unsigned retry=0;retry<80;++retry) {
        XINPUT_STATE s{};
        if(XInputGetState(slot,&s)==ERROR_SUCCESS) {
            const auto& g=s.Gamepad;
            if(g.wButtons==expected.buttons && g.bLeftTrigger==expected.lt && g.bRightTrigger==expected.rt &&
               g.sThumbLX==expected.lx && g.sThumbLY==expected.ly && g.sThumbRX==expected.rx && g.sThumbRY==expected.ry) return true;
        }
        Sleep(25);
    }
    std::cout<<"XInput state mismatch at slot "<<slot<<"\n";
    slot_snapshot();return false;
}
inline bool readback(Session& s) {
    std::array<ULONG,2> index{};
    if(!distinct_slots(s,index)) {
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

}
