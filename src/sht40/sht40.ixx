module;
#include <cstddef>
#include <stdexcept>
#include <array>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

export module sht40;

import io;

constexpr unsigned int deserialize_u16(std::array<std::byte, 2> bytes)
{
	return static_cast<unsigned int>(bytes[0]) * 256 + static_cast<unsigned int>(bytes[1]);
}

export class SHT40
{
	static constexpr __u16 address = 0x44;

	io::unique_fd fd;

	enum class command : unsigned char
	{
		RESET = 0x94,
		READ_SN = 0x89,
		MEASURE_HIGH_PRECISION = 0xFD,
		MEASURE_MEDIUM_PRECISION = 0xF6,
		MEASURE_LOW_PRECISION = 0xE0,
	};

	void send_command(command cmd)
	{
		struct i2c_msg msg = {
			.addr = address,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = reinterpret_cast<__u8*>(&cmd)
		};

		struct i2c_rdwr_ioctl_data rdwr_data = {
			.msgs = &msg,
			.nmsgs = 1
		};

		io::throw_error(ioctl(fd, I2C_RDWR, &rdwr_data));
	}

	std::array<std::byte, 4> read_response()
	{
		std::byte bytes[6];

		struct i2c_msg msg = {
			.addr = address,
			.flags = I2C_M_RD,
			.len = sizeof(bytes),
			.buf = reinterpret_cast<__u8*>(&bytes[0])
		};

		struct i2c_rdwr_ioctl_data rdwr_data = {
			.msgs = &msg,
			.nmsgs = 1
		};

		io::throw_error(ioctl(fd, I2C_RDWR, &rdwr_data));

		// TODO: CRC
		return { bytes[0], bytes[1], bytes[3], bytes[4] };
	}

	auto decode_result(std::array<std::byte, 4> bytes)
	{
		struct result { float temperature, humidity; };
		return result {
			-45.0f + 175.0f * deserialize_u16({ bytes[0], bytes[1] }) / 65535,
			 -6.0f + 125.0f * deserialize_u16({ bytes[2], bytes[3] }) / 65535
		};
	}

public:
	SHT40(const char* device)
		: fd(io::throw_error(open(device, O_RDWR)))
	{
		// unsigned long funcs;

		// io::throw_error(ioctl(fd, I2C_FUNCS, &funcs));

		// if (!(funcs & I2C_FUNC_I2C))
		// 	throw std::runtime_error("device does not support plain I2C");

		reset();
	}

	void reset() { send_command(command::RESET); usleep(1000); }

	auto measure_high_precision()
	{
		send_command(command::MEASURE_HIGH_PRECISION);
		usleep(8300);
		return decode_result(read_response());
	}

	auto measure_medium_precision()
	{
		send_command(command::MEASURE_MEDIUM_PRECISION);
		usleep(4500);
		return decode_result(read_response());
	}

	auto measure_low_precision()
	{
		send_command(command::MEASURE_LOW_PRECISION);
		usleep(1600);
		return decode_result(read_response());
	}
};
