#include "DebugLogger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace babot {

DebugLogger::DebugLogger()
    : queue_(nullptr),
      stream_(nullptr),
      queuedCount_(0),
      droppedCount_(0),
      flushedCount_(0),
      directFlushCount_(0) {}

DebugLogger::~DebugLogger() {
  if (queue_ != nullptr) {
    vQueueDelete(queue_);
  }
}

bool DebugLogger::begin(Stream &stream) {
  stream_ = &stream;
  if (queue_ == nullptr) {
    queue_ = xQueueCreate(kDebugQueueLength, sizeof(DebugMessage));
  }
  return queue_ != nullptr;
}

bool DebugLogger::tryLog(const char *message) {
  if (!kEnableDebugLogging) {
    return false;
  }

  if (message == nullptr) {
    return false;
  }

  return enqueueMessage(message);
}

bool DebugLogger::tryLogf(const char *format, ...) {
  if (!kEnableDebugLogging) {
    return false;
  }

  if (format == nullptr) {
    return false;
  }

  char body[kDebugMessageCapacity];
  va_list args;
  va_start(args, format);
  vsnprintf(body, sizeof(body), format, args);
  va_end(args);

  char message[kDebugMessageCapacity];
  snprintf(message, sizeof(message), "[%10lu] %s", millis(), body);
  return enqueueMessage(message);
}

size_t DebugLogger::flushSome(size_t budget) {
  if (queue_ == nullptr || stream_ == nullptr || budget == 0) {
    return 0;
  }

  size_t flushed = 0;
  DebugMessage message{};
  while (flushed < budget && xQueueReceive(queue_, &message, 0) == pdTRUE) {
    stream_->println(message.text);
    ++flushed;
    ++flushedCount_;
  }

  return flushed;
}

size_t DebugLogger::flushDirect(size_t budget) {
  const size_t flushed = flushSome(budget);
  if (flushed > 0) {
    ++directFlushCount_;
  }
  return flushed;
}

DebugLoggerStats DebugLogger::stats() const {
  DebugLoggerStats snapshot{};
  snapshot.queued = queuedCount_;
  snapshot.dropped = droppedCount_;
  snapshot.flushed = flushedCount_;
  snapshot.directFlushes = directFlushCount_;
  snapshot.pending = (queue_ == nullptr) ? 0u : static_cast<size_t>(uxQueueMessagesWaiting(queue_));
  return snapshot;
}

bool DebugLogger::enqueueMessage(const char *message) {
  if (queue_ == nullptr) {
    return false;
  }

  DebugMessage entry{};
  copyString(entry.text, sizeof(entry.text), message);
  if (xQueueSend(queue_, &entry, 0) != pdTRUE) {
    ++droppedCount_;
    return false;
  }

  ++queuedCount_;
  return true;
}

void DebugLogger::copyString(char *destination, size_t capacity, const char *source) {
  if (destination == nullptr || capacity == 0) {
    return;
  }

  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = '\0';
}

}
