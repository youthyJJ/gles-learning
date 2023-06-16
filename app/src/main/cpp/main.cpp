#include <jni.h>

#include "AndroidOut.h"
#include "Renderer.h"

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

extern "C" {

#include <game-activity/native_app_glue/android_native_app_glue.c>

/*!
 * Handles commands sent to this Android application
 * @param pApp the app the commands are coming from
 * @param cmd the command to handle
 */
void handle_cmd_callback(android_app *pApp, int32_t cmd) {
    switch (cmd) {

        // A new window is created, associate a renderer with it.
        // You may replace this with a "game" class if that suits your needs.
        // Remember to change all instances of userData
        case APP_CMD_INIT_WINDOW:
            // if you change the class here as a reinterpret_cast is dangerous this in the
            // android_main function and the APP_CMD_TERM_WINDOW handler case.
            pApp->userData = new Renderer(pApp);
            break;

            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
        case APP_CMD_TERM_WINDOW:
            // We have to check if userData is assigned just in case this comes in really quickly
            if (nullptr != pApp->userData) {
                auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pApp->userData = nullptr;
                delete pRenderer;
            }
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle;
 * not handled events are passed back to OS for further processing.
 * For this example case, only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_callback(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Can be removed, useful to ensure your code is running
    debug << "Welcome to android_main" << std::endl;

    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd_callback;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_callback);

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    int outEvents;
    android_poll_source *outData;
    do {
        // Process all pending events before running game logic.
        int result = ALooper_pollAll(
                0,
                nullptr,
                &outEvents,
                (void **) &outData
        );
        if (result >= 0 && nullptr != outData) {
            outData->process(pApp, outData);
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (nullptr != pApp->userData) {

            // We know that our user data is a Renderer, so reinterpret cast it. 
            // If you change your user data remember to change it here
            auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);

            // Process game input
            pRenderer->handleInput();

            // Render a frame
            pRenderer->render();
        }
    } while (!pApp->destroyRequested);
}

}