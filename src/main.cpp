#include <openvr.h>
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <args.hxx>
#include <curses.h>
#include <stdlib.h>
#include "SimpleIni.h"
#include "setup.hpp"

using namespace vr;

static constexpr const char *version = "0.1.1";

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

bool loadSettings() {
	// Get ini file
	CSimpleIniA ini;
	SI_Error rc = ini.LoadFile("settings.ini");
	if (rc < 0)
		return false;

	// Get setting values
	autoStart = std::stoi(ini.GetValue("Initialization", "autoStart",
									   std::to_string(autoStart).c_str()));
	initialRes = std::stof(ini.GetValue("Initialization", "initialRes",
										std::to_string(initialRes * 100.0f).c_str())) /
				 100.0f;

	minRes = std::stof(ini.GetValue("Resolution change", "minRes",
									std::to_string(minRes * 100.0f).c_str())) / 100.0f;
	maxRes = std::stof(ini.GetValue("Resolution change", "maxRes",
									std::to_string(maxRes * 100.0f).c_str())) / 100.0f;
	dataPullDelayMs = std::stol(ini.GetValue("Resolution change", "dataPullDelayMs",
											 std::to_string(dataPullDelayMs).c_str()));
	resChangeDelayMs = std::stol(ini.GetValue("Resolution change", "resChangeDelayMs",
											  std::to_string(
													  resChangeDelayMs).c_str()));
	minGpuTimeThreshold = std::stof(
			ini.GetValue("Resolution change", "minGpuTimeThreshold",
						 std::to_string(minGpuTimeThreshold).c_str()));
	resIncreaseMin = std::stof(ini.GetValue("Resolution change", "resIncreaseMin",
											std::to_string(
													resIncreaseMin * 100.0f).c_str())) /
					 100.0f;
	resDecreaseMin = std::stof(ini.GetValue("Resolution change", "resDecreaseMin",
											std::to_string(
													resDecreaseMin * 100.0f).c_str())) /
					 100.0f;
	resIncreaseScale = std::stof(ini.GetValue("Resolution change", "resIncreaseScale",
											  std::to_string(resIncreaseScale *
															 100.0f).c_str())) / 100.0f;
	resDecreaseScale = std::stof(ini.GetValue("Resolution change", "resDecreaseScale",
											  std::to_string(resDecreaseScale *
															 100.0f).c_str())) / 100.0f;
	resIncreaseThreshold = std::stof(
			ini.GetValue("Resolution change", "resIncreaseThreshold",
						 std::to_string(resIncreaseThreshold * 100.0f).c_str())) /
						   100.0f;
	resDecreaseThreshold = std::stof(
			ini.GetValue("Resolution change", "resDecreaseThreshold",
						 std::to_string(resDecreaseThreshold * 100.0f).c_str())) /
						   100.0f;
	dataAverageSamples = std::stoi(
			ini.GetValue("Resolution change", "dataAverageSamples",
						 std::to_string(dataAverageSamples).c_str()));
	alwaysReproject = std::stoi(ini.GetValue("Resolution change", "alwaysReproject",
											 std::to_string(alwaysReproject).c_str()));
	return true;
}

long getCurrentTimeMillis() {
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

int main(int argc, char *argv[]) {
	// Check for errors
	EVRInitError init_error = VRInitError_None;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(
			VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error != VRInitError_None) {
		system = nullptr;
		fmt::print("Unable to init VR runtime: {}\n",
				   VR_GetVRInitErrorAsEnglishDescription(init_error));
		return EXIT_FAILURE;
	}
	if (!VRCompositor()) {
		fmt::print("Failed to initialize VR compositor!");
		return EXIT_FAILURE;
	}
	int rows, cols; // max rows and cols
	int row, col; // current row and col
	initscr(); // Initialize screen
	cbreak(); // Disable line-buffering (for input)
	noecho(); // Don't show what the user types

	// Load settings from ini file
	bool settingsLoaded = loadSettings();

	// Set auto-start
	int autoStartResult = handle_setup(autoStart);

	{
		using namespace std::chrono_literals;
		if (autoStartResult == 1) {
			std::this_thread::sleep_for(1s);
			printw("Clear!");
			refresh();
			std::this_thread::sleep_for(600ms);
		} else if (autoStartResult == 2) {
			std::this_thread::sleep_for(5s);
		}
	}
	clear();
	refresh();

	// Set default resolution
	vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleScale_Float, initialRes);

	// Initialize loop variables
	long lastChangeTime = 0;
	std::list<float> gpuTimes;
	std::list<bool> gpuReprojections;

	// event loop
	while (true) {
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Update resolution and target fps
		float currentRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section,
													  vr::k_pch_SteamVR_SupersampleScale_Float);
		float targetFps = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0,
																				   Prop_DisplayFrequency_Float));
		float targetGpuTime = 1000 / targetFps;

		// Get current frame
		auto *frameTiming = new vr::Compositor_FrameTiming;
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
		for (float time: gpuTimes) {
			averageGpuTime += time;
		}
		averageGpuTime /= static_cast<float>(gpuTimes.size());

		// Check if GPU hasn't been the offender for a given time
		gpuReprojections.push_front(reprojectionFlag == 276);
		if (gpuReprojections.size() > dataAverageSamples * 4)
			gpuReprojections.pop_back();
		bool stableGpuReprojection = true;
		for (bool gpuReproj: gpuReprojections) {
			if (!gpuReproj)
				stableGpuReprojection = false;
		}
		clear();
		getmaxyx(stdscr, rows, cols);
		// Write info to console
		mvprintw(0, 0, "%s", fmt::format("OpenVR Dynamic Resolution v.{}\n", version).c_str());
		if (settingsLoaded) {
			printw("%s", fmt::format("settings.ini successfully loaded").c_str());
		} else {
			printw("%s", fmt::format("Error loading settings.ini").c_str());
		}

		mvprintw(4, 0, "%s", fmt::format("Target FPS: {} fps\n", std::to_string(int(targetFps))).c_str());
		printw("%s", fmt::format("Target GPU frametime: {} ms\n\n", std::to_string(targetGpuTime)).c_str());

		printw("%s", fmt::format("VSync'd FPS: {} fps\n",
								 std::to_string(int(targetFps / static_cast<float>(frameRepeat)))).c_str());
		printw("%s", fmt::format("GPU frametime: {} ms\n\n", std::to_string(averageGpuTime)).c_str());

		if (frameRepeat > 1) {
			printw("%s", fmt::format("Reprojecting: Yes\n").c_str());

			char const *reason = "Other";
			if (reprojectionFlag == 20)
				reason = "CPU";
			else if (reprojectionFlag == 276)
				reason = "GPU";
			else if (reprojectionFlag == 240)
				reason = "Prediction mask";
			else if (reprojectionFlag == 3840)
				reason = "Throttle mask";
			printw("%s", fmt::format("Reprojection reason: {}", reason).c_str());
		} else {
			printw("%s", fmt::format("Reprojecting: No").c_str());
		}

		getyx(stdscr, row, col);
		mvprintw(row + 2 + (frameRepeat == 1), 0, "%s", fmt::format("Resolution = {}%",
																	std::to_string(int(currentRes * 100))).c_str());

		mvprintw(rows - 2, 0, "%s", fmt::format("https://github.com/Louka3000/OpenVR-Dynamic-Resolution\n").c_str());
		printw("%s", fmt::format(
				"If the terminal is frozen (quick-edit mode), press any key to un-freeze.").c_str());

		// Adjust resolution
		if (currentTime - resChangeDelayMs > lastChangeTime) {
			lastChangeTime = currentTime;
			if (averageGpuTime > minGpuTimeThreshold) {
				// Double target frametime if CPU bottlenecks or if user wants to.
				if ((frameRepeat > 1 && !stableGpuReprojection) || alwaysReproject == 1)
					targetGpuTime *= 2;

				// Calculate new resolution
				float newRes = currentRes;
				if (averageGpuTime < targetGpuTime * resIncreaseThreshold) {
					newRes += (((targetGpuTime - averageGpuTime) / targetGpuTime) *
							   resIncreaseScale) + resIncreaseMin;
				} else if (averageGpuTime > targetGpuTime * resDecreaseThreshold) {
					newRes -= (std::abs(
							(averageGpuTime - targetGpuTime) / targetGpuTime) *
							   resDecreaseScale) + resDecreaseMin;
				}
				// Clamp resolution
				if (newRes > maxRes)
					newRes = maxRes;
				else if (newRes < minRes)
					newRes = minRes;

				// Sets the new resolution
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
										   vr::k_pch_SteamVR_SupersampleScale_Float,
										   newRes);
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
