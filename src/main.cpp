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

// Loading png
#include "lodepng.h"

using namespace std::chrono_literals;
using namespace vr;

static constexpr const char *version = "v1.0.0";

static constexpr const char *iconPath = "icon.png";

static constexpr const std::chrono::milliseconds refreshIntervalBackground = 167ms; // 6fps
static constexpr const std::chrono::milliseconds refreshIntervalFocused = 33ms;		// 30fps

static constexpr const int mainWindowWidth = 350;
static constexpr const int mainWindowHeight = 300;

static constexpr const float bitsToGB = 1073741824;

#pragma region Config
#pragma region Default settings
// Initialization
int autoStart = 1;
int minimizeOnStart = 0;

// Resolution
float initialRes = 1.0f;
float minRes = 0.75;
float maxRes = 5.0f;
long resChangeDelayMs = 1800;
float minCpuTimeThreshold = 1.0f;
float resIncreaseMin = 0.03f;
float resDecreaseMin = 0.09f;
float resIncreaseScale = 0.60f;
float resDecreaseScale = 0.90;
float resIncreaseThreshold = 0.80;
float resDecreaseThreshold = 0.85f;
int dataAverageSamples = 100;
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

const std::string mergeConfigValue(std::set<std::string> &valSet)
{
	std::string result = "";

	for (std::string val : valSet)
	{
		result += (val + " ");
	}

	return result;
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

	initialRes = std::stof(ini.GetValue("Resolution", "initialRes", std::to_string(initialRes * 100.0f).c_str())) / 100.0f;
	minRes = std::stof(ini.GetValue("Resolution", "minRes", std::to_string(minRes * 100.0f).c_str())) / 100.0f;
	maxRes = std::stof(ini.GetValue("Resolution", "maxRes", std::to_string(maxRes * 100.0f).c_str())) / 100.0f;
	resChangeDelayMs = std::stol(ini.GetValue("Resolution", "resChangeDelayMs", std::to_string(resChangeDelayMs).c_str()));
	minCpuTimeThreshold = std::stof(ini.GetValue("Resolution", "minCpuTimeThreshold", std::to_string(minCpuTimeThreshold).c_str()));
	resIncreaseMin = std::stof(ini.GetValue("Resolution", "resIncreaseMin", std::to_string(resIncreaseMin * 100.0f).c_str())) / 100.0f;
	resDecreaseMin = std::stof(ini.GetValue("Resolution", "resDecreaseMin", std::to_string(resDecreaseMin * 100.0f).c_str())) / 100.0f;
	resIncreaseScale = std::stof(ini.GetValue("Resolution", "resIncreaseScale", std::to_string(resIncreaseScale * 100.0f).c_str())) / 100.0f;
	resDecreaseScale = std::stof(ini.GetValue("Resolution", "resDecreaseScale", std::to_string(resDecreaseScale * 100.0f).c_str())) / 100.0f;
	resIncreaseThreshold = std::stof(ini.GetValue("Resolution", "resIncreaseThreshold", std::to_string(resIncreaseThreshold * 100.0f).c_str())) / 100.0f;
	resDecreaseThreshold = std::stof(ini.GetValue("Resolution", "resDecreaseThreshold", std::to_string(resDecreaseThreshold * 100.0f).c_str())) / 100.0f;
	dataAverageSamples = std::stoi(ini.GetValue("Resolution", "dataAverageSamples", std::to_string(dataAverageSamples).c_str()));
	if (dataAverageSamples > 128)
		dataAverageSamples = 128; // Max stored by OpenVR
	resetOnThreshold = std::stoi(ini.GetValue("Resolution", "resetOnThreshold", std::to_string(resetOnThreshold).c_str()));
	alwaysReproject = std::stoi(ini.GetValue("Resolution", "alwaysReproject", std::to_string(alwaysReproject).c_str()));
	vramLimit = std::stoi(ini.GetValue("Resolution", "vramLimit", std::to_string(vramLimit * 100.0f).c_str())) / 100.0f;
	vramTarget = std::stoi(ini.GetValue("Resolution", "vramTarget", std::to_string(vramTarget * 100.0f).c_str())) / 100.0f;
	vramMonitorEnabled = std::stoi(ini.GetValue("Resolution", "vramMonitorEnabled", std::to_string(vramMonitorEnabled).c_str()));
	vramOnlyMode = std::stoi(ini.GetValue("Resolution", "vramOnlyMode", std::to_string(vramOnlyMode).c_str()));
	preferReprojection = std::stoi(ini.GetValue("Resolution", "preferReprojection", std::to_string(preferReprojection).c_str()));
	ignoreCpuTime = std::stoi(ini.GetValue("Resolution", "ignoreCpuTime", std::to_string(ignoreCpuTime).c_str()));
	disabledApps = splitConfigValue(ini.GetValue("Resolution", "disabledApps", mergeConfigValue(disabledApps).c_str()));

	return true;
}

void saveSettings()
{
	// Get ini file
	CSimpleIniA ini;

	// Set setting values
	ini.SetValue("Initialization", "autoStart", std::to_string(autoStart).c_str());
	ini.SetValue("Initialization", "minimizeOnStart", std::to_string(minimizeOnStart).c_str());

	ini.SetValue("Resolution", "initialRes", std::to_string(initialRes * 100.0f).c_str());
	ini.SetValue("Resolution", "minRes", std::to_string(minRes * 100.0f).c_str());
	ini.SetValue("Resolution", "maxRes", std::to_string(maxRes * 100.0f).c_str());
	ini.SetValue("Resolution", "resChangeDelayMs", std::to_string(resChangeDelayMs).c_str());
	ini.SetValue("Resolution", "minCpuTimeThreshold", std::to_string(minCpuTimeThreshold).c_str());
	ini.SetValue("Resolution", "resIncreaseMin", std::to_string(resIncreaseMin * 100.0f).c_str());
	ini.SetValue("Resolution", "resDecreaseMin", std::to_string(resDecreaseMin * 100.0f).c_str());
	ini.SetValue("Resolution", "resIncreaseScale", std::to_string(resIncreaseScale * 100.0f).c_str());
	ini.SetValue("Resolution", "resDecreaseScale", std::to_string(resDecreaseScale * 100.0f).c_str());
	ini.SetValue("Resolution", "resIncreaseThreshold", std::to_string(resIncreaseThreshold * 100.0f).c_str());
	ini.SetValue("Resolution", "resDecreaseThreshold", std::to_string(resDecreaseThreshold * 100.0f).c_str());
	ini.SetValue("Resolution", "dataAverageSamples", std::to_string(dataAverageSamples).c_str());
	ini.SetValue("Resolution", "resetOnThreshold", std::to_string(resetOnThreshold).c_str());
	ini.SetValue("Resolution", "alwaysReproject", std::to_string(alwaysReproject).c_str());
	ini.SetValue("Resolution", "vramLimit", std::to_string(vramLimit * 100.0f).c_str());
	ini.SetValue("Resolution", "vramTarget", std::to_string(vramTarget * 100.0f).c_str());
	ini.SetValue("Resolution", "vramMonitorEnabled", std::to_string(vramMonitorEnabled).c_str());
	ini.SetValue("Resolution", "vramOnlyMode", std::to_string(vramOnlyMode).c_str());
	ini.SetValue("Resolution", "preferReprojection", std::to_string(preferReprojection).c_str());
	ini.SetValue("Resolution", "ignoreCpuTime", std::to_string(ignoreCpuTime).c_str());
	ini.SetValue("Resolution", "disabledApps", mergeConfigValue(disabledApps).c_str());

	// Save changes to disk
	ini.SaveFile("settings.ini");
}
#pragma endregion

#pragma region NVML
bool nvmlEnabled = true;

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

void printLine(GLFWwindow *window, std::string text, long duration)
{
	long startTime = getCurrentTimeMillis();

	while (getCurrentTimeMillis() < startTime + duration && !glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Create the main window
		ImGui::Begin("", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

		// Set position and size to fill the main window
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

		// Display the line
		ImGui::TextWrapped(text.c_str());

		// Stop creating the main window
		ImGui::End();

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		// Sleep to display the text for a set duration
		std::this_thread::sleep_for(refreshIntervalBackground);
	}
}

int main(int argc, char *argv[])
{
#pragma region GUI init
	if (!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Make window non-resizable
	glfwWindowHint(GLFW_RESIZABLE, false);

	// Create window with graphics context
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
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03, 0.03, 0.03, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12, 0.12, 0.12, 12));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25, 0.25, 0.25, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.17, 0.17, 0.17, 1));

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Set window icon
	GLFWimage icon;
	unsigned iconWidth, iconHeight;
	unsigned test = lodepng_decode32_file(&(icon.pixels), &(iconWidth), &(iconHeight), iconPath);
	icon.width = (int)iconWidth;
	icon.height = (int)iconHeight;
	glfwSetWindowIcon(window, 1, &icon);

	// TODO Add tray icon
	// https://github.com/Soundux/traypp

#pragma endregion

#pragma region VR init check
	EVRInitError init_error = VRInitError_None;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(
		VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error != VRInitError_None)
	{
		system = nullptr;
		printLine(window, VR_GetVRInitErrorAsEnglishDescription(init_error), 6000l);
		return EXIT_FAILURE;
	}
	if (!VRCompositor())
	{
		printLine(window, "Failed to initialize VR compositor.", 6000l);
		return EXIT_FAILURE;
	}
#pragma endregion

	// Load settings from ini file
	if (!loadSettings())
	{
		saveSettings();
		printLine(window, "Error loading settings.ini, restoring defaults", 4100l);
	}

	// Set auto-start
	int autoStartResult = handle_setup(autoStart);

	if (autoStartResult == 1)
	{
		printLine(window, "Enabled auto-start", 2800l);
	}
	else if (autoStartResult == 2)
	{
		printLine(window, "Disabled auto-start", 2800l);
	}
	else if (autoStartResult != 0)
	{
		printLine(window, fmt::format("Error toggling auto-start ({}) ", std::to_string(autoStartResult)), 6000l);
	}

	// Minimize or hide the window according to config
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

#pragma region Initialise NVML
	HMODULE nvmlLibrary;
	nvmlDevice_t nvmlDevice;
	nvmlMemory_t nvmlMemory;
	float vramTotalGB;

	if (vramMonitorEnabled)
	{
		nvmlInit_t nvmlInitPtr;
#ifdef _WIN32
		nvmlLibrary = LoadLibraryA("nvml.dll");
		nvmlInitPtr = (nvmlInit_t)GetProcAddress(nvmlLibrary, "nvmlInit");
#else
		void *nvmlLibrary = dlopen("libnvidia-ml.so", RTLD_LAZY);
		nvmlInitPtr = (nvmlInit_t)dlsym(nvmlLibrary, "nvmlInit");
#endif
		if (!nvmlInitPtr)
			nvmlEnabled = false;

		if (nvmlEnabled)
		{
			nvmlReturn_t result;
			// Initialize NVML library
			result = nvmlInitPtr();
			if (result != NVML_SUCCESS)
				nvmlEnabled = false;

			// Get device handle
			nvmlDeviceGetHandleByIndex_t nvmlDeviceGetHandleByIndexPtr;
#ifdef _WIN32
			nvmlDeviceGetHandleByIndexPtr = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(nvmlLibrary, "nvmlDeviceGetHandleByIndex");
#else
			nvmlDeviceGetHandleByIndexPtr = (nvmlDeviceGetHandleByIndex_t)dlsym(nvmlLibrary, "nvmlDeviceGetHandleByIndex");
#endif
			if (!nvmlDeviceGetHandleByIndexPtr)
				nvmlEnabled = false;
			else
				result = nvmlDeviceGetHandleByIndexPtr(0, &nvmlDevice);

			if (result != NVML_SUCCESS || !nvmlEnabled)
			{
				// Shutdown NVML
				nvmlShutdown_t nvmlShutdownPtr;
#ifdef _WIN32
				nvmlShutdownPtr = (nvmlShutdown_t)GetProcAddress(nvmlLibrary, "nvmlShutdown");
#else
				nvmlShutdownPtr = (nvmlShutdown_t)dlsym(nvmlLibrary, "nvmlShutdown");
#endif
				if (nvmlShutdownPtr)
					nvmlShutdownPtr();
			}
			else
			{
				// Get memory info
				nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfoPtr;
#ifdef _WIN32
				nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)GetProcAddress(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#else
				nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)dlsym(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#endif
				if (nvmlDeviceGetMemoryInfoPtr(nvmlDevice, &nvmlMemory) != NVML_SUCCESS)
					nvmlEnabled = false;
				else
					vramTotalGB = (float)nvmlMemory.total / bitsToGB;
			}
		}
	}
	else
	{
		nvmlEnabled = false;
	}
#pragma endregion

	// Initialize loop variables
	auto *frameTiming = new vr::Compositor_FrameTiming;
	long lastChangeTime = getCurrentTimeMillis();
	bool adjustResolution = true;
	bool openvrQuit = false;

	float averageGpuTime = 0;
	float averageCpuTime = 0;
	int averageFrameShown = 0;
	float newRes = 0;
	float targetFps = 0;
	float targetFrametime = 0;
	float hmdHz = 0;
	float hmdFrametime = 0;
	int currentFps = 0;
	float vramUsedGB = 0;

	bool showSettings = false;

	// event loop
	while (!glfwWindowShouldClose(window) && !openvrQuit)
	{
		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Doesn't run every loop
		if (currentTime - resChangeDelayMs > lastChangeTime)
		{
			lastChangeTime = currentTime;

#pragma region Getting data
			// Fetch resolution and target fps
			newRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float);
			float lastRes = newRes;
			targetFps = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
			targetFrametime = 1000 / targetFps;
			hmdHz = targetFps;
			hmdFrametime = targetFrametime;

			// Define totals
			float totalGpuTime = 0;
			float totalCpuTime = 0;
			int frameShownTotal = 0;

			// Loop through past frames
			frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
			for (int i = 0; i < dataAverageSamples; i++)
			{
				// Get current frame
				vr::VRCompositor()->GetFrameTiming(frameTiming, i);

				// Get GPU frametime
				float gpuTime = frameTiming->m_flTotalRenderGpuMs;

				// Calculate CPU frametime
				// https://github.com/Louka3000/OpenVR-Dynamic-Resolution/issues/18#issuecomment-1833105172
				float cpuTime = frameTiming->m_flCompositorRenderCpuMs									 // Compositor
								+ (frameTiming->m_flNewFrameReadyMs - frameTiming->m_flNewPosesReadyMs); // Application & Late Start

				// How many times the current frame repeated (>1 = reprojecting)
				uint32_t frameShown = frameTiming->m_nNumFramePresents;
				// Reason reprojection is happening
				uint32_t reprojectionFlag = frameTiming->m_nReprojectionFlags;

				// Calculate the adjusted CPU time off reprojection.
				cpuTime *= min(frameShown, floor(gpuTime / targetFrametime) + 1);

				// Add to totals
				totalGpuTime += gpuTime;
				totalCpuTime += cpuTime;
				frameShownTotal += frameShown;
			}

			// Calculate averages
			averageGpuTime = totalGpuTime / dataAverageSamples;
			averageCpuTime = totalCpuTime / dataAverageSamples;
			averageFrameShown = frameShownTotal / dataAverageSamples;

			// Estimated current FPS
			currentFps = targetFps / averageFrameShown;
			if (averageCpuTime > targetFrametime)
				currentFps /= fmod(averageCpuTime, targetFrametime) / targetFrametime + 1;

			// Double the target frametime if the user wants to,
			// or if CPU Frametime is double the target frametime,
			// or if preferReprojection is true and CPU Frametime is greated than targetFrametime.
			if ((((averageCpuTime > targetFrametime && preferReprojection) ||
				  averageCpuTime / 2 > targetFrametime) &&
				 !ignoreCpuTime) ||
				alwaysReproject)
			{
				targetFps /= 2;
				targetFrametime *= 2;
			}

			// Get VRAM usage
			vramUsedGB = (float)nvmlMemory.used / bitsToGB;
			float vramUsed;
			if (nvmlEnabled) // Get the VRAM used in %
				vramUsed = (float)nvmlMemory.used / (float)nvmlMemory.total;
			else // Assume we always have free VRAM
				vramUsed = 0;

			// Check if the SteamVR dashboard is open
			bool inDashboard = vr::VROverlay()->IsDashboardVisible();
			// Check that we're in a supported application
			std::string appKey = getCurrentApplicationKey();
			bool isCurrentAppDisabled = disabledApps.find(appKey) != disabledApps.end() || appKey == "";
#pragma endregion

#pragma region Resolution adjustment
			// Only adjust resolution if not in dashboard and in a supported application
			adjustResolution = !inDashboard && !isCurrentAppDisabled;
			if (adjustResolution)
			{
				// Adjust resolution
				if ((averageCpuTime > minCpuTimeThreshold || vramOnlyMode))
				{
					// Frametime
					if (averageGpuTime < targetFrametime * resIncreaseThreshold && vramUsed < vramTarget && !vramOnlyMode)
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
					if (vramUsed > vramLimit)
					{
						// Force the resolution to decrease when the vram limit is reached
						newRes -= resDecreaseMin;
					}
					else if (vramOnlyMode && newRes < initialRes && vramUsed < vramTarget)
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
#pragma endregion

#pragma region Gui rendering
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

#pragma region Main window
		{
			// Create the main window
			ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

			// Set position and size to fill the viewport
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

			// HMD Hz
			ImGui::Text(fmt::format("HMD refresh rate: {} hz ({} ms)", std::to_string(int(hmdHz)), std::to_string(hmdFrametime).substr(0, 4)).c_str());

			// Target FPS and frametime
			if (!vramOnlyMode)
			{
				ImGui::Text(fmt::format("Target FPS: {} fps ({} ms)", std::to_string(int(targetFps)), std::to_string(targetFrametime).substr(0, 4)).c_str());
			}
			else
			{
				ImGui::Text("Target FPS: Disabled");
			}

			// VRAM target and limit
			if (nvmlEnabled && nvmlEnabled)
			{
				ImGui::Text(fmt::format("VRAM target: {} GB", std::to_string(vramTarget * vramTotalGB).substr(0, 4)).c_str());
				ImGui::Text(fmt::format("VRAM limit: {} GB ", std::to_string(vramLimit * vramTotalGB).substr(0, 4)).c_str());
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

			// VRAM usage
			if (nvmlEnabled)
				ImGui::Text(fmt::format("VRAM usage: {} GB", std::to_string(vramUsedGB).substr(0, 4)).c_str());
			else
				ImGui::Text(fmt::format("VRAM usage: Disabled").c_str());

			ImGui::NewLine();

			// Reprojecting status
			if (averageFrameShown > 1)
			{
				std::string reason;
				if (averageGpuTime > averageCpuTime)
					reason = "GPU";
				else
					reason = "CPU";

				ImGui::Text(fmt::format("Reprojecting: Yes ({}x, {})", averageFrameShown, reason).c_str());
			}
			else
			{
				ImGui::Text("Reprojecting: No");
			}

			// Current resolution
			ImGui::Text(fmt::format("Resolution = {}", std::to_string(int(newRes * 100))).c_str());
			// Resolution adjustment status
			if (!adjustResolution)
			{
				ImGui::SameLine(0, 10);
				ImGui::Text("(adjustment paused)");
			}

			ImGui::NewLine();

			showSettings = ImGui::Button("Settings", ImVec2(82, 28));
			// TODO show settings panel

			// Stop creating the main window
			ImGui::End();
		}
#pragma endregion

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
#pragma endregion

		// Check if OpenVR is quitting so we can quit alongside it
		VREvent_t vrEvent;
		while (vr::VRSystem()->PollNextEvent(&vrEvent, sizeof(vr::VREvent_t)))
		{
			if (vrEvent.eventType == vr::VREvent_Quit)
			{
				vr::VRSystem()->AcknowledgeQuit_Exiting();
				openvrQuit = true;
				break;
			}
		}

		// Calculate how long to sleep for depending on if the window is focused or not.
		std::chrono::milliseconds sleepTime;
		if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
			sleepTime = refreshIntervalFocused;
		else 
			sleepTime = refreshIntervalBackground;

		// ZZzzzz
		std::this_thread::sleep_for(sleepTime);
	}

#pragma region Cleanup
	// OpenVR cleanup
	vr::VR_Shutdown();

	// NVML cleanup
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

	// GUI cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();

#pragma endregion

	return 0;
}

// Custom WinMain method to not have the terminal
#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	main(NULL, NULL);
}
#endif
