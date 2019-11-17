
# "Ham Radio of Things" MQTT gateway.

CC ?= gcc
CFLAGS = -Wall -g
LDFLAGS = -lmosquitto

INSTALL ?= install
INSTALL_PROGRAM ?= ${INSTALL} -D --mode=755
DESTDIR ?= /usr/local


all: hrotgw

hrotgw : hrotgw.o subscribe.o aprsmsg.o
	$(CC) -o $@ $^ $(LDFLAGS)

*.o : *.h


# Install - Must be done as root.

.PHONY : install
install:
	$(INSTALL_PROGRAM) hrotgw $(DESTDIR)/bin/hrotgw

# Test / Demonstration.

# Mosquitto must already be running.
# A simulated sensor thing publishes to tempurature and humdity. 
# Two other simulated things subscribe to one of those each.

.PHONY : check
check:
	@rm -rf /dev/shm/RQ /dev/shm/TQ
	@if [ ! -d /dev/shm/RQ ] ; then mkdir /dev/shm/RQ ; fi
	@if [ ! -d /dev/shm/TQ ] ; then mkdir /dev/shm/TQ ; fi
	@bash -c "timeout 10 ./hrotgw -m BROKER-15 " &
	@sleep 2
	@echo "HEATER>X::BROKER-15:sub:temperature" > /dev/shm/RQ/1
	@echo "WATER>X::BROKER-15:sub:humidity" > /dev/shm/RQ/2
	@sleep 1
	@echo "SENSOR>X::BROKER-15:pub:temperature=50"  > /dev/shm/RQ/3
	@echo "SENSOR>X::BROKER-15:pub:humidity=60"  > /dev/shm/RQ/4
	@sleep 1
	@echo "SENSOR>X::BROKER-15:pub:temperature=70"  > /dev/shm/RQ/5
	@echo "SENSOR>X::BROKER-15:pub:humidity=80"  > /dev/shm/RQ/6
	@sleep 1
	@echo "HEATER>X::BROKER-15:uns:temperature" > /dev/shm/RQ/7
	@echo "WATER>X::BROKER-15:uns:humidity" > /dev/shm/RQ/8
	@sleep 1
	@echo "SENSOR>X::BROKER-15:pub:temperature=99"  > /dev/shm/RQ/9
	@echo "SENSOR>X::BROKER-15:pub:humidity=99"  > /dev/shm/RQ/10
	@sleep 1
	@if [ `ls /dev/shm/RQ | wc -l` != 0 ] ; then echo FAILURE-R ; exit 1 ; fi	
	@if [ `ls /dev/shm/TQ | wc -l` != 4 ] ; then echo FAILURE-T ; exit 1 ; fi	
	@echo SUCCESS

.PHONY : clean
clean :
	rm -f hrotgw *.o
