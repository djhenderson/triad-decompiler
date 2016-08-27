#PREFIX=/usr
PREFIX=$(HOME)

mrproper: triad clean 
debug:
	$(MAKE) -C src debug
triad: 
	$(MAKE) -C src triad
sys_tests:
	$(MAKE) -C tests sys_tests && tests/do_tests.sh
sys_tests64:
	$(MAKE) -C tests sys_tests64 && tests/do_tests64.sh
clean:
	$(MAKE) -C src clean
clean_tests:
	$(MAKE) -C tests clean
install:
	install src/triad $(HOME)/bin/triad
