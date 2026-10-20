// This probe also builds against pre-fix main: no new Interpreter API is used.
// A condition-variable barrier pins a real async call inside its function frame.
#include "jtml/interpreter.h"
#include <condition_variable>
#include <cstdlib>
#include <chrono>

struct BoundaryGate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, release = false;
};
struct BoundaryReturn : ReturnStatementNode {
    BoundaryGate& gate;
    mutable int displays = 0;
    explicit BoundaryReturn(BoundaryGate& g) : gate(g) {
        expr = std::make_unique<NumberLiteralExpressionStatementNode>(Token{TokenType::NUMBER_LITERAL,"7",0,1,1});
    }
    std::string toString() const override {
        if (++displays != 2) return "boundary return";
        std::unique_lock<std::mutex> lock(gate.mutex);
        gate.entered = true;
        gate.cv.notify_all();
        if (!gate.cv.wait_for(lock, std::chrono::seconds(5), [&] { return gate.release; })) std::_Exit(3);
        return "boundary return";
    }
};
int main() {
    JtmlTranspiler transpiler;
    // Live mode selects the same execution lock as HTTP/WS callbacks.
    Interpreter interpreter(transpiler, InterpreterConfig{0, true});
    BoundaryGate gate;
    auto env = interpreter.getCurrentEnvironment();
    std::vector<std::unique_ptr<ASTNode>> body;
    body.push_back(std::make_unique<BoundaryReturn>(gate));
    {
        // This is a borrowed Environment: fixture mutation must obey the same
        // adapter serialization as production callers (including startup logs).
        std::lock_guard lock(JTML::localServerExecutionMutex());
        env->defineFunction({env->instanceID,"hold"}, std::make_shared<JTML::Function>(
            "hold", std::vector<Parameter>{}, "", std::move(body), env, true));
    }
    FunctionCallExpressionStatementNode call("hold", {});
    interpreter.evaluateExpression(&call, env);
    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        if (!gate.cv.wait_for(lock, std::chrono::seconds(5), [&] { return gate.entered; })) std::_Exit(4);
    }
    auto& mutex = JTML::localServerExecutionMutex();
    if (mutex.try_lock()) {
        mutex.unlock();
        std::cerr << "FAIL: active async frame does not own the adapter execution lock; reset/snapshot can race it\n" << std::flush;
        // Deliberately avoid releasing the pinned old worker onto freed state.
        std::_Exit(1);
    }
    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.release = true;
    }
    gate.cv.notify_all();
    interpreter.getStateJSON(); // serializes after the pinned call completes
    {
        std::lock_guard lock(JTML::localServerExecutionMutex());
        std::cout << "PASS: active async frame excludes concurrent adapter state access\n";
    }
}
