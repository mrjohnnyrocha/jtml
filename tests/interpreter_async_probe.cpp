// Every mode is a separate CTest subprocess with a hard timeout. All ordering
// comes from condition variables or execution locks, never sleep-based races.
#include "jtml/interpreter.h"
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <cstdlib>

static void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, released = false;
    void enter() {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_all();
        require(cv.wait_for(lock, std::chrono::seconds(5), [&] { return released; }), "gate release timeout");
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        require(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }), "gate entry timeout");
    }
    void release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        cv.notify_all();
    }
};
struct CallbackReturn : ReturnStatementNode {
    std::function<void()> callback;
    mutable int displays = 0;
    explicit CallbackReturn(std::function<void()> value) : callback(std::move(value)) {
        expr = std::make_unique<NumberLiteralExpressionStatementNode>(Token{TokenType::NUMBER_LITERAL,"7",0,1,1});
    }
    std::string toString() const override {
        if (++displays == 2) callback(); // first display belongs to caller's diagnostic
        return "callback return";
    }
};
struct InterpreterAsyncProbe {
    static void serialized(Interpreter& interpreter, const std::function<void()>& callback) {
        std::lock_guard<JTML::LocalServerExecutionMutex> lock(interpreter.executionMutex);
        callback();
    }
    static void stopping(Interpreter& interpreter) {
        std::unique_lock<std::mutex> lock(interpreter.asyncMutex);
        require(interpreter.asyncCondition.wait_for(lock, std::chrono::seconds(5),
            [&] { return interpreter.asyncStopping; }), "destructor did not close admission");
    }
    static void frontend(Interpreter& interpreter, std::function<void(const std::string&)> callback) {
        interpreter.renderer->setFrontendCallback(std::move(callback));
    }
    static void broadcast(Interpreter& interpreter) {
        interpreter.renderer->sendBindingUpdate("probe", "7");
    }
};
static std::string invoke(Interpreter& interpreter, const std::shared_ptr<JTML::Environment>& env,
                          const std::string& name) {
    FunctionCallExpressionStatementNode call(name, {});
    return interpreter.evaluateExpression(&call, env)->toString();
}
static void define(Interpreter& interpreter, const std::string& name, std::function<void()> callback,
                   bool async = true) {
    InterpreterAsyncProbe::serialized(interpreter, [&] {
        auto env = interpreter.getCurrentEnvironment();
        std::vector<std::unique_ptr<ASTNode>> body;
        body.push_back(std::make_unique<CallbackReturn>(std::move(callback)));
        env->defineFunction({env->instanceID,name}, std::make_shared<JTML::Function>(
            name, std::vector<Parameter>{}, "", std::move(body), env, async));
    });
}
static void rejected(const std::function<void()>& call, const std::string& text) {
    bool found = false;
    try { call(); } catch (const std::exception& e) { found = std::string(e.what()).find(text) != std::string::npos; }
    require(found, "missing diagnostic: " + text);
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected mode");
        const std::string mode = argv[1];
        JtmlTranspiler transpiler;
        if (mode == "locked-destruction" || mode == "self-destruction") {
            // The parent harness checks this exact diagnostic exit, with a hard
            // timeout. No core dump/retry turns unsupported destruction into PASS.
            std::set_terminate([] { std::_Exit(86); });
            auto* raw = new Interpreter(transpiler, InterpreterConfig{0, false});
            if (mode == "self-destruction") {
                auto env = raw->getCurrentEnvironment();
                define(*raw, "destroy", [raw] { delete raw; });
                invoke(*raw, env, "destroy");
                raw->waitForAsync();
            } else {
                std::lock_guard<JTML::LocalServerExecutionMutex> lock(JTML::localServerExecutionMutex());
                delete raw;
            }
            return 1; // rejection must terminate before this point
        }
        if (mode == "early-stop") {
            JTML::WebSocketServer server;
            server.stop();
            server.run(0); // a stop requested before startup must not be lost
            std::cout << "PASS async early-stop\n";
            return 0;
        }
        if (mode == "shutdown" || mode == "repeated") {
            const int count = mode == "repeated" ? 40 : 1;
            for (int i = 0; i < count; ++i) {
                Gate gate;
                std::atomic<bool> completed{false}, destroyed{false};
                std::atomic<int> descendants{0};
                auto interpreter = std::make_unique<Interpreter>(transpiler, InterpreterConfig{0, mode == "shutdown"});
                auto* raw = interpreter.get();
                auto env = raw->getCurrentEnvironment();
                define(*raw, "leaf", [&] { ++descendants; });
                define(*raw, "hold", [&] {
                    gate.enter();
                    if (mode == "shutdown") invoke(*raw, env, "leaf");
                    completed = true;
                });
                require(invoke(*raw, env, "hold").empty(), "async call must return before body completes");
                gate.wait();
                std::thread destroyer([&] { interpreter.reset(); destroyed = true; });
                InterpreterAsyncProbe::stopping(*raw);
                require(!destroyed && !completed, "destruction escaped active worker");
                gate.release();
                destroyer.join();
                require(completed && destroyed, "destruction did not drain worker");
                require(descendants == (mode == "shutdown" ? 1 : 0), "shutdown lost nested async task");
            }
        } else {
            Interpreter interpreter(transpiler, InterpreterConfig{0, mode == "cancel" || mode == "callbacks"});
            auto env = interpreter.getCurrentEnvironment();
            if (mode == "execute") {
                const auto caller = std::this_thread::get_id();
                int leaves = 0;
                define(interpreter, "leaf", [&] { require(std::this_thread::get_id() != caller, "async made synchronous"); ++leaves; });
                define(interpreter, "root", [&] { require(invoke(interpreter, env, "leaf").empty(), "nested async result"); });
                require(invoke(interpreter, env, "root").empty(), "async dispatch result");
                interpreter.waitForAsync();
                require(leaves == 1, "nested async work not drained");
            } else if (mode == "cancel") {
                int executed = 0;
                {
                    std::lock_guard<JTML::LocalServerExecutionMutex> lock(JTML::localServerExecutionMutex());
                    define(interpreter, "pending", [&] { ++executed; });
                    invoke(interpreter, env, "pending");
                    interpreter.reset();
                    require(interpreter.getCurrentEnvironment() != env, "reset did not replace state");
                }
                rejected([&] { interpreter.waitForAsync(); }, "cancelled by reset/reload");
                require(executed == 0, "old task entered replacement program");
                interpreter.waitForAsync(); // errors consumed exactly once
            } else if (mode == "reset" || mode == "reload" || mode == "snapshots") {
                Gate gate, request;
                std::atomic<bool> completed{false};
                define(interpreter, "hold", [&] { gate.enter(); completed = true; });
                invoke(interpreter, env, "hold");
                gate.wait();
                std::exception_ptr error;
                std::thread reader([&] {
                    try {
                        { std::lock_guard<std::mutex> lock(request.mutex); request.entered = true; request.cv.notify_all(); }
                        if (mode == "reset") interpreter.reset();
                        else if (mode == "reload") {
                            std::vector<std::unique_ptr<ASTNode>> empty;
                            interpreter.reload(empty);
                        } else {
                            require(nlohmann::json::parse(interpreter.getStateJSON()).is_structured(), "snapshot JSON");
                            require(nlohmann::json::parse(interpreter.getBindingsJSON()).is_structured(), "snapshot JSON");
                            require(nlohmann::json::parse(interpreter.getComponentsJSON()).is_structured(), "snapshot JSON");
                        }
                        require(completed, "state operation overlapped active async frame");
                    } catch (...) { error = std::current_exception(); }
                });
                request.wait();
                gate.release();
                reader.join();
                interpreter.waitForAsync();
                if (error) std::rethrow_exception(error);
                if (mode != "snapshots") require(interpreter.getCurrentEnvironment() != env, "replacement failed");
            } else if (mode == "callbacks") {
                int callbacks = 0;
                define(interpreter, "inner", [] {}, false);
                InterpreterAsyncProbe::frontend(interpreter, [&](const std::string&) {
                    // Mirrors WebSocket broadcast's recursive adapter acquisition.
                    std::lock_guard<JTML::LocalServerExecutionMutex> lock(JTML::localServerExecutionMutex());
                    require(nlohmann::json::parse(interpreter.getStateJSON()).is_structured(), "snapshot JSON");
                    require(invoke(interpreter, interpreter.getCurrentEnvironment(), "inner") == "7", "reentrant sync call");
                    rejected([&] { interpreter.reset(); }, "active function callback");
                    rejected([&] { interpreter.waitForAsync(); }, "holding an execution lock");
                    ++callbacks;
                });
                define(interpreter, "emit", [&] { InterpreterAsyncProbe::broadcast(interpreter); });
                invoke(interpreter, env, "emit");
                interpreter.waitForAsync();
                require(callbacks == 1, "callback delivery");
                { std::lock_guard<JTML::LocalServerExecutionMutex> lock(JTML::localServerExecutionMutex());
                  rejected([&] { interpreter.waitForAsync(); }, "holding an execution lock"); }
            } else if (mode == "cross-wait") {
                JtmlTranspiler otherTranspiler;
                Interpreter other(otherTranspiler, InterpreterConfig{0, false});
                define(interpreter, "cross", [&] {
                    rejected([&] { other.waitForAsync(); }, "holding an execution lock");
                });
                invoke(interpreter, env, "cross");
                interpreter.waitForAsync();
            } else if (mode == "errors") {
                define(interpreter, "fail", [] { throw std::runtime_error("deliberate worker failure"); });
                invoke(interpreter, env, "fail");
                rejected([&] { interpreter.waitForAsync(); }, "deliberate worker failure");
                interpreter.waitForAsync();
                // Synchronous arity failure in an async task is also observable.
                auto function = env->getFunction({env->instanceID,"fail"});
                function->parameters.push_back(Parameter{"required", ""});
                invoke(interpreter, env, "fail");
                rejected([&] { interpreter.waitForAsync(); }, "expects 1 arguments");
            } else throw std::runtime_error("unknown mode");
        }
        std::cout << "PASS async " << mode << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
