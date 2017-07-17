#pragma once
#include <string>
#include <tuple>
#include <cassert>
#include <stdexcept>
std::string tracee_readstr(int child, unsigned long addr);
void tracee_copydata(int child, unsigned long addr, char *buf, ssize_t len);

#define GETOBJ(ch,addr,obj) \
	tracee_copydata(ch,addr,(char *)obj,sizeof(obj))

template <typename T, size_t n=1>
std::tuple<int, T> tracee_readvals(int child, unsigned long addr)
{
	errno = 0;
	unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr);
	return {errno, (T)tmp};
}

#define GET(T, ch, addr) \
	({ auto t = tracee_readvals<T>(ch,addr); if(std::get<0>(t) != 0) { throw std::runtime_error(std::string("errno: ") + std::to_string(errno)); }; std::get<1>(t); })
