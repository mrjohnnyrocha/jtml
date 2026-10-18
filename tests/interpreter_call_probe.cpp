// Each mode runs in its own CTest subprocess with a hard timeout: a deadlock
// must fail CI instead of hanging its test runner.
#include "jtml/interpreter.h"
#include <stdexcept>
#include <condition_variable>
#include <chrono>
#include <sstream>
#include <thread>

static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

struct Gate {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool released = false;
};

// Pause only after executeFunction has entered its frame; no browser, port,
// timer race, or production-only hook is needed to test overlapping entries.
struct GatedReturn : ReturnStatementNode {
    Gate& gate;
    mutable int displays = 0;
    explicit GatedReturn(Gate& value) : gate(value) {
        expr = std::make_unique<NumberLiteralExpressionStatementNode>(
            Token{TokenType::NUMBER_LITERAL, "7", 0, 1, 1});
    }
    std::string toString() const override {
        // The evaluator prints the body once before entering executeFunction.
        if (++displays == 1) return "gated return";
        std::unique_lock<std::mutex> lock(gate.mutex);
        gate.entered = true;
        gate.condition.notify_all();
        if (!gate.condition.wait_for(lock, std::chrono::seconds(3), [&] { return gate.released; }))
            throw std::runtime_error("probe gate timed out");
        return "gated return";
    }
};

static std::string invoke(Interpreter& interpreter,
                          const std::shared_ptr<JTML::Environment>& env,
                          const std::string& name) {
    FunctionCallExpressionStatementNode expression(name, {});
    return interpreter.evaluateExpression(&expression, env)->toString();
}

static void overlap(Interpreter& interpreter, const std::shared_ptr<JTML::Environment>& env,
                    bool sameInterpreter) {
    JtmlTranspiler otherTranspiler;
    Interpreter other(otherTranspiler, InterpreterConfig{8080, false});
    // A second instance has its own declarations/environment and call depth.
    other.interpret("function inner()\\\\\n return 7\\\\\n\\\\\n");
    auto otherEnv = other.getCurrentEnvironment();
    Gate gate;
    std::vector<std::unique_ptr<ASTNode>> body;
    body.push_back(std::make_unique<GatedReturn>(gate));
    env->defineFunction({env->instanceID, "hold"}, std::make_shared<JTML::Function>(
        "hold", std::vector<Parameter>{}, "", std::move(body), env));
    std::exception_ptr workerError;
    std::thread worker([&] {
        try { require(invoke(interpreter, env, "hold") == "7", "held result"); }
        catch (...) { workerError = std::current_exception(); }
    });
    std::exception_ptr mainError;
    try {
        {
            std::unique_lock<std::mutex> lock(gate.mutex);
            require(gate.condition.wait_for(lock, std::chrono::seconds(2), [&] { return gate.entered; }), "frame entry timeout");
        }
        if (sameInterpreter) {
            bool rejected = false;
            try { invoke(interpreter, env, "inner"); }
            catch (const std::exception& e) {
                rejected = std::string(e.what()).find("Concurrent function execution") != std::string::npos;
            }
            require(rejected, "overlapping same-interpreter call was not rejected");
        } else {
            require(invoke(other, otherEnv, "inner") == "7", "independent interpreter result");
        }
    } catch (...) { mainError = std::current_exception(); }
    {
        std::lock_guard<std::mutex> lock(gate.mutex);
        gate.released = true;
    }
    gate.condition.notify_all();
    worker.join();
    if (mainError) std::rethrow_exception(mainError);
    if (workerError) std::rethrow_exception(workerError);
    require(invoke(interpreter, env, "inner") == "7", "owner was not released");
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected probe mode");
        JtmlTranspiler transpiler;
        Interpreter interpreter(transpiler, InterpreterConfig{8080, false});
        Lexer lexer(R"JTML(
function inner()\\
    return 7\\
\\
function outer()\\
    return inner()\\
\\
function count(n)\\
    if (n <= 0)\\
        return 1\\
    \\
    else\\
        return count(n - 1) + 1\\
    \\
\\
)JTML");
        auto tokens = lexer.tokenize();
        require(lexer.getErrors().empty(), "probe lexer errors");
        Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        for (const auto& error : parser.getErrors()) std::cerr << error << "\n";
        require(parser.getErrors().empty(), "probe parser errors");
        interpreter.interpret(program);
        auto original = interpreter.getCurrentEnvironment();
        auto call = [&](const std::string& name, const std::string& number = "") {
            std::vector<std::unique_ptr<ExpressionStatementNode>> args;
            if (!number.empty()) args.push_back(std::make_unique<NumberLiteralExpressionStatementNode>(
                Token{TokenType::NUMBER_LITERAL, number, 0, 1, 1}));
            FunctionCallExpressionStatementNode expression(name, std::move(args));
            return interpreter.evaluateExpression(&expression, original)->toString();
        };
        const std::string mode = argv[1];
        if (mode == "simple") require(call("inner") == "7", "simple result");
        else if (mode == "nested") require(call("outer") == "7", "nested result");
        else if (mode == "recursion") require(call("count", "16") == "17", "bounded recursion result");
        else if (mode == "independent" || mode == "concurrent") {
            overlap(interpreter, original, mode == "concurrent");
        }
        else if (mode == "excessive") {
            for (int repeat = 0; repeat < 3; ++repeat) {
                bool failed = false;
                try { call("count", "10000"); }
                catch (const std::exception& e) {
                    failed = std::string(e.what()).find("Maximum recursion depth exceeded") != std::string::npos;
                }
                require(failed, "excessive recursion must fail explicitly");
                require(interpreter.getCurrentEnvironment() == original, "exception leaked function environment");
                require(call("count", "16") == "17", "depth was not restored");
            }
        } else throw std::runtime_error("unknown probe mode");
        require(interpreter.getCurrentEnvironment() == original, "return leaked function environment");
        std::ostringstream diagnostics;
        auto* previous = std::cerr.rdbuf(diagnostics.rdbuf());
        interpreter.interpret("return 9\\\\\n");
        std::cerr.rdbuf(previous);
        require(diagnostics.str().find("Return statement outside function context") != std::string::npos,
                "function context leaked past return/exception");
        std::cout << "PASS " << mode << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
