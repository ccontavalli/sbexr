
all: sitemap

sitemap: main.go
	go build

test: sitemap
	./sitemap --json-file ../test/indexes/index.output.json --source-url "http://sbexr.rabexc.org/latest/sources" --tree-dir ../test/output/sources/ --sitemap-url "http://sbexr.rabexc.org" --output ./test

.PHONY: clean

clean:
	rm -f sitemap*
	rm -f ./test/sitemap*
