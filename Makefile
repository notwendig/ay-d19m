# Makefile ay-d19m kernel modul
#
#	AUTHOR "Jürgen Willi Sievers <JSievers@NadiSoft.de>";
#	Mi 20. Mai 01:23:43 CEST 2020 Version 1.0.0 untested
#

MODSRC=$(shell pwd)


obj-m += ay_d19m.o 
#ay_d19m-y := decoder.o

default:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=${MODSRC} modules
	sudo depmod -a

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=${MODSRC} clean
