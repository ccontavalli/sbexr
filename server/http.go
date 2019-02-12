package main

import (
	"bytes"
	"encoding/json"
	"github.com/ccontavalli/goutils/misc"
	"github.com/ccontavalli/sbexr/server/structs"
	"github.com/ccontavalli/sbexr/server/templates"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"strings"
	"sync"
)

type SourceServer struct {
	root     string
	tags     map[string]structs.JNavData
	tagslock sync.RWMutex
}

var JSeparator = []byte{'\n', '-', '-', '-', '\n'}

func SplitJhtml(content []byte) ([]byte, []byte) {
	separator := bytes.Index(content, JSeparator)
	if separator < 0 {
		return content, content[0:0]
	}
	return content[0:separator], content[separator+len(JSeparator):]
}

func ReadJhtmlFile(fpath string, jheader interface{}) ([]byte, error) {
	data, err := ioutil.ReadFile(fpath)
	if err != nil {
		return nil, err
	}

	header, content := SplitJhtml(data)

	err = json.Unmarshal(header, jheader)
	return content, err
}

var kSourceRoot = "/sources/"

func getIndex(root, upath string, dir structs.JDir) []byte {
	var index structs.JFile
	for _, file := range dir.Files {
		if misc.ArrayStringIndex(*indexfiles, file.Name) >= 0 && (file.Type == "text" || file.Type != "parsed") && file.Href != "" {
			index = file
			break
		}
	}
	if index.Name == "" || index.Href == "" {
		return []byte{}
	}

	indexpath := path.Join(root, upath, index.Href)
	var jdir structs.JDir
	content, err := ReadJhtmlFile(indexpath+".jhtml", &jdir)
	if err != nil {
		return []byte{}
	}
	return content
}

func (ss *SourceServer) getTagData(upath string, sources int) structs.JNavData {
	tag := path.Base(upath[:sources])
	ss.tagslock.RLock()
	tdata := ss.tags[tag]
	ss.tagslock.RUnlock()
	return tdata
}

func (ss *SourceServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	upath := path.Clean(r.URL.Path)

	sources := strings.Index(upath, kSourceRoot)
	log.Printf("sources: %s - %d", upath, sources)
	if sources < 0 {
		http.ServeFile(w, r, filepath.Join(ss.root, filepath.Clean(upath)))
		return
	}

	subpath := upath[sources+len(kSourceRoot):]
	log.Printf("subpath: %s - %d", subpath, sources)
	if subpath == "meta/globals.js" {
		templates.WriteGlobalsJs(w, ss.getTagData(upath, sources))
		return
	}

	extension := path.Ext(subpath)
	subpath = strings.TrimSuffix(subpath, extension)
	if subpath == "meta/about" {
		aboutpage := &templates.AboutPage{templates.BasePage(ss.getTagData(upath, sources))}
		templates.WritePageTemplate(w, aboutpage)
		return
	}
	if subpath == "meta/help" {
		helppage := &templates.HelpPage{templates.BasePage(ss.getTagData(upath, sources))}
		templates.WritePageTemplate(w, helppage)
		return
	}

	cpath := filepath.Join(ss.root, strings.TrimSuffix(upath, extension))
	log.Printf("cpath: %s - %s (%s)\n", cpath, extension, upath)
	// Pseudo code:
	// 1) try to read jhtml file, and corresponding json.
	var jdir structs.JDir
	content, err := ReadJhtmlFile(cpath+".jhtml", &jdir)
	if err != nil {
		log.Printf("jhtml failed for %s - %s %#v\n", upath, cpath, err)
		_, ispatherror := err.(*os.PathError)
		if !ispatherror {
			http.Error(w, "CORRUPTED FILE", http.StatusInternalServerError)
			return
		}

		if extension == "" {
			extension = ".html"
		}
		http.ServeFile(w, r, cpath+extension)
		return
	}

	// 2) if directory -> render directory template.
	// 3) if file -> render file template.
	if len(jdir.Dirs) > 0 || len(jdir.Files) > 0 {
		dirpage := templates.DirectoryPage{jdir, getIndex(ss.root, upath, jdir)}
		templates.WritePageTemplate(w, &dirpage)
		return
	}

	filepage := &templates.SourcePage{jdir, content}
	templates.WritePageTemplate(w, filepage)
}

func (ss *SourceServer) Update() {
	list, err := ioutil.ReadDir(ss.root)
	if err != nil {
		// FIXME: logging!
		return
	}

	newtags := make(map[string]structs.JNavData)
	for _, dir := range list {
		if !dir.IsDir() {
			continue
		}
		globals := filepath.Join(ss.root, dir.Name(), "sources", "meta", "globals.json")
		data, err := ioutil.ReadFile(globals)
		if err != nil {
			log.Printf("DIR: %s - skipped, no readable globals.json - %s (%s)\n", dir.Name(), err, globals)
			continue
		}

		var gdata structs.JNavData
		err = json.Unmarshal(data, &gdata)
		if err != nil {
			log.Printf("DIR: %s - does not have a valid globals.json - %s (%s)\n", dir.Name(), err, globals)
			// FIXME: logging!
			continue
		}
		newtags[dir.Name()] = gdata
	}

	ss.tagslock.Lock()
	ss.tags = newtags
	ss.tagslock.Unlock()
}

func NewSourceServer(path string) *SourceServer {
	ss := SourceServer{root: path}
	ss.Update()

	return &ss
}
