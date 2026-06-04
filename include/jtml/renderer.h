// renderer.h

#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include "jtml/value.h"




namespace JTMLInterpreter {


    class Renderer {
    public:
        Renderer() {

            }

        // Destructor
        ~Renderer() {
            std::cout << "[DEBUG] Renderer destroyed\n";
        }
        // Set the callback to communicate with the frontend (e.g., WebSocket sender)
        void setFrontendCallback(std::function<void(const std::string&)> callback) {
            sendToFrontend = callback;
        }

        // Inject initial HTML into the DOM
        void injectHTML(const std::string& htmlContent) {
            std::string message = "{\"type\": \"injectHTML\", \"content\": \"" + escapeJSON(htmlContent) + "\"}";
            sendToFrontend(message);
        }

        // Update content bindingsMap
        void sendBindingUpdate(const std::string& elementId, const std::string& newValue) {
            std::string message = "{\"type\": \"updateBinding\", \"elementId\": \"" + elementId + "\", \"value\": \"" + escapeJSON(newValue) + "\"}";
            sendToFrontend(message);
        }

        // Update attribute bindingsMap
        void sendAttributeUpdate(const std::string& elementId, const std::string& attribute, const std::string& newValue) {
            std::string message = "{\"type\": \"updateAttribute\", \"elementId\": \"" + elementId + "\", \"attribute\": \"" + attribute + "\", \"value\": \"" + escapeJSON(newValue) + "\"}";
            sendToFrontend(message);
        }

        // Send batch updates
        void sendBatchBindingUpdates(const std::unordered_map<std::string, std::string>& contentUpdates,
                                     const std::unordered_map<std::string, std::pair<std::string, std::string>>& attributeUpdates) {
            std::string message = "{\"type\": \"batchUpdate\", \"contentUpdates\": {";

            bool first = true;
            for (const auto& [id, value] : contentUpdates) {
                if (!first) message += ",";
                message += "\"" + id + "\": \"" + escapeJSON(value) + "\"";
                first = false;
            }
            message += "}, \"attributeUpdates\": {";

            first = true;
            for (const auto& [id, attrPair] : attributeUpdates) {
                if (!first) message += ",";
                message += "\"" + id + "\": {\"attribute\": \"" + attrPair.first + "\", \"value\": \"" + escapeJSON(attrPair.second) + "\"}";
                first = false;
            }
            message += "}}";

            sendToFrontend(message);
        }


        // Send error messages
        void sendError(const std::string& errorMessage) {
            std::string message = "{\"type\": \"error\", \"message\": \"" + escapeJSON(errorMessage) + "\"}";
            sendToFrontend(message);
        }

        // Send acknowledgment messages
        void sendAcknowledgment(const std::string& messageId) {
            std::string message = "{\"type\": \"acknowledgment\", \"messageId\": \"" + messageId + "\"}";
            sendToFrontend(message);
        }

    private:
        // Communication with frontend (e.g., WebSocket client)
        std::mutex bindingsMutex;
        std::function<void(const std::string&)> sendToFrontend;


        // Simple JSON escaping function
        std::string escapeJSON(const std::string& text) {
            std::string escaped;
            for (char c : text) {
                switch (c) {
                    case '\"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\b': escaped += "\\b"; break;
                    case '\f': escaped += "\\f"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default:
                        if ('\x00' <= c && c <= '\x1f') {
                            escaped += "\\u" + to_hex(c);
                        } else {
                            escaped += c;
                        }
                }
            }
            return escaped;
        }

        std::string to_hex(char c) {
            const char* hex_digits = "0123456789ABCDEF";
            std::string hex = "";
            hex += hex_digits[(c >> 4) & 0xF];
            hex += hex_digits[c & 0xF];
            return hex;
        }
    };

}
