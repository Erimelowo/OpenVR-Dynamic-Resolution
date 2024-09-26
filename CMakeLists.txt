cmake_minimum_required(VERSION 3.16)

project(OVR-Dynamic-Resolution)
set(CMAKE_CXX_STANDARD 17)

include(cmake/CPM.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

## Find dependencies
find_package(Threads REQUIRED)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Under Linux, you may need to add some dependencies.")
  message(STATUS "Ubuntu users should try:")
  message(STATUS "apt-get install libwayland-dev libxrandr-dev libxkbcommon-x11-dev libxinerama-dev libxcursor-dev libxi-dev  libglu1-mesa-dev freeglut3-dev mesa-common-dev")
endif()


IF(WIN32)
  set(OPENVR_SHARED ON)
ELSE()
  set(OPENVR_SHARED OFF)
ENDIF()
CPMAddPackage(
  NAME openvr 
  URL https://github.com/ValveSoftware/openvr/archive/refs/tags/v2.5.1.zip
  OPTIONS "BUILD_SHARED ${OPENVR_SHARED}"
)

get_directory_property(OPENVR_LIBNAME
    DIRECTORY ${openvr_SOURCE_DIR}
    DEFINITION LIBNAME)

get_directory_property(OPENVR_PLATFORM_NAME
    DIRECTORY ${openvr_SOURCE_DIR}
    DEFINITION PLATFORM_NAME)
get_directory_property(OPENVR_PROCESSOR_ARCH
    DIRECTORY ${openvr_SOURCE_DIR}
    DEFINITION PROCESSOR_ARCH)
get_directory_property(OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY
    DIRECTORY ${openvr_SOURCE_DIR}
    DEFINITION CMAKE_LIBRARY_OUTPUT_DIRECTORY)

message(STATUS "OpenVR detected your system as ${OPENVR_PLATFORM_NAME}${OPENVR_PROCESSOR_ARCH}")

if(EXISTS "${OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
   message(STATUS "Recovering OpenVR library from ${OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
   message(WARNING "Can't find OpenVR library at ${OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY} Is your system supported?")
endif()

CPMAddPackage("gh:glfw/glfw#3.4")

CPMAddPackage("gh:fmtlib/fmt#11.0.2")

CPMAddPackage("gh:brofield/simpleini#v4.22")
add_library(simpleini INTERFACE)
target_include_directories(simpleini INTERFACE "${simpleini_SOURCE_DIR}")

CPMAddPackage("gh:ocornut/imgui@1.91.2")
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_link_libraries(imgui PUBLIC glfw)
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}/backends")

CPMAddPackage(
    NAME lodepng
    URL https://github.com/lvandeve/lodepng/archive/ed3c5f14b136c936d615ee3b38aaa7e7e642f12c.zip
)
add_library(lodepng STATIC "${lodepng_SOURCE_DIR}/lodepng.cpp")
target_include_directories(lodepng PUBLIC  "${lodepng_SOURCE_DIR}")

IF(WIN32)
CPMAddPackage(
    NAME tray
    URL https://github.com/Soundux/traypp/archive/698db7d58dd450cc9e30dc12d3bd0c5ca4d6a5b1.zip
)
ENDIF()

set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH $ORIGIN)

# Project
# Set some Win32 Specific Settings
IF(WIN32)
SET(GUI_TYPE WIN32)
ENDIF(WIN32)
# Set some Apple MacOS Specific settings
IF (APPLE)
SET(GUI_TYPE MACOSX_BUNDLE)
ENDIF (APPLE)

link_directories("${openvr_SOURCE_DIR}/lib/${OPENVR_PLATFORM_NAME}${OPENVR_PROCESSOR_ARCH}")

if(WIN32)
link_directories("${OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()
if(WIN32)
add_executable("${PROJECT_NAME}" ${GUI_TYPE} "src/main.cpp" "src/pathtools_excerpt.cpp" "src/setup.cpp")
else()
add_executable("${PROJECT_NAME}" ${GUI_TYPE} "src/main.cpp" "src/setup.cpp")
endif()

target_link_libraries("${PROJECT_NAME}" openvr_api fmt::fmt-header-only simpleini imgui lodepng Threads::Threads)
if(TARGET tray)
  target_link_libraries("${PROJECT_NAME}" tray)
endif()

target_include_directories("${PROJECT_NAME}" PRIVATE ${CMAKE_CURRENT_BINARY_DIR} PUBLIC "${openvr_SOURCE_DIR}/headers")
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_17)

# IDE Config
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${SOURCES})

add_custom_target(
    version
    COMMAND ${CMAKE_COMMAND}
        -Dlocal_dir="${CMAKE_CURRENT_SOURCE_DIR}"
        -Doutput_dir="${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gitversion.cmake"
)
add_dependencies("${PROJECT_NAME}" version)

install(TARGETS "${PROJECT_NAME}"
    RUNTIME
    DESTINATION .
    BUNDLE DESTINATION ${CMAKE_SOURCE_DIR}
)

set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
if(${CPACK_SYSTEM_NAME} MATCHES Windows)
  if(CMAKE_CL_64)
    set(CPACK_SYSTEM_NAME win64)
    set(CPACK_IFW_TARGET_DIRECTORY "@RootDir@/Program Files/${CMAKE_PROJECT_NAME}")
  else()
    set(CPACK_SYSTEM_NAME win32)
  endif()
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PROJECT_NAME}-${CPACK_SYSTEM_NAME})
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)

if(${OPENVR_PLATFORM_NAME}$ MATCHES "win.*")
  set(OPENVR_DLL_NAME "openvr_api.dll")
elseif(${OPENVR_PLATFORM_NAME}$ MATCHES "osx.*")
  set(OPENVR_DLL_NAME "libopenvr_api.dylib")
else()
  set(OPENVR_DLL_NAME "libopenvr_api.so")
endif()


if(WIN32)
    # Visual Studio
    set(ACTUAL_CURRENT_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR}/Release)
else()
    set(ACTUAL_CURRENT_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})
endif()
file(MAKE_DIRECTORY ${ACTUAL_CURRENT_BINARY_DIR}) # make sure it always exists
# Copy files
file(GLOB APP_RESOURCES "${CMAKE_SOURCE_DIR}/resources/*")
file(COPY ${OPENVR_CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${OPENVR_DLL_NAME} ${APP_RESOURCES} DESTINATION ${ACTUAL_CURRENT_BINARY_DIR})
