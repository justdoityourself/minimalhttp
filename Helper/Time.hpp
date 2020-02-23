#pragma once

#include <time.h>

namespace atk
{
	uint32_t epoch_ts()
	{
		return (uint32_t)time(nullptr);
	}
}