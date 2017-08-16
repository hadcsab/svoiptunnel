# Copyright 2008 Lubomir Bourdev and Hailin Jin
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

.SUFFIXES: .cpp 
CXX=g++
#CXX_FLAGS=-Wall -O2 -DNDEBUG
CXX_FLAGS=-Wall -O2 -D_DEBUG

#LDFLAGS=-g
LDLIBS=-lboost_system -lboost_regex -lboost_date_time -lssl -lcrypto -lpthread

TARGET=voiptunnel

SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET):$(OBJECTS)
	@echo linking $@
	@${CXX} ${OBJECTS} ${LDFLAGS} ${LDLIBS} -o $@

.cpp.o:
	@echo compiling $<
	@${CXX} -c ${CXX_FLAGS} $< -o $@

clean:
	@echo "Cleaning"
	@-rm -f ${OBJECTS} ${TARGET}

depend: .depend

.depend: 
	@echo "building dependency"
	makedepend $(CXX_FLAGS) $(SOURCES) -f.depend

include .depend
