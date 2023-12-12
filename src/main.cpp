#include <exception>
#include <iostream>
#include <thread>
#include <atomic>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

import display;
import gpio;
import io;


int main()
{
	try
	{
		// std::atomic<bool> run = true;

		// gpio::pin_set<
		// 	{ 23, gpio::pin::flag::INPUT | gpio::pin::flag::EDGE_RISING | gpio::pin::flag::EDGE_FALLING }
		// > pins("/dev/gpiochip0");

		// std::thread gpio_monitor([&]
		// {
		// 	while (run)
		// 	{
		// 		auto e = pins.wait_event();

		// 		std::cout << e.timestamp_ns << ": " << e.offset << (static_cast<int>(e.id) == 1 ? "rising" : "falling") << '\n';
		// 	}
		// });

		// std::cout << "bla\n";

		IT8951 display;

		// auto fd = open("/dev/spidev0.0", O_RDWR);

		// __u8 spi_mode = 0;
		// io::throw_error(ioctl(fd, SPI_IOC_WR_MODE, &spi_mode));

		// __u8 spi_lsb_first = 0;
		// io::throw_error(ioctl(fd, SPI_IOC_WR_LSB_FIRST, &spi_lsb_first));

		// __u8 spi_bits_per_word = 8;
		// io::throw_error(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits_per_word));

		// __u32 spi_max_speed = 100'000;
		// io::throw_error(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_max_speed));


		// unsigned char bytes[] = { 0x60, 0x00 };

		// spi_ioc_transfer transfer = {
		// 	.tx_buf = reinterpret_cast<__u64>(bytes),
		// 	.len = sizeof(bytes),
		// 	.cs_change = true
		// };

		// io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));

		// pins.set<18>(true);
		// usleep(1000);
		// pins.set<18>(false);

		// gpio_monitor.join();
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
