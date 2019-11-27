#******************************************************************************
#**  CarMaker - Version 8.0.2
#**  Vehicle Dynamic Simulation Toolkit
#**
#**  Copyright (C)   IPG Automotive GmbH
#**                  Bannwaldallee 60             Phone  +49.721.98520.0
#**                  76185 Karlsruhe              Fax    +49.721.98520.99
#**                  Germany                      WWW    www.ipg-automotive.com
#******************************************************************************

include C:/IPG/carmaker/win64-8.0.2/include/MakeDefs.win64


#OPT_CFLAGS =	-g
OPT_CFLAGS =	-Os -DNDEBUG

LIBS =		$(CARMAKER_LIB_DIR)/libapo-client-$(ARCH).a

OBJS =		ApoClntDemo.o ApoClnt.o GuiCmd.o DVAWrite.o DVARead.o


ApoClntDemo.$(ARCH)$(EXE_EXT):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) $(LD_LIBS_OS)

clean:
	rm -f *~ *.o a.out a.exe core core.* ApoClntDemo.$(ARCH)$(EXE_EXT)



ApoClntDemo.o:	ApoClnt.h DVA.h
ApoClnt.o:	ApoClnt.h
GuiCmd.o:	GuiCmd.h ApoClnt.h
DVARead.o:	DVA.h ApoClnt.h
DVAWrite.o:	DVA.h

