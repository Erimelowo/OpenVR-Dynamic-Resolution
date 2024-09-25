#include <openvr.h>
#include <fmt/core.h>
#include <memory>

#include "pathtools_excerpt.h"

static constexpr const char *rel_manifest_path = "./manifest.vrmanifest";
static constexpr const char *application_key = "ovr-dynamic-resolution";

void shutdown_vr(vr::IVRSystem *_system)
{
	vr::VR_Shutdown();
}

/**
 * 0 = nothing
 * 1 = enabled
 * 2 = disabled
 * other: error
 */
int handle_setup(bool install)
{
	vr::IVRApplications *apps = vr::VRApplications();
	vr::EVRApplicationError app_error;

	bool currently_installed = apps->IsApplicationInstalled(application_key);

	std::string manifest_path = Path_MakeAbsolute(rel_manifest_path, Path_StripFilename(Path_GetExecutablePath()));
	if (install)
	{
		if (currently_installed)
		{
			if (!apps->GetApplicationAutoLaunch(application_key))
			{
				apps->SetApplicationAutoLaunch(application_key, true);
				return 1;
			}
			return 0;
		}

		app_error = apps->AddApplicationManifest(manifest_path.c_str());
		if (app_error)
			return app_error;

		app_error = apps->SetApplicationAutoLaunch(application_key, true);
		if (app_error)
			return app_error;

		return 1;
	}
	else if (currently_installed)
	{
		if (!apps->GetApplicationAutoLaunch(application_key))
			return 0;

		app_error = apps->SetApplicationAutoLaunch(application_key, false);
		if (app_error)
			return app_error;

		return 2;
	}
	else
		return 0;
}