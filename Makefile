CC=g++
DEBUG=-g
CFLAGS=$(DEBUG) -Wall
CXXFLAGS=$(DEBUG) -Wall

VER=1.2

OBJS=fspcc.o scanner.o parser.o symbols_table.o lts.o context.o translator.o utils.o callbacks.o circular_buffer.o serializer.o shell.o

WCIN=callbacks.?pp context.?pp fspcc.cpp fsp.lex fsp.ypp input.fsp interface.hpp lts.?pp Makefile scanner.hpp symbols_table.?pp translator.?pp utils.?pp circular_buffer.?pp serializer.?pp shell.?pp ltsee csee.sh parser.diff
SOURCES=$(WCIN) fspcc.1 fsp.y

#REPORT=--report=all
REPORT=

all: fspcc

fspcc: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o fspcc -lncurses

fspcc.o: symbols_table.hpp lts.cpp interface.hpp parser.hpp

symbols_table.o: symbols_table.hpp symbols_table.cpp

strings_set.o: strings_set.hpp strings_set.cpp

lts.o: lts.hpp symbols_table.hpp parser.hpp shell.hpp

context.o: context.hpp symbols_table.hpp

parser.o: context.hpp symbols_table.hpp parser.cpp lts.hpp utils.hpp scanner.hpp translator.hpp callbacks.o interface.hpp circular_buffer.hpp

translator.o: translator.hpp parser.hpp

utils.o: utils.hpp parser.hpp

callbacks.o: callbacks.hpp utils.hpp context.hpp translator.hpp lts.hpp parser.hpp

circular_buffer.o: circular_buffer.hpp

serializer.o: serializer.hpp

shell.o: shell.hpp lts.hpp parser.hpp

parser.cpp parser.hpp: fsp.ypp fsp.y parser.diff
	bison $(REPORT) fsp.ypp
	patch parser.cpp < parser.diff

# This rule has been made explicit only to avoid compiler warnings (-Wall)
scanner.o: scanner.cpp
	$(CC) $(DEBUG) -c scanner.cpp

scanner.cpp: fsp.lex parser.hpp
	flex fsp.lex

tags:
	cscope -R

clean: cleanaur clc
	-rm *.o fspcc scanner.cpp parser.cpp parser.hpp *.out

testing: fspcc
	tests/test.sh

lines:
	wc -l $(WCIN)

aurlocal: fspcc-$(VER).tar.gz
	python create_pkgbuild.py local $(VER)

aur:
	-rm fspcc-$(VER).tar.gz
	wget "https://bitbucket.org/vmaffione/fspc/downloads/fspcc-$(VER).tar.gz"
	python create_pkgbuild.py remote $(VER)

fspcc-$(VER).tar.gz:
	tar -czf fspcc-$(VER).tar.gz $(SOURCES)

cleanaur:
	-rm *.tar.gz PKGBUILD

clc:
	-rm *.gv *.lts *.png .*.png *.bfsp
