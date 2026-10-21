# This makefile is for all platforms.

CC      = cc
CXX     = c++
CFLAGS  = -g -O3 -Wall -std=c++17 -Wno-psabi -pthread -MMD -MD -I/usr/local/include
LIBS    = -lpthread -lutil -lmosquitto
LDFLAGS = -g -L/usr/local/lib

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all:	MMDVM-Host

MMDVM-Host:	GitVersion.h $(OBJS) 
		$(CXX) $(OBJS) $(LDFLAGS) $(LIBS) -o MMDVM-Host

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<
-include $(DEPS)

.PHONY install:
install: all
		install -m 755 MMDVM-Host /usr/local/bin/

.PHONY install-service:
install-service: install /etc/MMDVM-Host.ini
		@useradd --user-group -M --system mmdvm --shell /bin/false || true
		@usermod --groups dialout --append mmdvm || true
		@mkdir /var/log/mmdvm || true
		@chown mmdvm:mmdvm /var/log/mmdvm
		@cp ./linux/systemd/mmdvmhost.service /lib/systemd/system/
		@systemctl enable mmdvmhost.service

/etc/MMDVM-Host.ini:
		@cp -n MMDVM-Host.ini /etc/MMDVM-Host.ini
		@sed -i 's/FilePath=./FilePath=\/var\/log\/mmdvm\//' /etc/MMDVM-Host.ini
		@sed -i 's/Daemon=0/Daemon=1/' /etc/MMDVM-Host.ini
		@chown mmdvm:mmdvm /etc/MMDVM-Host.ini

.PHONY uninstall-service:
uninstall-service:
		@systemctl stop mmdvmhost.service || true
		@systemctl disable mmdvmhost.service || true
		@rm -f /usr/local/bin/MMDVM-Host || true
		@rm -f /lib/systemd/system/mmdvmhost.service || true

clean:
		$(RM) MMDVM-Host *.o *.d *.bak *~ GitVersion.h

# Export the current git version if the index file exists, else 000...
GitVersion.h:
ifneq ("$(wildcard .git/index)","")
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@
else
	echo "const char *gitversion = \"0000000000000000000000000000000000000000\";" > $@
endif
