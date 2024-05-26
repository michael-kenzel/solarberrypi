module;
#include <cerrno>
#include <string_view>
#include <string>
#include <thread>
#include <sstream>
#include <iostream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

export module server;

import io;

using namespace std::literals;

// export struct source
// {
// 	virtual void fetch(std::ostream&) = 0;

// 	source() = default;
// 	source(const source&) = default;
// 	source(source&&) = default;
// 	source& operator =(const source&) = default;
// 	source& operator =(source&&) = default;
// 	~source() = default;
// };

export class server
{
	io::unique_fd socket;

	std::thread thread;

	void serve(auto& source)
	{
		while (true)
		{
			sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);

			auto s =
				io::unique_fd(
					io::throw_error(
						::accept(socket, reinterpret_cast<sockaddr*>(&addr), &addr_len)));

			std::cout << "connection from "sv
			          << ((addr.sin_addr.s_addr >>  0) & 0xFF) << '.'
			          << ((addr.sin_addr.s_addr >>  8) & 0xFF) << '.'
			          << ((addr.sin_addr.s_addr >> 16) & 0xFF) << '.'
			          << ((addr.sin_addr.s_addr >> 24) & 0xFF) << '\n';

			// char buf[1024];
			// io::throw_error(read(s, buf, sizeof(buf)));

			// std::cout << buf << '\n';

			// std::ostringstream res;
			// source(res);

			// io::throw_error(write(s, res.view().data(), res.view().length()));
			::shutdown(s, SHUT_RDWR);
		}
	}

public:
	server(auto& source, int port = 8080)
		: socket(io::throw_error(::socket(AF_INET, SOCK_STREAM, 0)))
	{
		int on = 1;

		io::throw_error(::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)));

		const sockaddr_in addr = {
			.sin_family = AF_INET,
			.sin_port = htons(port)
		};

		io::throw_error(::bind(socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)));
		io::throw_error(::listen(socket, 8));
		thread = std::thread(&server::serve<decltype(source)>, this, std::ref(source));
	}

	~server()
	{
		shutdown();
		thread.join();
	}

	void shutdown() noexcept
	{
		::shutdown(socket, SHUT_RDWR);
	}
};
