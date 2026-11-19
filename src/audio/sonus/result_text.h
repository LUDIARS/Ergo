#pragma once
/// Names of Sonus' snd_result and end-reason values for ergo_audio's log lines.
#include "snd/types.h"

namespace ergo::audio::sonus_detail {

const char* result_name(snd_result r);          ///< "SND_ERR_NO_DEVICE", ...; "unknown" otherwise
const char* end_reason_name(snd_end_reason r);  ///< "COMPLETED", "REJECTED", ...

} // namespace ergo::audio::sonus_detail
