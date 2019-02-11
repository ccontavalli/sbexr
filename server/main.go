package main

import (
	"flag"
	"github.com/ccontavalli/sbexr/server/db"
	"log"
	"net/http"
	"net/http/fcgi"
	"runtime"
	"time"
)

import _ "net/http/pprof"

const kMaxResults = 30

var listen = flag.String("listen", "", "Address to listen on - if unspecified, use fcgi. Example: 0.0.0.0:9000")
var root = flag.String("root", "", "If specified, directory with static content to serve")
var indexdir = flag.String("index-dir", "", "If specified, directory with indexes to load")

type Index struct {
	Tree      db.TagSet
	BinSymbol db.TagSet
}

func NewIndex(indexroot string) Index {
	index := Index{}

	index.Tree = db.NewTagSet("tree", db.NewSingleDirTagSetHandler(indexroot, ".files.json", db.LoadJsonTree))
	index.BinSymbol = db.NewTagSet("symbol", db.NewSingleDirTagSetHandler(indexroot, ".symbols.json", db.LoadSymbols))

	return index
}

func init() {
	runtime.GOMAXPROCS(runtime.NumCPU() - 1)
}

func Updater(index *Index) {
	for {
		index.Tree.Update()
		index.BinSymbol.Update()

		index.Tree.AddHandlers()
		index.BinSymbol.AddHandlers()

		runtime.GC()
		time.Sleep(10 * time.Second)
	}
}

func main() {
	flag.Parse()
	if len(*indexdir) <= 0 {
		log.Fatal("Must supply flag --index-dir - to match the one used with sbexr")
	}

	index := NewIndex(*indexdir)
	go Updater(&index)

	if *root != "" {
		http.Handle("/", http.FileServer(http.Dir(*root)))
	}

	var err error
	if *listen != "" {
		err = http.ListenAndServe(*listen, nil) // set listen port
	} else {
		err = fcgi.Serve(nil, nil)
	}
	if err != nil {
		log.Fatal("ListenAndServe: ", err)
	}
}
