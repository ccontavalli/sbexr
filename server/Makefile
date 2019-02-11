
.PHONY: update-templates

all: update-templates
	go build

update-templates:
	cd templates && ~/gopath/bin/qtc

testindex: all
	make -C ../src test-gen
	./server --root=../test/ --index-dir=../test/indexes --listen=:10000 ../test/output

clean:
	rm -f server

format:
	go fmt
	cd db && go fmt
	cd structs && go fmt
	
