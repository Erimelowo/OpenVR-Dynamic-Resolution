#include <openvr.h>
#include <thread>
#include <fmt/core.h>
#include <fmt/color.h>
#include <args.hxx>

#include "SimpleIni.h"
#include "setup.hpp"

using namespace vr;

static constexpr const char* version = "0.1.2";

int autoStart = 0;
float initialRes = 1.0f;

float minRes = 0.8;
float maxRes = 4.0f;
long dataPullDelayMs = 250;
long resChangeDelayMs = 1400;
float minGpuTimeThreshold = 0.7f;
float resIncreaseMin = 0.02f;
float resDecreaseMin = 0.06f;
float resIncreaseScale = 0.3f;
float resDecreaseScale = 0.5;
float resIncreaseThreshold = 0.74;
float resDecreaseThreshold = 0.86f;
int dataAverageSamples = 3;
int alwaysReproject = 0;

void setCursorPosition(int x, int y){
    static const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    std::cout.flush();
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hOut, coord);
}

bool loadSettings(){
	// Get ini file
	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("settings.ini");
	if (rc < 0)
		return false;

	// Get setting values
	autoStart = std::stoi(ini.GetValue("Initialization", "autoStart", std::to_string(autoStart).c_str()));
	initialRes = std::stof(ini.GetValue("Initialization", "initialRes", std::to_string(initialRes * 100.0f).c_str())) / 100.0f;

	minRes = std::stof(ini.GetValue("Resolution change", "minRes", std::to_string(minRes * 100.0f).c_str())) / 100.0f;
	maxRes = std::stof(ini.GetValue("Resolution change", "maxRes", std::to_string(maxRes * 100.0f).c_str())) / 100.0f;
	dataPullDelayMs = std::stol(ini.GetValue("Resolution change", "dataPullDelayMs", std::to_string(dataPullDelayMs).c_str()));
	resChangeDelayMs = std::stol(ini.GetValue("Resolution change", "resChangeDelayMs", std::to_string(resChangeDelayMs).c_str()));
	minGpuTimeThreshold = std::stof(ini.GetValue("Resolution change", "minGpuTimeThreshold", std::to_string(minGpuTimeThreshold).c_str()));
	resIncreaseMin = std::stof(ini.GetValue("Resolution change", "resIncreaseMin", std::to_string(resIncreaseMin * 100.0f).c_str())) / 100.0f;
	resDecreaseMin = std::stof(ini.GetValue("Resolution change", "resDecreaseMin", std::to_string(resDecreaseMin * 100.0f).c_str())) / 100.0f;
	resIncreaseScale = std::stof(ini.GetValue("Resolution change", "resIncreaseScale", std::to_string(resIncreaseScale * 100.0f).c_str())) / 100.0f;
	resDecreaseScale = std::stof(ini.GetValue("Resolution change", "resDecreaseScale", std::to_string(resDecreaseScale * 100.0f).c_str())) / 100.0f;
	resIncreaseThreshold = std::stof(ini.GetValue("Resolution change", "resIncreaseThreshold", std::to_string(resIncreaseThreshold * 100.0f).c_str())) / 100.0f;
	resDecreaseThreshold = std::stof(ini.GetValue("Resolution change", "resDecreaseThreshold", std::to_string(resDecreaseThreshold * 100.0f).c_str())) / 100.0f;
	dataAverageSamples = std::stoi(ini.GetValue("Resolution change", "dataAverageSamples", std::to_string(dataAverageSamples).c_str()));
	alwaysReproject = std::stoi(ini.GetValue("Resolution change", "alwaysReproject", std::to_string(alwaysReproject).c_str()));
	return true;
}

long getCurrentTimeMillis(){
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

int main(int argc, char* argv[]) {
	// Check for errors
	EVRInitError init_error = VRInitError_None;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error != VRInitError_None) {
		system = nullptr;
		fmt::print("Unable to init VR runtime: {}\n", VR_GetVRInitErrorAsEnglishDescription(init_error));
		return EXIT_FAILURE;
	}
	if (!VRCompositor()) {
		fmt::print("Failed to initialize VR compositor!");
		return EXIT_FAILURE;
	}

	// Load settings from ini file
	bool settingsLoaded = loadSettings();

	// Set auto-start
	int autoStartResult = handle_setup(autoStart);

	if(autoStartResult == 1){
		Sleep(1000);
		fmt::print("Done!");
		Sleep(600);
		std::system("cls");
	}else if(autoStartResult == 2){
		Sleep(5000);
		std::system("cls");
	}

	// Set default resolution
	vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, initialRes);

	// Initialize loop variables
	long lastChangeTime = 0;
	std::list<float> gpuTimes;

	// event loop
	while (true) {
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Update resolution and target fps
		float currentRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float);
		float targetFps = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
		float targetGpuTime = 1000 / targetFps;

		// Get current frame
		vr::Compositor_FrameTiming* frameTiming = new vr::Compositor_FrameTiming;
		frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
		vr::VRCompositor()->GetFrameTiming(frameTiming, 0);

		// Get info about current frame
		uint32_t frameIndex = frameTiming->m_nFrameIndex;
		uint32_t frameRepeat = frameTiming->m_nNumFramePresents;
		if (frameRepeat < 1) frameRepeat = 1;
		float gpuTime = frameTiming->m_flTotalRenderGpuMs;
		uint32_t reprojectionFlag = frameTiming->m_nReprojectionFlags;

		// Calculate average GPU frametime
		gpuTimes.push_front(gpuTime);
		if (gpuTimes.size() > dataAverageSamples)
			gpuTimes.pop_back();
		float averageGpuTime = 0;
		for each (float time in gpuTimes){
			averageGpuTime += time;
		}
		averageGpuTime /= gpuTimes.size();

		// Write info to console
		setCursorPosition(0, 0);
		fmt::print("OpenVR Dynamic Resolution v.{}\n", version);
		if(settingsLoaded == true)
			fmt::print("settings.ini successfully loaded\n\n\n");
		else
			fmt::print("Error loading settings.ini\n\n\n");

		fmt::print("Target FPS: {} fps  \n", std::to_string(int(targetFps)));
		fmt::print("Target GPU frametime: {} ms  \n\n", std::to_string(targetGpuTime));

		fmt::print("VSync'd FPS: {} fps  \n", std::to_string(int(targetFps / frameRepeat)));
		fmt::print("GPU frametime: {} ms  \n\n", std::to_string(averageGpuTime));

		if (frameRepeat > 1) {
			fmt::print("Reprojecting: Yes\n");

			char* reason = "Other";
			if (reprojectionFlag == 20)
				reason = "CPU";
			else if (reprojectionFlag == 276)
				reason = "GPU";
			else if (reprojectionFlag == 240)
				reason = "Prediction mask";
			else if (reprojectionFlag == 3840)
				reason = "Throttle mask";
			fmt::print("Reprojection reason: {}                  \n\n", reason);
		}
		else {
			fmt::print("Reprojecting: No \n");
			fmt::print("                                         \n\n");
		}

		fmt::print("Resolution = {}%    \n\n\n\n\n\n\n\n\n\n\n\n\n\n", std::to_string(int(currentRes * 100)));

		fmt::print("https://github.com/Louka3000/OpenVR-Dynamic-Resolution\n");
		fmt::print("If the terminal is frozen (quick-edit mode), press any key to un-freeze.");

		// Adjust resolution
		if (currentTime - resChangeDelayMs > lastChangeTime) {
			lastChangeTime = currentTime;
			if(averageGpuTime > minGpuTimeThreshold){
				// Double the target frametime if the user wants to.
				if (alwaysReproject == 1)
					targetGpuTime *= 2;

				// Calculate new resolution
				float newRes = currentRes;
				if (averageGpuTime < targetGpuTime * resIncreaseThreshold) {
					newRes += (((targetGpuTime - averageGpuTime) / targetGpuTime) * resIncreaseScale) + resIncreaseMin;
				}
				else if (averageGpuTime > targetGpuTime * resDecreaseThreshold) {
					// FIXME this math for resDecreaseScale sucks
					newRes -= (std::abs((averageGpuTime - targetGpuTime) / targetGpuTime) * resDecreaseScale) + resDecreaseMin;
				}
				// Clamp resolution
				if (newRes > maxRes)
					newRes = maxRes;
				else if (newRes < minRes)
					newRes = minRes;

				// Sets the new resolution
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, newRes);
			}
		}

		// Calculate how long to sleep for
		long sleepTime = dataPullDelayMs;
		if (dataPullDelayMs < currentTime - lastChangeTime && resChangeDelayMs < currentTime - lastChangeTime)
			sleepTime = (currentTime - lastChangeTime) - resChangeDelayMs;
		else if(resChangeDelayMs < dataPullDelayMs)
			sleepTime = resChangeDelayMs;
		// ZZzzzz
		Sleep(sleepTime);
	}
}
