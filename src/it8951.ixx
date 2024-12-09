export module display;

import io;

export class IT8951
{
	io::unique_fd fd;

	void write_reg(unsigned short reg, unsigned short value);

	auto read_system_info();

public:
	IT8951(const char* path);
};
