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
target_link_libraries(OverlayApp PRIVATE Qt6::Widgets xdo)

# --- 2. THE SPY LIBRARY (.so) ---
add_library(spy SHARED spy.cpp)
set_target_properties(spy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(spy PRIVATE X11 pthread)

# --- 3. STEAM LAUNCH OPTIONS GENERATOR ---
set(LIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/libspy.so")
set(APP_PATH "${CMAKE_CURRENT_BINARY_DIR}/OverlayApp")

# Attached to 'spy' so it prints only after the .so is finished
add_custom_command(TARGET spy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "======================================================"
    COMMAND ${CMAKE_COMMAND} -E echo "   BUILD COMPLETE - STEAM CONFIGURATION"
    COMMAND ${CMAKE_COMMAND} -E echo "======================================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Copy the line below into Steam -> Properties -> Launch Options:"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "LD_PRELOAD=\"${LIB_PATH}\" %command% & ${APP_PATH}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Note: The overlay will auto-close when the game exits."
    COMMAND ${CMAKE_COMMAND} -E echo "======================================================"
    VERBATIM
)