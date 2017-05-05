package db

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path"
	"strings"
	"sync"
	"sync/atomic"
	"time"
        "github.com/NYTimes/gziphandler"
)

// A Tag contains the configurations that are common across all tags
// of the source tree.
type Tag struct {
	Registered bool
	Mutex      sync.RWMutex
	Changed    time.Time

	Handler ApiHandler
}

type Stats struct {
	scanned  uint64
	matched  uint64
	returned uint64

	optimized       string
	buildresulttime time.Duration
}

type ApiHandler interface {
	HandleSearch(w http.ResponseWriter, query *JsonRequest) (interface{}, *Stats)
	Delete()
}

type PageHandler interface {
	HandlePage(w http.ResponseWriter, r *http.Request, data *TagData, m *sync.RWMutex)
}

type TagSet struct {
	Name    string
	Tag     map[string]*Tag
	Handler TagSetHandler
}

var flag_project = flag.String("project", "", "If specified, name of the project. Passed down to templates.")

type TagData struct {
	Tag     string
	Url     string
	Project string
}

func GetSearchUrl(tag, tagset string) string {
	return "/api/" + tag + "/" + tagset
}
func GetPageUrl(tag, tagset string) string {
	return "/" + tag + "/sources/" + tagset + "/"
}

var queryid uint64

func (v *Tag) HandleSearch(resp http.ResponseWriter, req *http.Request) {
	querystart := time.Now()

	decoder := json.NewDecoder(req.Body)
	var query JsonRequest

	err := decoder.Decode(&query)
	if err != nil {
		log.Printf("WARN: invalid request - %s\n", err)
		return
	}

	atomic.AddUint64(&queryid, 1)

	log.Printf("< %08x QUERY %+v FROM %s AKA %s URL %s\n", queryid, query, req.RemoteAddr, req.Header.Get("X-Forwarded-For"), req.RequestURI)
	if query.P {
		log.Printf("PING PROCESSED\n")
		return
	}

	v.Mutex.RLock()
	searchstart := time.Now()
	result, stats := v.Handler.HandleSearch(resp, &query)
	searchtime := time.Since(searchstart)
	v.Mutex.RUnlock()

	encoder := json.NewEncoder(resp)
	err = encoder.Encode(result)
	querytime := time.Since(querystart)

	// log.Printf("> SYMBOL QUERY %s START %d SCANNED %d records, %d/%d bytes, RESULTS %d TOOK '%s'\n", query.Q, query.S, count+1, offset, len(data.symbols), found, time.Since(start))
	log.Printf("> %08x QUERY %+v OPT {%s} URL %s RETURNED %d MATCHED %d SCANNED %d TIME BUILD %s SEARCH %s QUERY %s\n", queryid, query, stats.optimized, req.RequestURI, stats.returned, stats.matched, stats.scanned, stats.buildresulttime, searchtime, querytime)

	if err != nil {
		log.Printf("WARN: could not send response %s\n", err)
		return
	}
}

func (tagset *TagSet) AddHandler(k string, v *Tag) {
	if v.Handler != nil && !v.Registered {
		url := GetSearchUrl(k, tagset.Name)
		http.Handle(url, gziphandler.GzipHandler(http.HandlerFunc(v.HandleSearch)))
		log.Printf("WAITING FOR QUERIES ON %s\n", url)

		if phandler, ok := v.Handler.(PageHandler); ok {
			url := GetPageUrl(k, tagset.Name)
			data := &TagData{k, url, *flag_project}
			http.Handle(url, gziphandler.GzipHandler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				phandler.HandlePage(w, r, data, &v.Mutex)
			})))
			log.Printf("WAITING FOR QUERIES ON %s\n", url)
		}
		v.Registered = true
	}
}

func (tagset *TagSet) AddHandlers() {
	for k, v := range tagset.Tag {
		tagset.AddHandler(k, v)
	}
}

func (tagset *TagSet) Update() {
	if tagset.Handler == nil {
		return
	}

	tagset.Handler.Update(tagset.Tag)
}

type TagSetHandler interface {
	Update(Tag map[string]*Tag)
}

func NewTagSet(name string, handler TagSetHandler) TagSet {
	return TagSet{name, make(map[string]*Tag), handler}
}

type TagSetLoader func(filename string) (ApiHandler, error)

type SpreadTagSet struct {
	filename string
	paths    []string
	loader   TagSetLoader
}

func NewSpreadTagSetHandler(filename string, paths []string, loader TagSetLoader) *SpreadTagSet {
	ts := SpreadTagSet{}
	ts.filename = filename
	ts.paths = paths
	ts.loader = loader

	return &ts
}

func (this *SpreadTagSet) Update(tag map[string]*Tag) {
	for _, start := range this.paths {
		content, err := ioutil.ReadDir(start)
		if err != nil {
			log.Printf("SKIPPING %s: %s\n", start, err)
			continue
		}
		for _, directory := range content {
			if !directory.IsDir() {
				continue
			}

			fullname := path.Join(start, directory.Name(), this.filename)
			info, err := os.Stat(fullname)
			if err != nil {
				continue
			}

			t := tag[directory.Name()]
			if t == nil || info.ModTime().After(t.Changed) {
				handler, err := this.loader(fullname)
				if err != nil {
					log.Printf("SKIPPING %s: %s\n", fullname, err)
					continue
				}

				if t == nil {
					t = &Tag{}
					t.Handler = handler

					tag[directory.Name()] = t
				} else {
					t.Mutex.Lock()
					t.Handler.Delete()
					t.Handler = handler
					t.Mutex.Unlock()
				}
				t.Changed = info.ModTime()
			}
		}
	}
}

type SingleDirTagSet struct {
	paths  []string
	loader TagSetLoader
}

func NewSingleDirTagSetHandler(paths []string, loader TagSetLoader) *SingleDirTagSet {
	ts := SingleDirTagSet{}
	ts.paths = paths
	ts.loader = loader

	return &ts
}

func (this *SingleDirTagSet) Update(tag map[string]*Tag) {
	for _, start := range this.paths {
		content, err := ioutil.ReadDir(start)
		if err != nil {
			log.Printf("SKIPPING %s: %s\n", start, err)
			continue
		}
		for _, file := range content {
			if file.IsDir() {
				continue
			}

			filename := file.Name()
			if !strings.HasPrefix(filename, "index.") {
				continue
			}
			if !strings.HasSuffix(filename, ".json") {
				continue
			}

			tagname := filename
			tagname = strings.TrimPrefix(tagname, "index.")
			tagname = strings.TrimSuffix(tagname, ".json")

			fullname := path.Join(start, filename)
			info, err := os.Stat(fullname)
			if err != nil {
				continue
			}

			t := tag[tagname]
			if t == nil || info.ModTime().After(t.Changed) {
				handler, err := this.loader(fullname)
				if err != nil {
					log.Printf("SKIPPING %s[%s]: %s\n", fullname, tagname, err)
					continue
				}

				if t == nil {
					t = &Tag{}
					t.Handler = handler

					tag[tagname] = t
				} else {
					t.Mutex.Lock()
					t.Handler = handler
					t.Mutex.Unlock()
				}
				t.Changed = info.ModTime()
			}
		}
	}
}
