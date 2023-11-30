#include <openvr.h>
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include <args.hxx>
#include <curses.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include "SimpleIni.h"
#include "setup.hpp"

using namespace std::chrono_literals;
using namespace vr;

static constexpr const char *version = "v.0.4.0";

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
float vramTarget = 0.8;
float vramLimit = 0.9;
int vramMonitorEnabled = 1;
int vramOnlyMode = 0;
int preferReprojection = 0;
int ignoreCpuTime = 0;

// NVML stuff
typedef enum nvmlReturn_enum
{
	NVML_SUCCESS = 0,					// The operation was successful.
	NVML_ERROR_UNINITIALIZED = 1,		// NVML was not first initialized with nvmlInit.
	NVML_ERROR_INVALID_ARGUMENT = 2,	// A supplied argument is invalid.
	NVML_ERROR_NOT_SUPPORTED = 3,		// The requested operation is not available on target device.
	NVML_ERROR_NO_PERMISSION = 4,		// The currrent user does not have permission for operation.
	NVML_ERROR_ALREADY_INITIALIZED = 5, // NVML has already been initialized.
	NVML_ERROR_NOT_FOUND = 6,			// A query to find an object was unccessful.
	NVML_ERROR_UNKNOWN = 7,				// An internal driver error occurred.
} nvmlReturn_t;
typedef nvmlReturn_t (*nvmlInit_t)();
typedef nvmlReturn_t (*nvmlShutdown_t)();
typedef struct
{
	unsigned long long total;
	unsigned long long free;
	unsigned long long used;
} nvmlMemory_t;
typedef nvmlReturn_t (*nvmlDevice_t)();
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_t)(unsigned int, nvmlDevice_t *);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfo_t)(nvmlDevice_t, nvmlMemory_t *);
#ifdef _WIN32
typedef HMODULE(nvmlLib);
#else
typedef void *(nvmlLib);
#endif

nvmlDevice_t nvmlDevice;

float getVramUsage(nvmlLib nvmlLibrary)
{
	if (!vramMonitorEnabled || !nvmlLibrary)
		return 0.0f;

	nvmlReturn_t result;
	nvmlMemory_t memoryInfo;
	unsigned int deviceCount;

	// Get memory info
	nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfoPtr;
#ifdef _WIN32
	nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)GetProcAddress(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#else
	nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)dlsym(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#endif
	result = nvmlDeviceGetMemoryInfoPtr(nvmlDevice, &memoryInfo);
	if (result != NVML_SUCCESS)
		return -result;

	// Return VRAM usage as a percentage
	return (float)memoryInfo.used / (float)memoryInfo.total;
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
	vramLimit = std::stoi(ini.GetValue("Resolution change", "vramLimit", std::to_string(vramLimit * 100.0f).c_str())) / 100.0f;
	vramTarget = std::stoi(ini.GetValue("Resolution change", "vramTarget", std::to_string(vramTarget * 100.0f).c_str())) / 100.0f;
	vramMonitorEnabled = std::stoi(ini.GetValue("Resolution change", "vramMonitorEnabled", std::to_string(vramMonitorEnabled).c_str()));
	vramOnlyMode = std::stoi(ini.GetValue("Resolution change", "vramOnlyMode", std::to_string(vramOnlyMode).c_str()));
	preferReprojection = std::stoi(ini.GetValue("Resolution change", "preferReprojection", std::to_string(preferReprojection).c_str()));
	ignoreCpuTime = std::stoi(ini.GetValue("Resolution change", "ignoreCpuTime", std::to_string(ignoreCpuTime).c_str()));

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
#ifdef _WIN32
	HMODULE nvmlLibrary = LoadLibraryA("nvml.dll");
#else
	void *nvmlLibrary = dlopen("libnvidia-ml.so", RTLD_LAZY);
#endif

	int rows, cols;		 // max rows and cols
	initscr();			 // Initialize screen
	cbreak();			 // Disable line-buffering (for input)
	noecho();			 // Don't show what the user types
	resize_term(18, 64); // Sets the initial (y, x) resolution

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

#if defined(_WIN32)
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

	// Initialise NVML
	nvmlInit_t nvmlInitPtr;
#ifdef _WIN32
	nvmlInitPtr = (nvmlInit_t)GetProcAddress(nvmlLibrary, "nvmlInit");
#else
	nvmlInitPtr = (nvmlInit_t)dlsym(nvmlLibrary, "nvmlShutdown");
#endif
	if (!nvmlInitPtr)
		vramMonitorEnabled = 0;
	if (vramMonitorEnabled)
	{
		nvmlReturn_t result;
		// Initialize NVML library
		result = nvmlInitPtr();
		if (result != NVML_SUCCESS)
			vramMonitorEnabled = 0;

		// Get device handle
		nvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndexPtr;
#ifdef _WIN32
		nvmlDeviceGetHandleByIndexPtr = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(nvmlLibrary, "nvmlDeviceGetHandleByIndex");
#else
		nvmlDeviceGetHandleByIndexPtr = (nvmlDeviceGetHandleByIndex_t)dlsym(nvmlLibrary, "nvmlDeviceGetHandleByIndex");
#endif
		if (!nvmlDeviceGetHandleByIndexPtr)
			vramMonitorEnabled = 0;
		else
			result = nvmlDeviceGetHandleByIndexPtr(0, &nvmlDevice);
		if (result != NVML_SUCCESS || !vramMonitorEnabled)
		{
			nvmlShutdown_t nvmlShutdownPtr;
#ifdef _WIN32
			nvmlShutdownPtr = (nvmlShutdown_t)GetProcAddress(nvmlLibrary, "nvmlShutdown");
#else
			nvmlShutdownPtr = (nvmlShutdown_t)dlsym(nvmlLibrary, "nvmlShutdown");
#endif
			vramMonitorEnabled = 0;
			if (nvmlShutdownPtr)
			{
				nvmlShutdownPtr();
			}
		}
	}

	// Initialize loop variables
	long lastChangeTime = getCurrentTimeMillis();
	std::list<float> gpuTimes;
	std::list<float> cpuTimes;
	float lastRes = initialRes;

	// event loop
	while (true)
	{
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Fetch resolution and target fps
		float currentRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float);
		float targetFps = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
		float targetFrametime = 1000 / targetFps;

		// Get current frame
		auto *frameTiming = new vr::Compositor_FrameTiming;
		frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
		vr::VRCompositor()->GetFrameTiming(frameTiming);

		// Get total GPU Frametime
		float gpuTime = frameTiming->m_flTotalRenderGpuMs;
		// Calculate total CPU Frametime
		// https://github.com/Louka3000/OpenVR-Dynamic-Resolution/issues/18#issuecomment-1833105172
		float cpuTime = frameTiming->m_flCompositorRenderCpuMs												// Compositor
						+ (frameTiming->m_flNewFrameReadyMs - frameTiming->m_flNewPosesReadyMs)				// Application
						+ max(0, frameTiming->m_flWaitGetPosesCalledMs - frameTiming->m_flNewPosesReadyMs); // Late Start

		// How many times the current frame repeated (>1 = reprojecting)
		uint32_t frameRepeat = max(1, frameTiming->m_nNumFramePresents);
		// Reason reprojection is happening
		uint32_t reprojectionFlag = frameTiming->m_nReprojectionFlags;

		// Adjust the CPU time off GPU reprojection.
		int gpuFrameRepeat = floor(gpuTime / targetFrametime) + 1;
		if(frameRepeat > 1)
			cpuTime *= gpuFrameRepeat;

		// Calculate average GPU frametime
		gpuTimes.push_front(gpuTime);
		if (gpuTimes.size() > dataAverageSamples)
			gpuTimes.pop_back();
		float averageGpuTime = 0;
		for (float time : gpuTimes)
			averageGpuTime += time;
		averageGpuTime /= gpuTimes.size();
		// Caculate average CPU frametime
		cpuTimes.push_front(cpuTime);
		if (cpuTimes.size() > dataAverageSamples)
			cpuTimes.pop_back();
		float averageCpuTime = 0;
		for (float time : cpuTimes)
			averageCpuTime += time;
		averageCpuTime /= cpuTimes.size();

		// Estimated current FPS
		uint32_t currentFps = targetFps / frameRepeat;
		if(averageCpuTime > targetFrametime)
			currentFps /= fmod(averageCpuTime, targetFrametime) / targetFrametime + 1;

		// Double the target frametime if the user wants to.
		if (alwaysReproject)
		{
			targetFps /= 2;
			targetFrametime *= 2;
		}

		// Clear console
		clear();
		getmaxyx(stdscr, rows, cols);

		// Write info to console
		attron(A_UNDERLINE);
		mvprintw(0, 0, "%s", fmt::format("OpenVR Dynamic Resolution {}", version).c_str());
		attroff(A_UNDERLINE);
		if (settingsLoaded)
			mvprintw(1, 0, "%s", fmt::format("settings.ini successfully loaded").c_str());
		else
			mvprintw(1, 0, "%s", fmt::format("Error loading settings.ini").c_str());

		if (!vramOnlyMode)
		{
			mvprintw(4, 0, "%s", fmt::format("Target FPS: {} fps", std::to_string(int(targetFps))).c_str());

			std::string displayedTime = std::to_string(targetFrametime).substr(0, 4);
			mvprintw(5, 0, "%s", fmt::format("Target frametime: {} ms", displayedTime).c_str());
		}
		else
		{
			mvprintw(4, 0, "%s", "Target FPS: Disalbed");
			mvprintw(5, 0, "%s", "Target frametime: Disabled");
		}
		if (vramMonitorEnabled)
		{
			std::string vramTargetString = std::to_string(vramTarget * 100).substr(0, 4);
			mvprintw(6, 0, "%s", fmt::format("VRAM target: {}%", vramTargetString).c_str());
			std::string vramLimitString = std::to_string(vramLimit * 100).substr(0, 4);
			mvprintw(7, 0, "%s", fmt::format("VRAM limit: {}%", vramLimitString).c_str());
		}
		else
		{
			mvprintw(6, 0, "%s", fmt::format("VRAM target: Disabled").c_str());
			mvprintw(7, 0, "%s", fmt::format("VRAM limit: Disabled").c_str());
		}

		mvprintw(9, 0, "%s", fmt::format("FPS: {} fps", std::to_string(currentFps)).c_str());

		std::string displayedAverageGpuTime = std::to_string(averageGpuTime).substr(0, 4);
		mvprintw(10, 0, "%s", fmt::format("GPU frametime: {} ms", displayedAverageGpuTime).c_str());

		std::string displayedAverageCpuTime = std::to_string(averageCpuTime).substr(0, 4);
		if(frameRepeat > 1 && gpuFrameRepeat > 1)
			displayedAverageCpuTime += fmt::format(" (x{})", gpuFrameRepeat).c_str();
		mvprintw(11, 0, "%s", fmt::format("CPU frametime: {} ms", displayedAverageCpuTime).c_str());

		if (vramMonitorEnabled)
		{
			std::string vramUsage = std::to_string(getVramUsage(nvmlLibrary) * 100).substr(0, 4);
			mvprintw(12, 0, "%s", fmt::format("VRAM usage: {}%", vramUsage).c_str());
		}
		else
		{
			mvprintw(12, 0, "%s", fmt::format("VRAM usage: Disabled").c_str());
		}

		if (frameRepeat > 1)
		{
			mvprintw(14, 0, "Reprojecting: Yes");

			std::string reason = fmt::format("Other ({})", reprojectionFlag).c_str();
			if (reprojectionFlag == 20)
				reason = "CPU";
			else if (reprojectionFlag == 276)
				reason = "GPU";
			mvprintw(15, 0, "%s", fmt::format("Reprojection reason: {}", reason).c_str());
		}
		else
		{
			mvprintw(14, 0, "Reprojecting: No");
		}

		attron(A_BOLD);
		mvprintw(17, 0, "%s", fmt::format("Resolution = {}%", std::to_string(int(currentRes * 100))).c_str());
		attroff(A_BOLD);

		// Resolution handling
		if (currentTime - resChangeDelayMs > lastChangeTime && !VROverlay()->IsDashboardVisible())
		{
			lastChangeTime = currentTime;
			bool vramLimitReached = vramLimit < getVramUsage(nvmlLibrary);
			bool vramTargetReached = vramTarget < getVramUsage(nvmlLibrary);

			// If CPU Frametime is double the target frametime,
			// or if preferReprojection is true and CPU Frametime is greated than targetFrametime.
			if (((averageCpuTime > targetFrametime && preferReprojection) || averageCpuTime / 2 > targetFrametime) && !alwaysReproject && !ignoreCpuTime)
			{
				// Double the target frametime
				targetFrametime *= 2;
			}

			// Adjust resolution
			if ((averageGpuTime > minGpuTimeThreshold || vramOnlyMode))
			{
				float newRes = currentRes;

				// Frametime
				if (averageGpuTime < targetFrametime * resIncreaseThreshold && !vramTargetReached && !vramOnlyMode)
				{
					// Increase resolution
					newRes += ((((targetFrametime * resIncreaseThreshold) - averageGpuTime) / targetFrametime) *
							   resIncreaseScale) +
							  resIncreaseMin;
				}
				else if (averageGpuTime > targetFrametime * resDecreaseThreshold && !vramOnlyMode)
				{
					// Decrease resolution
					newRes -= (((averageGpuTime - (targetFrametime * resDecreaseThreshold)) / targetFrametime) *
							   resDecreaseScale) +
							  resDecreaseMin;
				}

				// VRAM
				if (vramTargetReached)
				{
					if (newRes > lastRes)
					{
						// Make sure the resolution doesn't ever increase when the vram target is reached
						newRes = lastRes;
					}
					if (vramLimitReached)
					{
						// Force the resolution to decrease when the vram limit is reached
						newRes -= resDecreaseMin;
					}
				}
				else if (vramOnlyMode)
				{
					// When in VRAM-only mode, make sure the res goes back up when possible.
					if(newRes < initialRes)
						newRes = min(initialRes, newRes + resIncreaseMin);
				}

				// Clamp the new resolution
				newRes = std::clamp(newRes, minRes, maxRes);

				if (lastRes != newRes)
				{
					// Sets the new resolution
					vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
											   vr::k_pch_SteamVR_SupersampleScale_Float,
											   newRes);
					lastRes = newRes;
				}
			}
			else if (resetOnThreshold && lastRes != initialRes && averageGpuTime < minGpuTimeThreshold)
			{
				// Reset to initialRes because resolution fell below the threshold
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
										   vr::k_pch_SteamVR_SupersampleScale_Float,
										   initialRes);
				lastRes = initialRes;
			}
		}

		// Displays the information
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

	// TODO actually be able to get out of the while loop

#ifdef _WIN32
	nvmlShutdown_t nvmlShutdownPtr = (nvmlShutdown_t)GetProcAddress(nvmlLibrary, "nvmlShutdown");
	if (nvmlShutdownPtr)
	{
		nvmlShutdownPtr();
	}
	FreeLibrary(nvmlLibrary);
#else
	nvmlShutdown_t nvmlShutdownPtr = (nvmlShutdown_t)dlsym(nvmlLibrary, "nvmlShutdown");
	if (nvmlShutdownPtr)
	{
		nvmlShutdownPtr();
	}
	dlclose(nvmlLibrary);
#endif

	return 0;
}
