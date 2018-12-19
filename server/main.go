package main

import (
	"flag"
	"log"
	"net/http"
	"net/http/fcgi"
        "github.com/ccontavalli/sbexr/server/db"
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

func NewIndex() Index {
	index := Index{}

	index.Tree = db.NewTagSet("tree", db.NewSpreadTagSetHandler("tree.json", flag.Args(), db.LoadJsonTree))

	if len(*indexdir) > 0 {
		index.BinSymbol = db.NewTagSet("symbol", db.NewSingleDirTagSetHandler([]string{*indexdir}, db.LoadSymbols))
	} else {
		panic("--index-dir must be specified")
	}

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

		//log.Printf("AFTER UPDATE %+v\n", index)
		//for key, value := range index.Symbol.Tag {
		//      log.Printf("  SYMBOLS %+v, %+v\n", key, value)
		//}

		// FIXME: set more aggressive goals.
		runtime.GC()
		time.Sleep(10 * time.Second)
	}
}

func main() {
	flag.Parse()

	index := NewIndex()
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
