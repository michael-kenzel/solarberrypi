module;
#include <cstddef>
#include <concepts>
#include <array>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "it8951/it8951.h"

module display;

enum class register
{
	I80CPCR = 0x04
};

void IT8951::write_reg(unsigned short reg, unsigned short value)
{
	io::throw_error(ioctl(fd, IT8951_IOCTL_COMMAND, 0x0011));
	io::throw_error(read(fd, bytes, sizeof(bytes)));
}

auto IT8951::read_system_info()
{
	io::throw_error(ioctl(fd, IT8951_IOCTL_COMMAND, 0x0302));
	std::byte bytes[40];
	io::throw_error(read(fd, bytes, sizeof(bytes)));
}

IT8951::IT8951(const char* path)
	: fd(io::throw_error(open(path, O_RDWR)))
{
	io::throw_error(ioctl(fd, IT8951_IOCTL_RESET));

	// SYS_RUN
	io::throw_error(ioctl(fd, IT8951_IOCTL_COMMAND, 0x0001));

	return;
}
