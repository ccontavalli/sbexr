
.PHONY: update-templates

all: update-templates
	go build

update-templates:
	cd templates && ~/gopath/bin/qtc

testindex: all
	./server --root=../test/ --index-dir=./testindex --listen=:10000 ../test/

clean:
	rm -f server
