src = $(wildcard src/*.c)

obj = $(patsubst %.c,   %.o, $(wildcard *.c)) \
      $(patsubst %.cpp, %.o, $(wildcard *.cpp))

bin = oculus

libgl = -lglfw3 -lglew
libovr = -L/Users/yu/Desktop/Dissertation/Coding/Oculus/OculusSDK/LibOVR/Lib/Mac/Release -lovr
libmath = -lm
libx11 = -I/opt/X11/include -L/opt/X11/lib -lX11

CFLAGS = -pedantic -Wall
LDFLAGS = $(libgl) $(libmath) $(libovr) $(libx11)
FRAMEWORKS= -framework OpenGL  -framework Cocoa -framework IOKit -framework CoreVideo

$(bin): $(obj)
	clang++ -v -o $(bin) $(obj) $(CFLAGS) $(LDFLAGS) $(FRAMEWORKS)

-include $(patsubst %.o, %.d, $(obj))

.PHONY: clean
clean:
	rm -rf $(wildcard *.o *.d *.log $(bin))
