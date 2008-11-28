OBJ = bin/common.o bin/hash.o bin/suicide.o bin/pns.o bin/egtb.o \
	bin/webserver.o

# Targets:
# all: full-strength with book, EGTB and hash tables
# weak: none of the above
# research: book and EGTB, but no hash (since no alpha-beta)
# browse: book only, no EGTB and no hash (all we do is query the book)
# no_egtb: full-strength, but without the endgame tables

ifeq "$(MAKECMDGOALS)" "weak"
WEAK_OPT = -D USE_BOOK=0 -D USE_EGTB=0 -D USE_HASH=0 -D WEAKENED=1
else
ifeq "$(MAKECMDGOALS)" "research"
WEAK_OPT = -D USE_BOOK=1 -D USE_EGTB=1 -D USE_HASH=0 -D WEAKENED=0
else
ifeq "$(MAKECMDGOALS)" "browse"
WEAK_OPT = -D USE_BOOK=1 -D USE_EGTB=0 -D USE_HASH=0 -D WEAKENED=0
else
ifeq "$(MAKECMDGOALS)" "no_egtb"
WEAK_OPT = -D USE_BOOK=1 -D USE_EGTB=0 -D USE_HASH=1 -D WEAKENED=0
else
WEAK_OPT =
endif
endif
endif
endif

CC_OPT = -Wall -O3 $(WEAK_OPT)


all: bin nilatac nilatac-pn
weak: bin nilatac
research: bin nilatac-pn
browse: bin nilatac-pn
no_egtb: bin nilatac nilatac-pn

nilatac: $(OBJ) nilatac.cc
	g++ $(CC_OPT) -lpthread -o nilatac $(OBJ) nilatac.cc

nilatac-pn: $(OBJ) nilatac-pn.cc
	g++ $(CC_OPT) -lpthread -o nilatac-pn $(OBJ) nilatac-pn.cc

lenthep: $(OBJ) lenthep.cc
	g++ $(CC_OPT) -lpthread -o lenthep $(OBJ) lenthep.cc

bin/common.o: common.cc common.h
	g++ $(CC_OPT) -c -o bin/common.o common.cc

bin/egtb.o: egtb.cc egtb.h
	g++ $(CC_OPT) -c -o bin/egtb.o egtb.cc

bin/hash.o: hash.cc hash.h
	g++ $(CC_OPT) -c -o bin/hash.o hash.cc

bin/pns.o: pns.cc pns.h
	g++ $(CC_OPT) -c -o bin/pns.o pns.cc

bin/suicide.o: suicide.cc suicide.h
	g++ $(CC_OPT) -c -o bin/suicide.o suicide.cc

bin/webserver.o: webserver.cc webserver.h
	g++ $(CC_OPT) -c -o bin/webserver.o webserver.cc

bin:
	mkdir -p bin
	test -L egtb || ln -s ../egtb

clean:
	rm -rf bin
	rm -f nilatac nilatac-pn lenthep egtb *~
