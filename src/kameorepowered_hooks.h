#pragma once

#include <atomic>

#include <rex/ppc.h>

extern std::atomic<uint32_t> g_kameo_infinite_energy_enabled;
extern std::atomic<uint32_t> g_kameo_infinite_health_enabled;

void KameoUnlockDlc(PPCRegister& r3);
void KameoProcessPendingDlcSwapMid();
void KameoOverrideDlcSelectorMid(PPCRegister& r3, PPCRegister& r4, PPCRegister& r1);
void KameoForceReloadOnSameRecord(PPCCRRegister& cr6);
bool KameoShouldSkipNextDlcModelLoad();
void KameoInfiniteEnergy(PPCRegister& r3);
void KameoInfiniteEnergyCurrent(PPCRegister& r3);
void KameoInfiniteEnergyMax(PPCRegister& r3);
void KameoRefillMeterFloat(PPCRegister& r31, PPCRegister& r27);
void KameoRefillMeterFloatPlus4(PPCRegister& r31, PPCRegister& r27);
void KameoRefillHealth(PPCRegister& r29);
