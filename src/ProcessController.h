#ifndef PROCESSCONTROLLER_H
#define PROCESSCONTROLLER_H

#include "ProcessManager.h"
#include <string>

enum class ProcessAction {
    PAUSE,      // SIGSTOP
    RESUME,     // SIGCONT
    TERMINATE   // SIGTERM
};

struct ControlResult {
    bool success;
    std::string message;
};

class ProcessController {
public:
    ProcessController() = default;

    ControlResult pauseProcess(pid_t pid);
    ControlResult resumeProcess(pid_t pid);
    ControlResult terminateProcess(pid_t pid, bool confirmed = false);

    void displayProcessDetails(pid_t pid, const ProcessManager& pm);
    bool isProtectedProcess(pid_t pid) const;

private:
    ControlResult sendSignal(pid_t pid, int signal, const std::string& actionName);
};

#endif // PROCESSCONTROLLER_H
