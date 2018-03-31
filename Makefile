CFLAGS := -std=c99 -g -Wall 
LFLAGS := -std=c99 -g 
LIBS := 

SOURCES := $(wildcard src/*.c) 
OBJDIR := build
OBJECTS := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET := $(OBJDIR)/shader_sim

$(TARGET) : directories $(OBJECTS)
	$(CC) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJDIR)/%.o : src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean directories

directories:
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)
