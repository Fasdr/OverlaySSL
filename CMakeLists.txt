cmake_minimum_required(VERSION 3.16)
project(GameOverlay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON) 
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(X11 REQUIRED)

# --- 1. THE OVERLAY APP ---
add_executable(OverlayApp main.cpp)
# CHANGED: Added X11 here
target_link_libraries(OverlayApp PRIVATE Qt6::Widgets xdo X11)

# --- 2. THE SPY LIBRARY (.so) ---
add_library(spy SHARED spy.cpp)
set_target_properties(spy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(spy PRIVATE X11 pthread)

# --- 3. GENERATE INSTRUCTION FILE ---
set(LIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/libspy.so")
set(APP_PATH "${CMAKE_CURRENT_BINARY_DIR}/OverlayApp")

file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/steam_setup.txt" CONTENT 
"======================================================
   BUILD COMPLETE - STEAM CONFIGURATION
======================================================
Copy the line below into Steam -> Properties -> Launch Options:

LD_PRELOAD=\"${LIB_PATH}\" %command% & ${APP_PATH}

Note: The overlay will auto-close when the game exits.
======================================================
")