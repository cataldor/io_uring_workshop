BIN	:= $(notdir $(shell pwd))
SRCS	:= $(wildcard *.c)
OBJS	:= $(patsubst %.c, $(OBJDIR)/%.o, $(SRCS))

LINKERFLAGS += -luring

.PHONY: all clean test check

all: $(SRCS) $(BIN)

$(BIN): $(OBJS)
	$(Q)$(CC) $(CFLAGS) $(CGCCFLAGS) $(DFLAGS) $(OPT) $(OBJS) -o $@ $(LINKERFLAGS)
	@printf "LD\t$(BIN)\n"

$(OBJDIR)/%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(CGCCFLAGS) $(DFLAGS) $(OPT) -c $< -o $@
	@printf "CC\t$@\n"

check:
	$(Q)$(CC2) $(CFLAGS) $(CCLANGFLAGS) $(DFLAGS) $(OPT) -o $(BIN) $(SRCS) $(LINKERFLAGS)

test:
	$(Q)./$(BIN) $(TESTFILES) > $(TESTOUTDIR)/$(BIN)

clean:
	@rm -f $(OBJS)
	@rm -f $(BIN)
	@rm -f $(TESTOUTDIR)/$(BIN)

