#include <windows.h>
#include <cmath>
#include "toml++/toml.hpp"
#include "nya_commonhooklib.h"
#include "fo2tirephys.h"

float fSlidingFactor = 3.75;
float fStabilityFactor = 1.2;
float fSlidingHackFactor = 1;
float fStabilityHackFactor = 1;
float fSlidingHackFactor2 = 1;
float fStabilityHackFactor2 = 1;
bool bBrakeHack = false;

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

uintptr_t SuspensionASM_jmp = 0x45D2A5;
void __attribute__((naked)) SuspensionASM() {
	__asm__ (
		"mov eax, [edi]\n\t"
		"mov edx, [eax+0x90]\n\t"
		"mov ecx, edi\n\t"
		"pushad\n\t"
		"call %1\n\t"
		"popad\n\t"
		"jmp %0\n\t"
			:
			:  "m" (SuspensionASM_jmp), "i" (WriteSuspensionValues)
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

	// FO2 overrides some digital steering values after reading the DB, so just using the values from Steering_PC alone won't work
	f[33] = 0.99; // CenteringSpeed
	f[35] = 1.5; // MinDigitalSpeed
	f[36] = 3.5; // MaxDigitalSpeed
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

double __cdecl FO2TirePhysics(float a1, float a2, float a3, float a4, float a5, float extraMult, float extraMagicNumber, float extraMagicNumber2) {
	// FOUC behavior:
	//auto a1 = arg0 * a2;
	//auto v7 = atan(a1) * a5 + (1.0 - a5) * a1;
	//auto v8 = atan(v7) * a3;
	//auto v9 = v8 - 1.570796370506287;
	//return (cos(v9) * a4);

	//v154 = fabs(v149);
	//v155 = v154 * v7[3];
	//CalculateSomeTirePhysicsStuff(v155, 0.852, 2.3, 0.50999999, -2.75);

	//v42 = fabs(v92) * v23[3] * 0.852;
	//v43 = cos(atan2(3.75 * v42 - atan2(v42, 1.0) * 2.75, 1.0) * 2.3 - 1.5707964) * 0.50999999;

	//CalculateSomeTirePhysicsStuff(v47, 0.71399999, 1.4, 1.0, -0.2);
	//cos(atan2(1.2 * (v93 * 0.714) - atan2(v93 * 0.714, 1.0) * 0.2, 1.0) * 1.4 - 1.5707964);

	auto v42 = a1 * a2;
	// technically correct, exact FO2 code but doesn't feel right?
	//return cos(atan2(extraMult * v42 - atan2(v42, 1.0) * -a5, 1.0) * a3 - 1.5707964) * a4;
	// this feels a lot more FO2-y
	return cos(atan2(extraMult * v42 - atan2(extraMagicNumber * v42, 1.0) * -a5, 1.0) * a3 - 1.5707964) * a4 * extraMagicNumber2;

	// FO1:
	// v117 = cos(atan2(v16 / v15[8], 1.0) + atan2(v16 / v15[8], 1.0) - 1.5707964) * v15[7];
}

// +0x1E14 brake torque
// +0x1E18 handbrake torque

// brakes are written to 0xD60 and 0x1110
// handbrake is written to 0x14C0 and 0x1870
// later read in tire func at 004553FC, stored into +0x3C

double __cdecl FO2TirePhysics1(float a1, float a2, float a3, float a4, float a5) {
	return FO2TirePhysics(a1, a2, a3, a4, a5, fSlidingFactor, fSlidingHackFactor, fSlidingHackFactor2);
}

double __cdecl FO2TirePhysics2(float a1, float a2, float a3, float a4, float a5) {
	return FO2TirePhysics(a1, a2, a3, a4, a5, fStabilityFactor, fStabilityHackFactor, fStabilityHackFactor2);
}

uintptr_t FO2AddrToTirePhysicsAddr(uintptr_t addr) {
	return (addr - 0x4408D0) + (uintptr_t)aTirePhysicsCode;
}

void FixupFO2TirePhysicsCode() {
	// note: a1 20, 21, 22 are 21, 22, 23 in fouc

	NyaHookLib::PatchRelative(NyaHookLib::CALL, FO2AddrToTirePhysicsAddr(0x440D85), 0x443AF0);

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
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x4408D5), &flt_67DB74);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440D90), &flt_67DB74);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440924 + 2), &flt_67DC2C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44094D + 2), &flt_67DC2C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440937), &flt_67E128);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440962), &flt_67E128);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44093D), &flt_67DC00);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440968), &flt_67DC00);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440A16), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440C26), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440C91), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440CDD), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440DE1), &flt_67DB6C);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440B3C), &flt_67DCBC);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440D20), &flt_67DCBC);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440ABA), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440B02), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440C9D), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440CE9), &flt_67DC24);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440DAF), &flt_67DBB4);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440DBD), &flt_67DBB4);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440DCC), &flt_67DC30);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440DF9), &flt_67DB78);

	// switch case jmp
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x4409AF), FO2AddrToTirePhysicsAddr(0x440E5C));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440A6B), FO2AddrToTirePhysicsAddr(0x440E6C));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440C4C), FO2AddrToTirePhysicsAddr(0x440E7C));

	// jump tables
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E5C), FO2AddrToTirePhysicsAddr(0x4409B3));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E60), FO2AddrToTirePhysicsAddr(0x440A4F));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E64), FO2AddrToTirePhysicsAddr(0x440E19));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E68), FO2AddrToTirePhysicsAddr(0x440BC3));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E6C), FO2AddrToTirePhysicsAddr(0x440A6F));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E70), FO2AddrToTirePhysicsAddr(0x440A88));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E74), FO2AddrToTirePhysicsAddr(0x440AD2));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E78), FO2AddrToTirePhysicsAddr(0x440B2E));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E7C), FO2AddrToTirePhysicsAddr(0x440C50));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E80), FO2AddrToTirePhysicsAddr(0x440C69));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E84), FO2AddrToTirePhysicsAddr(0x440CB9));
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440E88), FO2AddrToTirePhysicsAddr(0x440D15));

	// some offset
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x44091A), 0x3A8); // 398 -> 3A8
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440920), 0x3A8); // 398 -> 3A8

	// increase some brake-related offset
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440A3E + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440A41 + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440A44 + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440BA8 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440BAF + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440BB4 + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E3F + 2), 0x50);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E48 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E4F + 2), 0x5C);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440D82 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440DAA + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440DB6 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E03 + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E0B + 2), 0x54);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E08 + 2), 0x58);
	NyaHookLib::Patch<uint8_t>(FO2AddrToTirePhysicsAddr(0x440E0E + 2), 0x5C);

	// environment ptrs
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x4409B3 + 2), 0x8465F0);
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440BC3 + 1), 0x8465F0);

	// some environment ptr offset
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x4409BE), 0x290); // 1B0 -> 290
	NyaHookLib::Patch(FO2AddrToTirePhysicsAddr(0x440BCD), 0x290); // 1B0 -> 290
	// surprisingly +20724 remains identical

	// eax -> edi for parameter a1
	NyaHookLib::Patch<uint16_t>(FO2AddrToTirePhysicsAddr(0x4408DB), 0xF78B);
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
			fSlidingFactor = config["main"]["slide_factor"].value_or(3.75);
			fStabilityFactor = config["main"]["stability_factor"].value_or(1.2);
			fSlidingHackFactor = config["main"]["slide_hack_factor"].value_or(1.0);
			fStabilityHackFactor = config["main"]["stability_hack_factor"].value_or(1.0);
			fSlidingHackFactor2 = config["main"]["slide_scalar_hack_factor"].value_or(1.0);
			fStabilityHackFactor2 = config["main"]["stability_scalar_hack_factor"].value_or(1.0);

			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45EC27, &GetSteeringASM);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45CA68, &SteeringASM);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45D29B, &SuspensionASM);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x45CC2A, &HardcodedSteeringASM);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x47CF24, &FO2ControllerSteeringASM);

			// get sqrt of car speed for max steer angle
			NyaHookLib::Patch<uint16_t>(0x47D323, 0xFAD9);
			NyaHookLib::Patch(0x47D2F9 + 2, 0x294);
			NyaHookLib::Patch(0x47D2FF + 2, 0x290);
			NyaHookLib::Patch(0x47D313 + 2, 0x298);

			// skip new multipliers in the handling code
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x42B7B7, 0x42B7C5);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x42C040, 0x42C046);

			// remove some divisions from steering math
			// remove division by fFrontMinLength
			NyaHookLib::Patch<uint16_t>(0x429FBB, 0xD8DD);
			NyaHookLib::Patch<uint16_t>(0x429FCF, 0xD8DD);
			// remove division by fRearMinLength
			NyaHookLib::Patch<uint16_t>(0x42A4A0, 0xD8DD);
			NyaHookLib::Patch<uint16_t>(0x42A4B4, 0xD8DD);

			static double fBrakePowerMult = 1.0;
			NyaHookLib::Patch(0x443FDB + 2, &fBrakePowerMult);
			NyaHookLib::Patch(0x444064 + 2, &fBrakePowerMult);

			FixupFO2TirePhysicsCode();

			DWORD oldProt;
			VirtualProtect(aTirePhysicsCode, sizeof(aTirePhysicsCode), PAGE_EXECUTE_READWRITE, &oldProt);
			NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x443D40, aTirePhysicsCode);


			//NyaHookLib::Patch<float>(0x6F81A0, 0.852 * 3.75);
			//NyaHookLib::Patch<float>(0x6F8198, 0.714 * 1.2);
			//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x453940, &FO2TirePhysics);

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x454CBA, &FO2TirePhysics1); // 3.75
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x454DD4, &FO2TirePhysics2); // 1.2
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x45B90F, &FO2TirePhysics2); // 1.2

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