package main

import (
	"flag"
	"golang.org/x/net/html"
	"github.com/ccontavalli/goutils/scanner"
	"github.com/ccontavalli/goutils/config"
	"github.com/ccontavalli/sbexr/server/structs"
	"os"
	"log"
	"path/filepath"
	"regexp"
	"fmt"
	"bytes"
	"io"
)

var root = flag.String("root", "", "Directory with pages to serve.")

var kGenerated = regexp.MustCompile("^sources/[a-f0-9]{2}/[a-f0-9]{14}\\.[a-zA-Z0-9_.-]*$")
var kSourceOrDir = regexp.MustCompile("^sources/[a-f0-9]{2}/[a-f0-9]{14}\\.jhtml$")
var kAllBlanks = regexp.MustCompile("^[[:space:]]*$")

type Warning struct {
	File string
	Message string
}

type Stats struct {
	Scanned int

	SourceOrDir int
	Dir int
	Source int

	Generated int
}

type Properties struct {
	AbsoluteRoot string

	Validated map[string]bool
	Pending map[string]string
}

type Validator struct {
	Root string

	Stats Stats
	Props Properties

	Warning []Warning
}

func (vs *Validator) AddWarning(path, message string, a ...interface{}) {
	formatted := fmt.Sprintf(message, a...)
	log.Printf("%s: WARNING - %s\n", path, formatted)

	vs.Warning = append(vs.Warning, Warning{path, formatted})
}

func (vs *Validator) AssertNotEmpty(apath, value, description string) bool {
	if !kAllBlanks.MatchString(value) {
		return true
	}
	vs.AddWarning(apath, "expected NOT to be empty %s", description)
	return false
}

func (vs *Validator) AssertNoFiles(apath string, v []structs.JFile, description string) bool {
	if len(v) == 0 {
		return true
	}
	vs.AddWarning(apath, "expected to have no elements %s", description)
	return false
}

func (vs *Validator) AssertEmpty(apath, value, description string) bool {
	if kAllBlanks.MatchString(value) {
		return true
	}
	vs.AddWarning(apath, "expected to be empty %s", description)
	return false
}

type LinkType int
const (
	kDir LinkType = iota
	kFile
)

func (vs *Validator) AssertHrefValid(apath string, ty LinkType, href string) bool {
	// TODO -> verify correctness of reference.
	return true
}

func (vs *Validator) AssertLinkValid(apath string, ty LinkType, jlink structs.JLink) bool {
	// TODO -> how do we verify name?
	return vs.AssertHrefValid(apath, ty, jlink.Href)
}

func (vs *Validator) AssertLinksValid(apath string, ty LinkType, jlinks []structs.JLink) bool {
	retval := true
	for _, jlink := range jlinks {
		valid := vs.AssertLinkValid(apath, ty, jlink)
		retval = retval && valid
	}
	return retval
}

func (vs *Validator) ValidateJNavData(apath string, jnav structs.JNavData) {
	if kAllBlanks.MatchString(jnav.Name) {
		if vs.Props.AbsoluteRoot != "" {
			vs.AddWarning(apath, "multiple roots detected (only one root can have empty JNavData.Name - other root: %s", apath, vs.Props.AbsoluteRoot)
		} else {
			vs.Props.AbsoluteRoot = apath
		}
	}

	vs.AssertNotEmpty(apath, jnav.Path, "JNavData Path")
	if vs.AssertNotEmpty(apath, jnav.Root, "JNavData Root") {
		vs.AssertHrefValid(apath, kDir, jnav.Root)
	}

	vs.AssertLinksValid(apath, kDir, jnav.Parents)
	// TODO: walk parents to verify correctness of chain, not only that they are valid links!
	// TODO: check that project, tag and tags are consistent everywhere.
}

func (vs *Validator) ValidateSourceContent(apath string, content []byte) {
	z := html.NewTokenizer(bytes.NewReader(content))

	text_c := 0
	tag_open_c := 0
	tag_closed_c := 0
	tag_stack := []string{}

	for {
		tt := z.Next()
		switch tt {
		case html.ErrorToken:
			err := z.Err()
			if err == io.EOF {
				return
			}
			vs.AddWarning(apath, "HTML parser returned error %#v", err)

		case html.TextToken:
			text := z.Text()
			text_c += len(text)

		case html.StartTagToken:
			tag_open_c += 1
			name, has_attr := z.TagName()
			for has_attr {
				var key, val []byte
				key, val, has_attr = z.TagAttr()
				switch string(key) {
				case "href":
					log.Printf("href %s\n", string(val))
				case "class":
					log.Printf("class %s\n", string(val))
				case "id":
					log.Printf("id %s\n", string(val))
				default:
					log.Printf("UNKNOWN ATTRIBUTE: %s - %s\n", key, val)
				}
			}
			tag_stack = append(tag_stack, string(name))

		case html.EndTagToken:
			tag_closed_c += 1
			name, _ := z.TagName()
			if string(name) != tag_stack[len(tag_stack) - 1] {
				vs.AddWarning(apath, "HTML last opened tag was %s, but closed %s", name, tag_stack[len(tag_stack) - 1])
			}
			tag_stack = tag_stack[:len(tag_stack) - 1]

		default:
			log.Printf("SUSPICIOUS TOKEN %#v", tt)
		}
	}

	if tag_open_c != tag_closed_c || len(tag_stack) != 0 {
		vs.AddWarning(apath, "HTML open/closed tags do not match or tag stack not empty %d/%d/%v", tag_open_c, tag_closed_c, tag_stack)
	}
}

func (vs *Validator) ValidateSource(apath string, jdir structs.JDir, content []byte) {
	// If we are here, jdir.Files and jdir.Dirs are empty. Validate them anyway
	// in case we change the caller, or this is used as a library.
	vs.AssertNoFiles(apath, jdir.Files, "JDir.Files element for a file")
	vs.AssertNoFiles(apath, jdir.Dirs, "JDir.Dirs element for a file")
	vs.ValidateSourceContent(apath, content)
}

func (vs *Validator) ValidateDir(apath string, jdir structs.JDir) {
	for _, file := range jdir.Files {
		vs.AssertLinkValid(apath, kFile, file.JLink)
		vs.AssertNotEmpty(apath, file.Type, "jFile.Type for an element of JDir.Files")
		// TODO: verify that the type is one of the known ones.
		// TODO: verify that mtime is in a valid format.
	}
	for _, dir := range jdir.Dirs {
		vs.AssertLinkValid(apath, kDir, dir.JLink)
		vs.AssertEmpty(apath, dir.Type, "jFile.Type for an element of JDir.Dirs")
		// TODO: verify that size matches the number of elements of child?
		// TODO: verify that mtime is in a valid format.
	}
}

func (vs *Validator) ValidateSourceOrDir(apath string) {
	var jdir structs.JDir
	content, err := config.ParseJhtmlFile(apath, &jdir)
	if err != nil {
		vs.AddWarning(apath, "could not parse jhtml - %s", err)
		return
	}

	vs.ValidateJNavData(apath, jdir.JNavData)
	if len(jdir.Files) > 0 || len(jdir.Dirs) > 0 {
		if len(content) > 0 {
			vs.AddWarning(apath, "is a directory, but content not empty as it ought to be - %s (%s)", string(content), content)
		}
		vs.Stats.Dir += 1
		vs.ValidateDir(apath, jdir)
	} else {
		vs.Stats.Source += 1
		vs.ValidateSource(apath, jdir, content)
	}
}

func (vs *Validator) ValidateGenerated(apath string) {
}

func (vs *Validator) ValidateFile(state interface{}, path string, file os.FileInfo) error {
	root := vs.Root
	apath := path
	rpath, _ := filepath.Rel(root, apath)

	vs.Stats.Scanned += 1
	switch {
	case kSourceOrDir.MatchString(rpath):
		vs.Stats.SourceOrDir += 1
		vs.ValidateSourceOrDir(apath)

	case kGenerated.MatchString(rpath):
		vs.Stats.Generated += 1
		vs.ValidateGenerated(apath)

	case rpath == "sources/meta/globals.json":
	case rpath == "sources/meta/index.jhtml":

	default:
		vs.AddWarning(apath, "unexpected file")
	}

	return nil
}

func (vs *Validator) Scan() {
	scanner.ScanTree(vs.Root, nil, nil, vs.ValidateFile, nil)
}

func (vs *Validator) String() string {
	return fmt.Sprintf("%#v", vs.Stats)
}

func main() {
	flag.Parse()
	v := Validator{Root: *root}
	v.Scan()

	log.Printf("%s\n", &v)
}
