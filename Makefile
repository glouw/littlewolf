CC = clang

NAME = littlewolf

SRCS = main.c

# CompSpec defined in windows environment.
ifdef ComSpec
	BIN = $(NAME).exe
else
	BIN = $(NAME)
endif

CFLAGS =
ifdef ComSpec
	CFLAGS += -I ../SDL2-2.0.7/i686-w64-mingw32/include
	CFLAGS += -I ../SDL2-2.0.7/i686-w64-mingw32/include/SDL2
endif
CFLAGS += -std=c99
CFLAGS += -Wshadow -Wall -Wpedantic -Wextra -Wdouble-promotion
CFLAGS += -g
CFLAGS += -O3 -march=native

LDFLAGS =
ifdef ComSpec
	LDFLAGS += -L..\SDL2-2.0.7\i686-w64-mingw32\lib
	LDFLAGS += -lmingw32
	LDFLAGS += -lSDL2main
endif
LDFLAGS += -lSDL2 -lm

ifdef ComSpec
	RM = del /F /Q
	MV = ren
else
	RM = rm -f
	MV = mv -f
endif

# Link.
$(BIN): $(SRCS:.c=.o)
	$(CC) $(CFLAGS) $(SRCS:.c=.o) $(LDFLAGS) -o $(BIN)

# Compile.
%.o : %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -MF $*.td -c $<
	@$(RM) $*.d
	@$(MV) $*.td $*.d
%.d: ;
-include *.d

clean:
	$(RM) vgcore.*
	$(RM) cachegrind.out.*
	$(RM) callgrind.out.*
	$(RM) $(BIN)
	$(RM) $(SRCS:.c=.o)
	$(RM) $(SRCS:.c=.d)
