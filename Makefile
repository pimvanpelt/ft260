TARGET  = ft260
CC      = gcc
CFLAGS  = -g -O2 -pedantic -Werror -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition
LINKER  = gcc
LFLAGS  = -O -Wall -I. -lm -ludev

.PHONY: default all clean

default: $(TARGET)
all: default

OBJDIR  = build
SRCDIR  = src
INCDIR  = include
BINDIR  = .

SRCS     := $(shell find -L $(SRCDIR) -type f -name '*.c')
INCS     := $(shell find -L $(SRCDIR) $(INCDIR) -type d -name 'include')
INCFLAGS := $(patsubst %,-I %, $(INCS))
OBJS     := $(patsubst %.c, build/%.o, $(SRCS))
RM       = rm -f
RMDIR    = rm -r -f

$(BINDIR)/$(TARGET): $(OBJS)
	$(LINKER) $(OBJS) $(LFLAGS) -o $@

$(OBJS): $(OBJDIR)/%.o : %.c
	@mkdir -p $(shell dirname $(OBJS))
	$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RMDIR) $(OBJDIR)
	$(RM) $(BINDIR)/$(TARGET)
