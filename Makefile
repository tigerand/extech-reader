#
# Makefile for extech reader program
#
# Copyright 2017, Low Power Company, Inc.
# Copyright 2017, Andrew Sharp

CFLAGS+=-O2
CC := gcc

#.c.o:
#	$(CC) $(CFLAGS) -c $^

MAIN=extech_rdr

OBJS := \
	extech.o		\
	measurement.o	\
	$(MAIN).o

SRCS := $(OBJS:.o=.c)

$(MAIN): $(OBJS)
	$(CC) $(OBJS) -lpthread -o $(MAIN)

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

# throw away builtin rules for .cpp files, thankyouverymuch
%.o : %.cpp

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
