cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(GxmDumper)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTAIPOOL_AS_STDLIB -g -Wall -O3 -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (NOT ${RELEASE})
  add_definitions(-DENABLE_LOGGING)
endif()

add_executable(GxmDumper
  main.c
)

target_link_libraries(GxmDumper
  taihen_stub
  SceLibKernel_stub_weak
  SceDisplay_stub_weak
  SceAppMgr_stub_weak
  SceCtrl_stub_weak
  SceIofilemgr_stub_weak
  SceGxm_stub_weak
  SceSysmem_stub_weak
  SceRtc_stub_weak
  SceShellSvc_stub_weak
  k
  gcc
  kuio_stub
)

set_target_properties(GxmDumper
  PROPERTIES LINK_FLAGS "-nostdlib"
)

vita_create_self(GxmDumper.suprx GxmDumper
  CONFIG ${CMAKE_SOURCE_DIR}/GxmDumper.yml
)
