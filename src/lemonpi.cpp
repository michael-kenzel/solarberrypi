#include <exception>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>

using namespace std::literals;

#if 0
import icp10125;
import sht40;
#else
import mockup;
#endif

import server;

namespace
{
	volatile std::sig_atomic_t run = 1;

	extern "C" void cancel(int /*sig*/)
	{
		run = 0;
	}
}

int main()
{
	std::signal(SIGINT, &cancel);

	try
	{
		struct
		{
			// ICP10125 icp10125 = ICP10125("/dev/i2c-1");
			// SHT40 sht45 = SHT40("/dev/i2c-1"), sht40 = SHT40("/dev/i2c-4");

			void operator ()(std::ostream& out)
			{
			}

			void run()
			{
				while (::run)
				{
					// auto [p, t_3] = icp10125.measure();
					// auto [t_1, rh_1] = sht45.measure_high_precision();
					// auto [t_2, rh_2] = sht40.measure_high_precision();

					// std::cout //<< std::chrono::utc_clock::now() << ' '
					// 			<< std::fixed << std::setprecision(1) << t_1 << ' ' << rh_1 << ' ' << t_2 << ' ' << rh_2 << ' ' << t_3 << ' ' << p / 100.0f << '\n';

					// std::this_thread::sleep_for(2s);
				}
			}
		} source;

		class server server(source);

		source.run();

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << '\n';
	}
	catch (...)
	{
		std::cerr << "ERROR: unknown exception\n";
	}

	return EXIT_FAILURE;
}
