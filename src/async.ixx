module;
#include <concepts>
#include <utility>
#include <chrono>
// #include <atomic>

export module async;

export namespace async
{
	// template <typename T>
	// class measurement_source
	// {
	// 	using clock = std::chrono::steady_clock;

	// 	struct result
	// 	{
	// 		T value;
	// 		clock::time_point t;
	// 	};

	// 	std::atomic<result> last_result;

	// protected:
	// 	void publish(T value, clock::time_point t = clock::now())
	// 	{
	// 		// TODO: handle case of timestamp not having changed since last measurement
	// 		last_result.store({ std::move(value), t });
	// 	}

	// public:
	// 	class future
	// 	{
	// 		const measurement_source& source;
	// 		clock::time_point eta;

	// 	public:
	// 		future(const measurement_source& source, clock::time_point eta)
	// 			: source(source), eta(eta)
	// 		{
	// 		}
	// 	};

	// 	auto promise_measurement(std::chrono::microseconds delay)
	// 	{
	// 		return future(last_result, clock::now() + delay);
	// 	}
	// };

	template <std::invocable F, typename Clock, typename Duration = Clock::duration>
	class future : F
	{
		std::chrono::time_point<Clock, Duration> eta;

	public:
		future(F&& fetch_result, std::chrono::time_point<Clock, Duration> eta)
			: F(std::move(fetch_result)), eta(eta)
		{
		}

		template <typename Rep, typename Period>
		future(F&& fetch_result, std::chrono::duration<Rep, Period> delay)
			: future(std::move(fetch_result), std::chrono::steady_clock::now() + delay)
		{
		}
	};

	template <std::invocable F, typename Rep, typename Period>
	future(F&&, std::chrono::duration<Rep, Period>) -> future<F, std::chrono::steady_clock>;
}
