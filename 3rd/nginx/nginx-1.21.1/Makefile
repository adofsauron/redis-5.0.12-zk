
default:	build

clean:
	rm -rf Makefile objs

.PHONY:	default clean

build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

modules:
	$(MAKE) -f objs/Makefile modules

upgrade:
	/usr/bin/sbin/nginx -t

	kill -USR2 `cat /usr/bin/logs/nginx.pid`
	sleep 1
	test -f /usr/bin/logs/nginx.pid.oldbin

	kill -QUIT `cat /usr/bin/logs/nginx.pid.oldbin`

.PHONY:	build install modules upgrade
