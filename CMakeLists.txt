cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable pointing to your SDK path!")
  endif()
endif()

project(PSV_Cleaner VERSION 1.0 LANGUAGES C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Info app
set(VITA_APP_NAME "PSV Cleaner")
set(VITA_TITLEID "CLEAN0000")
set(VITA_VERSION "01.00")

# Flags C
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -fno-lto")

# Include directories
include_directories(${PROJECT_SOURCE_DIR})

# Eseguibile
add_executable(${PROJECT_NAME}
    psv_cleaner_ui.c
    psv_cleaner_core.c
)

# Librerie VitaSDK
target_link_libraries(${PROJECT_NAME}
    vita2d
    SceCtrl_stub
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    ScePgf_stub
    SceAppMgr_stub
    SceCommonDialog_stub
    png16
    z
    m
)

# Genera eboot.bin
vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

# Genera VPK e includi risorse
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}

    # Icone e LiveArea
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml

           # Risorse interne
           FILE resources/Reihen.png resources/Reihen.png
)
