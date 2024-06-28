#include <vd/Ap4HttpStream.h>

#include <Ap4File.h>
#include <Ap4FileByteStream.h>
#include <Ap4Movie.h>

#include <iostream>
#include <format>

template <
	typename CharT,
	typename... Args,
	class Traits = std::char_traits<CharT>>
void Printf(
	std::basic_ostream<CharT, Traits>& os,
	const std::format_string<Args...> fstr,
	Args &&... args)
{
	os << std::format(fstr, std::forward<Args>(args)...) << std::flush;
}

template <typename... Args>
void Printf(
	const std::format_string<Args...> fstr,
	Args &&... args)
{
	std::cout << std::format(fstr, std::forward<Args>(args)...) << std::flush;
}

template <typename T>
void Println(const T &val)
{
	std::cout << val << std::endl;
}

int main(int argc, char* argv[])
{
	try
	{
		"Утф8 хочу"
		vd::HttpSource s("");
	}
	catch(const std::exception &e)
	{
		Println(e.what());
	}
	return 0;
}