#include <openvr.h>
#include <chrono>
#include <set>
#include <sstream>
#include <thread>
#include <fmt/core.h>
#include <args.hxx>
#include <stdlib.h>

// To include the nvml library at runtime
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

// Loading and saving .ini configuration file
#include "SimpleIni.h"
#include "setup.hpp"

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

using namespace std::chrono_literals;
using namespace vr;

static constexpr const char *version = "v1.0.0-rc.1";

#pragma region config
#pragma region default settings
// Initialization
int autoStart = 1;
int minimizeOnStart = 0;

// Resolution
float initialRes = 1.0f;
float minRes = 0.60;
float maxRes = 5.0f;
long dataPullDelayMs = 250;
long resChangeDelayMs = 1400;
float minCpuTimeThreshold = 1.0f;
float resIncreaseMin = 0.03f;
float resDecreaseMin = 0.09f;
float resIncreaseScale = 0.60f;
float resDecreaseScale = 0.9;
float resIncreaseThreshold = 0.75;
float resDecreaseThreshold = 0.85f;
int dataAverageSamples = 16;
int resetOnThreshold = 1;
int alwaysReproject = 0;
float vramTarget = 0.8;
float vramLimit = 0.9;
int vramMonitorEnabled = 1;
int vramOnlyMode = 0;
int preferReprojection = 0;
int ignoreCpuTime = 0;
std::set<std::string> disabledApps = {"steam.app.620981"};
#pragma endregion

std::set<std::string> splitConfigValue(const std::string &val)
{
	std::set<std::string> set;
	std::stringstream ss(val);
	std::string word;

	while (ss >> word)
	{
		set.insert(word);
	}

	return set;
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
	minCpuTimeThreshold = std::stof(ini.GetValue("Resolution change", "minCpuTimeThreshold", std::to_string(minCpuTimeThreshold).c_str()));
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
	disabledApps = splitConfigValue(ini.GetValue("Resolution change", "disabledApps", ""));

	return true;
}
#pragma endregion

#pragma region NVML stuff
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
#pragma endregion

long getCurrentTimeMillis()
{
	auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

/**
 * Returns the current VR application key (steam.app.000000)
 * or an empty string if no app is running.
 */
std::string getCurrentApplicationKey()
{
	char applicationKey[vr::k_unMaxApplicationKeyLength];

	uint32_t processId = vr::VRApplications()->GetCurrentSceneProcessId();
	if (processId == 0)
		return "";

	EVRApplicationError err = vr::VRApplications()->GetApplicationKeyByProcessId(processId, applicationKey, vr::k_unMaxApplicationKeyLength);
	if (err != VRApplicationError_None)
		return "";

	return {applicationKey};
}

int main(int argc, char *argv[])
{
	// Load NVML
#ifdef _WIN32
	HMODULE nvmlLibrary = LoadLibraryA("nvml.dll");
#else
	void *nvmlLibrary = dlopen("libnvidia-ml.so", RTLD_LAZY);
#endif

	if (!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Make window non-resizable
	glfwWindowHint(GLFW_RESIZABLE, false);

	// Create window with graphics context
	static int mainWindowWidth = 420;
	static int mainWindowHeight = 360;
	GLFWwindow *window = glfwCreateWindow(mainWindowWidth, mainWindowHeight, fmt::format("OVR Dynamic Resolution {}", version).c_str(), nullptr, nullptr);
	if (window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	// Don't save Dear ImGui window state
	io.IniFilename = NULL;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Set window icon TODO
	// glfwSetWindowIcon(window, 1, )

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Check for VR init errors
	EVRInitError init_error = VRInitError_None;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(
		VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error != VRInitError_None)
	{
		system = nullptr;
		// printw("%s", fmt::format("Unable to init VR runtime: {}\n", VR_GetVRInitErrorAsEnglishDescription(init_error)).c_str());
		// refresh();
		std::this_thread::sleep_for(4000ms);
		return EXIT_FAILURE;
	}
	if (!VRCompositor())
	{
		// printw("Failed to initialize VR compositor.");
		std::this_thread::sleep_for(4000ms);
		return EXIT_FAILURE;
	}

	// Load settings from ini file
	bool settingsLoaded = loadSettings();

	// Set auto-start TODO: test
	int autoStartResult = handle_setup(autoStart);

	if (autoStartResult == 1)
	{
		std::this_thread::sleep_for(1000ms);
		// printw("Done!");
		std::this_thread::sleep_for(600ms);
	}
	else if (autoStartResult == 2)
	{
		std::this_thread::sleep_for(4000ms);
	}

	// Minimize or hide the window if user wants to
	if (minimizeOnStart == 1) // Minimize
		glfwIconifyWindow(window);
	else if (minimizeOnStart == 2) // Hide
		glfwHideWindow(window);

	// Make sure we can set resolution ourselves (Custom instead of Auto)
	vr::VRSettings()->SetInt32(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleManualOverride_Bool, true);

	// Set default resolution
	vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleScale_Float, initialRes);

	// Initialise NVML
	nvmlInit_t nvmlInitPtr;
#ifdef _WIN32
	nvmlInitPtr = (nvmlInit_t)GetProcAddress(nvmlLibrary, "nvmlInit");
#else
	nvmlInitPtr = (nvmlInit_t)dlsym(nvmlLibrary, "nvmlInit");
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
	bool adjustResolution = true;
	std::list<float> gpuTimes;
	std::list<float> cpuTimes;

	// event loop
	while (!glfwWindowShouldClose(window))
	{
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Fetch resolution and target fps
		float newRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float);
		float lastRes = newRes;
		float targetFps = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
		float targetFrametime = 1000 / targetFps;
		float realTargetFrametime = targetFrametime;

		// Get current frame
		auto *frameTiming = new vr::Compositor_FrameTiming;
		frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
		vr::VRCompositor()->GetFrameTiming(frameTiming);

		// Get total GPU Frametime
		float gpuTime = frameTiming->m_flTotalRenderGpuMs;
		// Calculate total CPU Frametime
		// https://github.com/Louka3000/OpenVR-Dynamic-Resolution/issues/18#issuecomment-1833105172
		float cpuTime = frameTiming->m_flCompositorRenderCpuMs									 // Compositor
						+ (frameTiming->m_flNewFrameReadyMs - frameTiming->m_flNewPosesReadyMs); // Application & Late Start

		// How many times the current frame repeated (>1 = reprojecting)
		uint32_t frameShown = frameTiming->m_nNumFramePresents;
		// Reason reprojection is happening
		uint32_t reprojectionFlag = frameTiming->m_nReprojectionFlags;

		// Adjust the CPU time off GPU reprojection.
		float realCpuTime = cpuTime;
		cpuTime *= min(frameShown, floor(gpuTime / targetFrametime) + 1);

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
		uint32_t currentFps = targetFps / frameShown;
		if (averageCpuTime > targetFrametime)
			currentFps /= fmod(averageCpuTime, targetFrametime) / targetFrametime + 1;

		// Double the target frametime if the user wants to,
		// or if CPU Frametime is double the target frametime,
		// or if preferReprojection is true and CPU Frametime is greated than targetFrametime.
		if ((((averageCpuTime > targetFrametime && preferReprojection) || averageCpuTime / 2 > targetFrametime) && !ignoreCpuTime) || alwaysReproject)
		{
			targetFrametime *= 2;
		}

		// Get VRAM usage
		float vramUsage = getVramUsage(nvmlLibrary);

		// Resolution handling
		if (currentTime - resChangeDelayMs > lastChangeTime)
		{
			lastChangeTime = currentTime;

			// Check if the SteamVR dashboard is open
			bool inDashboard = vr::VROverlay()->IsDashboardVisible();
			// Check that we're in a supported application
			std::string appKey = getCurrentApplicationKey();
			bool isCurrentAppDisabled = disabledApps.find(appKey) != disabledApps.end() || appKey == "";
			// Only adjust resolution if not in dashboard and in a supported application
			adjustResolution = !inDashboard && !isCurrentAppDisabled;

			if (adjustResolution)
			{
				// Adjust resolution
				if ((averageCpuTime > minCpuTimeThreshold || vramOnlyMode))
				{
					// Frametime
					if (averageGpuTime < targetFrametime * resIncreaseThreshold && vramUsage < vramTarget && !vramOnlyMode)
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
					if (vramUsage > vramLimit)
					{
						// Force the resolution to decrease when the vram limit is reached
						newRes -= resDecreaseMin;
					}
					else if (vramOnlyMode && newRes < initialRes && vramUsage < vramTarget)
					{
						// When in VRAM-only mode, make sure the res goes back up when possible.
						newRes = min(initialRes, newRes + resIncreaseMin);
					}

					// Clamp the new resolution
					newRes = std::clamp(newRes, minRes, maxRes);
				}
				else if (resetOnThreshold && lastRes != initialRes && averageCpuTime < minCpuTimeThreshold && !vramOnlyMode)
				{
					// Reset to initialRes because CPU time fell below the threshold
					newRes = initialRes;
				}
			}
			else if (isCurrentAppDisabled)
			{
				// We've switched into an unsupported application, let's reset
				newRes = initialRes;
			}

			if (newRes != lastRes)
			{
				// Sets the new resolution
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, newRes);
			}
		}

		glfwPollEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Create the main window
		ImGui::Begin("", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		// Set position and size to fill the main window
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

		// Settings status
		if (settingsLoaded)
			ImGui::Text(fmt::format("settings.ini successfully loaded").c_str());
		else
			ImGui::Text(fmt::format("Error loading settings.ini (default values used)").c_str());

		ImGui::NewLine();

		// HMD Hz
		ImGui::Text(fmt::format("HMD Hz: {} fps", std::to_string(int(targetFps))).c_str());

		// Target frametime
		if (!vramOnlyMode)
		{
			ImGui::Text(fmt::format("HMD Hz target frametime: {} ms", std::to_string(realTargetFrametime).substr(0, 4)).c_str());
			ImGui::Text(fmt::format("Adjusted target frametime: {} ms", std::to_string(targetFrametime).substr(0, 4)).c_str());
		}
		else
		{
			ImGui::Text("Adjusted target frametime: Disabled");
			ImGui::Text("HMD Hz target frametime: Disabled");
		}

		// VRAM target and limit
		if (vramMonitorEnabled)
		{
			ImGui::Text(fmt::format("VRAM target: {}%", std::to_string(vramTarget * 100).substr(0, 4)).c_str());
			ImGui::Text(fmt::format("VRAM limit: {}%", std::to_string(vramLimit * 100).substr(0, 4)).c_str());
		}
		else
		{
			ImGui::Text(fmt::format("VRAM target: Disabled").c_str());
			ImGui::Text(fmt::format("VRAM limit: Disabled").c_str());
		}

		ImGui::NewLine();

		// FPS and frametimes
		ImGui::Text(fmt::format("FPS: {} fps", std::to_string(currentFps)).c_str());
		ImGui::Text(fmt::format("GPU frametime: {} ms", std::to_string(averageGpuTime).substr(0, 4)).c_str());
		ImGui::Text(fmt::format("CPU frametime: {} ms", std::to_string(averageCpuTime).substr(0, 4)).c_str());
		ImGui::Text(fmt::format("Raw CPU frametime: {} ms", std::to_string(realCpuTime).substr(0, 4)).c_str());

		// VRAM usage
		if (vramMonitorEnabled)
			ImGui::Text(fmt::format("VRAM usage: {}%", std::to_string(vramUsage * 100).substr(0, 4)).c_str());
		else
			ImGui::Text(fmt::format("VRAM usage: Disabled").c_str());

		ImGui::NewLine();

		// Reprojecting status
		if (frameShown > 1)
		{
			std::string reason = fmt::format("Other [{}]", reprojectionFlag).c_str();
			if (reprojectionFlag == 20)
				reason = "CPU";
			else if (reprojectionFlag == 276)
				reason = "GPU";

			ImGui::Text(fmt::format("Reprojecting: Yes ({}x, {})", frameShown, reason).c_str());
		}
		else
		{
			ImGui::Text("Reprojecting: No");
		}

		ImGui::NewLine();

		// Current resolution
		ImGui::Text(fmt::format("Resolution = {}%", std::to_string(int(newRes * 100))).c_str());

		// Resolution adjustment status
		if (!adjustResolution)
		{
			ImGui::Text("Resolution adjustment paused");
		}

		// Stop creating the main window
		ImGui::End();

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		// Calculate how long to sleep for
		long sleepTime = dataPullDelayMs;
		if (dataPullDelayMs < currentTime - lastChangeTime && resChangeDelayMs < currentTime - lastChangeTime)
		{
			sleepTime = (currentTime - lastChangeTime) - resChangeDelayMs;
		}
		else if (resChangeDelayMs < dataPullDelayMs)
		{
			sleepTime = resChangeDelayMs;
		}

		// ZZzzzz
		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	}

	// TODO actually be able to get out of the while loop !!!

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

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

// Custom WinMain method to not have the terminal
#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	main(NULL, NULL);
}
#endif
