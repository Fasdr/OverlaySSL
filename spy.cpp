// Installation: g++ -shared -fPIC -O2 -o libspy.so spy.cpp -lX11 -lpthread

#include <X11/Xlib.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>  // for snprintf
#include <cstring> // for memset

// Control flag
std::atomic<bool> running{true};

// Function to send the signal WITH coordinates
void trigger_overlay(int x, int y) {
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/game_overlay_socket", sizeof(addr.sun_path) - 1);

    // Format message: "SHOW 1920 1080"
    char msg[64];
    snprintf(msg, sizeof(msg), "SHOW %d %d", x, y);

    sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
}

// Background thread
void poll_mouse_loop() {
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return;

    Window root = DefaultRootWindow(dpy);
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;

    bool is_pressed = false;
    auto press_start = std::chrono::steady_clock::now();
    bool overlay_triggered = false;

    // --- CONFIGURATION ---
    const int LONG_PRESS_MS = 1000; // 1 Second
    // ---------------------

    while (running) {
        if (XQueryPointer(dpy, root, &root_return, &child_return, 
                          &root_x, &root_y, &win_x, &win_y, &mask_return)) {
            
            // Check Left Mouse Button (Button1)
            bool current_press = (mask_return & Button1Mask);

            if (current_press) {
                if (!is_pressed) {
                    // Pressed just now
                    is_pressed = true;
                    press_start = std::chrono::steady_clock::now();
                    overlay_triggered = false;
                } else if (!overlay_triggered) {
                    // Still holding
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - press_start).count();
                    
                    if (duration > LONG_PRESS_MS) {
                        // Pass the current X and Y to the trigger
                        trigger_overlay(root_x, root_y);
                        overlay_triggered = true;
                    }
                }
            } else {
                // Released
                is_pressed = false;
                overlay_triggered = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    XCloseDisplay(dpy);
}

// Entry point
extern "C" __attribute__((constructor))
void on_load() {
    std::thread(poll_mouse_loop).detach();
}