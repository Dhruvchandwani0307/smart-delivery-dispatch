#pragma once
#include <stdexcept>
#include <string>

// ============================================================================
// Exceptions.h
// Domain specific exceptions. All derive from std::runtime_error so callers
// that just want a message can catch std::exception generically, while
// callers that need to branch on the exact failure can catch the specific
// type (used by the API layer to map to correct HTTP status codes).
// ============================================================================

class NoAgentAvailableException : public std::runtime_error {
public:
    explicit NoAgentAvailableException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class AgentCapacityExceededException : public std::runtime_error {
public:
    explicit AgentCapacityExceededException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class OrderNotFoundException : public std::runtime_error {
public:
    explicit OrderNotFoundException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class AgentNotFoundException : public std::runtime_error {
public:
    explicit AgentNotFoundException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class InvalidRequestException : public std::runtime_error {
public:
    explicit InvalidRequestException(const std::string& msg)
        : std::runtime_error(msg) {}
};
