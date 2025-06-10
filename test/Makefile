all: spec sge iov resource qp
CC = gcc
CFLAGS = -fPIC -Wall -g -O0 -I include -D_GNU_SOURCE -Wshadow -Wformat=2 -Wwrite-strings -fstack-protector-strong -Wnull-dereference -Wunreachable-code
INC=-I../include
LDLIBS = -lcunit

spec: ib udp ip ipv6

ib:
	$(CC) test-ib-spec.c $(INC) $(LDLIBS) -Wall -g -o test-ib-spec
	./test-ib-spec

udp:
	$(CC) test-udp-spec.c $(INC) $(LDLIBS) -Wall -g -o test-udp-spec
	./test-udp-spec

ip:
	$(CC) test-ip-spec.c $(INC) $(LDLIBS) -Wall -g -o test-ip-spec
	./test-ip-spec

ipv6:
	$(CC) test-ipv6-spec.c $(INC) $(LDLIBS) -Wall -g -o test-ipv6-spec
	./test-ipv6-spec

sge:
	$(CC) test-sge.c $(INC) -Wall $(LDLIBS) -g -o test-sge
	./test-sge

iov:
	$(CC) test-iov.c $(INC) -Wall $(LDLIBS) -g -o test-iov
	./test-iov

resource:
	$(CC) test-resource.c ../libroce.a $(INC) -Wall $(LDLIBS) -g -o test-resource
	./test-resource

gsi:
	$(CC) test-gsi.c ../libroce.a $(INC) $(LDLIBS) -lpthread $(CFLAGS) -g -o test-gsi
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./test-gsi

qp:
	$(CC) test-qp.c ../libroce.a $(INC) $(LDLIBS) -lpthread $(CFLAGS) -g -o test-qp
	./test-qp

valgrind: qp
	valgrind --track-origins=yes --show-possibly-lost=no --leak-check=full ./test-qp

clean:
	@rm -rf test-ib-spec
	@rm -rf test-udp-spec
	@rm -rf test-ip-spec
	@rm -rf test-ipv6-spec
	@rm -rf test-sge test-iov
	@rm -rf test-resource
	@rm -rf test-qp
	@rm -rf test-gsi
	@rm -rf *.cap
