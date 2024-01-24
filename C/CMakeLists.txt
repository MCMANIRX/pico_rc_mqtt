cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico_w)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)
include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)


project(rc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico_w)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(rc
  #src/rc_mqtt.c
  src/main.C
  #src/wifi_temp2.c
  #src/motor_test.c
 



)

target_include_directories(rc PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts


        
)

target_sources(rc PRIVATE ${PICO_SDK_PATH}/lib/lwip/src/apps/mqtt/mqtt.c)


target_link_libraries(rc
        pico_stdlib              
        pico_cyw43_arch_lwip_threadsafe_background 
        pico_multicore
        hardware_spi
        hardware_vreg # overclock
        hardware_i2c
        hardware_pwm
        pico_lwip_mqtt
        pico_lwip_arch

)

# enable usb output, disable uart output
pico_enable_stdio_usb(rc 1)
pico_enable_stdio_uart(rc 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(rc)
