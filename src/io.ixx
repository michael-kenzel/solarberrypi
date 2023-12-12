module;
#include <cerrno>
#include <cstring>
#include <utility>
#include <exception>

#include <unistd.h>
#include <fcntl.h>

export module io;

export namespace io
{
	struct error : std::exception
	{
		int errnum;

	public:
		explicit error(int errnum) : errnum(errnum) {}

		const char* what() const noexcept override
		{
			return std::strerror(errnum);
		}
	};

	inline int throw_error(int result)
	{
		if (result < 0)
			throw error(errno);
		return result;
	}

	inline constexpr int null_value = -1;  // WORKAROUND for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112899

	class unique_fd
	{
		// static constexpr int null_value = -1;

		int fd = -1;

		constexpr void close(int fd_to_close) noexcept
		{
			if (*this)
				::close(fd_to_close);
		}

	public:
		// unique_fd(const unique_fd&) = delete;
		// unique_fd& operator =(const unique_fd&) = delete;

		unique_fd() = default;

		constexpr explicit unique_fd(int fd) noexcept
			: fd(fd)
		{
		}

		constexpr unique_fd(unique_fd&& source) noexcept
			: fd(std::exchange(source.fd, null_value))
		{
		}

		constexpr ~unique_fd()
		{
			close(fd);
		}

		constexpr unique_fd& operator =(unique_fd&& source) noexcept
		{
			reset(source.release());
			return *this;
		}

		constexpr operator int() const noexcept { return fd; }

		constexpr int release() noexcept
		{
			return std::exchange(fd, null_value);
		}

		constexpr unique_fd& reset(int new_fd = null_value) noexcept
		{
			close(std::exchange(fd, new_fd));
			return *this;
		}

		constexpr explicit operator bool() const noexcept { return fd != null_value; }

		friend bool operator ==(const unique_fd&, const unique_fd&) = default;
		friend bool operator <=>(const unique_fd&, const unique_fd&) = delete;

		friend constexpr void swap(unique_fd& a, unique_fd& b) noexcept
		{
			using std::swap;
			swap(a.fd, b.fd);
		}
	};
}
