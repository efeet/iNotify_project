CC=gcc
CFLAGS=-Isrc/
DEPS=src/ChkKernel.h src/CurrTime.h src/EnumIpInter.h src/IncludeLibraries.h src/RotateLog.h src/SockClient.h
OBJ=src/ChkKernel.o src/CurrTime.o src/EnumIpInter.o src/RotateLog.o src/SockClient.o src/inotify.o
APP_NAME=iNotify

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(APP_NAME): $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

clean:
	rm -f src/*.o $(APP_NAME) inotify.cfg
