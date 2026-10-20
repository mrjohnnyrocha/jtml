// Reduced use of the real CLI stream-redirection guard. Two writers start at
// a deterministic barrier. 'guarded' models the existing adapter lock policy.
// A sanitizer failure here proves the logging mechanism, not historical crash
// attribution. Run each mode in an isolated process with a hard timeout.
#include "cli/util.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <iostream>

int main(int argc, char** argv) {
    const bool guarded = argc > 1 && std::string(argv[1]) == "guarded";
    std::mutex barrierMutex, outputMutex;
    std::condition_variable condition;
    int arrived = 0;
    {
        jtml::cli::SilenceStdout silence;
        auto writer = [&] {
            {
                std::unique_lock<std::mutex> lock(barrierMutex);
                ++arrived;
                condition.notify_all();
                condition.wait(lock, [&] { return arrived == 2; });
            }
            const std::string block(16384, 'x');
            for (int i = 0; i < 32; ++i) {
                if (guarded) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << block << '\n';
                } else std::cout << block << '\n';
            }
        };
        std::thread thread(writer);
        writer();
        thread.join(); // no execution/output lock held while joining
    }
    std::cout << "completed " << (guarded ? "guarded" : "unguarded") << '\n';
}
