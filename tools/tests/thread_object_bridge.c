/* Actual guest object bridge, real native workers, and DWORD ETHREAD reads. */
#include "kernel/kernel_bridge.c"
#include <assert.h>
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_seh_ebp;
ptrdiff_t g_xbox_mem_offset;
XboxRuntimeLayout g_xbox_runtime_layout;
static unsigned live[128], frees, stack_frees;
uint32_t xbox_HeapAlloc(uint32_t size,uint32_t alignment)
{
    assert(size==0x140 && alignment==16);
    for(unsigned i=0;i<128;++i) if(!live[i]) {
        live[i]=1; uint32_t va=0x4000+i*0x140; memset(XBOX_TO_NATIVE(va),0,0x140); return va;
    }
    return 0;
}
void xbox_HeapFree(uint32_t va)
{
    if(!va) return;
    assert(va>=0x4000 && (va-0x4000)%0x140==0);
    unsigned i=(va-0x4000)/0x140; assert(i<128 && live[i]); live[i]=0; ++frees;
}
void xbox_FreeThreadStack(uint32_t top) { assert(top==0); ++stack_frees; }
VOID __fastcall xbox_ObfReferenceObject(PVOID p) { (void)p; assert(0); }
VOID __fastcall xbox_ObfDereferenceObject(PVOID p) { (void)p; assert(0); }
static HANDLE gate;
static DWORD WINAPI worker(LPVOID mode)
{
    WaitForSingleObject(gate,INFINITE);
    if((uintptr_t)mode==1) {
        g_is_spawned_thread=1; g_esp=0x2000;
        BRIDGE_MEM32(g_esp)=0xBADCAFEu;
        bridge_PsTerminateSystemThread(); assert(0);
    }
    return (uintptr_t)mode==2 ? 259u : 0x12345678;
}
static uint32_t reference(uint32_t token,uint32_t type)
{
    g_esp=0x1000; BRIDGE_MEM32(g_esp)=token;
    BRIDGE_MEM32(g_esp+4)=type; BRIDGE_MEM32(g_esp+8)=0x3000;
    BRIDGE_MEM32(0x3004)=0x89ABCDEF;
    bridge_ObReferenceObjectByHandle();
    assert(BRIDGE_MEM32(0x3004)==0x89ABCDEF);
    return BRIDGE_MEM32(0x3000);
}
int main(void)
{
    static uint8_t memory[0x20000]; g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    for(unsigned mode=0;mode<3;++mode) {
        gate=CreateEventA(NULL,TRUE,FALSE,NULL);
        HANDLE h=CreateThread(NULL,0,worker,(LPVOID)(uintptr_t)mode,0,NULL);
        uint32_t token=bridge_handle_token(h);
        uint32_t view=reference(token,0); assert(!g_eax && view);
        assert(BRIDGE_MEM32(view+4)==0 && BRIDGE_MEM32(view+0x120)==259u);
        g_ecx=view; bridge_ObfReferenceObject(); bridge_ObfDereferenceObject();
        assert(live[(view-0x4000)/0x140]); bridge_ObfDereferenceObject();
        SetEvent(gate); assert(WaitForSingleObject(h,2000)==WAIT_OBJECT_0);
        view=reference(token,0); assert(!g_eax && view);
        assert(BRIDGE_MEM8(view+4)==1);
        assert(BRIDGE_MEM32(view+0x120)==(mode==1?0xBADCAFEu:mode==2?259u:0x12345678u));
        CloseHandle(bridge_take_handle(token)); /* object reference survives handle close */
        assert(BRIDGE_MEM8(view+4)==1);
        g_ecx=view; bridge_ObfDereferenceObject(); CloseHandle(gate);
        assert(!reference(token,0) && g_eax==0xC0000008u);
    }
    assert(stack_frees==1);
    assert(!reference(0x1234,0) && g_eax==0xC0000008u);
    HANDLE event=CreateEventA(NULL,TRUE,FALSE,NULL);
    uint32_t token=bridge_handle_token(event);
    assert(!reference(token,0) && g_eax==0xC0000024u);
    CloseHandle(bridge_take_handle(token));
    assert(!reference(0xFFFFFFFEu,123) && g_eax==0xC0000024u);
    uint32_t view=reference(0xFFFFFFFEu,0); assert(!g_eax && view && !BRIDGE_MEM32(view+4));
    g_ecx=view; bridge_ObfDereferenceObject();
    for(unsigned i=0;i<128;++i) assert(!live[i]);
    assert(frees==7);
    uint32_t views[64];
    for(unsigned i=0;i<64;++i) { views[i]=reference(0xFFFFFFFEu,0); assert(!g_eax && views[i]); }
    assert(!reference(0xFFFFFFFEu,0) && g_eax==0xC000009Au);
    for(unsigned i=0;i<64;++i) { g_ecx=views[i]; bridge_ObfDereferenceObject(); }
    BRIDGE_MEM32(g_esp+8)=0xFFFFFFFFu; bridge_ObReferenceObjectByHandle();
    assert(g_eax==0xC000000Du);
    for(unsigned i=0;i<128;++i) assert(!live[i]);
    puts("PASS: live/exited guest ETHREAD fields, return/ExitThread status, ref lifetime, invalid/type checks, DWORD canary");
}
