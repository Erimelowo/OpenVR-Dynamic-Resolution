#pragma once
#include <openvr.h>

// little wrapper for unique_ptr
void shutdown_vr(vr::IVRSystem* _system);

int handle_setup(bool install_manifest);