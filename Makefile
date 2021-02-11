TARGET		= mcomms

CFILES		= 
CXXFILES	= main.cpp serial.cpp siofs.cpp upload.cpp

ifeq ($(OS),Windows_NT)

INCLUDE		=
LIBS		= -lshlwapi

else

INCLUDE		=
LIBS		=

endif

INCLUDE		+=
LIBS		+=

OFILES		= $(addprefix build/,$(CFILES:.c=.o) $(CXXFILES:.cpp=.o))

CFLAGS		= -O2
CXXFLAGS	= $(CFLAGS)

CC			= gcc
CXX			= g++

all: $(OFILES)
	$(CXX) $(CFLAGS) $(OFILES) $(LIBS) -o $(TARGET)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
	
build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
	
clean:
	rm -Rf build
