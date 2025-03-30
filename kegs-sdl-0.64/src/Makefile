# KEGS Makefile--set all options in the "vars" file!

include vars

PACKAGE= kegs-sdl
VERSION = 0.64
OBJECTS1 = adb.o clock.o dis.o joystick_driver.o paddles.o scc.o scc_driver.o iwm.o moremem.o sim65816.o smartport.o sound.o video.o video_driver.o video_driver_x11.o video_driver_win32.o video_driver_sdl.o config_sdl.o configmenu.o sound_driver.o functions.o font.o ki.o rom.o
PROJROOT = .

SPECIALS = $(EXTRA_SPECIALS) 8inst_c 16inst_c size_c

.PHONY : all clean depend dist specials_clean

all: $(NAME)$(SUFFIX) partls to_pro dc2raw

clean: specials_clean
	rm -f *.o kegs-sdl partls to_pro dc2raw *~ #*#

depend:
	makedepend $(OPTS) $(XOPTS) $(OBJECTS1:.o=.c)

dist:
	cd ../..; tar cf - `cat $(PACKAGE)-$(VERSION)/FILELIST | sed -e s@^@$(PACKAGE)-$(VERSION)/@` |gzip -c > $(PACKAGE)-$(VERSION).tar.gz

specials_clean:
	rm -f $(SPECIALS) size_s Alib.h


$(NAME)$(SUFFIX): $(SPECIALS) $(OBJECTS) compile_time.o
	$(LD) $(LDOPTS) $(OBJECTS) compile_time.o $(LDFLAGS) -o $(NAME)$(SUFFIX) $(XLIBS) $(EXTRA_LIBS)

8inst_s: instable.h
	$(PERL) $(PROJROOT)/make_inst s 8 instable.h > 8inst_s

16inst_s: instable.h
	$(PERL) $(PROJROOT)/make_inst s 16 instable.h > 16inst_s

8inst_c: instable.h
	$(PERL) $(PROJROOT)/make_inst c 8 instable.h > 8inst_c

16inst_c: instable.h
	$(PERL) $(PROJROOT)/make_inst c 16 instable.h > 16inst_c

8size: size_tab.h
	$(PERL) $(PROJROOT)/make_size 8 size_tab.h > 8size

16size: size_tab.h
	$(PERL) $(PROJROOT)/make_size 16 size_tab.h > 16size

size_c: size_tab.h
	$(PERL) $(PROJROOT)/make_size c size_tab.h > size_c

size_s: size_tab.h
	$(PERL) $(PROJROOT)/make_size s size_tab.h > size_s

Alib.h:
	rm -f Alib.h
	$(PERL) $(PROJROOT)/make_alib

engine_s.o: 8inst_s 8size 16inst_s 16size

engine_c.o: 8inst_c 16inst_c

.s.o:
	$(AS) -c $(OPTS) $*.s

.c.o:
	$(CC) $(CCOPTS) $(XOPTS) -c $(OPTS) $*.c

partls: partls.c
	$(CC) $(CCOPTS) $(OPTS) -o partls$(SUFFIX) partls.c

dc2raw: dc2raw.c
	$(CC) $(CCOPTS) $(OPTS) -o dc2raw$(SUFFIX) dc2raw.c

to_pro: prodos.h prodos_protos.h to_pro.c
	$(CC) $(CCOPTS) $(OPTS) -o to_pro$(SUFFIX) to_pro.c

compile_time.o: $(OBJECTS)

rom.c: makerom$(SUFFIX)
	./$< > rom.c || (rm -f rom.c && false)

makerom$(SUFFIX): makerom.c
	$(CC) $(CCOPTS) $(OPTS) -o $@ $<

# DO NOT DELETE
