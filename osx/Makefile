SRCS_PATHS = ../tests ..
SRCS = $(foreach p,$(SRCS_PATHS),$(wildcard $(p)/*.cpp))
OBJS_PATH = objs
OBJS = $(foreach f,$(SRCS),$(OBJS_PATH)/$(basename $(notdir $(f))).o)
CFLAGS = -c -std=c++11 -I.

TARGET = server

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) -lstdc++

$(OBJS_PATH)/%.o : ../%.cpp
	$(CC) $(CFLAGS) -o $@ $<

$(OBJS_PATH)/%.o : ../tests/%.cpp
	$(CC) $(CFLAGS) -o $@ $<

run :
	cd ../tests && ../osx/server

$(info OBJS = $(OBJS))
