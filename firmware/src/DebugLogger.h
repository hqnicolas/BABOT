#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "BoardConfig.h"

namespace babot {

struct DebugLoggerStats {
  uint32_t queued;
  uint32_t dropped;
  uint32_t flushed;
  uint32_t directFlushes;
  size_t pending;
};

class DebugLogger {
 public:
  DebugLogger();
  ~DebugLogger();

  bool begin(Stream &stream);
  bool tryLog(const char *message);
  bool tryLogf(const char *format, ...);
  size_t flushSome(size_t budget = kDebugFlushBudgetPerLoop);
  size_t flushDirect(size_t budget = kDebugFlushBudgetPerLoop);
  DebugLoggerStats stats() const;

 private:
  struct DebugMessage {
    char text[kDebugMessageCapacity];
  };

  bool enqueueMessage(const char *message);
  static void copyString(char *destination, size_t capacity, const char *source);

  QueueHandle_t queue_;
  Stream *stream_;
  volatile uint32_t queuedCount_;
  volatile uint32_t droppedCount_;
  volatile uint32_t flushedCount_;
  volatile uint32_t directFlushCount_;
};

}
