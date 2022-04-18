CFLAGS ?= -g

all: speakupmap.h

mapdata.h: makemapdata
	./$< > $@

genmap: genmap.c mapdata.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

speakupmap.h: speakupmap.map genmap
	./genmap speakupmap.map >$@

clean:
	rm -f genmap makemapdata mapdata.h speakupmap.h
