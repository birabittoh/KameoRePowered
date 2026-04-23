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
#include "rex_macros.h"
#include "Log.h"
