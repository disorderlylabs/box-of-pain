#pragma once
#include <string>
#include <tuple>
std::string tracee_readstr(int child, unsigned long addr);

template <typename T, size_t n=1>
std::tuple<int, T> tracee_readvals(int child, unsigned long addr)
{
	unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr);
	return {errno, (T)tmp};
}
