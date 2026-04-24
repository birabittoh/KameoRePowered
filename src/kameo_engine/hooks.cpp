// rekameo hand-written hooks
//
// Add PPC hooks and midasm-hook implementations in this TU.
//
// Pattern for a classic function replacement (address in
// config/rekameo_hooked.toml):
//
//     int myGameFunction_ADDR_Hook(ArgT arg) {
//         // custom implementation
//         return 0;
//     }
//     REX_PPC_HOOK(myGameFunction_ADDR);
//
// Pattern for a midasm hook (entry in config/rekameo_midasm.toml):
//
//     extern "C" void my_midasm_hook(PPCContext& ctx, uint8_t* base) {
//         // inspect or mutate ctx / guest memory
//     }

#include "hooks.h"
#include "Log.h"
#include "rekameo_init.h"
#include "rex_macros.h"

// Hook for the integrity/region check at 0x8251F190.
int IntegrityCheck_Hook() {
  REKAMEO_INFO("!!! Bypassing integrity check at 0x8251F190 !!!");
  return 0;
}
REX_PPC_HOOK(IntegrityCheck);

// Mid-assembly hook at the very start of xstart (0x8251F320).
// No extern "C", no params — must match what codegen emits for registers=[].
void xstart_Trace() {
  REKAMEO_INFO("xstart: entry");
}

void xstart_after_82520178() {
  REKAMEO_INFO("xstart: returned from sub_82520178");
}

void xstart_after_8251FFD0() {
  REKAMEO_INFO("xstart: returned from sub_8251FFD0");
}

void xstart_after_82529EF8() {
  REKAMEO_INFO("xstart: returned from sub_82529EF8");
}

void xstart_after_8251FF58() {
  REKAMEO_INFO("xstart: returned from sub_8251FF58");
}

void xstart_after_8251FE78() {
  REKAMEO_INFO("xstart: returned from sub_8251FE78 (pre-IntegrityCheck)");
}

void sub_8230D160_vtable_call(PPCRegister& r7) {
  REKAMEO_INFO("sub_8230D160 vtable bctrl: r7(CTR)=0x{:08x}", r7.u32);
}

void static_init_call_1(PPCRegister& r11) {
  REKAMEO_INFO("static_init bctrl#1: calling 0x{:08x}", r11.u32);
}

void static_init_call_2(PPCRegister& r11) {
  REKAMEO_INFO("static_init bctrl#2 (init table): calling 0x{:08x}", r11.u32);
}

void static_init_call_3(PPCRegister& r11) {
  REKAMEO_INFO("static_init bctrl#3 (pre-init table): calling 0x{:08x}", r11.u32);
}

void xstart_after_IntegrityCheck(PPCRegister& r3) {
  REKAMEO_INFO("xstart: IntegrityCheck returned r3=0x{:x}", r3.u32);
}

void render_825F8A50_buf_ptr(PPCRegister& r3, PPCRegister& r26) {
  REKAMEO_INFO("825F8A50: sub_826074C8 ret=0x{:08x} (input obj=0x{:08x})", r3.u32, r26.u32);
}

void render_825F8A50_entry(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5, PPCRegister& r6, PPCRegister& r7) {
  REKAMEO_INFO("825F8A50 entry: r3=0x{:08x} r4=0x{:08x} r5=0x{:08x} r6=0x{:08x} r7=0x{:08x}", r3.u32, r4.u32, r5.u32, r6.u32, r7.u32);
}

void vbuf_init_826073A0_entry(PPCRegister& r3, PPCRegister& r4, PPCRegister& r5) {
  REKAMEO_INFO("826073A0 (vbuf_init) entry: obj=0x{:08x} src=0x{:08x} count=0x{:08x}", r3.u32, r4.u32, r5.u32);
}
