#pragma once

#include <Print.h>

class Logger {
public:
    enum class Level { NONE, ERROR, INFO, DEBUG };

    explicit Logger(Print& output, Level level = Level::INFO)
        : output_(output), level_(level) {}

    void setLevel(Level level) { level_ = level; }
    Level level() const { return level_; }

    template<typename... Args>
    void printf(Level severity, const char* format, Args... args) {
        if (severity != Level::NONE && severity <= level_) {
            output_.printf(format, args...);
        }
    }

private:
    Print& output_;
    Level level_;
};
