# CMake toolchain definition for STM32CubeIDE

set (CMAKE_SYSTEM_PROCESSOR "arm" CACHE STRING "")
set (CMAKE_SYSTEM_NAME "Generic" CACHE STRING "")

# Skip link step during toolchain validation.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Specify toolchain. NOTE When building from inside STM32CubeIDE the location of the toolchain is resolved by the "MCU Toolchain" project setting (via PATH).
# For command-line builds, specify the full path to the toolchain below.

# Common STM32CubeIDE toolchain locations - update this to match your installation
set(TOOLCHAIN_PATHS
    "C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin"
    "C:/Users/HP/.platformio/packages/toolchain-gccarmnoneeabi/bin"
    "C:/Users/HP/.platformio/packages/toolchain-gccarmnoneeabi@1.70201.0/bin"
)

# Try to find the toolchain
set(TOOLCHAIN_BIN_DIR "")
foreach(TOOLCHAIN_PATH ${TOOLCHAIN_PATHS})
    if(EXISTS "${TOOLCHAIN_PATH}/arm-none-eabi-gcc.exe")
        set(TOOLCHAIN_BIN_DIR ${TOOLCHAIN_PATH})
        message(STATUS "Found ARM toolchain at: ${TOOLCHAIN_BIN_DIR}")
        break()
    endif()
endforeach()

# If not found in predefined paths, try to use PATH
if(TOOLCHAIN_BIN_DIR STREQUAL "")
    find_program(ARM_GCC_COMPILER arm-none-eabi-gcc)
    if(ARM_GCC_COMPILER)
        get_filename_component(TOOLCHAIN_BIN_DIR ${ARM_GCC_COMPILER} DIRECTORY)
        message(STATUS "Found ARM toolchain in PATH at: ${TOOLCHAIN_BIN_DIR}")
    else()
        message(WARNING "ARM toolchain not found! Please add the toolchain path to TOOLCHAIN_PATHS in cubeide-gcc.cmake")
    endif()
endif()

# Set toolchain programs
if(NOT TOOLCHAIN_BIN_DIR STREQUAL "")
    set(TOOLCHAIN_PREFIX   "${TOOLCHAIN_BIN_DIR}/arm-none-eabi-")
else()
    set(TOOLCHAIN_PREFIX   "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PREFIX}gcc.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++.exe")
set(CMAKE_AR           "${TOOLCHAIN_PREFIX}ar.exe")
set(CMAKE_LINKER       "${TOOLCHAIN_PREFIX}ld.exe")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_PREFIX}objcopy.exe")
set(CMAKE_RANLIB       "${TOOLCHAIN_PREFIX}ranlib.exe")
set(CMAKE_SIZE         "${TOOLCHAIN_PREFIX}size.exe")
set(CMAKE_STRIP        "${TOOLCHAIN_PREFIX}strip.exe")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)