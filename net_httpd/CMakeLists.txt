cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

set(SHORT_NAME net_http_sample)
project(${SHORT_NAME})
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Net example")
set(VITA_TITLEID  "VSDK00032")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(
  ../common
)

add_executable(${SHORT_NAME}
  src/main.c
)

target_link_libraries(${SHORT_NAME}
  m
  SceCtrl_stub
  SceAudio_stub 
  SceDisplay_stub
  SceShellSvc_stub
  ScePower_stub
  SceSysmodule_stub
  SceNet_stub
  SceNetCtl_stub
  SceJpegEnc_stub
  SceCamera_stub
)

vita_create_self(${SHORT_NAME}.self ${SHORT_NAME} UNSAFE)
vita_create_vpk(${SHORT_NAME}.vpk ${VITA_TITLEID} ${SHORT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
