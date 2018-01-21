SRCS_PATHS = ../tests ..
SRCS = $(foreach p,$(SRCS_PATHS),$(wildcard $(p)/*.cpp))
OBJS_PATH = objs
OBJS = $(foreach f,$(SRCS),$(OBJS_PATH)/$(basename $(notdir $(f))).o)
CFLAGS = -c -std=c++11 -I.

TARGET = server

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) -lstdc++

$(OBJS_PATH)/%.o : ../%.cpp ../http_server.h
	mkdir -p $(OBJS_PATH) && $(CC) $(CFLAGS) -o $@ $<

$(OBJS_PATH)/%.o : ../tests/%.cpp
	mkdir -p $(OBJS_PATH) && $(CC) $(CFLAGS) -o $@ $<

run :
	cd ../tests && ../osx/server

clean : 
	rm $(OBJS)

$(info OBJS = $(OBJS))
