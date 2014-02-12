TARGET = ftpsp
OBJS   = source/main.o source/ftpsp.o source/psp_functions.o \
         source/utils.o  source/mutex-imports.o

INCDIR   = include
CFLAGS   = -G0 -Wall -O2 -Wno-unused-function
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS  = $(CFLAGS)
LIBS     = -lpspsystemctrl_user -lpspgu -lpspnet -lpspdisplay \
           -lpspgum -lz -lm

BUILD_PRX       = 1 
PSP_FW_VERSION  = 371
EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = ftpsp

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak


copy: EBOOT.PBP
	mkdir -p "/media/$(USER)/disk/PSP/GAME/$(notdir $(CURDIR))"
	cp EBOOT.PBP "/media/$(USER)/disk/PSP/GAME/$(notdir $(CURDIR))"
	sync
