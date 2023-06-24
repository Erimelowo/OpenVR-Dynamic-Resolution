#include <openvr.h>
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <args.hxx>
#include <curses.h>
#include <stdlib.h>

#include "SimpleIni.h"
#include "setup.hpp"
#include <iostream>
#include <Windows.h>
#include <nvml.h>

using namespace std::chrono_literals;
using namespace vr;

static constexpr const char *version = "0.2.2";

int autoStart = 1;
int minimizeOnStart = 0;
float initialRes = 1.0f;

float minRes = 0.60;
float maxRes = 5.0f;
long dataPullDelayMs = 250;
long resChangeDelayMs = 1400;
float minGpuTimeThreshold = 0.75f;
float resIncreaseMin = 0.03f;
float resDecreaseMin = 0.09f;
float resIncreaseScale = 0.60f;
float resDecreaseScale = 0.9;
float resIncreaseThreshold = 0.75;
float resDecreaseThreshold = 0.85f;
int dataAverageSamples = 3;
int resetOnThreshold = 1;
int alwaysReproject = 0;
int vramLimit = 90;
int vramTarget = 80;


float getVramUsage() {
    nvmlReturn_t result;
    nvmlDevice_t device;
    nvmlMemory_t memory;
    unsigned int deviceCount;

    // Initialize NVML library
    result = nvmlInit();
    if (NVML_SUCCESS != result) {
        return -1.0f;
    }

    // Get device handle
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (NVML_SUCCESS != result) {
        nvmlShutdown();
        return -1.0f;
    }

    // Get memory info
    result = nvmlDeviceGetMemoryInfo(device, &memory);
    if (NVML_SUCCESS != result) {
        nvmlShutdown();
        return -1.0f;
    }

    // Shutdown NVML library
    nvmlShutdown();

    // Return VRAM usage as a percentage
    return (float)memory.used / (float)memory.total;
}

bool loadSettings()
{
	// Get ini file
	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("settings.ini");
	if (rc < 0)
		return false;

	// Get setting values
	autoStart = std::stoi(ini.GetValue("Initialization", "autoStart", std::to_string(autoStart).c_str()));
	minimizeOnStart = std::stoi(ini.GetValue("Initialization", "minimizeOnStart", std::to_string(minimizeOnStart).c_str()));
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
	resetOnThreshold = std::stoi(ini.GetValue("Resolution change", "resetOnThreshold", std::to_string(resetOnThreshold).c_str()));
	alwaysReproject = std::stoi(ini.GetValue("Resolution change", "alwaysReproject", std::to_string(alwaysReproject).c_str()));
	vramLimit = std::stoi(ini.GetValue("Resolution change", "vramLimit", std::to_string(vramLimit).c_str()));
	vramTarget = std::stoi(ini.GetValue("Resolution change", "vramTarget", std::to_string(vramTarget).c_str()));
	return true;
}

long getCurrentTimeMillis()
{
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

int main(int argc, char *argv[])
{
	int rows, cols;		 // max rows and cols
	initscr();			 // Initialize screen
	cbreak();			 // Disable line-buffering (for input)
	noecho();			 // Don't show what the user types
	resize_term(17, 64); // Sets the initial (y, x) resolution

	// Check for errors
	EVRInitError init_error = VRInitError_None;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(
		VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error != VRInitError_None)
	{
		system = nullptr;
		printw("%s", fmt::format("Unable to init VR runtime: {}\n", VR_GetVRInitErrorAsEnglishDescription(init_error)).c_str());
		refresh();
		std::this_thread::sleep_for(4000ms);
		return EXIT_FAILURE;
	}
	if (!VRCompositor())
	{
		printw("Failed to initialize VR compositor.");
		refresh();
		std::this_thread::sleep_for(4000ms);
		return EXIT_FAILURE;
	}

	// Load settings from ini file
	bool settingsLoaded = loadSettings();

#if defined(WIN32)
	// Minimize the window if user wants to
	if (minimizeOnStart == 1)
		ShowWindow(GetConsoleWindow(), SW_MINIMIZE);
	else if (minimizeOnStart == 2)
		ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

	// Set auto-start
	int autoStartResult = handle_setup(autoStart);

	if (autoStartResult == 1)
	{
		refresh();
		std::this_thread::sleep_for(1000ms);
		printw("Done!");
		refresh();
		std::this_thread::sleep_for(600ms);
	}
	else if (autoStartResult == 2)
	{
		refresh();
		std::this_thread::sleep_for(4000ms);
	}
	clear();
	refresh();

	// Set default resolution
	vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleScale_Float, initialRes);

	// Initialize loop variables
	long lastChangeTime = getCurrentTimeMillis();
	std::list<float> gpuTimes;
	float lastRes = initialRes;

	// event loop
	while (true)
	{
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Update resolution and target fps
		float currentRes = vr::VRSettings()
							   ->GetFloat(vr::k_pch_SteamVR_Section,
										  vr::k_pch_SteamVR_SupersampleScale_Float);
		float targetFps = std::round(vr::VRSystem()
										 ->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
		float targetGpuTime = 1000 / targetFps;

		// Get current frame
		auto *frameTiming = new vr::Compositor_FrameTiming;
		frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
		vr::VRCompositor()->GetFrameTiming(frameTiming, 0);

		// Get info about current frame
		uint32_t frameIndex = frameTiming->m_nFrameIndex;
		uint32_t frameRepeat = frameTiming->m_nNumFramePresents;
		if (frameRepeat < 1)
			frameRepeat = 1;
		uint32_t currentFps = targetFps / frameRepeat;
		float gpuTime = frameTiming->m_flTotalRenderGpuMs;
		uint32_t reprojectionFlag = frameTiming->m_nReprojectionFlags;

		// Double the target frametime if the user wants to.
		if (alwaysReproject == 1)
		{
			targetFps /= 2;
			targetGpuTime *= 2;
		}

		// Calculate average GPU frametime
		gpuTimes.push_front(gpuTime);
		if (gpuTimes.size() > dataAverageSamples)
			gpuTimes.pop_back();
		float averageGpuTime = 0;
		for (float time : gpuTimes)
		{
			averageGpuTime += time;
		}
		averageGpuTime /= gpuTimes.size();

		// Clear console
		clear();
		getmaxyx(stdscr, rows, cols);

		// Write info to console
		attron(A_UNDERLINE);
		mvprintw(0, 0, "%s", fmt::format("OpenVR Dynamic Resolution v.{}", version).c_str());
		attroff(A_UNDERLINE);
		if (settingsLoaded)
			mvprintw(1, 0, "%s", fmt::format("settings.ini successfully loaded").c_str());
		else
			mvprintw(1, 0, "%s", fmt::format("Error loading settings.ini").c_str());

		mvprintw(4, 0, "%s", fmt::format("Target FPS: {} fps", std::to_string(int(targetFps))).c_str());
		std::string displayedTime = std::to_string(targetGpuTime).substr(0, 4);
		mvprintw(5, 0, "%s", fmt::format("Target GPU frametime: {} ms", displayedTime).c_str());

		mvprintw(7, 0, "%s", fmt::format("VSync'd FPS: {} fps", std::to_string(currentFps)).c_str());
		std::string displayedAverageTime = std::to_string(averageGpuTime).substr(0, 4);
		mvprintw(8, 0, "%s", fmt::format("GPU frametime: {} ms", displayedAverageTime).c_str());

		std::string vramUsage = std::to_string(getVramUsage()*100).substr(0, 4);
		std::string vramLimitString = std::to_string(vramTarget).substr(0, 4);
		mvprintw(9, 0, "%s", fmt::format("VRAM usage: {} %, target: {} %", vramUsage, vramLimitString).c_str());
		if (frameRepeat > 1)
		{
			mvprintw(10, 0, "Reprojecting: Yes");

			char const *reason = "Other";
			if (reprojectionFlag == 20)
				reason = "CPU";
			else if (reprojectionFlag == 276)
				reason = "GPU";
			else if (reprojectionFlag == 240)
				reason = "Prediction mask";
			else if (reprojectionFlag == 3840)
				reason = "Throttle mask";
			mvprintw(11, 0, "%s", fmt::format("Reprojection reason: {}", reason).c_str());
		}
		else
		{
			mvprintw(10, 0, "Reprojecting: No");
		}

		attron(A_BOLD);
		mvprintw(13, 0, "%s", fmt::format("Resolution = {}%", std::to_string(int(currentRes * 100))).c_str());
		attroff(A_BOLD);

		// Adjust resolution
		if (currentTime - resChangeDelayMs > lastChangeTime)
		{
			lastChangeTime = currentTime;
			bool vramLimitReached = vramLimit<(int)(getVramUsage()*100);
			bool vramTargetReached = vramTarget<(int)(getVramUsage()*100);
			bool dashboardOpen = VROverlay()->IsDashboardVisible();

			if (averageGpuTime > minGpuTimeThreshold && !dashboardOpen)
			{
				// Calculate new resolution
				float newRes = currentRes;
				if (averageGpuTime < targetGpuTime * resIncreaseThreshold && !vramTargetReached)
				{
					newRes += ((((targetGpuTime * resIncreaseThreshold) - averageGpuTime) / targetGpuTime) *
							   resIncreaseScale) +
							  resIncreaseMin;
				}
				else if (averageGpuTime > targetGpuTime * resDecreaseThreshold)
				{
					newRes -= (((averageGpuTime - (targetGpuTime * resDecreaseThreshold)) / targetGpuTime) *
							   resDecreaseScale) +
							  resDecreaseMin;
				}
				// Clamp resolution
				newRes=std::clamp(newRes, minRes, maxRes);

				if (vramTargetReached)
				{	

					if (lastRes < newRes){
						//make sure the resolution doesnt ever increase when the vram target is reached
						newRes = lastRes;
					}
					if (vramLimitReached){
						//force the resolution to decrease when the vram limit is reached
						newRes -= resDecreaseMin;
					}
				}
				newRes=std::clamp(newRes, minRes, maxRes);
				// Sets the new resolution
				if (lastRes != newRes)
				{
					vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
											   vr::k_pch_SteamVR_SupersampleScale_Float,
											   newRes);
					lastRes = newRes;
				}
			}
			else if (resetOnThreshold == 1 && lastRes != initialRes && !dashboardOpen)
			{
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
										   vr::k_pch_SteamVR_SupersampleScale_Float,
										   initialRes);
				lastRes = initialRes;
			}
		}

		refresh();
		// Calculate how long to sleep for
		long sleepTime = dataPullDelayMs;
		if (dataPullDelayMs < currentTime - lastChangeTime &&
			resChangeDelayMs < currentTime - lastChangeTime)
			sleepTime = (currentTime - lastChangeTime) - resChangeDelayMs;
		else if (resChangeDelayMs < dataPullDelayMs)
			sleepTime = resChangeDelayMs;
		// ZZzzzz
		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	}
}
