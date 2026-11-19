#include "result_text.h"

namespace ergo::audio::sonus_detail {

const char* result_name(snd_result r) {
    switch (r) {
    case SND_OK: return "SND_OK";
    case SND_ERR_INVALID_ARGUMENT: return "SND_ERR_INVALID_ARGUMENT";
    case SND_ERR_OUT_OF_BUDGET: return "SND_ERR_OUT_OF_BUDGET";
    case SND_ERR_NO_DEVICE: return "SND_ERR_NO_DEVICE";
    case SND_ERR_WRONG_THREAD: return "SND_ERR_WRONG_THREAD";
    case SND_ERR_THREAD_NOT_REGISTERED: return "SND_ERR_THREAD_NOT_REGISTERED";
    case SND_ERR_COMMAND_BUFFER_FULL: return "SND_ERR_COMMAND_BUFFER_FULL";
    case SND_ERR_INVALID_HANDLE: return "SND_ERR_INVALID_HANDLE";
    case SND_ERR_NOT_FOUND: return "SND_ERR_NOT_FOUND";
    case SND_ERR_BANK_IN_USE: return "SND_ERR_BANK_IN_USE";
    case SND_ERR_BANK_DEPENDENCY: return "SND_ERR_BANK_DEPENDENCY";
    case SND_ERR_IO: return "SND_ERR_IO";
    case SND_ERR_CORRUPT: return "SND_ERR_CORRUPT";
    case SND_ERR_VERSION_MISMATCH: return "SND_ERR_VERSION_MISMATCH";
    case SND_ERR_LIMIT_REACHED: return "SND_ERR_LIMIT_REACHED";
    case SND_ERR_UNSUPPORTED: return "SND_ERR_UNSUPPORTED";
    case SND_ERR_NOT_IMPLEMENTED: return "SND_ERR_NOT_IMPLEMENTED";
    case SND_ERR_NOT_READY: return "SND_ERR_NOT_READY";
    case SND_ERR_BANK_ALREADY_LOADED: return "SND_ERR_BANK_ALREADY_LOADED";
    default: return "unknown";
    }
}

const char* end_reason_name(snd_end_reason r) {
    switch (r) {
    case SND_END_COMPLETED: return "COMPLETED";
    case SND_END_STOPPED: return "STOPPED";
    case SND_END_STOLEN: return "STOLEN";
    case SND_END_REJECTED: return "REJECTED";
    case SND_END_DISCARDED: return "DISCARDED";
    case SND_END_ERROR: return "ERROR";
    case SND_END_NOT_LOADED: return "NOT_LOADED";
    default: return "unknown";
    }
}

} // namespace ergo::audio::sonus_detail
