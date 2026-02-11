#pragma once

#include <Windows.h>
#include <functional>
#include <memory>
#include <iostream>
#include <stdexcept>

namespace Bridge {
    /**
     * @brief The Static Entry Point required by the Windows API.
     * It unpacks the 'std::function' box and executes it.
     */
     static DWORD WINAPI Execute(LPVOID lpParam) {
        // Cast "blind" pointer back to tester member function
        auto* func = static_cast<std::function<void()>*>(lpParam);

        if (func) {
            try {
                (*func)(); // Run code inside lambda
            } catch (const std::runtime_error& e) {
                // Catch hardware/process errors from device.cpp
                std::cerr << "[Thread Error] Runtime Exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                // Catch all other standard exceptions
                std::cerr << "[Thread Error] Standard Exception: " << e.what() << std::endl;
            } catch (...) {
                // Catch-all for non-standard exceptions
                std::cerr << "[Thread Error] Unknown critical error occurred." << std::endl;
            }
            delete func; // Delete heap-allocated box to prevent memory leaks
        }

        return 0;
     }

     /**
     * @brief The Generic Launcher.
     * Use this to run any member function or logic in a background thread.
     */
    template<typename Callable>
    HANDLE start(Callable&& task) {
        // Create "box" on heap to ensure it survives thread start
        auto* taskPtr = new std::function<void()>(std::forward<Callable>(task));

        if (!taskPtr) return NULL;

        // Tell Windows to run static Execute and pass the box pointer
        HANDLE hThread = CreateThread(
            NULL,                   // Default security 
            0,                      // Default stack size
            Execute,                // Static bridge function
            taskPtr,                // Pointer to code box
            CREATE_SUSPENDED,       // Do not run immediately
            NULL                    // Do not need thread ID
        );

        if (!hThread) delete taskPtr;

        return hThread;
    }
}