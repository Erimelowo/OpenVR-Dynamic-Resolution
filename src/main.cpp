#include <chrono>
#include <set>
#include <sstream>
#include <thread>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

// OpenVR to interact with VR
#include <openvr.h>

// fmt for text formatting
#include <fmt/core.h>

// To include the nvml library at runtime
#ifdef _WIN32
#define NOMINMAX
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

// Tray icon
#include "tray.h"

#ifndef WIN32
#define HMODULE void *
#endif

#pragma region Modify InputText so we can use std::string
namespace ImGui
{
	// ImGui::InputText() with std::string
	// Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
	IMGUI_API bool InputText(const char *label, std::string *str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
	IMGUI_API bool InputTextMultiline(const char *label, std::string *str, const ImVec2 &size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
	IMGUI_API bool InputTextWithHint(const char *label, const char *hint, std::string *str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
}
#pragma endregion

using namespace std::chrono_literals;
using namespace vr;

static constexpr const char *version = "v1.3.1";

static constexpr const char *iconPath = "icon.png";

static constexpr const std::chrono::milliseconds refreshIntervalBackground = 167ms; // 6fps
static constexpr const std::chrono::milliseconds refreshIntervalFocused = 33ms;		// 30fps

static constexpr const int mainWindowWidth = 350;
static constexpr const int mainWindowHeight = 304;

static constexpr const float bitsToGB = 1073741824;

static constexpr const int openvrMaxFrames = 128;

GLFWwindow *glfwWindow;

bool trayQuit = false;

#pragma region Config
#pragma region Default settings
// Initialization
bool autoStart = true;
int minimizeOnStart = 1;
// General
bool closeToTray = false;
bool externalResChangeCompatibility = true;
std::string blacklistApps = "steam.app.620980 steam.app.658920 steam.app.2177750 steam.app.2177760";
std::set<std::string> blacklistAppsSet = {"steam.app.620980", "steam.app.658920", "steam.app.2177750", "steam.app.2177760"};
bool whitelistEnabled = false;
std::string whitelistApps = "";
std::set<std::string> whitelistAppsSet = {};
// Resolution
int resChangeDelayMs = 6000;
int initialRes = 100;
int minRes = 70;
int maxRes = 190;
float resIncreaseThreshold = 79;
float resDecreaseThreshold = 89;
int resIncreaseMin = 2;
int resDecreaseMin = 3;
int resIncreaseScale = 200;
int resDecreaseScale = 200;
float minCpuTimeThreshold = 0.6f;
bool resetOnThreshold = true;
// Reprojection
bool alwaysReproject = false;
bool preferReprojection = false;
bool ignoreCpuTime = false;
// VRAM
int vramTarget = 80;
int vramLimit = 90;
bool vramMonitorEnabled = true;
bool vramOnlyMode = false;
int gpuIndex = 0;
#pragma endregion

/// Newline-delimited string to a set
std::set<std::string> multilineStringToSet(const std::string &val)
{
	std::set<std::string> set;
	std::stringstream ss(val);
	std::string word;

	for (std::string line; std::getline(ss, line, '\n');)
		set.insert(line);

	return set;
}

/// Set to a space-delimited string
std::string setToConfigString(std::set<std::string> &valSet)
{
	std::string result;

	for (std::string val : valSet)
	{
		result += (val + " ");
	}
	if (!result.empty())
	{
		// remove trailing space
		result.pop_back();
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

	try
	{
		// Startup
		autoStart = std::stoi(ini.GetValue("Startup", "autoStart", std::to_string(autoStart).c_str()));
		minimizeOnStart = std::stoi(ini.GetValue("Startup", "minimizeOnStart", std::to_string(minimizeOnStart).c_str()));

		// General
		closeToTray = std::stoi(ini.GetValue("General", "closeToTray", std::to_string(closeToTray).c_str()));
		externalResChangeCompatibility = std::stoi(ini.GetValue("General", "externalResChangeCompatibility", std::to_string(externalResChangeCompatibility).c_str()));
		// blacklist
		blacklistApps = ini.GetValue("General", "disabledApps", blacklistApps.c_str());
		std::replace(blacklistApps.begin(), blacklistApps.end(), ' ', '\n');
		blacklistAppsSet = multilineStringToSet(blacklistApps);
		// whitelist
		whitelistEnabled = std::stoi(ini.GetValue("General", "whitelistEnabled", std::to_string(whitelistEnabled).c_str()));
		whitelistApps = ini.GetValue("General", "whitelistApps", blacklistApps.c_str());
		std::replace(whitelistApps.begin(), whitelistApps.end(), ' ', '\n');
		whitelistAppsSet = multilineStringToSet(whitelistApps);

		// Resolution
		resChangeDelayMs = std::stoi(ini.GetValue("General", "resChangeDelayMs", std::to_string(resChangeDelayMs).c_str()));
		initialRes = std::stoi(ini.GetValue("Resolution", "initialRes", std::to_string(initialRes).c_str()));
		minRes = std::stoi(ini.GetValue("Resolution", "minRes", std::to_string(minRes).c_str()));
		maxRes = std::stoi(ini.GetValue("Resolution", "maxRes", std::to_string(maxRes).c_str()));
		resIncreaseThreshold = std::stof(ini.GetValue("Resolution", "resIncreaseThreshold", std::to_string(resIncreaseThreshold).c_str()));
		resDecreaseThreshold = std::stof(ini.GetValue("Resolution", "resDecreaseThreshold", std::to_string(resDecreaseThreshold).c_str()));
		resIncreaseMin = std::stoi(ini.GetValue("Resolution", "resIncreaseMin", std::to_string(resIncreaseMin).c_str()));
		resDecreaseMin = std::stoi(ini.GetValue("Resolution", "resDecreaseMin", std::to_string(resDecreaseMin).c_str()));
		resIncreaseScale = std::stoi(ini.GetValue("Resolution", "resIncreaseScale", std::to_string(resIncreaseScale).c_str()));
		resDecreaseScale = std::stoi(ini.GetValue("Resolution", "resDecreaseScale", std::to_string(resDecreaseScale).c_str()));
		minCpuTimeThreshold = std::stof(ini.GetValue("Resolution", "minCpuTimeThreshold", std::to_string(minCpuTimeThreshold).c_str()));
		resetOnThreshold = std::stoi(ini.GetValue("Resolution", "resetOnThreshold", std::to_string(resetOnThreshold).c_str()));

		// Reprojection
		alwaysReproject = std::stoi(ini.GetValue("Reprojection", "alwaysReproject", std::to_string(alwaysReproject).c_str()));
		preferReprojection = std::stoi(ini.GetValue("Reprojection", "preferReprojection", std::to_string(preferReprojection).c_str()));
		ignoreCpuTime = std::stoi(ini.GetValue("Reprojection", "ignoreCpuTime", std::to_string(ignoreCpuTime).c_str()));

		// VRAM
		vramMonitorEnabled = std::stoi(ini.GetValue("VRAM", "vramMonitorEnabled", std::to_string(vramMonitorEnabled).c_str()));
		vramOnlyMode = std::stoi(ini.GetValue("VRAM", "vramOnlyMode", std::to_string(vramOnlyMode).c_str()));
		vramTarget = std::stoi(ini.GetValue("VRAM", "vramTarget", std::to_string(vramTarget).c_str()));
		vramLimit = std::stoi(ini.GetValue("VRAM", "vramLimit", std::to_string(vramLimit).c_str()));
		gpuIndex = std::stoi(ini.GetValue("VRAM", "gpuIndex", std::to_string(gpuIndex).c_str()));

		return true;
	}
	catch (...)
	{
		return false;
	}
}

void saveSettings()
{
	// Get ini file
	CSimpleIniA ini;

	// Startup
	ini.SetValue("Startup", "autoStart", std::to_string(autoStart).c_str());
	ini.SetValue("Startup", "minimizeOnStart", std::to_string(minimizeOnStart).c_str());

	// General
	ini.SetValue("General", "closeToTray", std::to_string(closeToTray).c_str());
	ini.SetValue("General", "externalResChangeCompatibility", std::to_string(externalResChangeCompatibility).c_str());
	ini.SetValue("General", "disabledApps", setToConfigString(blacklistAppsSet).c_str());
	ini.SetValue("General", "whitelistEnabled", std::to_string(whitelistEnabled).c_str());
	ini.SetValue("General", "whitelistApps", setToConfigString(whitelistAppsSet).c_str());

	// Resolution
	ini.SetValue("General", "resChangeDelayMs", std::to_string(resChangeDelayMs).c_str());
	ini.SetValue("Resolution", "initialRes", std::to_string(initialRes).c_str());
	ini.SetValue("Resolution", "minRes", std::to_string(minRes).c_str());
	ini.SetValue("Resolution", "maxRes", std::to_string(maxRes).c_str());
	ini.SetValue("Resolution", "resIncreaseThreshold", std::to_string(resIncreaseThreshold).c_str());
	ini.SetValue("Resolution", "resDecreaseThreshold", std::to_string(resDecreaseThreshold).c_str());
	ini.SetValue("Resolution", "resIncreaseMin", std::to_string(resIncreaseMin).c_str());
	ini.SetValue("Resolution", "resDecreaseMin", std::to_string(resDecreaseMin).c_str());
	ini.SetValue("Resolution", "resIncreaseScale", std::to_string(resIncreaseScale).c_str());
	ini.SetValue("Resolution", "resDecreaseScale", std::to_string(resDecreaseScale).c_str());
	ini.SetValue("Resolution", "minCpuTimeThreshold", std::to_string(minCpuTimeThreshold).c_str());
	ini.SetValue("Resolution", "resetOnThreshold", std::to_string(resetOnThreshold).c_str());

	// Reprojection
	ini.SetValue("Reprojection", "alwaysReproject", std::to_string(alwaysReproject).c_str());
	ini.SetValue("Reprojection", "preferReprojection", std::to_string(preferReprojection).c_str());
	ini.SetValue("Reprojection", "ignoreCpuTime", std::to_string(ignoreCpuTime).c_str());

	// VRAM
	ini.SetValue("VRAM", "vramMonitorEnabled", std::to_string(vramMonitorEnabled).c_str());
	ini.SetValue("VRAM", "vramOnlyMode", std::to_string(vramOnlyMode).c_str());
	ini.SetValue("VRAM", "vramTarget", std::to_string(vramTarget).c_str());
	ini.SetValue("VRAM", "vramLimit", std::to_string(vramLimit).c_str());
	ini.SetValue("VRAM", "gpuIndex", std::to_string(gpuIndex).c_str());

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

void pushGrayButtonColour()
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12, 0.12, 0.12, 12));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25, 0.25, 0.25, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.17, 0.17, 0.17, 1));
}

void pushRedButtonColour()
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62, 0.12, 0.12, 12));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.73, 0.25, 0.25, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.68, 0.17, 0.17, 1));
}

void pushGreenButtonColour()
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12, 0.62, 0.12, 12));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25, 0.73, 0.25, 1));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.17, 0.68, 0.17, 1));
}

/**
 * Returns the current VR application key (steam.app.000000)
 * or an empty string if no app is running.
 */
std::string getCurrentApplicationKey()
{
	char applicationKey[vr::k_unMaxApplicationKeyLength];

	uint32_t processId = vr::VRApplications()->GetCurrentSceneProcessId();
	if (!processId)
		return "";

	EVRApplicationError err = vr::VRApplications()->GetApplicationKeyByProcessId(processId, applicationKey, vr::k_unMaxApplicationKeyLength);
	if (err)
		return "";

	return {applicationKey};
}

bool isApplicationBlacklisted(std::string appKey)
{
	return appKey == "" || blacklistAppsSet.find(appKey) != blacklistAppsSet.end();
}

bool isApplicationWhitelisted(std::string appKey)
{
	return appKey != "" && whitelistAppsSet.find(appKey) != whitelistAppsSet.end();
}

bool shouldAdjustResolution(std::string appKey, bool manualRes, float cpuTime)
{
	// Check if the SteamVR dashboard is open
	bool inDashboard = vr::VROverlay()->IsDashboardVisible();
	// Check that we're in a supported application
	bool isCurrentAppSupported = !isApplicationBlacklisted(appKey) && (!whitelistEnabled || isApplicationWhitelisted(appKey));
	// Only adjust resolution if not in dashboard, in a supported application. user didn't pause res and cpu time isn't below threshold
	return !inDashboard && isCurrentAppSupported && !manualRes && !(resetOnThreshold && cpuTime < minCpuTimeThreshold);
}

void printLine(std::string text, long duration)
{
	long startTime = getCurrentTimeMillis();

	while (getCurrentTimeMillis() < startTime + duration && !glfwWindowShouldClose(glfwWindow))
	{
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Create the Print window
		ImGui::Begin("printLine", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

		// Set position and size to fill the main window
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

		// Display the line
		ImGui::TextWrapped("%s", text.c_str());

		// Stop creating the main window
		ImGui::End();

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(glfwWindow);

		// Sleep to display the text for a set duration
		std::this_thread::sleep_for(refreshIntervalBackground);
	}
}

void cleanup(nvmlLib nvmlLibrary)
{
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
	if (nvmlEnabled)
	{
		nvmlShutdown_t nvmlShutdownPtr = (nvmlShutdown_t)dlsym(nvmlLibrary, "nvmlShutdown");
		if (nvmlShutdownPtr)
		{
			nvmlShutdownPtr();
		}
		dlclose(nvmlLibrary);
	}
#endif

	// GUI cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(glfwWindow);
	glfwTerminate();
}

void addTooltip(const char *text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(mainWindowWidth - 4);
		ImGui::TextWrapped("%s", text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

std::string executable_path;

std::string get_executable_path()
{
	return executable_path;
}

int main(int argc, char *argv[])
{
	executable_path = argc > 0 ? std::filesystem::absolute(std::filesystem::path(argv[0])).string() : "";

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
	glfwWindow = glfwCreateWindow(mainWindowWidth, mainWindowHeight, "OVR Dynamic Resolution", nullptr, nullptr);
	if (glfwWindow == nullptr)
		return 1;
	glfwMakeContextCurrent(glfwWindow);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	// Don't save Dear ImGui window state
	io.IniFilename = NULL;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03, 0.03, 0.03, 1));

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Set window icon
	GLFWimage icon;
	unsigned iconWidth, iconHeight;
	if (lodepng_decode32_file(&(icon.pixels), &(iconWidth), &(iconHeight), iconPath) == 0)
	{
		icon.width = (int)iconWidth;
		icon.height = (int)iconHeight;
		glfwSetWindowIcon(glfwWindow, 1, &icon);
	}
#pragma endregion

#pragma region VR init
	EVRInitError init_error;
	std::unique_ptr<IVRSystem, decltype(&shutdown_vr)> system(
		VR_Init(&init_error, VRApplication_Overlay), &shutdown_vr);
	if (init_error)
	{
		system = nullptr;
		printLine(VR_GetVRInitErrorAsEnglishDescription(init_error), 6000l);
		return EXIT_FAILURE;
	}
	if (!VRCompositor())
	{
		printLine("Failed to initialize VR compositor.", 6000l);
		return EXIT_FAILURE;
	}
#pragma endregion

	// Load settings from ini file
	if (!loadSettings())
	{
		std::replace(blacklistApps.begin(), blacklistApps.end(), ' ', '\n'); // Set blacklist newlines
		saveSettings();														 // Restore settings
	}

	// Set auto-start
	int autoStartResult = handle_setup(autoStart);
	if (autoStartResult != 0)
		printLine(fmt::format("Error toggling auto-start ({}) ", autoStartResult), 6000l);

	// Minimize or hide the window according to config
	if (minimizeOnStart == 1) // Minimize
		glfwIconifyWindow(glfwWindow);
	else if (minimizeOnStart == 2) // Hide
		glfwHideWindow(glfwWindow);

	// Make sure we can set resolution ourselves (Custom instead of Auto)
	vr::VRSettings()->SetInt32(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleManualOverride_Bool, true);

	// Set default resolution
	vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section,
							   vr::k_pch_SteamVR_SupersampleScale_Float, initialRes / 100.0f);

#pragma region Initialise NVML
	HMODULE nvmlLibrary;
	nvmlDevice_t nvmlDevice;
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
		{
			nvmlEnabled = false;
		}
		else
		{
			nvmlReturn_t result;
			// Initialize NVML library
			result = nvmlInitPtr();
			if (result != NVML_SUCCESS)
			{
				nvmlEnabled = false;
			}
			else
			{
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
					result = nvmlDeviceGetHandleByIndexPtr(gpuIndex, &nvmlDevice);

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
			}
		}
	}
	else
	{
		nvmlEnabled = false;
	}
#pragma endregion
#pragma region Tray
#if defined(_WIN32)
	const char *hideToggleText = "Hide";
	if (minimizeOnStart == 2)
	{
		hideToggleText = "Show";
	}

	tray trayInstance = {
		"icon.ico",
		"OVR Dynamic Resolution",
		[](tray *trayInstance)
		{ trayInstance->menu->text = "Hide"; tray_update(trayInstance); glfwShowWindow(glfwWindow); },
		new tray_menu_item[5]{
			{hideToggleText, 0, 0, [](tray_menu_item *item)
			 { if (item->text == "Hide") { item->text = "Show"; tray_update(tray_get_instance()); glfwHideWindow(glfwWindow); } 
			   else { item->text = "Hide"; tray_update(tray_get_instance()); glfwShowWindow(glfwWindow); } }},
			{"-", 0, 0, nullptr},
			{"Quit", 0, 0, [](tray_menu_item *item)
			 { trayQuit = true; }},
			{nullptr, 0, 0, nullptr}}};

	tray_init(&trayInstance);

	std::thread trayThread([&]
						   { while(tray_loop(1) == 0); trayQuit = true; });
#endif // _WIN32
#pragma endregion

	// Initialize loop variables
	Compositor_FrameTiming *frameTiming = new vr::Compositor_FrameTiming[openvrMaxFrames];
	long lastChangeTime = getCurrentTimeMillis() - resChangeDelayMs - 1;
	bool adjustResolution = true;
	bool openvrQuit = false;
	bool manualRes = false;

	// Resolution variables to display in GUI
	float averageGpuTime = 0;
	int gpuFps = 0;
	float averageCpuTime = 0;
	int cpuFps = 0;
	float averageFrameShown = 0;
	float newRes = initialRes;
	int targetFpsHigh = 0;
	int targetFpsLow = 0;
	float targetFrametimeHigh = 0;
	float targetFrametimeLow = 0;
	int hmdHz = 0;
	float hmdFrametime = 0;
	int currentFps = 0;
	float vramUsedGB = 0;
	uint32_t hmdWidthRes = 0;
	uint32_t hmdHeightRes = 0;
	int resIncreaseThresholdFps = 0;
	int resDecreaseThresholdFps = 0;

	// GUI variables
	bool showSettings = false;
	bool prevAutoStart = autoStart;

	// event loop
	while ((!glfwWindowShouldClose(glfwWindow) || closeToTray) && !openvrQuit && !trayQuit)
	{
		// Close to tray
		if (glfwWindowShouldClose(glfwWindow) && closeToTray)
		{
			glfwSetWindowShouldClose(glfwWindow, false);
			glfwHideWindow(glfwWindow);
#if defined(_WIN32)
			tray_get_instance()->menu->text = "Show"; 
			tray_update(tray_get_instance());
#endif
		}

		// Get current time
		long currentTime = getCurrentTimeMillis();

		// Doesn't run every loop
		if (currentTime - resChangeDelayMs > lastChangeTime)
		{
			lastChangeTime = currentTime;

#pragma region Getting data
			float currentRes = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float) * 100.0f;

			// Check for external resolution change (if resolution got changed and it wasn't us)
			if (externalResChangeCompatibility && std::fabs(newRes - currentRes) > 0.001f && !manualRes)
				manualRes = true;

			// Check for end of external resolution change (if automatic resolution is enabled)
			if (externalResChangeCompatibility && !vr::VRSettings()->GetInt32(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleManualOverride_Bool))
			{
				vr::VRSettings()->SetInt32(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleManualOverride_Bool, true);
				manualRes = false;
			}

			// Fetch resolution and target fps
			newRes = currentRes;
			float lastRes = newRes;
			int displayFrequency = std::round(vr::VRSystem()->GetFloatTrackedDeviceProperty(0, Prop_DisplayFrequency_Float));
			if (hmdHz != displayFrequency)
			{
				hmdHz = displayFrequency;
				hmdFrametime = 1000.0f / displayFrequency;
				resIncreaseThresholdFps = std::round(1000.0f / ((resIncreaseThreshold / 100.0f) * hmdFrametime));
				resDecreaseThresholdFps = std::round(1000.0f / ((resDecreaseThreshold / 100.0f) * hmdFrametime));
			}
			targetFpsHigh = resIncreaseThresholdFps;
			targetFpsLow = resDecreaseThresholdFps;
			targetFrametimeHigh = 1000.0f / resIncreaseThresholdFps;
			targetFrametimeLow = 1000.0f / resDecreaseThresholdFps;

			// Define totals
			float totalGpuTime = 0;
			float totalCpuTime = 0;
			int frameShownTotal = 0;

			// Loop through past frames
			frameTiming->m_nSize = sizeof(Compositor_FrameTiming);
			vr::VRCompositor()->GetFrameTimings(frameTiming, openvrMaxFrames);
			for (int i = 0; i < openvrMaxFrames; i++)
			{
				// Get GPU frametime
				float gpuTime = frameTiming[i].m_flTotalRenderGpuMs;

				// Calculate CPU frametime
				// https://github.com/Louka3000/OpenVR-Dynamic-Resolution/issues/18#issuecomment-1833105172
				float cpuTime = frameTiming[i].m_flCompositorRenderCpuMs									 // Compositor
								+ (frameTiming[i].m_flNewFrameReadyMs - frameTiming[i].m_flNewPosesReadyMs); // Application & Late Start

				// How many times the current frame repeated (>1 = reprojecting)
				int frameShown = std::max((int)frameTiming[i].m_nNumFramePresents, 1);
				// Reason reprojection is happening
				int reprojectionFlag = frameTiming[i].m_nReprojectionFlags;

				// Add to totals
				totalGpuTime += gpuTime;
				totalCpuTime += std::max(cpuTime, .0f);
				frameShownTotal += frameShown;
			}

			// Calculate averages
			averageGpuTime = totalGpuTime / openvrMaxFrames;
			averageCpuTime = totalCpuTime / openvrMaxFrames;
			averageFrameShown = (float)frameShownTotal / (float)openvrMaxFrames;

			// GUI
			gpuFps = std::round(1000.0f / averageGpuTime);
			cpuFps = std::round(1000.0f / averageCpuTime);

			// Estimated current FPS
			currentFps = hmdHz / averageFrameShown;

			// Double the target frametime if the user wants to,
			// or if CPU Frametime is double the target frametime,
			// or if preferReprojection is true and CPU Frametime is greated than targetFrametime.
			if ((((averageCpuTime > hmdFrametime && preferReprojection) ||
				  averageCpuTime / 2 > hmdFrametime) &&
				 !ignoreCpuTime) ||
				alwaysReproject)
			{
				targetFpsHigh /= 2;
				targetFpsLow /= 2;
				targetFrametimeHigh *= 2;
				targetFrametimeLow *= 2;
			}

			// Get VRAM usage
			float vramUsed = 0; // Assume we always have free VRAM by default
			if (nvmlEnabled)
			{
				// Get memory info
				nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfoPtr;
#ifdef _WIN32
				nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)GetProcAddress(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#else
				nvmlDeviceGetMemoryInfoPtr = (nvmlDeviceGetMemoryInfo_t)dlsym(nvmlLibrary, "nvmlDeviceGetMemoryInfo");
#endif
				nvmlMemory_t nvmlMemory;
				if (nvmlDeviceGetMemoryInfoPtr(nvmlDevice, &nvmlMemory) != NVML_SUCCESS)
					nvmlEnabled = false;
				else
					vramTotalGB = nvmlMemory.total / bitsToGB;

				vramUsedGB = nvmlMemory.used / bitsToGB;
				if (nvmlEnabled) // Get the VRAM used in %
					vramUsed = (float)nvmlMemory.used / (float)nvmlMemory.total;
			}
#pragma endregion

#pragma region Resolution adjustment
			// Get the current application key
			std::string appKey = getCurrentApplicationKey();
			adjustResolution = shouldAdjustResolution(appKey, manualRes, averageCpuTime);
			if (adjustResolution)
			{
				// Adjust resolution
				if ((averageCpuTime > minCpuTimeThreshold || vramOnlyMode))
				{
					// Frametime
					if (averageGpuTime < targetFrametimeHigh && vramUsed < vramTarget / 100.0f && !vramOnlyMode)
					{
						// Increase resolution
						newRes += ((targetFrametimeHigh - averageGpuTime) * (resIncreaseScale / 100.0f)) + resIncreaseMin;
					}
					else if (averageGpuTime > targetFrametimeLow && !vramOnlyMode)
					{
						// Decrease resolution
						newRes -= ((averageGpuTime - targetFrametimeLow) * (resDecreaseScale / 100.0f)) + resDecreaseMin;
					}

					// VRAM
					if (vramUsed > vramLimit / 100.0f)
					{
						// Force the resolution to decrease when the vram limit is reached
						newRes -= resDecreaseMin;
					}
					else if (vramOnlyMode && newRes < initialRes && vramUsed < vramTarget / 100.0f)
					{
						// When in VRAM-only mode, make sure the res goes back up when possible.
						newRes = std::min(initialRes, (int)std::round(newRes) + resIncreaseMin);
					}

					// Clamp the new resolution
					newRes = std::clamp((int)std::round(newRes), minRes, maxRes);
				}
			}
			else if ((appKey == "" || (resetOnThreshold && averageCpuTime < minCpuTimeThreshold)) && !manualRes)
			{
				// If (in SteamVR void or cpuTime below threshold) and user didn't pause res
				// Reset to initialRes
				newRes = initialRes;
			}

			if (newRes != lastRes)
			{
				// Sets the new resolution
				vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, newRes / 100.0f);
			}

			vr::VRSystem()->GetRecommendedRenderTargetSize(&hmdWidthRes, &hmdHeightRes);
		}
#pragma endregion

#pragma region Gui rendering
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Make sure buttons are gray
		pushGrayButtonColour();

#pragma region Main window
		if (!showSettings)
		{
			// Create the main window
			ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

			// Set position and size to fill the viewport
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

			// Title
			ImGui::Text(fmt::format("OVR Dynamic Resolution {}", version).c_str());

			ImGui::Separator();
			ImGui::NewLine();

			// HMD Hz
			ImGui::Text("%s", fmt::format("HMD refresh rate: {} hz ({:.2f} ms)", hmdHz, hmdFrametime).c_str());

			// Target FPS and frametime
			if (!vramOnlyMode)
			{
				ImGui::Text("%s", fmt::format("Target FPS: {}-{} fps ({:.2f}-{:.2f} ms)", targetFpsLow, targetFpsHigh, targetFrametimeLow, targetFrametimeHigh).c_str());
			}
			else
			{
				ImGui::Text("Target FPS: Disabled");
			}

			// VRAM target and limit
			if (nvmlEnabled && nvmlEnabled)
			{
				ImGui::Text("%s", fmt::format("VRAM target: {:.2f} GB", vramTarget / 100.0f * vramTotalGB).c_str());
				ImGui::Text("%s", fmt::format("VRAM limit: {:.2f} GB ", vramLimit / 100.0f * vramTotalGB).c_str());
			}
			else
			{
				ImGui::Text("VRAM target: Disabled");
				ImGui::Text("VRAM limit: Disabled");
			}

			ImGui::NewLine();

			// FPS and frametimes
			ImGui::Text("%s", fmt::format("Displayed FPS: {} fps", currentFps).c_str());
			ImGui::Text("%s", fmt::format("GPU frametime: {:.2f} ms ({} fps)", averageGpuTime, gpuFps).c_str());
			ImGui::Text("%s", fmt::format("CPU frametime: {:.2f} ms ({} fps)", averageCpuTime, cpuFps).c_str());

			// VRAM usage
			if (nvmlEnabled)
				ImGui::Text("%s", fmt::format("VRAM usage: {:.2f} GB", vramUsedGB).c_str());
			else
				ImGui::Text("%s", fmt::format("VRAM usage: Disabled").c_str());

			ImGui::NewLine();

			// Reprojection ratio
			ImGui::Text("%s", fmt::format("Reprojection ratio: {:.2f}", averageFrameShown - 1).c_str());

			// Current resolution
			if (manualRes)
			{
				ImGui::Text("Resolution =");
			}
			else
			{
				ImGui::Text("%s", fmt::format("Resolution = {:.0f} ({} x {})", newRes, hmdWidthRes, hmdHeightRes).c_str());
			}

			// Resolution adjustment status
			if (!adjustResolution)
			{
				ImGui::SameLine(0, 10);
				if (manualRes)
				{
					ImGui::PushItemWidth(192);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
					if (ImGui::SliderFloat("##", &newRes, 20.0f, 500.0f, fmt::format("%.0f ({} x {})", hmdWidthRes, hmdHeightRes).c_str(), ImGuiSliderFlags_AlwaysClamp))
					{
						vr::VRSettings()->SetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_SupersampleScale_Float, newRes / 100.0f);
						vr::VRSystem()->GetRecommendedRenderTargetSize(&hmdWidthRes, &hmdHeightRes);
					}
					ImGui::PopStyleVar();
				}
				else
				{
					ImGui::Text("(paused)");
				}
			}

			ImGui::NewLine();

			// Open settings
			bool settingsPressed = ImGui::Button("Settings", ImVec2(82, 28));
			if (settingsPressed)
				showSettings = true;

			// Resolution pausing
			ImGui::SameLine();
			const char *pauseText;
			if (!manualRes)
				pauseText = "Manual resolution";
			else
				pauseText = "Dynamic resolution";
			bool pausePressed = ImGui::Button(pauseText, ImVec2(142, 28));
			if (pausePressed)
			{
				manualRes = !manualRes;
				adjustResolution = shouldAdjustResolution(getCurrentApplicationKey(), manualRes, averageCpuTime);
			}

			// Stop creating the main window
			ImGui::End();
		}
#pragma endregion

#pragma region Settings window
		if (showSettings)
		{
			// Create the settings window
			ImGui::Begin("Settings", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

			// Set position and size to fill the viewport
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::SetWindowSize(ImVec2(mainWindowWidth, mainWindowHeight));

			// Set the labels' width
			ImGui::PushItemWidth(96);

			// Title
			ImGui::Text("Settings");

			ImGui::Separator();
			ImGui::NewLine();

			// GUI settings inputs
			if (ImGui::CollapsingHeader("Startup"))
			{
				ImGui::Checkbox("Start with SteamVR", &autoStart);
				addTooltip("Automatically launch OVRDR alongside SteamVR.");

				ImGui::Text("Window startup behaviour:");
				ImGui::RadioButton("Visible", &minimizeOnStart, 0);
				addTooltip("Keep the OVRDR window visible on startup.");
				ImGui::RadioButton("Minimized (taskbar)", &minimizeOnStart, 1);
				addTooltip("Minimize the OVRDR window to the taskbar on startup.");
				ImGui::RadioButton("Hidden (tray)", &minimizeOnStart, 2);
				addTooltip("Hide the OVRDR window completely on startup. You can still show the window by clicking \"Show\" in the tray icon's context menu.");
			}

			if (ImGui::CollapsingHeader("General"))
			{
				ImGui::Checkbox("Close to tray", &closeToTray);
				addTooltip("Minimize the window to the tray when closing it instead of closing the application.");

				ImGui::Checkbox("External res change compatibility", &externalResChangeCompatibility);
				addTooltip("Automatically switch to manual resolution adjustment within the app when VR resolution is changed from an external source (SteamVR setting, Oyasumi, etc.) as to let the external source control the resolution. Automatically switches back to dynamic resolution adjustment when resolution is set to automatic.");

				ImGui::Text("Blacklist");
				addTooltip("Don't allow resolution changes in blacklisted applications.");
				if (ImGui::InputTextMultiline("Blacklisted apps", &blacklistApps, ImVec2(130, 60), ImGuiInputTextFlags_CharsNoBlank))
					blacklistAppsSet = multilineStringToSet(blacklistApps);
				addTooltip("List of OpenVR application keys that should be blacklisted for resolution adjustment in the format \'steam.app.APPID\' (e.g. \'steam.app.620980\' for Beat Saber). One per line.");
				if (ImGui::Button("Blacklist current app", ImVec2(160, 26)))
				{
					std::string appKey = getCurrentApplicationKey();
					if (!isApplicationBlacklisted(appKey))
					{
						blacklistAppsSet.insert(appKey);
						if (blacklistApps != "")
							blacklistApps += "\n";
						blacklistApps += appKey;
					}
				}
				addTooltip("Adds the current application to the blacklist.");

				ImGui::Checkbox("Enable whitelist", &whitelistEnabled);
				addTooltip("Only allow resolution changes in whitelisted applications.");
				if (ImGui::InputTextMultiline("Whitelisted apps", &whitelistApps, ImVec2(130, 60), ImGuiInputTextFlags_CharsNoBlank))
					whitelistAppsSet = multilineStringToSet(whitelistApps);
				addTooltip("List of OpenVR application keys that should be whitelisted for resolution adjustment in the format \'steam.app.APPID\' (e.g. \'steam.app.620980\' for Beat Saber). One per line.");
				if (ImGui::Button("Whitelist current app", ImVec2(164, 26)))
				{
					std::string appKey = getCurrentApplicationKey();
					if (!isApplicationWhitelisted(appKey))
					{
						whitelistAppsSet.insert(appKey);
						if (whitelistApps != "")
							whitelistApps += "\n";
						whitelistApps += appKey;
					}
				}
				addTooltip("Adds the current application to the whitelist.");
			}

			if (ImGui::CollapsingHeader("Resolution"))
			{
				if (ImGui::InputInt("Resolution change delay ms", &resChangeDelayMs, 100))
					resChangeDelayMs = std::max(resChangeDelayMs, 100);
				addTooltip("Delay in milliseconds between resolution changes.");

				if (ImGui::InputInt("Initial resolution", &initialRes, 5))
					initialRes = std::clamp(initialRes, 20, 500);
				addTooltip("The resolution set at startup. Also used when resetting resolution.");

				if (ImGui::InputInt("Minimum resolution", &minRes, 5))
					minRes = std::clamp(minRes, 20, 500);
				addTooltip("The minimum resolution OVRDR will set.");

				if (ImGui::InputInt("Maximum resolution", &maxRes, 5))
					maxRes = std::clamp(maxRes, 20, 500);
				addTooltip("The maximum resolution OVRDR will set.");

				if (ImGui::TreeNodeEx("Advanced", ImGuiTreeNodeFlags_NoTreePushOnOpen))
				{
					if (ImGui::InputInt("High FPS target", &resIncreaseThresholdFps, 1))
						resIncreaseThreshold = std::max((float)hmdHz / (float)resIncreaseThresholdFps * 100.0f, 0.0f);
					addTooltip("When the framerate is higher than this value, resolution is allowed to increase.");

					if (ImGui::InputInt("Low FPS target", &resDecreaseThresholdFps, 1))
						resDecreaseThreshold = std::max((float)hmdHz / (float)resDecreaseThresholdFps * 100.0f, 0.0f);
					addTooltip("When the framerate is lower than this value, resolution starts decreasing.");

					ImGui::InputInt("Resolution increase constant", &resIncreaseMin, 1);
					addTooltip("Constant percentages to increase resolution when available.");

					ImGui::InputInt("Resolution decrease constant", &resDecreaseMin, 1);
					addTooltip("Constant percentages to decrease resolution when needed.");

					ImGui::InputInt("Resolution increase scale", &resIncreaseScale, 10);
					addTooltip("The more frametime headroom and the higher this value is, the more resolution will increase each time.");

					ImGui::InputInt("Resolution decrease scale", &resDecreaseScale, 10);
					addTooltip("The more frametime excess and the higher this value is, the more resolution will decrease each time.");

					ImGui::InputFloat("Minimum CPU time threshold", &minCpuTimeThreshold, 0.1);
					addTooltip("Don't increase resolution if the CPU frametime is below this value (useful to prevent resolution increases during loading screens).");

					ImGui::Checkbox("Reset on CPU time threshold", &resetOnThreshold);
					addTooltip("Reset the resolution to the initial resolution whenever the \"Minimum CPU time threshold\" is met.");
				}
			}

			if (ImGui::CollapsingHeader("Reprojection"))
			{
				ImGui::Checkbox("Always reproject", &alwaysReproject);
				addTooltip("Always double the target frametime.");

				ImGui::Checkbox("Prefer reprojection", &preferReprojection);
				addTooltip("If enabled, double the target frametime as soon as the CPU frametime is over the initial target frametime. Else, only double the target frametime if the CPU frametime is over double the initial target frametime.");

				ImGui::Checkbox("Ignore CPU time", &ignoreCpuTime);
				addTooltip("Never change the target frametime depending on the CPU frametime (stops both behaviours described in \"Prefer reprojection\" tooltip).");
			}

			if (ImGui::CollapsingHeader("VRAM"))
			{
				ImGui::Checkbox("VRAM monitor enabled", &vramMonitorEnabled);
				addTooltip("Enable VRAM specific features. If disabled, it is assumed that free VRAM is always available.");

				ImGui::Checkbox("VRAM-only mode", &vramOnlyMode);
				addTooltip("Always stay at the initial resolution or lower based off available VRAM alone (ignoring frametimes).");

				if (ImGui::InputInt("VRAM target", &vramTarget, 2))
					vramTarget = std::clamp(vramTarget, 0, 100);
				addTooltip("Resolution stops increasing once VRAM usage exceeds this percentage.");

				if (ImGui::InputInt("VRAM limit", &vramLimit, 2))
					vramLimit = std::clamp(vramLimit, 0, 100);
				addTooltip("Resolution starts descreasing once VRAM usage exceeds this percentage.");

				ImGui::InputInt("GPU Index", &gpuIndex, 1);
				addTooltip("The index of the GPU to use for VRAM monitoring. Only useful in systems with multiple GPUs. Needs restart to take effect.");
			}

			ImGui::NewLine();

			// Save settings
			bool closePressed = ImGui::Button("Close", ImVec2(82, 28));
			if (closePressed)
			{
				showSettings = false;
			}
			ImGui::SameLine();
			pushRedButtonColour();
			bool revertPressed = ImGui::Button("Revert", ImVec2(82, 28));
			if (revertPressed)
			{
				loadSettings();
			}
			ImGui::PopStyleColor(3); // pushRedButtonColour();
			ImGui::SameLine();
			pushGreenButtonColour();
			bool savePressed = ImGui::Button("Save", ImVec2(82, 28));
			if (savePressed)
			{
				saveSettings();
				if (prevAutoStart != autoStart)
				{
					handle_setup(autoStart);
					prevAutoStart = autoStart;
				}
			}
			ImGui::PopStyleColor(3); // pushGreenButtonColour();

			// Stop creating the settings window
			ImGui::End();
		}
#pragma endregion
		ImGui::PopStyleColor(3); // pushGrayButtonColour();

		ImGui::PopStyleColor(); // ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03, 0.03, 0.03, 1));

		// Rendering
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(glfwWindow);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03, 0.03, 0.03, 1));
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
		if (glfwGetWindowAttrib(glfwWindow, GLFW_FOCUSED))
			sleepTime = refreshIntervalFocused;
		else
			sleepTime = refreshIntervalBackground;

		// ZZzzzz
		std::this_thread::sleep_for(sleepTime);
	}

	cleanup(nvmlLibrary);

#if defined(_WIN32)
	tray_exit();
#endif // _WIN32

	return 0;
}

// Custom WinMain method to not have the terminal
#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	main(NULL, NULL);
}
#endif
