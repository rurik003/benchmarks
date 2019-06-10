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
static const auto seq_len = 61;

void reverse_comp_offset(char* start, char* end, const int offset, int num_rows)
{
	int rows = 0;
	while (rows < num_rows) {
		for (int i = 1; (i < offset); ++i) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
			++start;
			--end;
		}
		--end;
		for (int i = 0; (i < (seq_len - offset)); ++i) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
			++start;
			--end;
		}
		++start;
		++rows;
	}
}

void reverse_comp_offset_final(char* start, char* end, const int offset)
{
	while (start < end) {
		for (int i = 1; (i < offset) && (start < end); ++i) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
			++start;
			--end;
		}
		--end;
		for (int i = 0; (i < (seq_len - offset)) && (start < end); ++i) {
			char c = lut[*end];
			*end = lut[*start];
			*start = c;
			++start;
			--end;
		}
		++start;
	}
}

// perform reverse complement
// skip over new line characters by traversing data in strides
// to eliminate branching
void reverse_comp(char* start, char* end)
{
	static auto hwconcurrency = std::thread::hardware_concurrency();
	static auto nthreads = (hwconcurrency == 0) ? 4 : hwconcurrency;
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
	for (unsigned int i = 0; i < (nthreads - 1); ++i) {
		threads.push_back(std::thread(reverse_comp_offset, start + strides, end - strides, offset, rows_per_thread[i]));
		strides += rows_per_thread[i] * seq_len;
	}
	threads.push_back(std::thread(reverse_comp_offset_final, start + strides, end - strides, offset));
	for (auto& t : threads)
		t.join();
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
	
	// find locations of > to denote new entry
	for (char* arrow = input.get(), *end = input.get() + len + 1;
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
