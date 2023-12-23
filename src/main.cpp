#include <exception>
#include <iostream>

import display;

int main()
{
	try
	{
		IT8951 display("/dev/it8951-spi0.0");
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << '\n';
		return -1;
	}
	catch (...)
	{
		std::cerr << "ERROR: unknown exception\n";
		return -128;
	}
}
