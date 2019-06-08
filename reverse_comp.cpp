#include <iostream>
#include <array>
#include <vector>
#include <memory>

#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

constexpr std::array<char, 128> make_LUT()
{
	const char inputs[] = "ACGTUMRWSYKVHDBN";
	const char outputs[] = "TGCAAKYWSRMBDHVN";
	std::array<char, 128> ret{};
	auto it = ret.begin();
	for (int i = 0; i < 16; ++i) {
		*(it + (inputs[i])) = outputs[i];
		*(it + ((inputs[i] + 32))) = outputs[i];
	}
	*(it + '\n') = '\n';
	return ret;
}

constexpr auto lut = make_LUT();

// perform reverse complement
// skip over new line characters by traversing data in strides
// to eliminate branching
void reverse_comp(char* start, char* end)
{
	static const auto seq_len = 61;
	const auto offset = (end - start) % seq_len;
	end -= 2;
	if (offset) {
		while (start < end) {
			for (int i = 1; (i < offset) && (start < end); ++i) {
				*start = lut[*start];
				*end = lut[*end];
				std::swap(*start, *end);
				++start;
				--end;
			}
			--end;
			for (int i = 0; i < (seq_len - offset) && (start < end); ++i) {
				*start = lut[*start];
				*end = lut[*end];
				std::swap(*start, *end);
				++start;
				--end;
			}
			++start;
		}
	} else {
		while (start < end) {
			for (int i = 1; i < seq_len; ++i) {
				*start = lut[*start];
				*end = lut[*end];
				std::swap(*start, *end);
				++start;
				--end;
			}
			++start;
			--end;
		}
	}
}

int main()
{
	auto fd = fileno(stdin);
	std::ios_base::sync_with_stdio(false);
	std::cout.tie(nullptr);
	std::cin.tie(nullptr);
	std::vector<char*> breakpoints;
	struct stat stat_buf;

	// allocate buffer the same size as file and read from stdin
	fstat(fd, &stat_buf);
	size_t len = stat_buf.st_size;
	auto input = std::unique_ptr<char>(new char[len + 1]());
	std::cin.read(input.get(), len);
	
	// find location of > to denote new entry
	char* arrow = input.get();
	char* end = input.get() + len + 1;
	for (;
	(arrow = reinterpret_cast<char*>(memchr(arrow, '>', end - arrow))) != nullptr;
	++arrow) {
		breakpoints.emplace_back(arrow);
	}
	breakpoints.push_back(input.get() + len);
	
	// perform reverse complement	
	for (size_t i = 0; i < (breakpoints.size() - 1); ++i) {
		char* s = breakpoints[i];
		while(*(++s) != '\n');
		reverse_comp(++s, breakpoints[i+1]);
	}
	
	std::cout.write(input.get(), len);
}
