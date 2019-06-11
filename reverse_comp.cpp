// The Computer Language Benchmarks Game
// https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
//
// Contributed by Will Rurik
// Compile with -O3 -march=native -pthread -funroll-loops
#include <iostream>
#include <array>
#include <vector>
#include <memory>
#include <thread>
#include <functional>

#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

//look up table of upper case and lower case conversions
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
	return ret;
}

constexpr auto lut = make_LUT();
static const auto seq_len = 61;

// perform reverse complement for num_rows rows
// skip over new line characters by traversing data in strides
void process_by_row(char* start, char* end, const int offset, int num_rows)
{
	const int remaining = seq_len - offset - 1;
	int rows = 0;
	while (rows < num_rows) {
		for (int i = 0; i < offset; ++i, ++start, --end) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
		}
		--end;
		for (int i = 0; i < remaining; ++i, ++start, --end) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
		}
		++start;
		++rows;
	}
}

// perform reverse complement for remainder of sequence
// skip over new line characters by traversing data in strides
void process_remaining(char* start, char* end, const int offset)
{
	const int remaining = seq_len - offset - 1;
	while (start < end) {
		for (int i = 0; (i < offset) && (start < end); ++i, ++start, --end) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
		}
		--end;
		for (int i = 0; (i < remaining) && (start < end); ++i, ++start, --end) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
		}
		++start;
	}
}

// perform reverse complement using multiple threads
void reverse_comp(char* start, char* end)
{
	static auto hwconcurrency = std::thread::hardware_concurrency();
	static int nthreads = (hwconcurrency == 0) ? 4 : hwconcurrency;
	const auto nbytes = end - start;
	const auto last_row = nbytes % seq_len;
	const auto offset = (last_row) == 0 ? seq_len : (last_row);
	const auto nrows = nbytes / seq_len;
	const auto nswaps = nrows >> 1;
	
	auto rows_per_thread = std::vector<unsigned int>(nthreads, nswaps / nthreads);
	auto rem_rows = nswaps % nthreads;
	for (int i = 0; rem_rows > 0; ++i, --rem_rows)
		++rows_per_thread[i];
	end -= 2;
	unsigned int strides = 0;
	std::vector<std::thread> threads;
	for (int i = 0; i < (nthreads - 1); ++i) {
		threads.push_back(std::thread(process_by_row, 
						start + strides,
					       	end - strides,
					       	offset - 1,
					       	rows_per_thread[i]));

		strides += rows_per_thread[i] * seq_len;
	}
	threads.push_back(std::thread(process_remaining,
					start + strides,
				       	end - strides,
				       	offset - 1));

	for (auto& t : threads)
		t.join();
}

int main()
{
	const auto BUF_SIZE = 8 * 1024 * 1024;
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
	
	for (int i = 0; std::cin.read(input.get() + i, BUF_SIZE); i += BUF_SIZE);
	
	// find locations of > to denote new entry
	for (char* arrow = input.get(), *end = input.get() + len + 1;
		(arrow = static_cast<char*>(memchr(arrow, '>', end - arrow))) != nullptr;
		++arrow) {
		breakpoints.emplace_back(arrow);
	}
	breakpoints.push_back(input.get() + len);
	
	// perform reverse complement	
	for (int i = 0; i < (static_cast<int>(breakpoints.size()) - 1); ++i) {
		char* s = breakpoints[i];
		while(*(++s) != '\n');
		reverse_comp(++s, breakpoints[i+1]);
	}
	
	std::cout.write(input.get(), len);
}
