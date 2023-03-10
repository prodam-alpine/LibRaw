all: sources library all_samples dcraw_binaries

PP=./internal/preprocess.pl

CC=gcc
CXX=g++

#CFLAGS=-O4 -march=core2 -march=core2 -mtune=core2 -fomit-frame-pointer -ffast-math -fstrict-aliasing -funsafe-math-optimizations -mfpmath=sse -msse2 -I. 
#CFLAGS=-g -Wall -I.
#CFLAGS=-g -I. -Wall
CFLAGS=-g -O4 -I. -Wall -Wno-long-long -Wno-conversion -Wno-sign-compare -fpack-struct=8 -fopenmp

# GCC 4.4 section
# CC=gcc44
# CXX=g++44
# CFLAGS= -O4 -march=core2 -I. -pedantic  -Wno-long-long -msse2 -mfpmath=sse  -fopenmp

# LCMS support
#LCMS_DEF=-DUSE_LCMS -I/usr/local/include
#LCMS_LIB=-L/usr/local/lib -llcms

DCRAW_GEN= internal/dcraw_common.cpp internal/dcraw_fileio.cpp
DCRAW_LIB_OBJECTS=object/dcraw_common.o object/libraw_cxx.o object/libraw_c_api.o object/dcraw_fileio.o
DCRAW_LIB_MT_OBJECTS=object/dcraw_common_mt.o object/libraw_cxx_mt.o object/libraw_c_api_mt.o object/dcraw_fileio_mt.o
LR_INCLUDES=libraw/libraw.h libraw/libraw_alloc.h libraw/libraw_const.h libraw/libraw_datastream.h libraw/libraw_internal.h libraw/libraw_types.h libraw/libraw_version.h

sources: ${DCRAW_GEN} Makefile ${PP}
library: lib/libraw.a lib/libraw_r.a

all_samples: bin/raw-identify bin/simple_dcraw  bin/dcraw_emu bin/dcraw_half bin/half_mt bin/mem_image bin/unprocessed_raw bin/4channels

bin/raw-identify: lib/libraw.a samples/raw-identify.cpp
	$(CXX) ${LCMS_DEF} ${CFLAGS} -o bin/raw-identify samples/raw-identify.cpp -L./lib -lraw  -lm  ${LCMS_LIB}

bin/simple_dcraw: lib/libraw.a samples/simple_dcraw.cpp
	$(CXX) -DLIBRAW_NOTHREADS ${LCMS_DEF} ${CFLAGS} -o bin/simple_dcraw samples/simple_dcraw.cpp -L./lib -lraw  -lm  ${LCMS_LIB}

bin/unprocessed_raw: lib/libraw.a samples/unprocessed_raw.cpp
	$(CXX) -DLIBRAW_NOTHREADS ${LCMS_DEF} ${CFLAGS} -o bin/unprocessed_raw samples/unprocessed_raw.cpp -L./lib -lraw  -lm  ${LCMS_LIB}

bin/4channels: lib/libraw.a samples/4channels.cpp
	$(CXX) -DLIBRAW_NOTHREADS ${LCMS_DEF} ${CFLAGS} -o bin/4channels samples/4channels.cpp -L./lib -lraw  -lm  ${LCMS_LIB}

bin/mem_image: lib/libraw.a samples/mem_image.cpp
	$(CXX) ${LCMS_DEF} ${CFLAGS} -o bin/mem_image samples/mem_image.cpp -L./lib -lraw  -lm  ${LCMS_LIB}

bin/dcraw_half: lib/libraw.a samples/dcraw_half.c
	$(CC) ${LCMS_DEF} ${CFLAGS} -o bin/dcraw_half samples/dcraw_half.c -L./lib -lraw  -lm -lstdc++  ${LCMS_LIB}

bin/half_mt: lib/libraw_r.a samples/half_mt.c
	$(CC) -pthread ${LCMS_DEF} ${CFLAGS} -o bin/half_mt samples/half_mt.c -L./lib -lraw_r  -lm -lstdc++  ${LCMS_LIB}

bin/dcraw_emu: lib/libraw.a samples/dcraw_emu.cpp
	$(CXX) ${CFLAGS} ${LCMS_DEF} -o bin/dcraw_emu samples/dcraw_emu.cpp -L./lib -lraw_r  -lm ${LCMS_LIB}

dcraw_binaries: bin/dcraw_dist

bin/dcraw_dist: dcraw/dcraw.c Makefile
	$(CXX) -w -O4 -DLIBRAW_NOTHREADS -DNO_JPEG -DNO_LCMS -I/usr/local/include -o bin/dcraw_dist dcraw/dcraw.c -lm -L/usr/local/lib

regenerate:
	${PP} -N -DDEFINES dcraw/dcraw.c  >internal/defines.h
	${PP} -N -DCOMMON dcraw/dcraw.c >internal/dcraw_common.cpp
	${PP} -N -DFILEIO dcraw/dcraw.c >internal/dcraw_fileio.cpp

internal/defines.h: dcraw/dcraw.c  ${PP}
	${PP} -DDEFINES dcraw/dcraw.c  >internal/defines.h

internal/dcraw_common.cpp: dcraw/dcraw.c internal/defines.h  ${PP} Makefile
	${PP} -DCOMMON dcraw/dcraw.c >internal/dcraw_common.cpp

internal/dcraw_fileio.cpp: dcraw/dcraw.c internal/defines.h  ${PP} Makefile
	${PP} -DFILEIO dcraw/dcraw.c >internal/dcraw_fileio.cpp

object/dcraw_common.o: internal/dcraw_common.cpp ${LR_INCLUDES}
	$(CXX) -c -DLIBRAW_NOTHREADS ${CFLAGS} ${LCMS_DEF} -o object/dcraw_common.o internal/dcraw_common.cpp

object/dcraw_fileio.o: internal/dcraw_fileio.cpp ${LR_INCLUDES}
	$(CXX) -c -DLIBRAW_NOTHREADS ${CFLAGS} ${LCMS_DEF} -o object/dcraw_fileio.o internal/dcraw_fileio.cpp

object/libraw_cxx.o: src/libraw_cxx.cpp ${LR_INCLUDES}
	$(CXX) -c -DLIBRAW_NOTHREADS ${LCMS_DEF} ${CFLAGS} -o object/libraw_cxx.o src/libraw_cxx.cpp

object/libraw_c_api.o: src/libraw_c_api.cpp ${LR_INCLUDES}
	$(CXX) -c -DLIBRAW_NOTHREADS ${LCMS_DEF}  ${CFLAGS} -o object/libraw_c_api.o src/libraw_c_api.cpp

lib/libraw.a: ${DCRAW_LIB_OBJECTS}
	rm -f lib/libraw.a
	ar crv lib/libraw.a ${DCRAW_LIB_OBJECTS}
	ranlib lib/libraw.a

lib/libraw_r.a: ${DCRAW_LIB_MT_OBJECTS}
	rm -f lib/libraw_r.a
	ar crv lib/libraw_r.a ${DCRAW_LIB_MT_OBJECTS}
	ranlib lib/libraw_r.a

object/dcraw_common_mt.o: internal/dcraw_common.cpp ${LR_INCLUDES}
	$(CXX) -c -pthread ${LCMS_DEF} ${CFLAGS} -o object/dcraw_common_mt.o internal/dcraw_common.cpp

object/dcraw_fileio_mt.o: internal/dcraw_fileio.cpp ${LR_INCLUDES}
	$(CXX)  -c -pthread ${LCMS_DEF} ${CFLAGS} -o object/dcraw_fileio_mt.o internal/dcraw_fileio.cpp

object/libraw_cxx_mt.o: src/libraw_cxx.cpp ${LR_INCLUDES}
	$(CXX) -c ${LCMS_DEF} -pthread ${CFLAGS} -o object/libraw_cxx_mt.o src/libraw_cxx.cpp

object/libraw_c_api_mt.o: src/libraw_c_api.cpp ${LR_INCLUDES}
	$(CXX) -c ${LCMS_DEF} -pthread ${CFLAGS} -o object/libraw_c_api_mt.o src/libraw_c_api.cpp

clean:
	rm -fr bin/*.dSYM
	rm -f *.o *~ src/*~ samples/*~ internal/*~ libraw/*~ lib/lib*.a bin/[4a-z]* object/*o dcraw/*~ doc/*~ bin/*~

fullclean: clean
	rm -f ${DCRAW_GEN}
