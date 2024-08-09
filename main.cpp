#include <windows.h>
#include <cmath>
#include "nya_commonhooklib.h"

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

float fSteerMultTest = 1.33;

uintptr_t SteerMultTest_jmp = 0x429F6D;
void __attribute__((naked)) SteerMultTest() {
	__asm__ (
		"lea eax, %1\n\t"
		"fld dword ptr [esp+0x10]\n\t"
		"fmul dword ptr [esp+0x18]\n\t"
		"fmul dword ptr [eax]\n\t"
		"fstp dword ptr [esp+0x18]\n\t"
		"fld dword ptr [esp+0xC]\n\t"
		"fmul dword ptr [esp+0x14]\n\t"
		"fmul dword ptr [eax]\n\t"
		"fstp dword ptr [esp+0x14]\n\t"
		"jmp %0\n\t"
			:
			:  "m" (SteerMultTest_jmp), "m" (fSteerMultTest)
	);
}

uintptr_t SteerMultTest2_jmp = 0x429F33;
void __attribute__((naked)) SteerMultTest2() {
	__asm__ (
		"lea eax, %1\n\t"
		"fld dword ptr [esp+0xC]\n\t"
		"fmul dword ptr [esp+0x18]\n\t"
		"fmul dword ptr [eax]\n\t"
		"fstp dword ptr [esp+0x18]\n\t"
		"fld dword ptr [esp+0x10]\n\t"
		"fmul dword ptr [esp+0x14]\n\t"
		"fmul dword ptr [eax]\n\t"
		"fstp dword ptr [esp+0x14]\n\t"
		"jmp %0\n\t"
			:
			:  "m" (SteerMultTest2_jmp), "m" (fSteerMultTest)
	);
}

double __cdecl FO2TirePhysics(float a1, float a2, float a3, float a4, float a5, float extraMult) {
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
	//cos(atan2(1.2 * (v93 * 0.71399999) - atan2(v93 * 0.71399999, 1.0) * 0.2, 1.0) * 1.4 - 1.5707964);

	float aMagicNumber = 1.5;

	auto v42 = a1 * a2;
	// technically correct, exact FO2 code but doesn't feel right?
	//return cos(atan2(extraMult * v42 - atan2(v42, 1.0) * -a5, 1.0) * a3 - 1.5707964) * a4;
	// this feels a lot more FO2-y
	return cos(atan2(extraMult * v42 - atan2(aMagicNumber * v42, 1.0) * -a5, 1.0) * a3 - 1.5707964) * a4;
}

double __cdecl FO2TirePhysics1(float a1, float a2, float a3, float a4, float a5) {
	return FO2TirePhysics(a1, a2, a3, a4, a5, 3.75);
}

double __cdecl FO2TirePhysics2(float a1, float a2, float a3, float a4, float a5) {
	return FO2TirePhysics(a1, a2, a3, a4, a5, 1.2);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x24CEF7) {
				MessageBoxA(nullptr, "Unsupported game version! Make sure you're using the Steam GFWL version (.exe size of 4242504 bytes)", "nya?!~", MB_ICONERROR);
				exit(0);
				return TRUE;
			}

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

			//NyaHookLib::Patch<float>(0x6F81A0, 0.852 * 3.75);
			//NyaHookLib::Patch<float>(0x6F8198, 0.714 * 1.2);
			//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x453940, &FO2TirePhysics);

			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x454CBA, &FO2TirePhysics1); // 3.75
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x454DD4, &FO2TirePhysics2); // 1.2
			NyaHookLib::PatchRelative(NyaHookLib::CALL, 0x45B90F, &FO2TirePhysics2); // 1.2

			//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x429F55, &SteerMultTest);
			//NyaHookLib::PatchRelative(NyaHookLib::JMP, 0x429F1B, &SteerMultTest2);

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