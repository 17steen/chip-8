CXX := g++ -m64 

DEBUGFLAGS := -g -DDEBUG=0

LIBS :=  $(shell ncursesw5-config --libs)
LIBS := $(LIBS)



NCURSES_FLAGS := $(shell ncursesw5-config --cflags)
CXXFLAGS := $(NCURSES_FLAGS) -std=c++20 -Wall -Wextra $(DEBUGFLAGS)


OBJ := main.o computer.o

all: $(OBJ)
	$(CXX)  $(CXXFLAGS) $(OBJ) $(LIBS)


main.o: main.cpp main.h

computer.o: computer.cpp computer.h

.PHONY: clean
clean:
	rm *.o

