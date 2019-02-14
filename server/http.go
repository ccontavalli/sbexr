package main

import (
	"encoding/json"
	"github.com/ccontavalli/goutils/misc"
	"github.com/ccontavalli/goutils/config"
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
	tagsmap  map[string]structs.JNavData
	tagslist []string
	tagslock sync.RWMutex
}

var kSourceRoot = "/sources/"

func findFile(files []structs.JFile, name string) int {
	for i, file := range files {
		if file.Name == name {
			return i
		}
	}
	return -1
}

func getIndex(root, upath string, dir structs.JDir) []byte {
	var index structs.JFile
	for _, file := range *indexfiles {
		found := findFile(dir.Files, file)
		if found < 0 {
			continue
		}

		entry := dir.Files[found]
		if (entry.Type == "text" || entry.Type == "parsed") && entry.Href != "" {
		  index = entry
		  break
		}
	}
	if index.Name == "" || index.Href == "" {
		return []byte{}
	}

	indexpath := path.Join(root, path.Dir(upath), index.Href)
	indexpath = strings.TrimSuffix(indexpath, path.Ext(indexpath))

	var jdir structs.JDir
	content, err := config.ParseJhtmlFile(indexpath+".jhtml", &jdir)
	if err != nil {
		return []byte{}
	}
	return content
}

func (ss *SourceServer) getTagData(upath string, sources int) structs.JNavData {
	tag := path.Base(upath[:sources])
	ss.tagslock.RLock()
	tdata := ss.tagsmap[tag]
	tlist := ss.tagslist
	ss.tagslock.RUnlock()

	tdata.Tag = tag
	tdata.Tags = tlist
	return tdata
}

func (ss *SourceServer) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	upath := misc.CleanPreserveSlash(r.URL.Path)

	sources := strings.Index(upath, kSourceRoot)
	if sources < 0 {
		http.ServeFile(w, r, filepath.Join(ss.root, filepath.Clean(upath)))
		return
	}

	tagdata := ss.getTagData(upath, sources)
	subpath := upath[sources+len(kSourceRoot):]
	if subpath == "meta/globals.js" {
		templates.WriteGlobalsJs(w, tagdata)
		return
	}

	extension := path.Ext(subpath)
	subpath = strings.TrimSuffix(subpath, extension)
	if subpath == "meta/about" {
		aboutpage := &templates.AboutPage{templates.BasePage(tagdata)}
		templates.WritePageTemplate(w, aboutpage)
		return
	}
	if subpath == "meta/help" {
		helppage := &templates.HelpPage{templates.BasePage(tagdata)}
		templates.WritePageTemplate(w, helppage)
		return
	}
	if subpath == "meta" {
		http.Redirect(w, r, upath+"/", http.StatusMovedPermanently)
		return
	}
	if subpath == "meta/" {
		upath = upath + "index"
	}

	cpath := filepath.Join(ss.root, strings.TrimSuffix(upath, extension))
	// Pseudo code:
	// 1) try to read jhtml file, and corresponding json.
	jdir := structs.JDir{JNavData: tagdata}
	content, err := config.ParseJhtmlFile(cpath+".jhtml", &jdir)
	if err != nil {
		_, ispatherror := err.(*os.PathError)
		if !ispatherror {
			log.Printf("CORRUPTED FILE - %s, %s, %#v", cpath, content, err)
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
		log.Printf("DIR: %s - could not read root \n", ss.root)
		return
	}

	newtagsmap := make(map[string]structs.JNavData)
	for _, dir := range list {
		if !dir.IsDir() {
			continue
		}
		globals := filepath.Join(ss.root, dir.Name(), "sources", "meta", "globals.json")
		data, err := ioutil.ReadFile(globals)
		if err != nil {
			continue
		}

		var gdata structs.JNavData
		err = json.Unmarshal(data, &gdata)
		if err != nil {
			log.Printf("DIR: %s - does not have a valid globals.json - %s (%s)\n", dir.Name(), err, globals)
			continue
		}
		newtagsmap[dir.Name()] = gdata
	}

	newtagslist := misc.StringKeysOrPanic(newtagsmap)

	ss.tagslock.Lock()
	ss.tagsmap = newtagsmap
	ss.tagslist = newtagslist
	ss.tagslock.Unlock()
}

func NewSourceServer(path string) *SourceServer {
	ss := SourceServer{root: path}
	ss.Update()
	return &ss
}
