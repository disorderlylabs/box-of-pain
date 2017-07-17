#include <sys/ptrace.h>
#include <stdlib.h>
#include <string>
#include "helper.h"
#include <tuple>
#include <errno.h>

std::string tracee_readstr(int child, unsigned long addr)
{
	std::string str = "";
	while(true) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr++);
		if(errno != 0) {
			break;
		}
		char *buf = (char *)&tmp;
		for(unsigned int i=0;i<sizeof(tmp);i++) {
			if(!*buf) {
				return str;
			}
			str += *buf++;
		}
	}
	return str;
}

void tracee_copydata(int child, unsigned long addr, char *buf, ssize_t len)
{
	while(true) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr++);
		if(errno != 0) {
			break;
		}
		char *b = (char *)&tmp;
		for(unsigned int i=0;i<sizeof(tmp) && len;i++, len--) {
			*buf++ = *b++;
		}
	}
}

