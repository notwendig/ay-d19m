# Makefile ay-d19m kernel modul
#
#	AUTHOR "Jürgen Willi Sievers <JSievers@NadiSoft.de>";
#	Mi 20. Mai 01:23:43 CEST 2020 Version 1.0.0 untested
#
# Comment/uncomment the following line to disable/enable debugging

#DEBUG = y

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)


MODSRC=$(shell pwd)
obj-m+=ay-d19m.o 

all:
	make -C /lib/modules/$(shell uname -r)/build M=${MODSRC} modules
	sudo depmod -a

clean:
	make -C /lib/modules/$(shell uname -r)/build M=${MODSRC} clean

INDENTFLAG=-nbad -bap -nbc -bbo -hnl -br -brs -c33 -cd33 -ncdb -ce -ci4 \
	-cli0 -d0 -di1 -nfc1 -i8 -ip0 -l80 -lp -npcs -nprs -npsl -sai \
	-saf -saw -ncs -nsc -sob -nfca -cp33 -ss -ts8 -il1

indent:
	indent $(INDENTFLAGS) ay-d19m.c
	indent $(INDENTFLAGS) ay-d19m.h
	indent $(INDENTFLAGS) decoder.c
	indent $(INDENTFLAGS) decoder.h