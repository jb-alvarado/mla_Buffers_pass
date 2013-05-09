EXENAME=mla_Buffers_pass

SSYSTEM=WINDOWS


CFLAGS=/TP /c /O2 /MD /nologo /W3 -DWIN_NT /EHsc

LFLAGS=/nologo /nodefaultlib:LIBC.LIB /OPT:NOREF /INCREMENTAL:NO

DEFINES=\
	/D "WIN32" \
	/D "NDEBUG" \
	/D "_CRT_SECURE_NO_WARNINGS"

INCLUDES=\
	/I "./" \
	/I "./src" \
	/I "./inc" \
	/I "./lib"
	
LIBS=\
	kernel32.lib\
	user32.lib\
	gdi32.lib\
	winspool.lib\
	comdlg32.lib\
	advapi32.lib\
	shell32.lib\
	comctl32.lib
	
LIBS32=\
	$(LIBS)\
	.\lib\x86\shader.lib
	
LIBS64=\
	$(LIBS)\
	.\lib\x64\shader.lib
	

OBJS=\
	src\mla_Buffers_pass.obj

	
OBJS32=\
	$(OBJS)
	
OBJS64=\
	$(OBJS)

32: $(OBJS32)
	@link $(LFLAGS) /MACHINE:IX86  /SUBSYSTEM:$(SSYSTEM) /DLL /OUT:./bin32/$(EXENAME).dll $(OBJS32) $(LIBS32)
	mt.exe -nologo -manifest ./bin32/$(EXENAME).dll.manifest -outputresource:./bin32/$(EXENAME).dll;2
	@echo - DONE -

64: $(OBJS64)
	@link $(LFLAGS) /MACHINE:AMD64 /SUBSYSTEM:$(SSYSTEM) /DLL /OUT:./bin64/$(EXENAME).dll $(OBJS64) $(LIBS64)
	mt.exe -nologo -manifest ./bin64/$(EXENAME).dll.manifest -outputresource:./bin64/$(EXENAME).dll;2
	@echo - DONE -

.cpp.obj:
	@cl $(CFLAGS) $(DEFINES) $(INCLUDES) /Fo$*.obj /c $<
.c.obj:
	@cl $(CFLAGS) $(DEFINES) $(INCLUDES) /Fo$*.obj /c $<

clean:
	@del .\*.idb
	@del .\bin32\*.exp
	@del .\bin32\*.lib
	@del .\bin64\*.exp
	@del .\bin64\*.lib
	@del src\*.obj
	
	