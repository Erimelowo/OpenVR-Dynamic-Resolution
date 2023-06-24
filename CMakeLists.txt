cmake_minimum_required(VERSION 3.15)

project(OpenVR-Dynamic-Resolution)

include("${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(fmt CONFIG REQUIRED)
if(WIN32)
  find_path(PDC_INCLUDES curses.h)
  include_directories(${PDC_INCLUDES})
  find_library(CURSES_LIBRARIES pdcurses REQUIRED)
else()
  find_package(Curses REQUIRED)
  include_directories(${CURSES_INCLUDE_DIRS})
endif()
find_library(OPENVR_LIB openvr_api)

set(protos_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protos)
file(MAKE_DIRECTORY "${protos_OUTPUT_DIR}")

include_directories(include)

set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH $ORIGIN)

include_directories("$ENV{LOCALAPPDATA}/Temp/cuda/cuda_nvml_dev/nvml_dev/include")
# Project
add_executable("${PROJECT_NAME}" "src/main.cpp" "src/pathtools_excerpt.cpp" "src/setup.cpp")
target_link_libraries("${PROJECT_NAME}" PRIVATE "${OPENVR_LIB}" fmt::fmt-header-only ${CURSES_LIBRARIES} "$ENV{LOCALAPPDATA}/Temp/cuda/cuda_nvml_dev/nvml_dev/lib/x64/nvml.lib")
target_include_directories("${PROJECT_NAME}" PUBLIC ${protos_OUTPUT_DIR} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
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

install(FILES "${PROJECT_SOURCE_DIR}/manifest.vrmanifest"
    DESTINATION "."
)

if (WIN32)
  set(VCPKG_BINARIES "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
  install(FILES "${VCPKG_BINARIES}/openvr_api.dll" DESTINATION ".")
elseif(UNIX)
    set(VCPKG_BINARIES "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/bin")
    install(FILES "${VCPKG_BINARIES}/libopenvr_api.so" DESTINATION ".")
endif()

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
