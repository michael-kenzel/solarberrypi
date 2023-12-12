module;

module io;

namespace io
{
	// defined in header for now as WORKAROUND for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=112820
	// const char* error::what() const noexcept
	// {
	// 	return std::strerror(errnum);  // TODO: this is technically not fine as the result could be overwritten by a subsequent call to std::strerror
	// }
}
