# Compiler and standard.
CC = gcc -std=c99

# Project name.
PROJ = littlewolf

# Source files.
SRCS = main.c

# Warnings flags.
CFLAGS = -Wshadow -Wall -Wpedantic -Wextra -Wimplicit-fallthrough=5

# Debugging flags.
CFLAGS+= -g

# Optimization flags.
CFLAGS+= -Ofast -march=native -flto -Wdouble-promotion

# Linker flags.
LDFLAGS = -lm -lSDL2

# Linker.
$(PROJ): $(SRCS:.c=.o)
	$(CC) $(CFLAGS) $(SRCS:.c=.o) $(LDFLAGS) -o $(PROJ)

# Compiler template; generates dependency targets.
%.o : %.c
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -MF $*.td -c $<
	@mv -f $*.td $*.d

# All dependency targets.
%.d: ;
-include *.d

clean:
	rm -f vgcore.*
	rm -f cachegrind.out.*
	rm -f callgrind.out.*
	rm -f $(PROJ)
	rm -f $(SRCS:.c=.o)
	rm -f $(SRCS:.c=.d)
