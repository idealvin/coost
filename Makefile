UNIX_CODE = $(shell find  base/ -regex ".*[\.cc]")
# WIN_CODE = $(shell find  base/ -regex ".*[\.cpp]")


all: co

co:
	echo $(UNIX_CODE)
	g++ -c $(UNIX_CODE)
	ar rc co.a *.o
	rm *.o

clean:
	rm -rf co.a
