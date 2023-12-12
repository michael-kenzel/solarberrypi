module;
#include <cstddef>
#include <utility>
#include <bit>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/gpio.h>

export module gpio;

import io;

export namespace gpio
{
	enum class edge
	{
		RISING = GPIO_V2_LINE_EVENT_RISING_EDGE,
		FALLING = GPIO_V2_LINE_EVENT_FALLING_EDGE
	};

	struct pin
	{
		enum class flag : __u64
		{
			ACTIVE_LOW = GPIO_V2_LINE_FLAG_ACTIVE_LOW,
			INPUT = GPIO_V2_LINE_FLAG_INPUT,
			OUTPUT = GPIO_V2_LINE_FLAG_OUTPUT,
			EDGE_RISING = GPIO_V2_LINE_FLAG_EDGE_RISING,
			EDGE_FALLING = GPIO_V2_LINE_FLAG_EDGE_FALLING,
			OPEN_DRAIN = GPIO_V2_LINE_FLAG_OPEN_DRAIN,
			OPEN_SOURCE = GPIO_V2_LINE_FLAG_OPEN_SOURCE,
			BIAS_PULL_UP = GPIO_V2_LINE_FLAG_BIAS_PULL_UP,
			BIAS_PULL_DOWN = GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN,
			BIAS_DISABLED = GPIO_V2_LINE_FLAG_BIAS_DISABLED,
			EVENT_CLOCK_REALTIME = GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME,
			EVENT_CLOCK_HTE = GPIO_V2_LINE_FLAG_EVENT_CLOCK_HTE,
		};

		int id;
		flag flags;
	};

	constexpr pin::flag operator |(pin::flag a, pin::flag b)
	{
		return static_cast<pin::flag>(static_cast<std::underlying_type_t<pin::flag>>(a) | static_cast<std::underlying_type_t<pin::flag>>(b));
	}

	constexpr bool operator &(pin::flag a, pin::flag b)
	{
		return (static_cast<std::underlying_type_t<pin::flag>>(a) & static_cast<std::underlying_type_t<pin::flag>>(b));
	}

	template <pin... pin>
	class pin_set
	{
		io::unique_fd fd;

		template <int id>
		static constexpr bool have_pin = ((pin.id == id) || ...);

		template <int id, pin::flag flag>
		static constexpr bool has_flag = (((pin.id == id) && (pin.flags & flag)) || ...);

		template <int id>
		static constexpr bool is_output = has_flag<id, pin::flag::OUTPUT>;

		template <int id>
		static constexpr bool is_input = has_flag<id, pin::flag::INPUT>;

		template <int id, int... i>
		static constexpr auto pin_index_helper(std::integer_sequence<int, i...>)
		{
			return ((pin.id == id ? i : 0) + ... + 0);
		}

		template <int id>
		static constexpr auto pin_index = pin_index_helper<id>(std::make_integer_sequence<int, sizeof...(pin)>());

		template <int... id>
		static constexpr auto pin_mask = ((__u64(1) << pin_index<id>) | ... | 0);

		auto open(const char* device)
		{
			static_assert(sizeof...(pin) + 1 <= GPIO_V2_LINE_NUM_ATTRS_MAX);  // we're using one attribute set per pin atm

			io::unique_fd gpio_chip(io::throw_error(::open(device, O_RDWR)));

			gpio_v2_line_request line_config = {
				.offsets = { pin.id... },
				.config = {
					.num_attrs = sizeof...(pin) + 1,
					.attrs = {
						{
							.attr = { .id = GPIO_V2_LINE_ATTR_ID_FLAGS, .flags = static_cast<__u64>(pin.flags) },
							.mask = pin_mask<pin.id>
						}...,
						{
							.attr = { .id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES, .values = 0 },
							.mask = (__u64(1) << sizeof...(pin)) - 1
						}
					}
				},
				.num_lines = sizeof...(pin),
			};

			io::throw_error(ioctl(gpio_chip, GPIO_V2_GET_LINE_IOCTL, &line_config));

			return io::unique_fd(line_config.fd);
		}

	public:
		pin_set(const char* device)
			: fd(open(device))
		{
		}

		template <int... id> requires (is_output<id> && ...)
		void set(bool value = true)
		{
			gpio_v2_line_values line_values = {
				.bits = static_cast<__u64>(value ? -1 : 0),
				.mask = pin_mask<id...>
			};

			io::throw_error(ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &line_values));
		}

		template <int... id> inline void unset() { set<id...>(false); }

		template <int id> requires is_input<id>
		bool get()
		{
			gpio_v2_line_values line_values = {
				.mask = pin_mask<id>
			};

			io::throw_error(ioctl(fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &line_values));

			return line_values.bits != 0;
		}

		template <int id> requires is_input<id>
		void wait(bool value)
		{
			while (get<id>() != value)
				continue;
		}

		auto wait_event()
		{
			while (true)
			{
				gpio_v2_line_event event;
				io::throw_error(::read(fd, &event, sizeof(event)));
				return event;
			}
		}

		template <int id> requires is_input<id>
		auto wait_edge(edge edge)
		{
			while (true)
			{
				auto event = wait_event();

				if (event.offset == id && event.id == static_cast<__u32>(edge))
					return event;
			}
		}
	};
}
