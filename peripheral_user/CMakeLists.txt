cmake_minimum_required(VERSION 3.8.2)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(NONE)

target_sources(app PRIVATE
  src/main.c
  src/services/ble_user.c
)

zephyr_library_include_directories($ENV{ZEPHYR_BASE}/samples/bluetooth)
