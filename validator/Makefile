
all:
	go build

test: all
	go test

run: all
	./validator --root=../test/output

clean:
	rm -f validator

format:
	go fmt
