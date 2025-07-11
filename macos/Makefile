QTPATH := ${shell brew --prefix qt}
BOOSTPATH := ${shell brew --prefix boost}
CXXFLAGS := -O3 -std=c++26 -Weverything -Werror -Wfatal-errors -pedantic-errors -ftemplate-backtrace-limit=1 --system-header-prefix=boost/ -I${BOOSTPATH}/include -xobjective-c++ -Wno-poison-system-directories -Wno-c++98-compat -Wno-padded -Wno-unqualified-std-cast-call
LDFLAGS := -Wfatal-errors
QTOBJECTS := rasterise_text.o qt/rasterise_text_impl.o
CORETEXTOBJECTS := rasterise_text.o coretext/rasterise_text_impl.o
BINS := qt/rasterise_text coretext/rasterise_text

build: qt/rasterise_text coretext/rasterise_text

rasterise_text.o: ../rasterise_text.cpp ../rasterise_text.hpp
	${CXX} ${CXXFLAGS} -c ../rasterise_text.cpp

qt/rasterise_text_impl.o: qt/rasterise_text_impl.cpp ../rasterise_text.hpp
	${CXX} ${CXXFLAGS} --system-header-prefix=QtGui/ --system-header-prefix=QtWidgets/ -F${QTPATH}/Frameworks -c qt/rasterise_text_impl.cpp -o $@
qt/rasterise_text: ${QTOBJECTS}
	${CXX} ${LDFLAGS} ${QTOBJECTS} -F${QTPATH}/Frameworks -framework QtGui -framework QtCore -framework QtWidgets -o $@

coretext/rasterise_text: ${CORETEXTOBJECTS}
	${CXX} ${LDFLAGS} ${CORETEXTOBJECTS} -framework ImageIO -framework CoreGraphics -framework CoreText -framework CoreServices -framework CoreFoundation -framework UniformTypeIdentifiers -o $@

dist: build
	mkdir -p ../dist
	cp qt/rasterise_text ../dist/rasterise_text_qt
	cp coretext/rasterise_text ../dist/rasterise_text_coretext
	cp generate_images ../dist/
	cp run_tests ../dist/

clean:
	${RM} -r ${QTOBJECTS} ${CORETEXTOBJECTS} ${BINS} dist/

.PHONY: build clean dist
