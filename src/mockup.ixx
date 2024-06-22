module;
#include <chrono>
#include <random>

#include <unistd.h>

export module mockup;

import async;

using namespace std::literals;


export class ICP10125
{
	auto generate_result()
	{
		static std::mt19937 rnd(99);

		struct result { float pressure, temperature; };
		return result {};
	}

public:
	ICP10125(const char* /*device*/)
	{
		reset();
	}

	void reset() { usleep(1000); }

	auto measure()
	{
		return async::future([this]{ return generate_result(); }, 94700us);
	}
};

export class SHT40
{
	auto generate_result()
	{
		static std::mt19937 rnd(68);

		struct result { float temperature, humidity; };
		return result {};
	}

public:
	SHT40(const char* /*device*/)
	{
		reset();
	}

	void reset() { usleep(1000); }

	auto measure_high_precision()
	{
		return async::future([this]{ return generate_result(); }, 8300us);
	}

	auto measure_medium_precision()
	{
		return async::future([this]{ return generate_result(); }, 4500us);
	}

	auto measure_low_precision()
	{
		return async::future([this]{ return generate_result(); }, 1600us);
	}
};
