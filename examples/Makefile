# Makefile for building all of the examples
# because sorting out the dependencies is hard enough already
# recursive make is fiiiiine, they're independant 
all: *
%: %/Makefile
	-@make -C $@

clean: 
	-@make -C $< clean
