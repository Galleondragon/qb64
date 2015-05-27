CXXFLAGS += -DDEPENDENCY_LOADFONT -DFREEGLUT_STATIC
LDFLAGS  += -lX11 -lGL -lGLU -lglut -lfreetype -lpthread -ldl -lrt

CURDIR ?= $(.CURDIR)
srcdir = $(CURDIR)
CXXFLAGS += -iquote $(srcdir)/internal/source

VPATH = $(srcdir)/internal/c
.PATH: $(VPATH)

.PHONY: all clean
all: qb64

objs = qbx.o libqb.o
qb64: $(objs)
	$(CXX) -o $@ $(objs) $(LDFLAGS)

clean:
	rm -f qb64 $(objs)

