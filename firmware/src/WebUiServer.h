#pragma once

#include "third_party/ESPAsyncWebServer/src/ESPAsyncWebServer.h"

#include "RuntimeState.h"

namespace babot {

class DebugLogger;

class WebUiServer {
 public:
  WebUiServer(SharedState &sharedState, DebugLogger &debugLogger);

  bool begin();

 private:
  static void taskEntryPoint(void *parameter);

  void taskLoop();
  void pollDebugOutput();
  void configureRoutes();
  void handleIndex(AsyncWebServerRequest *request);
  void handleState(AsyncWebServerRequest *request);
  void handleWebSocketEvent(AsyncWebSocket *server,
                            AsyncWebSocketClient *client,
                            AwsEventType type,
                            void *arg,
                            uint8_t *payload,
                            size_t length);
  void broadcastState();
  void sendStateToClient(AsyncWebSocketClient *client);
  void sendError(AsyncWebSocketClient *client, const char *message);
  void sendAck(AsyncWebSocketClient *client, const char *message);
  bool parseMessage(const uint8_t *payload, size_t length, UiCommand &command, String &errorMessage);

  SharedState &sharedState_;
  DebugLogger &debugLogger_;
  AsyncWebServer server_;
  AsyncWebSocket webSocket_;
  TaskHandle_t taskHandle_;
  unsigned long lastBroadcastMs_;
};

}
