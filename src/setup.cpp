#include <openvr.h>
#include <fmt/core.h>
#include <memory>
#include "pathtools_excerpt.h"

static constexpr const char* rel_manifest_path = "./manifest.vrmanifest";
static constexpr const char* application_key = "openvr-dynamic-resolution";

void shutdown_vr(vr::IVRSystem* _system) {
    vr::VR_Shutdown();
}

int handle_setup(bool install) {
    //vr::EVRInitError init_error = vr::VRInitError_None;
    //std::unique_ptr<vr::IVRSystem, decltype(&shutdown_vr)> system(vr::VR_Init(&init_error, vr::VRApplication_Utility), &shutdown_vr);
 
    //if (init_error != vr::VRInitError_None) {
    //    fmt::print("Unable to init VR runtime as utility: {}\n", VR_GetVRInitErrorAsEnglishDescription(init_error));
    //    return 2;
    //}

    vr::IVRApplications* apps = vr::VRApplications();
    vr::EVRApplicationError app_error = vr::VRApplicationError_None;

    bool currently_installed = apps->IsApplicationInstalled(application_key);

    std::string manifest_path = Path_MakeAbsolute(rel_manifest_path, Path_StripFilename(Path_GetExecutablePath()));
    if (install) {
        fmt::print("Enabling auto-start.\n");
        
        if (currently_installed) {
            if(apps->GetApplicationAutoLaunch(application_key) == false){
                apps->SetApplicationAutoLaunch(application_key, true);
                return 1;
            }
            return 0;
        }

        app_error = apps->AddApplicationManifest(manifest_path.c_str());
        if (app_error != vr::VRApplicationError_None) {
            fmt::print("Could not enable auto-start: {}\n", apps->GetApplicationsErrorNameFromEnum(app_error));
            return 2;
        }

        app_error = apps->SetApplicationAutoLaunch(application_key, true);
        if (app_error != vr::VRApplicationError_None) {
            fmt::print("Could not set auto-start: {}\n", apps->GetApplicationsErrorNameFromEnum(app_error));
            return 2;
        }
        return 1;
    }
    else if (currently_installed) {
        if (apps->GetApplicationAutoLaunch(application_key) == false){
            return 0;
        }

        fmt::print("Disabling auto-start\n");
        apps->SetApplicationAutoLaunch(application_key, false);
        return 1;
    }
    else {
        return 0;
    }
}