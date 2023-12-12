module;
#include <cstddef>
#include <concepts>
#include <array>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

module display;

import io;
import gpio;


void IT8951::send_preamble(unsigned short preamble)
{
	std::byte bytes[] = { static_cast<std::byte>(preamble >> 8), static_cast<std::byte>(preamble) };

	spi_ioc_transfer transfer = {
		.tx_buf = reinterpret_cast<__u64>(bytes),
		.len = sizeof(bytes),
		.cs_change = true
	};

	pins.wait<READY>(true);
	io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));
}

void IT8951::send_command(unsigned short command)
{
	send_preamble(0x6000);

	std::byte bytes[] = { static_cast<std::byte>(command >> 8), static_cast<std::byte>(command) };

	spi_ioc_transfer transfer = {
		.tx_buf = reinterpret_cast<__u64>(bytes),
		.len = sizeof(bytes)
	};

	pins.wait<READY>(true);
	io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));
}

template <int N>
auto IT8951::read_data()
{
	send_preamble(0x1000);

	{
		std::byte dummy[2];

		spi_ioc_transfer transfer = {
			.rx_buf = reinterpret_cast<__u64>(dummy),
			.len = sizeof(dummy),
			.cs_change = true
		};

		pins.wait<READY>(true);
		io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));
	}

	std::array<std::byte, N> bytes = {};

	spi_ioc_transfer transfer = {
		.rx_buf = reinterpret_cast<__u64>(&bytes[0]),
		.len = N
	};

	pins.wait<READY>(true);
	io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));

	return bytes;
}

void IT8951::write_data(unsigned short data)
{
	send_preamble(0x0000);

	std::byte bytes[] = { static_cast<std::byte>(data >> 8), static_cast<std::byte>(data) };

	spi_ioc_transfer transfer = {
		.tx_buf = reinterpret_cast<__u64>(bytes),
		.len = sizeof(bytes)
	};

	pins.wait<READY>(true);
	io::throw_error(ioctl(fd, SPI_IOC_MESSAGE(1), &transfer));
}

auto IT8951::read_register(unsigned short reg)
{
	send_command(0x0010);
	write_data(reg);
	return read_data<2>();
}

auto IT8951::read_system_info()
{
	send_command(0x0302);
	auto bytes = read_data<40>();


}

IT8951::IT8951()
	: fd(open("/dev/spidev0.0", O_RDWR)),
	  pins("/dev/gpiochip0")
{
	pins.set<RESET>();
	usleep(10'000);
	pins.unset<RESET>();

	__u8 spi_mode = 0;
	io::throw_error(ioctl(fd, SPI_IOC_WR_MODE, &spi_mode));

	__u8 spi_lsb_first = 0;
	io::throw_error(ioctl(fd, SPI_IOC_WR_LSB_FIRST, &spi_lsb_first));

	__u8 spi_bits_per_word = 8;
	io::throw_error(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits_per_word));

	__u32 spi_max_speed = 12'000'000;
	io::throw_error(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_max_speed));


	pins.wait_edge<READY>(gpio::edge::RISING);

	send_command(0x0001);

	auto bytes = read_register(0x1000);

	send_command(0x0302);
	auto bla = read_data<40>();

	return;
}
