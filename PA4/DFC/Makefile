EXE=dfc

# Main target
all: $(EXE)

#  MinGW
ifeq "$(OS)" "Windows_NT"
CFLG=-O3 -Wall

CLEAN=del *.exe *.o *.a
else
#  OSX
ifeq "$(shell uname)" "Darwin"
CFLG=-O3 -Wall -Wno-deprecated-declarations

#  Linux/Unix/Solaris
else
CFLG=-O3 -Wall

endif
#  OSX/Linux/Unix/Solaris
CLEAN=rm -f $(EXE) *.o *.a
endif

#  Compile and link
dfc:dfc.c
	gcc $(CFLG) -o $@ $^ 

#  Clean
clean:
	$(CLEAN)
