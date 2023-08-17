CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2
GCC = g++
LINK = $(GCC)
AR = ar

HEADERS	:= -I include -I objs/st-1.9/obj
SOURCES	:= $(wildcard src/*.cpp)
OBJS	:= $(addprefix objs/,$(patsubst %.cpp,%.o,$(SOURCES)))

.PHONY: clean server show
default: server

server: rtmp_server

show:
	@echo $(OBJS)
	@echo $(SOURCES)

objs/src/%.o : src/%.cpp 
	mkdir -p $(dir $@)
	$(GCC) -c $< $(CXXFLAGS) $(HEADERS) -o $@

rtmp_server: $(OBJS)
	mkdir -p $(dir $@)
	$(LINK)  -o objs/rtmp_server $(OBJS) objs/st-1.9/obj/libst.a -ldl

clean: 
	(cd objs; rm -rf src rtmp_server)