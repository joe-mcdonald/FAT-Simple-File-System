.phony all:
all: diskinfo disklist diskget diskput

diskinfo: diskinfo.c
	gcc diskinfo.c -lpthread -o diskinfo

disklist: disklist.c
	gcc disklist.c -lpthread -o disklist

diskget: diskget.c
	gcc diskget.c -lpthread -o diskget

diskput: diskput.c
	gcc diskput.c -lpthread -o diskput


.PHONY clean:
clean:
	-rm -rf *.o *.exe
