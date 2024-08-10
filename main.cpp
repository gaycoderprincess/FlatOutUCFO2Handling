#include <windows.h>
#include <cmath>
#include "toml++/toml.hpp"
#include "nya_commonhooklib.h"
#include "fo2brakephys.h"
#include "fo2tirephys.h"

bool bFO2SmoothSteering = true;
bool bFO2BrakePhysics = true;
bool bFO2TirePhysics = true;
bool bNoSteerSuspensionFactor = true;

void* pDBSteering = nullptr;

// read Steering_PC for all cars
uintptr_t SteeringASM_jmp = 0x45CA76;
void __attribute__((naked)) SteeringASM() {
	__asm__ (
		"mov eax, [%1]\n\t"
		"jmp %0\n\t"
			:
			:  "m" (SteeringASM_jmp), "m" (pDBSteering)
	);
}

uintptr_t GetSteeringASM_jmp = 0x45EC30;
void __attribute__((naked)) GetSteeringASM() {
	__asm__ (
		"mov ecx, esi\n\t"
		"call edx\n\t"
		"mov [%1], eax\n\t"
		"mov eax, [esi]\n\t"
		"mov edx, [eax+8]\n\t"
		"jmp %0\n\t"
			:
			:  "m" (GetSteeringASM_jmp), "m" (pDBSteering)
	);
}

// slidecontrolmultiplier [54]+0x18
// antispinmultiplier [54]+0x1C
// turbospinmultiplier [54]+0x20
// car + 0x1DD4
// car + 0x1DD8
// car + 0x1DDC
// turbospinmultiplier is actually never read

// fo2: v66 = 1.0 - v171 * *(a1 + 0x1CEC);
// fouc: v116 = (1.0 - *(a1 + 0x1DEC) * v116) * *(a1 + 0x1DD8);
// +0x1CEC = AntiSpinReduction
// +0x1DEC = AntiSpinReduction

// fo2: 1.0 - v171 * fAntiSpinReduction;
// fouc: (1.0 - fAntiSpinReduction * v116) * fAntiSpinMultiplier;

// steering in fouc is read around 0047CE51

// CompressionToleranceSpeed: car + 0x1E24 and 0x1E74
// CompressionMaxCorrection: car + 0x1E28 and 0x1E78
// DecompressionSpeed: car + 0x1E2C and 0x1E7C

// CompressionToleranceSpeed read at 42D935
// CompressionMaxCorrection read at 42D95D
// DecompressionSpeed read at 42D974

void WriteSuspensionValues() {
	// CompressionToleranceSpeed
	*(float*)0x849858 = 2;
	*(float*)0x84985C = 2;

	// CompressionMaxCorrection
	*(float*)0x849860 = 0;
	*(float*)0x849864 = 0;

	// DecompressionSpeed
	*(float*)0x849868 = 0;
	*(float*)0x84986C = 0;
}

uintptr_t HardcodedSuspensionASM_jmp = 0x45D2A5;
void __attribute__((naked)) HardcodedSuspensionASM() {
	__asm__ (
		"mov eax, [edi]\n\t"
		"mov edx, [eax+0x90]\n\t"
		"mov ecx, edi\n\t"
		"pushad\n\t"
		"call %1\n\t"
		"popad\n\t"
		"jmp %0\n\t"
			:
			:  "m" (HardcodedSuspensionASM_jmp), "i" (WriteSuspensionValues)
	);
}

float fSensitivity = 0.5;
float fMinAnalogSpeed = 0.1;
float fMaxAnalogSpeed = 2;
float fMinAtDelta = 1;
float fMaxAtDelta = 2;
float fSteeringSpeedRate[4] = { 2, 2, 2, 2 };
float fSteeringLimitSpeed[4] = { 20, 40, 100, 250 };

void __fastcall WriteHardcodedSteeringValues(float* f) {
	fSensitivity = f[28];
	fMaxAnalogSpeed = f[29];
	fMinAnalogSpeed = f[30];
	fMinAtDelta = f[31];
	fMaxAtDelta = f[32];
	fSteeringLimitSpeed[0] = f[44];
	fSteeringLimitSpeed[1] = f[45];
	fSteeringLimitSpeed[2] = f[46];
	fSteeringLimitSpeed[3] = f[47];
	fSteeringSpeedRate[0] = f[48];
	fSteeringSpeedRate[1] = f[49];
	fSteeringSpeedRate[2] = f[50];
	fSteeringSpeedRate[3] = f[51];

	if (bFO2SmoothSteering) {
		// FO2 overrides some digital steering values after reading the DB, so just using the values from Steering_PC alone won't work
		f[33] = 0.99; // CenteringSpeed
		f[35] = 1.5; // MinDigitalSpeed
		f[36] = 3.5; // MaxDigitalSpeed
	}
}

uintptr_t HardcodedSteeringASM_jmp = 0x45CC32;
void __attribute__((naked)) HardcodedSteeringASM() {
	__asm__ (
		"fstp dword ptr [esi+0x98]\n\t"
		"mov eax, [edi]\n\t"
		"pushad\n\t"
		"mov ecx, esi\n\t"
		"call %1\n\t"
		"popad\n\t"
		"jmp %0\n\t"
			:
			:  "m" (HardcodedSteeringASM_jmp), "i" (WriteHardcodedSteeringValues)
	);
}

// todo clean this up, this is awful
// FO2's smooth steering algorithm for controllers
void __fastcall FO2ControllerSteering(float* pCar, uint32_t a2_) {
	auto pCarPtr = (uintptr_t)pCar;
	auto a2 = *(float*)&a2_;

	// [574] - [428]
	// [562] - [414]
	// [561] - [415], guessed, [413] - [414]
	// [563] - [417]
	// 0x294 - 0x294
	// 0x314 - 0x3B8

	auto f415 = pCar[561] - pCar[562];

	auto v16 = pCar[562];
	auto vVelocity_ = *(uintptr_t*)(pCarPtr + 0x294);
	pCar[574] = pCar[562];
	vVelocity_ += 0x290;
	auto vVelocity = (float*)vVelocity_;
	auto v37 = 0.0;
	auto v38 = 0.0;
	auto v39 = 0.0;
	pCar[562] = a2 * 1000.0 / *(int32_t*)(pCarPtr + 0x314) * f415 + pCar[562];
	auto v15 = v16 * v16 * v16 * (1.0 - fSensitivity) + (1.0 - (1.0 - fSensitivity)) * v16 - pCar[563];
	auto fMaxSpeedFactor = fMaxAnalogSpeed * a2;
	auto fCarSpeed = std::sqrt(vVelocity[0] * vVelocity[0] + vVelocity[1] * vVelocity[1] + vVelocity[2] * vVelocity[2]) * 3.6;
	auto v18 = 0.0;
	auto v19 = 0.0;
	auto v23 = 0.0;
	if (fCarSpeed < fSteeringLimitSpeed[3]) {
		if (fCarSpeed < fSteeringLimitSpeed[2]) {
			if (fCarSpeed < fSteeringLimitSpeed[1]) {
				if (fCarSpeed < fSteeringLimitSpeed[0]) {
					v18 = fCarSpeed / fSteeringLimitSpeed[0];
					v39 = 1.0 - v18;
				} else {
					v37 = (fCarSpeed - fSteeringLimitSpeed[0]) / (fSteeringLimitSpeed[1] - fSteeringLimitSpeed[0]);
					v18 = 1.0 - v37;
				}
				v23 = 0.0;
			} else {
				v23 = (fCarSpeed - fSteeringLimitSpeed[1]) / (fSteeringLimitSpeed[2] - fSteeringLimitSpeed[1]);
				v37 = 1.0 - v23;
			}
		} else {
			v38 = (fCarSpeed - fSteeringLimitSpeed[2]) / (fSteeringLimitSpeed[3] - fSteeringLimitSpeed[2]);
			v23 = 1.0 - v38;
		}
		v19 = fSteeringSpeedRate[1] * v37
			  + fSteeringSpeedRate[0] * v18
			  + fSteeringSpeedRate[2] * v23
			  + fSteeringSpeedRate[3] * v38
			  + v39;
	} else {
		v19 = fSteeringSpeedRate[3];
	}
	auto v34 = fMaxSpeedFactor * v19;
	auto v27 = fMinAnalogSpeed * a2 * v19;
	auto v28 = (std::abs(v15) - fMinAtDelta * a2) / (fMaxAtDelta * a2 - fMinAtDelta * a2);
	if (v28 < 0.0) {
		v28 = 0.0;
	}
	else if (v28 > 1.0) {
		v28 = 1.0;
	}
	auto v35 = (v34 - v27) * v28 + v27;
	if (v35 >= fMaxSpeedFactor) {
		v35 = fMaxAnalogSpeed * a2;
	}
	if (v15 > v35) {
		v15 = v35;
	}
	if (v15 < -v35) {
		v15 = -v35;
	}
	pCar[563] = v15 + pCar[563];
}

uintptr_t FO2ControllerSteeringASM_jmp = 0x47D1CC;
void __attribute__((naked)) FO2ControllerSteeringASM() {
	__asm__ (
		"pushad\n\t"
		"mov ecx, esi\n\t"
		"mov edx, [ebp+8]\n\t"
		"call %1\n\t"
		"popad\n\t"
		"jmp %0\n\t"
			:
			:  "m" (FO2ControllerSteeringASM_jmp), "i" (FO2ControllerSteering)
	);
}

uintptr_t FO2AddrToBrakePhysicsAddr(uintptr_t addr) {
	return (addr - 0x4408D0) + (uintptr_t)aBrakePhysicsCode;
}

void FixupFO2BrakePhysicsCode() {
	// note: a1 20, 21, 22 are 21, 22, 23 in fouc

	NyaHookLib::PatchRelative(NyaHookLib::CALL, FO2AddrToBrakePhysicsAddr(0x440D85), 0x443AF0);

	static float flt_67DB74 = 1.0;
	static float flt_67DC2C = 2.0;
	static float flt_67E128 = 0.45;
	static float flt_67DC00 = 0.1;
	static float flt_67DB6C = 0.0;
	static float flt_67DCBC = -0.5;
	static float flt_67DC24 = 4.0;
	static float flt_67DBB4 = 0.01;
	static float flt_67DC30 = 100.0;
	static float flt_67DB78 = 0.5;
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x4408D5), &flt_67DB74);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440D90), &flt_67DB74);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440924 + 2), &flt_67DC2C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x44094D + 2), &flt_67DC2C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440937), &flt_67E128);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440962), &flt_67E128);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x44093D), &flt_67DC00);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440968), &flt_67DC00);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440A16), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440C26), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440C91), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440CDD), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440DE1), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440B3C), &flt_67DCBC);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440D20), &flt_67DCBC);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440ABA), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440B02), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440C9D), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440CE9), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440DAF), &flt_67DBB4);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440DBD), &flt_67DBB4);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440DCC), &flt_67DC30);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440DF9), &flt_67DB78);

	// switch case jmp
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x4409AF), FO2AddrToBrakePhysicsAddr(0x440E5C));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440A6B), FO2AddrToBrakePhysicsAddr(0x440E6C));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440C4C), FO2AddrToBrakePhysicsAddr(0x440E7C));

	// jump tables
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E5C), FO2AddrToBrakePhysicsAddr(0x4409B3));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E60), FO2AddrToBrakePhysicsAddr(0x440A4F));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E64), FO2AddrToBrakePhysicsAddr(0x440E19));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E68), FO2AddrToBrakePhysicsAddr(0x440BC3));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E6C), FO2AddrToBrakePhysicsAddr(0x440A6F));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E70), FO2AddrToBrakePhysicsAddr(0x440A88));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E74), FO2AddrToBrakePhysicsAddr(0x440AD2));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E78), FO2AddrToBrakePhysicsAddr(0x440B2E));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E7C), FO2AddrToBrakePhysicsAddr(0x440C50));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E80), FO2AddrToBrakePhysicsAddr(0x440C69));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E84), FO2AddrToBrakePhysicsAddr(0x440CB9));
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440E88), FO2AddrToBrakePhysicsAddr(0x440D15));

	// some offset
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x44091A), 0x3A8); // 398 -> 3A8
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440920), 0x3A8); // 398 -> 3A8

	// increase some brake-related offset
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440A3E + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440A41 + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440A44 + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440BA8 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440BAF + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440BB4 + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E3F + 2), 0x50);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E48 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E4F + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440D82 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440DAA + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440DB6 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E03 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E0B + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E08 + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToBrakePhysicsAddr(0x440E0E + 2), 0x5C);

	// environment ptrs
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x4409B3 + 2), 0x8465F0);
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440BC3 + 1), 0x8465F0);

	// some environment ptr offset
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x4409BE), 0x290); // 1B0 -> 290
	NyaHookLib::Patch(FO2AddrToBrakePhysicsAddr(0x440BCD), 0x290); // 1B0 -> 290
	// surprisingly +20724 remains identical

	// eax -> edi for parameter a1
	NyaHookLib::Patch<uint16_t>(FO2AddrToBrakePhysicsAddr(0x4408DB), 0xF78B);
}

uintptr_t FO2AddrToTirePhysicsAddr(uintptr_t addr) {
	return (addr - 0x44E0F0) + (uintptr_t)aTirePhysicsCode;
}

void FixupFO2TirePhysicsCode() {
	// this ptr:
	// 0x210 -> 0x220//
	// 0x214 not read
	// 0x218 not read
	// 0x21C -> 0x22C//
	// 0x220 -> 0x230//
	// 0x224 -> 0x234//
	// 0x228 -> 0x238//
	// 0x22C not read
	// 0x230 -> 0x240//
	// 0x234 -> 0x244//
	// 0x238 -> 0x248//
	// 0x23C not read
	// 0x240 -> 0x250//
	// 0x244 -> 0x254//
	// 0x248 not read
	// 0x24C not read
	// 0x250 -> 0x260//
	// 0x254 -> 0x264//
	// 0x258 -> 0x268//
	// 0x25C -> 0x26C//
	// 0x260 not read
	// 0x264 not read
	// 0x268 -> 0x278//
	// 0x26C -> 0x27C//
	// 0x270 -> 0x280//
	// 0x274 -> 0x284//
	// 0x278 -> 0x288//
	// 0x27C -> 0x28C//
	// 0x280 -> 0x290//
	// 0x284 -> 0x294//
	// 0x288 -> 0x298//
	// 0x28C -> 0x29C//
	// 0x290 -> 0x2A0//
	// 0x294 -> 0x2A4//
	// 0x298 -> 0x2A8//
	// 0x29C -> 0x2AC//
	// 0x2A0 -> 0x2B0//
	// 0x2A4 -> 0x2B4//
	// 0x2A8 -> 0x2B8//
	// 0x210->0x1B0 -> 0x220->0x1C0//

	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E0FC), 0x22C); // 21C -> 22C
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E103), 0x220); // 210 -> 220
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E111), 0x254); // 244 -> 254
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E117), 0x1C0); // 1B0 -> 1C0
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E12C), 0x2A8); // 298 -> 2A8
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E922), 0x2A8); // 298 -> 2A8
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E132), 0x2B0); // 2A0 -> 2B0
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E866), 0x2B0); // 2A0 -> 2B0
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E138), 0x2B4); // 2A4 -> 2B4
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E703), 0x2B4); // 2A4 -> 2B4
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44ED75), 0x2B4); // 2A4 -> 2B4
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E37E), 0x26C); // 25C -> 26C
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E5EB), 0x268); // 258 -> 268
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB2A), 0x294); // 284 -> 294
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44ED4F), 0x294); // 284 -> 294
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F05D), 0x294); // 284 -> 294
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB39), 0x298); // 288 -> 298
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44ED62), 0x298); // 288 -> 298
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F063), 0x298); // 288 -> 298
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E6CB), 0x240); // 230 -> 240
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EF9B), 0x240); // 230 -> 240
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F048), 0x240); // 230 -> 240
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E856), 0x2B8); // 2A8 -> 2B8
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F00A), 0x2B8); // 2A8 -> 2B8
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC65), 0x280); // 270 -> 280
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFD4), 0x280); // 270 -> 280
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB9F), 0x284); // 274 -> 284
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFF2), 0x284); // 274 -> 284
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EBA8), 0x288); // 278 -> 288
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFEC), 0x288); // 278 -> 288
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC39), 0x278); // 268 -> 278
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFE0), 0x278); // 268 -> 278
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC47), 0x27C); // 26C -> 27C
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFDA), 0x27C); // 26C -> 27C
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EF5E), 0x290); // 280 -> 290
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFC8), 0x290); // 280 -> 290
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F03A), 0x290); // 280 -> 290
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB67), 0x234); // 224 -> 234
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB99), 0x238); // 228 -> 238
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EFF8), 0x238); // 228 -> 238
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E711), 0x244); // 234 -> 244
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E6FD), 0x248); // 238 -> 248
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA21), 0x250); // 240 -> 250
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E9DC), 0x260); // 250 -> 260
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E85C), 0x29C); // 28C -> 29C
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E724), 0x2A0); // 290 -> 2A0
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E771), 0x2A0); // 290 -> 2A0
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC15), 0x2A0); // 290 -> 2A0

	uintptr_t a220Addresses[] = {
		0x44E11F,
		0x44E6E2,
		0x44E6EB,
		0x44EF6E,
		0x44EF7D,
		0x44EF93,
		0x44F026,
		0x44F040,
	};
	for (auto& addr : a220Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), 0x230); // 220 -> 230
	}

	uintptr_t a254Addresses[] = {
		0x44E144,
		0x44E5E5,
		0x44E677,
		0x44E87D,
		0x44E9EA,
		0x44EB45,
		0x44EB56,
		0x44EBDA,
		0x44EC0F,
		0x44ED49,
		0x44ED5C,
		0x44ED6F,
		0x44EF55,
	};
	for (auto& addr : a254Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), 0x264); // 254 -> 264
	}

	uintptr_t a284Addresses[] = {
		0x44E8BF,
		0x44E8D6,
		0x44E8EC,
		0x44E8F2,
		0x44F001,
	};
	for (auto& addr : a284Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), 0x2A4); // 284 -> 2A4
	}

	uintptr_t a27CAddresses[] = {
		0x44EB85,
		0x44EB8F,
		0x44EBB4,
		0x44EBBA,
		0x44EF77,
		0x44EFE6,
		0x44F01A,
		0x44F020,
	};
	for (auto& addr : a27CAddresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), 0x28C); // 27C -> 28C
	}

	uintptr_t a29CAddresses[] = {
		0x44E74F,
		0x44E75A,
		0x44E767,
		0x44E77D,
		0x44E791,
		0x44E7D7,
		0x44E7EB,
		0x44E7F5,
		0x44E808,
		0x44E814,
		0x44E81A,
		0x44E82D,
		0x44E839,
		0x44E84C,
		0x44E870,
		0x44EAA4,
		0x44EADD,
		0x44EFCE,
	};
	for (auto& addr : a29CAddresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), 0x2AC); // 29C -> 2AC
	}

	static float f001 = 0.001;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E14A), &f001);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E6D1), &f001);
	// other 0.001
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E5F1), &f001);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EAC8), &f001);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EAD7), &f001);
	static float f0001 = 0.0001;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E3F2), &f0001);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E4D9), &f0001);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E950), &f0001);
	static float f03 = 0.3;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E80E), &f03);
	static float f50 = 50.0;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E820), &f50);
	static float fNeg20 = -20.0;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E83F), &fNeg20);
	static float f39 = 39.0;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E8A4), &f39);
	static float f40 = 40.0;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E8AA), &f40);
	static float flt_67DCB8 = 1.5707964;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E61B), &flt_67DCB8);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E9A6), &flt_67DCB8);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA98), &flt_67DCB8);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EB76), &flt_67DCB8);
	static float f10000 = 10000.0;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E963), &f10000);
	static float f0852 = 0.852;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E978), &f0852);
	static float f375 = 3.75;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E97E), &f375);
	static float f275 = 2.75;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E994), &f275);
	static float f23 = 2.3;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E9A0), &f23);
	static float f051 = 0.51;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E9AE), &f051);
	static float flt_67DCD8 = 6.2831855;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA1B), &flt_67DCD8);
	static float flt_67DF20 = 0.71399999;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA6A), &flt_67DF20);
	static float f12 = 1.2;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA70), &f12);
	static float f02 = 0.2;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA86), &f02);
	static float f14 = 1.4;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EA92), &f14);
	static float flt_67DF18 = 0.00981;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EBAE), &flt_67DF18);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44F014), &flt_67DF18);
	static float f015 = 0.15;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EBC9), &f015);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC02), &f015);
	static float flt_67DF14 = 0.10193679;
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EBE4), &flt_67DF14);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44EC1F), &flt_67DF14);

	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44ED7B), 0x76414C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E72A), 0x764150);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E777), 0x764150);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44E8D0), 0x764150);

	uintptr_t f1Addresses[] = {
		0x44E427,
		0x44E559,
		0x44E59D,
		0x44E5AC,
		0x44E5B6,
		0x44E67D,
		0x44E732,
		0x44E890,
		0x44E942,
		0x44E94A,
		0x44EA2E,
	};
	static float f1 = 1.0;
	for (auto& addr : f1Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), &f1);
	}

	uintptr_t f0Addresses[] = {
		0x44E5D6,
		0x44E797,
		0x44E7A4,
		0x44E7B3,
		0x44E7C0,
		0x44E7FB,
		0x44E935,
		0x44E9B4,
		0x44E9C3,
		0x44E9CD,
		0x44EB06,
		0x44EB1B,
		0x44EDA9,
		0x44EF8B,
		0x44F034,
	};
	static float f0 = 0.0;
	for (auto& addr : f0Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), &f0);
	}

	uintptr_t fNeg03Addresses[] = {
		0x44E7D1,
		0x44E7E5,
		0x44EBF3,
		0x44EC2B,
	};
	static float fNeg03 = -0.3;
	for (auto& addr : fNeg03Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), &fNeg03);
	}

	uintptr_t f05Addresses[] = {
		0x44EA3A,
		0x44EA44,
		0x44EA4E,
		0x44EA58,
	};
	static float f05 = 0.5;
	for (auto& addr : f05Addresses) {
		NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(addr), &f05);
	}

	// v2 ptr's data is identical
	// negative offsets are identical
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x24CEF7) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using the Steam GFWL version (.exe size of 4242504 bytes)", "nya?!~", MB_ICONERROR);
				exit(0);
				return TRUE;
			}

			auto config = toml::parse_file("FlatOutUCFO2Handling_gcp.toml");
			bFO2SmoothSteering = config["main"]["fo2_smooth_steering"].value_or(true);
			bFO2BrakePhysics = config["main"]["fo2_brake_physics"].value_or(true);
			bFO2TirePhysics = config["main"]["fo2_tire_physics"].value_or(true);
			bNoSteerSuspensionFactor = config["main"]["no_steer_suspension_factor"].value_or(true);

			if (bFO2SmoothSteering) {
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45EC27, &GetSteeringASM);
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45CA68, &SteeringASM);
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x47CF24, &FO2ControllerSteeringASM);

				// get sqrt of car speed for max steer angle
				NyaHookLib::Patch<uint16_t>(0x47D323, 0xFAD9);
				NyaHookLib::Patch(0x47D2F9 + 2, 0x294);
				NyaHookLib::Patch(0x47D2FF + 2, 0x290);
				NyaHookLib::Patch(0x47D313 + 2, 0x298);
			}
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45CC2A, &HardcodedSteeringASM);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45D29B, &HardcodedSuspensionASM);

			// skip new multipliers in the handling code
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x42B7B7, 0x42B7C5);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x42C040, 0x42C046);

			if (bNoSteerSuspensionFactor) {
				// remove some divisions from steering math
				// remove division by fFrontMinLength
				NyaHookLib::Patch<uint16_t>(0x429FBB, 0xD8DD);
				NyaHookLib::Patch<uint16_t>(0x429FCF, 0xD8DD);
				// remove division by fRearMinLength
				NyaHookLib::Patch<uint16_t>(0x42A4A0, 0xD8DD);
				NyaHookLib::Patch<uint16_t>(0x42A4B4, 0xD8DD);
			}

			DWORD oldProt;
			if (bFO2BrakePhysics) {
				FixupFO2BrakePhysicsCode();
				VirtualProtect(aBrakePhysicsCode, sizeof(aBrakePhysicsCode), PAGE_EXECUTE_READWRITE, &oldProt);
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x443D40, aBrakePhysicsCode);
			}
			if (bFO2TirePhysics) {
				FixupFO2TirePhysicsCode();
				VirtualProtect(aTirePhysicsCode, sizeof(aTirePhysicsCode), PAGE_EXECUTE_READWRITE, &oldProt);
				NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x454320, aTirePhysicsCode);
			}

			static const char* steeringPath = "Data.Physics.Car.Steering_PC";
			NyaHookLib::Patch(0x45EC22 + 1, steeringPath);
			static const char* tempSlideControlDB = "SlideControlBalance";
			NyaHookLib::Patch(0x45CD04 + 1, tempSlideControlDB);
			NyaHookLib::Patch(0x45CD2D + 1, tempSlideControlDB);
			NyaHookLib::Patch(0x45CD56 + 1, tempSlideControlDB);
			static const char* tempSuspensionDB = "FrontDefaultCompression";
			NyaHookLib::Patch(0x45D223 + 1, tempSuspensionDB);
			NyaHookLib::Patch(0x45D23E + 1, tempSuspensionDB);
			NyaHookLib::Patch(0x45D267 + 1, tempSuspensionDB);
		} break;
		default:
			break;
	}
	return TRUE;
}