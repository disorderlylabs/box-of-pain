# Makefile for all of the examples
# ARGS is set to "clean" sometimes. otherwise empty
all: */

%: %Makefile FORCE
	-$(MAKE) -C $@ $(ARGS)

# always want above to run
FORCE:

# sorry.
.PHONY: clean
clean:
	-$(MAKE) -sC . ARGS=clean
