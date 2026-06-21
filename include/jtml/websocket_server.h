#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <functional>
#include <string>
#include <unordered_set>
#include <iostream>
#include <atomic>
#include <exception>

namespace JTMLInterpreter {

    typedef websocketpp::server<websocketpp::config::asio> server;
    typedef websocketpp::connection_hdl connection_hdl;

    // Hash function for connection_hdl
    struct ConnectionHash {
        std::size_t operator()(const connection_hdl& hdl) const noexcept {
            return std::hash<void*>()(hdl.lock().get());
        }
    };

    // Equality function for connection_hdl
    struct ConnectionEqual {
        bool operator()(const connection_hdl& lhs, const connection_hdl& rhs) const noexcept {
            return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
        }
    };

    class WebSocketServer {
    public:
        WebSocketServer() {
            // Initialize Asio
            wsServer.init_asio();
            wsServer.clear_access_channels(websocketpp::log::alevel::all);
            wsServer.clear_error_channels(websocketpp::log::elevel::all);

            // Register handler callbacks
            wsServer.set_open_handler(std::bind(&WebSocketServer::onOpen, this, std::placeholders::_1));
            wsServer.set_close_handler(std::bind(&WebSocketServer::onClose, this, std::placeholders::_1));
            wsServer.set_message_handler(std::bind(&WebSocketServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
        }

        // Start the server on a given port.
        // SO_REUSEADDR is enabled so that tools like `jtml tutorial`, which
        // may restart the process or rapidly recreate servers during
        // development, don't fail with "address already in use" due to
        // TIME_WAIT sockets from a previous run.
        void run(uint16_t port) {
            try {
                wsServer.set_reuse_addr(true);
                wsServer.listen(port);
                wsServer.start_accept();
                isRunning = true;
                std::cout << "[WebSocket] Server started on port " << port << "\n";
                wsServer.run();
            } catch (const websocketpp::exception& e) {
                std::cerr << "[WebSocket] Server error: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[WebSocket] Server error: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[WebSocket] Server error: unknown exception\n";
            }
            isRunning = false;
        }

        void stop() {
            if (!isRunning) {
                return;
            }
            try {
                wsServer.stop_listening();
                wsServer.stop();
            } catch (const websocketpp::exception& e) {
                std::cerr << "[WebSocket] Stop failed: " << e.what() << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[WebSocket] Stop failed: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[WebSocket] Stop failed: unknown exception\n";
            }
        }

        // Set the callback to handle incoming messages
        void setMessageCallback(std::function<void(const std::string&, connection_hdl)> callback) {
            messageCallback = callback;
        }

        void setOpenCallback(std::function<void(connection_hdl)> callback) {
            openCallback = callback;
        }

        // Send a message to a specific connection
        void sendMessage(connection_hdl hdl, const std::string& message) {
            try {
                wsServer.send(hdl, message, websocketpp::frame::opcode::text);
            }
            catch (const websocketpp::exception& e) {
                std::cerr << "[WebSocket] Send failed: " << e.what() << "\n";
            }
        }

        // Broadcast a message to all connected clients
        void broadcastMessage(const std::string& message) {
            for (const auto& hdl : connections) {
                wsServer.send(hdl, message, websocketpp::frame::opcode::text);
            }
        }

    private:
        server wsServer;
        std::atomic<bool> isRunning{false};
        std::unordered_set<connection_hdl, ConnectionHash, ConnectionEqual> connections;
        std::function<void(const std::string&, connection_hdl)> messageCallback;
        std::function<void(connection_hdl)> openCallback;

        void onOpen(connection_hdl hdl) {
             connections.insert(hdl);
            std::cout << "[WebSocket] Client connected.\n";
            if (openCallback) {
                openCallback(hdl);
            }
        }

        void onClose(connection_hdl hdl) {
            connections.erase(hdl);
            std::cout << "[WebSocket] Client disconnected.\n";
        }

        void onMessage(connection_hdl hdl, server::message_ptr msg) {
            if (messageCallback) {
                messageCallback(msg->get_payload(), hdl);
            }
        }
    };

}
