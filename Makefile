TARGET = yeahconsole
CC = gcc
#CC = cc
INSTALL = install

PREFIX = /usr/local

LIBS = -lX11 
INCLUDES = -I/usr/X11R6/include 
LIB_DIRS = -L/usr/X11R6/lib
FLAGS = -Os -Wall

OBJECTS := yeahconsole.o
SOURCES := yeahconsole.c

$(TARGET): $(OBJECTS) 
	$(CC) $(DEFINES) $(INCLUDES) $(LIB_DIRS) $(LIBS) -o $@ $<
	strip $@

$(OBJECTS): $(SOURCES) 
	$(CC) $(FLAGS)  $(DEFINES) $(INCLUDES) $(LIB_DIRS) -c -o $@ $<

clean:
	rm -rf $(TARGET) $(OBJECTS)

install: $(TARGET) $(MAN)
	$(INSTALL) -o root -g root -m 0755 $(TARGET) $(PREFIX)/bin
	

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
