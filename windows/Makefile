PS = powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command

CPPFLAGS =
CFLAGS = /utf-8 /EHsc /std:c++latest /nologo /experimental:external /external:anglebrackets /external:W0 /Wall /wd4514 /Z7
LINKLIBS = d2d1.lib dwrite.lib
LINKFLAGS = /link /DEBUG
OBJECTS	= rasterise_text.obj rasterise_text_impl.obj

build: rasterise_text.exe

rasterise_text.obj: ../rasterise_text.cpp
	$(CPP) $(CFLAGS) $(CPPFLAGS) /c ../rasterise_text.cpp

rasterise_text_impl.obj: rasterise_text_impl.cpp
	$(CPP) $(CFLAGS) $(CPPFLAGS) /c rasterise_text_impl.cpp

rasterise_text.exe: $(OBJECTS)
	$(CPP) /nologo $(OBJECTS) $(LINKLIBS) $(LINKFLAGS)

regenerate_masters: rasterise_text.exe
	$(PS) ./generate_images.ps1 -Rasteriser ./rasterise_text.exe -OutputRoot ./master_images -TypeFace $(TYPEFACEPATH)

dist: build
	$(PS) (Remove-Item -Force -Recurse ..\dist -ErrorAction Ignore)
	if not exist ..\dist mkdir ..\dist
	copy rasterise_text.exe ..\dist\rasterise_text_windows.exe
	copy rasterise_text.pdb ..\dist\rasterise_text_windows.pdb
	copy generate_images.ps1 ..\dist
	copy run_tests.ps1 ..\dist
	copy vendor\TestApiCore.* ..\dist
	copy vendor\TestApiCore-LICENCE.txt ..\dist
	copy ..\VERSION ..\dist
	copy ..\LICENCE ..\dist
	copy NOTICE ..\dist

clean:
	$(PS) (Remove-Item -Force rasterise_text.exe, rasterise_text.pdb, *.obj, ..\dist\* -ErrorAction Ignore)
	if exist ..\dist rmdir /Q ..\dist

.PHONY: clean build dist
