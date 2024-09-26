cmake_minimum_required(VERSION 3.28)

project(OVR-Dynamic-Resolution)

include(cmake/CPM.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

## Find dependencies
find_package(Threads REQUIRED)

#CPMAddPackage("gh:ValveSoftware/openvr#v2.5.1")
CPMAddPackage(
  NAME openvr 
  VERSION 2.5.1
  GITHUB_REPOSITORY ValveSoftware/openvr
  DOWNLOAD_ONLY TRUE
)

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

CPMAddPackage(
    NAME tray
    URL https://github.com/Soundux/traypp/archive/698db7d58dd450cc9e30dc12d3bd0c5ca4d6a5b1.zip
)
link_libraries(tray)

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

if (WIN32)
  link_directories("${openvr_SOURCE_DIR}/lib/win64")
elseif(UNIX)
  link_directories("${openvr_SOURCE_DIR}/lib/linux64")
endif()

add_executable("${PROJECT_NAME}" ${GUI_TYPE} "src/main.cpp" "src/pathtools_excerpt.cpp" "src/setup.cpp")
target_link_libraries("${PROJECT_NAME}" openvr_api fmt::fmt-header-only simpleini imgui lodepng tray Threads::Threads)
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

# Copy files
if (WIN32)
  set(RELEASE_FILES_DIR ${CMAKE_SOURCE_DIR}/ReleaseFiles)
  file(MAKE_DIRECTORY ${RELEASE_FILES_DIR})
  file(GLOB APP_RESOURCES "${CMAKE_SOURCE_DIR}/resources/*")
  
  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/Release/OVR-Dynamic-Resolution.exe
            ${RELEASE_FILES_DIR})

  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${openvr_SOURCE_DIR}/bin/win64/openvr_api.dll
            ${RELEASE_FILES_DIR})

  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${APP_RESOURCES}
            ${RELEASE_FILES_DIR})
endif(WIN32)