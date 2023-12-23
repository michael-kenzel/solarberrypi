module;
#include <type_traits>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <array>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

export module icp10125;

import io;

constexpr auto operator ""_b(unsigned long long value)
{
	return static_cast<std::byte>(value);
}

constexpr std::array<std::byte, 2> serialize_u16(unsigned short value)
{
	return {
		static_cast<std::byte>((value >> 8) & 0xFF),
		static_cast<std::byte>(value & 0xFF)
	};
}

constexpr unsigned int deserialize_u16(std::array<std::byte, 2> bytes)
{
	return (static_cast<unsigned int>(bytes[0]) << 8) |
	       (static_cast<unsigned int>(bytes[1]));
}

constexpr unsigned int deserialize_u24(std::array<std::byte, 3> bytes)
{
	return (static_cast<unsigned int>(bytes[0]) << 16) |
	       (static_cast<unsigned int>(bytes[1]) <<  8) |
	       (static_cast<unsigned int>(bytes[2]));
}

export class ICP10125
{
	static constexpr __u16 address = 0x63;

	io::unique_fd fd;

	std::array<short, 4> calibration;

	enum class command : unsigned short
	{
		RESET = 0x805D,
		READ_ID = 0xEFC8,
		READ_OTP = 0xC7F7,
		MEASURE_LP = 0x609C,
		MEASURE_N = 0x6825,
		MEASURE_LN = 0x70DF,
		MEASURE_ULN = 0x7866
	};

	template <std::size_t N>
	void send(std::array<std::byte, N> bytes)
	{
		struct i2c_msg msg = {
			.addr = address,
			.flags = 0,
			.len = std::size(bytes),
			.buf = reinterpret_cast<__u8*>(&bytes[0])
		};

		struct i2c_rdwr_ioctl_data rdwr_data = {
			.msgs = &msg,
			.nmsgs = 1
		};

		io::throw_error(ioctl(fd, I2C_RDWR, &rdwr_data));
	}

	template <std::convertible_to<std::byte>... Args>
	void send(Args... args)
	{
		send(std::array<std::byte, sizeof...(Args)>{args...});
	}

	template <int N>
	auto read()
	{
		return [this]<int... i>(std::integer_sequence<int, i...>) -> std::array<std::byte, N>
		{
			std::byte bytes[sizeof...(i) / 2 * 3];

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
			return { bytes[i / 2 * 3 + i % 2]... };
		}(std::make_integer_sequence<int, N>());
	}

	std::array<short, 4> read_otp()
	{
		send(0xC5_b, 0x95_b, 0x00_b, 0x66_b, 0x9C_b);
		auto read_param = [&]{ send_command(command::READ_OTP); return static_cast<short>(deserialize_u16(read<2>())); };
		return { read_param(), read_param(), read_param(), read_param() };
	}

	void send_command(command cmd)
	{
		send(serialize_u16(static_cast<unsigned short>(cmd)));
	}

	std::array<std::byte, 6> read_response()
	{
		return read<6>();
	}

	auto decode_result(std::array<std::byte, 6> bytes)
	{
		constexpr float Pa_ref[] = {45000.0f, 80000.0f, 105000.0f};

		const auto T = deserialize_u16({bytes[0], bytes[1]});

		const float t = T - 32768.0f;
		const float s[] = {
			3.5f * (1 << 20) + float(calibration[0] * t * t) / 16777216.0f,
			2048.0f * calibration[3] + float(calibration[1] * t * t) / 16777216.0f,
			11.5f * (1 << 20) + float(calibration[2] * t * t) / 16777216.0f
		};

		const float C = (s[0] * s[1] * (Pa_ref[0] - Pa_ref[1]) +
		                 s[1] * s[2] * (Pa_ref[1] - Pa_ref[2]) +
		                 s[2] * s[0] * (Pa_ref[2] - Pa_ref[0])) /
		                (s[2] * (Pa_ref[0] - Pa_ref[1]) +
		                 s[0] * (Pa_ref[1] - Pa_ref[2]) +
		                 s[1] * (Pa_ref[2] - Pa_ref[0]));
		const float A = (Pa_ref[0] * s[0] - Pa_ref[1] * s[1] - (Pa_ref[1] - Pa_ref[0]) * C) / (s[0] - s[1]);
		const float B = (Pa_ref[0] - A) * (s[0] + C);

		struct result { float pressure, temperature; };
		return result {
			A + B / (C + deserialize_u24({bytes[2], bytes[3], bytes[4]})),
			-45.0f + 175.0f / 65536.0f * T
		};
	}

public:
	ICP10125(const char* device)
		: fd(io::throw_error(open(device, O_RDWR))),
		  calibration((reset(), read_otp()))
	{
		return;
	}

	void reset() { send_command(command::RESET); usleep(1000); }

	auto measure()
	{
		send_command(command::MEASURE_ULN);
		usleep(94700);
		return decode_result(read_response());
	}
};
