PS = powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command

CPPFLAGS = /utf-8 /EHsc /std:c++latest /nologo /experimental:external /external:anglebrackets /external:W0 /Wall
LINKFLAGS = /nologo d2d1.lib dwrite.lib
OBJECTS	= rasterise_text.obj rasterise_text_impl.obj

build: rasterise_text.exe

rasterise_text.obj: ../rasterise_text.cpp
	$(CPP) $(CPPFLAGS) /c ../rasterise_text.cpp

rasterise_text.exe: $(OBJECTS)
	$(CPP) $(LINKFLAGS) $(OBJECTS)

regenerate_masters: rasterise_text.exe
	$(PS) ./generate_images.ps1 -Rasteriser ./rasterise_text.exe -OutputRoot ./master_images -TypeFace $(TYPEFACEPATH)

clean:
	$(PS) (Remove-Item -Force rasterise_text.exe, *.obj -ErrorAction Ignore)

.PHONY: clean build
