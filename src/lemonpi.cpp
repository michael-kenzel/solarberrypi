#include <exception>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>

#include <linux/types.h>
#include <linux/spi/spidev.h>

using namespace std::literals;

// import display;
import icp10125;
import sht40;
// import gpio;
// import io;

int main()
{
	try
	{
		// IT8951 display;

		ICP10125 icp10125("/dev/i2c-1");
		SHT40 sht45("/dev/i2c-1"), sht40("/dev/i2c-4");

		while (true)
		{
			auto [p, t_3] = icp10125.measure();
			auto [t_1, rh_1] = sht45.measure_high_precision();
			auto [t_2, rh_2] = sht40.measure_high_precision();

			std::cout //<< std::chrono::utc_clock::now() << ' '
			          << std::fixed << std::setprecision(1) << t_1 << ' ' << rh_1 << ' ' << t_2 << ' ' << rh_2 << ' ' << t_3 << ' ' << p / 100.0f << '\n';

			std::this_thread::sleep_for(2s);
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << '\n';
		return -1;
	}
	catch (...)
	{
		std::cerr << "ERROR: unknown exception\n";
		return -128;
	}
}
