# Compiler flags
CC     = g++
CFLAGS = -Wall $(INCLUDE) -pthread 
INCLUDE =  -Ilame-3.99.5/include 
LIB = -L. -lmp3lame
 
# Project files
SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)
EXE  = wav2mp3

# Debug build settings
DBGDIR = debug
DBGEXE = $(DBGDIR)/$(EXE)
DBGOBJS = $(addprefix $(DBGDIR)/, $(OBJS))
DBGCFLAGS = -g -O0 -DDEBUG

# Release build settings
RELDIR = release
RELEXE = $(RELDIR)/$(EXE)
RELOBJS = $(addprefix $(RELDIR)/, $(OBJS))
RELCFLAGS = -O3 -DNDEBUG

.PHONY: all clean debug prep release

# Default build
all: prep release

# Debug rules
debug: $(DBGEXE)

$(DBGEXE): $(DBGOBJS)
	$(CC) $(CFLAGS) $(DBGCFLAGS) $(DBGOBJS) $(LIB) -o $(DBGEXE)

$(DBGDIR)/%.o: %.cpp
	$(CC) -c $(CFLAGS) $(DBGCFLAGS) -o $@ $<

# Release rules
release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CC) $(CFLAGS) $(RELCFLAGS) $(RELOBJS) $(LIB) -o $(RELEXE)

$(RELDIR)/%.o: %.cpp
	$(CC) -c $(CFLAGS) $(RELCFLAGS) -o $@ $<

# Other rules
prep:
	@mkdir -p $(DBGDIR) $(RELDIR)

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS)

