
.PHONY: update-templates

all: update-templates
	go build

update-templates:
	cd templates && ~/gopath/bin/qtc

test-index:
	make -C ../src test-gen 2>/dev/null >/dev/null

test-run: test-index all
	./server --web-root=../test/output/ --index-dir=../test/indexes --listen=:10000

test: test-index all
	go test
	cd db && go test

clean:
	rm -f server

format:
	go fmt
	cd db && go fmt
	cd structs && go fmt
	
