# libutf8proc Makefile


# settings

cflags = -O2 -std=c99 -pedantic -Wall -fpic $(CFLAGS)
cc = $(CC) $(cflags)


# meta targets
front: utf8proc.o front.c mk_wcwidth.o
	$(CC) -g -o $@ $^

c-library: libutf8proc.a libutf8proc.so

ruby-library: ruby/utf8proc_native.so

pgsql-library: pgsql/utf8proc_pgsql.so


clean::
	rm -f utf8proc.o libutf8proc.a libutf8proc.so mk_wcwidth.o

# real targets

utf8proc.o: utf8proc.h utf8proc.c utf8proc_data.c
	$(cc) -g -c -o utf8proc.o utf8proc.c

libutf8proc.a: utf8proc.o
	rm -f libutf8proc.a
	ar rs libutf8proc.a utf8proc.o

libutf8proc.so: utf8proc.o
	$(cc) -shared -o libutf8proc.so utf8proc.o
	chmod a-x libutf8proc.so

libutf8proc.dylib: utf8proc.o
	$(cc) -dynamiclib -o $@ $^ -install_name $(libdir)/$@

ruby/Makefile: ruby/extconf.rb
	cd ruby && ruby extconf.rb

ruby/utf8proc_native.so: utf8proc.h utf8proc.c utf8proc_data.c \
		ruby/utf8proc_native.c ruby/Makefile
	cd ruby && make

ruby/gem/lib/utf8proc.rb: ruby/utf8proc.rb
	test -e ruby/gem/lib || mkdir ruby/gem/lib
	cp ruby/utf8proc.rb ruby/gem/lib/

ruby/gem/ext/extconf.rb: ruby/extconf.rb
	test -e ruby/gem/ext || mkdir ruby/gem/ext
	cp ruby/extconf.rb ruby/gem/ext/

ruby/gem/ext/utf8proc_native.c: utf8proc.h utf8proc_data.c utf8proc.c ruby/utf8proc_native.c
	test -e ruby/gem/ext || mkdir ruby/gem/ext
	cat utf8proc.h utf8proc_data.c utf8proc.c ruby/utf8proc_native.c | grep -v '#include "utf8proc.h"' | grep -v '#include "utf8proc_data.c"' | grep -v '#include "../utf8proc.c"' > ruby/gem/ext/utf8proc_native.c

ruby-gem:: ruby/gem/lib/utf8proc.rb ruby/gem/ext/extconf.rb ruby/gem/ext/utf8proc_native.c
	cd ruby/gem && gem build utf8proc.gemspec

pgsql/utf8proc_pgsql.so: utf8proc.h utf8proc.c utf8proc_data.c \
		pgsql/utf8proc_pgsql.c
	cd pgsql && make
mk_wcwidth.o: mk_wcwidth.c
	$(CC) -g -c -o $@ $^
