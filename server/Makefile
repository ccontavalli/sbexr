
.PHONY: update-templates

all: update-templates
	go build

update-templates:
	cd templates && ~/gopath/bin/qtc

test-index:
	make -C ../src VERBOSE= test-gen

test-run: test test-index all
	./server --metrics=../test/metrics --web-root=../test --index-dir=../test/indexes --listen=:10000

test: test-index all
	go test
	cd db && go test

clean:
	rm -f server

format:
	go fmt
	cd db && go fmt
	cd structs && go fmt
	
