export module display;

import io;
import gpio;


export class IT8951
{
	static constexpr int RESET = 17;
	static constexpr int READY = 24;

	io::unique_fd fd;

	gpio::pin_set<
		{ READY, gpio::pin::flag::INPUT | gpio::pin::flag::EDGE_RISING | gpio::pin::flag::EDGE_FALLING },
		{ RESET, gpio::pin::flag::OUTPUT | gpio::pin::flag::ACTIVE_LOW }
	> pins;

	void send_preamble(unsigned short preamble);
	void send_command(unsigned short command);

	template <int N>
	auto read_data();

	void write_data(unsigned short data);

	auto read_register(unsigned short reg);

	auto read_system_info();

public:
	IT8951();
};
