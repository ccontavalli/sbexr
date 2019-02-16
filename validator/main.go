package main

import (
	"bytes"
	"flag"
	"fmt"
	"github.com/ccontavalli/goutils/config"
	"github.com/ccontavalli/goutils/misc"
	"github.com/ccontavalli/goutils/scanner"
	"github.com/ccontavalli/sbexr/server/structs"
	"golang.org/x/net/html"
	"io"
	"os"
	"path"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

var root = flag.String("root", "", "Directory with pages to serve.")
var pedantic = flag.Bool("pedantic", false, "Be pedantic in reporting things noticed.")
var pstats = flag.Bool("print-stats", true, "Print statistics at the end of processing.")
var pdump = flag.Bool("print-dump", false, "Dump some of the things we learned about the files.")

var kGenerated = regexp.MustCompile("^sources/[a-f0-9]{2}/[a-f0-9]{14}\\.[a-zA-Z0-9_.-]*$")
var kSourceOrDir = regexp.MustCompile("^sources/[a-f0-9]{2}/[a-f0-9]{14}\\.jhtml$")
var kAllBlanks = regexp.MustCompile("^[[:space:]]*$")

type Warning struct {
	File    string
	Message string
}

type Stats struct {
	Scanned int // a file the parser found.

	SourceOrDir int // a .jhtml generated file.
	Dir         int // a .jhtml generated file with a dir.
	Source      int // a .jhtml generated file with source code.

	Generated  int // any file XX/YYYYYYYYYYYYYY.*
	Meta       int // sources/meta special files.
	Unexpected int // Do not match any pattern.

	HTMLTextBlocks       int
	HTMLTextBytes        int
	HTMLTagOpen          int
	HTMLTagClosed        int
	HTMLHrefAttr         int
	HTMLClassAttr        int
	HTMLIdAttr           int
	HTMLClassesUsed      int
	HTMLTagStackMaxDepth int

	ResourceRequested       int
	ResourceRequestedUsers  int
	ResourceTargetRequested int
	ResourceUnused          int
	ResourceTargetUnused    int

	Classes map[string]int
}

type ResourceType int

const (
	kRoot ResourceType = iota
	kDir
	kFile
	kOther
)

type Resource struct {
	Name   string
	Type   ResourceType
	Target map[string]struct{}
	User   map[string]struct{}
}

func (r *Resource) AddTarget(target string) {
	if r.Target == nil {
		r.Target = make(map[string]struct{})
	}
	r.Target[target] = struct{}{}
}

func (r *Resource) AddUser(user string) {
	if r.User == nil {
		r.User = make(map[string]struct{})
	}
	r.User[user] = struct{}{}
}

type Properties struct {
	AbsoluteRoot string
	RelativeRoot string

	Requested map[string]*Resource
	Provided  map[string]*Resource
}

type Validator struct {
	Root   string
	Output io.Writer

	Stats Stats
	Props Properties

	Warning  []Warning
	Pedantic []Warning
}

var kAnyName = ".."

func (vs *Validator) AddWarning(path, message string, a ...interface{}) {
	formatted := fmt.Sprintf(message, a...)
	fmt.Fprintf(vs.Output, "%s: WARNING - %s\n", path, formatted)

	vs.Warning = append(vs.Warning, Warning{path, formatted})
}

func (vs *Validator) AddPedantic(path, message string, a ...interface{}) {
	formatted := fmt.Sprintf(message, a...)
	if *pedantic {
		fmt.Fprintf(vs.Output, "%s: PEDANTIC - %s\n", path, formatted)
	}

	vs.Pedantic = append(vs.Pedantic, Warning{path, formatted})
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

func (vs *Validator) ValidateJNavData(apath string, jnav structs.JNavData) {
	if kAllBlanks.MatchString(jnav.Name) {
		if jnav.Path != "/" {
			vs.AddWarning(apath, "JNavData.Name is empty, but path is not root? %s instead", jnav.Path)
		} else {
			if vs.Props.AbsoluteRoot != "" {
				vs.AddWarning(apath, "JNavData.Name is empty, indicating an absolute root. But another root was detected? %s", vs.Props.AbsoluteRoot)
			} else {
				vs.Props.AbsoluteRoot = apath
			}
		}
	} else if jnav.Path == "/" {
		if vs.Props.RelativeRoot != "" {
			vs.AddWarning(apath, "JNavData.Path is set to '/' with a non empty Name, indicating a relative root. But another relative root was detected? %s", vs.Props.RelativeRoot)
		} else {
			vs.Props.RelativeRoot = apath
		}
	}

	vs.AssertNotEmpty(apath, jnav.Path, "JNavData Path")
	if vs.AssertNotEmpty(apath, jnav.Root, "JNavData Root") {
		vs.RecordRequested(apath, kRoot, "", jnav.Root)
	}

	for _, parent := range jnav.Parents {
		vs.RecordRequested(apath, kDir, parent.Name, parent.Href)
	}

	// TODO: walk parents to verify correctness of chain, not only that they are valid links!
	//       eg, that they link back to this page, for example.
	// TODO: check that project, tag and tags are consistent everywhere.
}

func (vs *Validator) ValidateSourceContent(apath, name string, content []byte) {
	r := vs.RecordProvided(apath, kFile, name)
	z := html.NewTokenizer(bytes.NewReader(content))

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
			vs.Stats.HTMLTextBlocks += 1
			vs.Stats.HTMLTextBytes += len(text)

		case html.StartTagToken:
			vs.Stats.HTMLTagOpen += 1

			tag_open_c += 1
			name, has_attr := z.TagName()
			for has_attr {
				var key, val []byte
				key, val, has_attr = z.TagAttr()
				switch string(key) {
				case "href":
					vs.RecordRequested(apath, kFile, kAnyName, string(val))
					vs.Stats.HTMLHrefAttr += 1
				case "class":
					vs.Stats.HTMLClassAttr += 1
					for _, class := range strings.Fields(string(val)) {
						vs.Stats.HTMLClassesUsed += 1
						vs.Stats.Classes[class] += 1
					}
				case "id":
					vs.Stats.HTMLIdAttr += 1
					r.AddTarget(string(val))
				default:
					vs.AddWarning(apath, "HTML found unknonw attribute %s - %s in tag %s", key, val, string(name))
				}
			}
			tag_stack = append(tag_stack, string(name))
			if len(tag_stack) > vs.Stats.HTMLTagStackMaxDepth {
				vs.Stats.HTMLTagStackMaxDepth = len(tag_stack)
			}

		case html.EndTagToken:
			vs.Stats.HTMLTagClosed += 1

			tag_closed_c += 1
			name, _ := z.TagName()
			if string(name) != tag_stack[len(tag_stack)-1] {
				vs.AddWarning(apath, "HTML last opened tag was %s, but closed %s", name, tag_stack[len(tag_stack)-1])
			}
			tag_stack = tag_stack[:len(tag_stack)-1]
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
	vs.ValidateSourceContent(apath, jdir.Name, content)
}

func cleanHref(href string) (string, string) {
	hash := strings.Index(href, "#")
	var id string
	if hash >= 0 {
		id = href[hash+1:]
		href = href[0:hash]
	}

	href = strings.TrimSuffix(href, path.Ext(href))
	slash := strings.LastIndex(href, "/")
	if slash < 0 || len(href)-slash < 2 {
		return href, id
	}
	return href[slash-2:], id
}

func (vs *Validator) RecordRequested(requirer string, ty ResourceType, name, orig string) *Resource {
	href, id := cleanHref(orig)
	resource, found := vs.Props.Requested[href]
	if !found {
		resource = &Resource{Name: name, Type: ty}
		vs.Props.Requested[href] = resource
	} else {
		if resource.Name != name && resource.Name != kAnyName && name != kAnyName && !((resource.Type == kRoot || ty == kRoot) && (name == "" || resource.Name == "")) {
			vs.AddWarning(requirer, "requires a resource '%s' named '%s', but it is named '%s' instead", href, name, resource.Name)
		} else if resource.Name == kAnyName {
			resource.Name = name
		}
		if resource.Type != ty && !(resource.Type == kRoot && ty == kDir) && !(ty == kRoot && resource.Type == kDir) {
			vs.AddWarning(requirer, "requires a resource '%s' named %s with type %d, but it is of type %d instead", href, name, ty, resource.Type)
		}
		if ty == kRoot {
			resource.Type = ty
		}
	}
	if id != "" {
		resource.AddTarget(id)
	}
	resource.AddUser(requirer)
	return resource
}
func (vs *Validator) RecordProvided(href string, ty ResourceType, name string) *Resource {
	href, id := cleanHref(href)
	resource, found := vs.Props.Provided[href]
	if !found {
		resource = &Resource{Name: name, Type: ty}
		vs.Props.Provided[href] = resource
	} else {
		if resource.Name != name && resource.Name != kAnyName && name != kAnyName && !((resource.Type == kRoot || ty == kRoot) && (name == "" || resource.Name == "")) {
			vs.AddWarning(href, "provided with name '%s' before, instead of name name '%s'", name, resource.Name)
		} else if resource.Name == kAnyName {
			resource.Name = name
		}
		if resource.Type != ty && !(resource.Type == kRoot && ty == kDir) && !(ty == kRoot && resource.Type == kDir) {
			vs.AddWarning(href, "%s provided with type %d before, instead of type %d", name, ty, resource.Type)
		}
		if resource.Type != ty {
			vs.AddWarning(href, "%s provided with type %d before, instead of type %d", name, ty, resource.Type)
		}
	}
	if id != "" {
		resource.AddTarget(id)
	}
	return resource
}

func (vs *Validator) ValidateDir(apath string, jdir structs.JDir) {
	vs.RecordProvided(apath, kDir, jdir.Name)

	for _, file := range jdir.Files {
		vs.AssertNotEmpty(apath, file.Type, "jFile.Type for an element of JDir.Files")
		vs.RecordRequested(apath, kFile, file.Name, file.Href)
		// TODO: verify that the type is one of the known ones.
		// TODO: verify that mtime is in a valid format.
	}
	for _, dir := range jdir.Dirs {
		vs.AssertEmpty(apath, dir.Type, "jFile.Type for an element of JDir.Dirs")
		vs.RecordRequested(apath, kDir, dir.Name, dir.Href)
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
	vs.RecordProvided(apath, kFile, kAnyName)
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
		vs.Stats.Meta += 1
	case rpath == "sources/meta/index.jhtml":
		vs.Stats.Meta += 1
	default:
		vs.AddWarning(apath, "unexpected file")
		vs.Stats.Unexpected += 1
	}

	return nil
}

func (vs *Validator) PerformFullTreeTests() {
	if vs.Props.RelativeRoot == "" {
		vs.AddWarning("", "No relative root found in tree?")
	}
	if vs.Props.AbsoluteRoot == "" {
		vs.AddWarning("", "No absolute root found in tree?")
	}

	provided := vs.Props.Provided
	for rk, rr := range vs.Props.Requested {
		vs.Stats.ResourceRequested += 1
		vs.Stats.ResourceRequestedUsers += len(rr.User)

		pr, ok := provided[rk]
		if !ok {
			keys, _ := misc.StringKeys(rr.User)
			vs.AddWarning("", "Resource %s aka %s is needed by %v, but could not be found", rk, rr.Name, keys)
			continue
		}

		i := 0
		for rt, _ := range rr.Target {
			vs.Stats.ResourceTargetRequested += 1

			_, ok := pr.Target[rt]
			if !ok {
				vs.AddWarning("", "Resource %s aka %s requires target %s, which is not provided (%d of %d request, %d provided)", rk, rr.Name, rt, i, len(rr.Target), len(pr.Target))
			} else {
				delete(pr.Target, rt)
			}
			i += 1
		}

		if len(pr.Target) <= 0 {
			delete(provided, rk)
		}
	}
	for pk, pr := range vs.Props.Provided {
		_, ok := vs.Props.Requested[pk]
		if len(pr.Target) <= 0 || !ok {
			vs.Stats.ResourceUnused += 1
			vs.AddPedantic("", "Resource %s aka %s is provided, but not used by anyone", pk, pr.Name)
		} else {
			for pt, _ := range pr.Target {
				vs.Stats.ResourceTargetUnused += 1
				vs.AddPedantic("", "Target %s in resource %s aka %s is provided, but not used by anyone", pt, pk, pr.Name)
			}
		}
	}
}

func (vs *Validator) Scan() {
	scanner.ScanTree(vs.Root, nil, nil, vs.ValidateFile, nil)
	vs.PerformFullTreeTests()
}

func (vs *Validator) String() string {
	retval := fmt.Sprintf("%#v - %d %d", vs.Stats, len(vs.Props.Requested), len(vs.Props.Provided))
	for key, resource := range vs.Props.Requested {
		retval += fmt.Sprintf("R:{%s,%s,%d,#%d,u%d}", key, resource.Name, resource.Type, len(resource.Target), len(resource.User))
	}
	for key, resource := range vs.Props.Provided {
		retval += fmt.Sprintf("R:{%s,%s,%d,#%d,u%d}", key, resource.Name, resource.Type, len(resource.Target), len(resource.User))
	}
	return retval
}

func (vs *Validator) PrintStats(w io.Writer) {
	fmt.Fprintf(vs.Output, "Warnings: %d, Pedantic: %d\n", len(vs.Warning), len(vs.Pedantic))
	fmt.Fprintf(vs.Output, "\n")
	fmt.Fprintf(vs.Output, "File system statistcs:\n")
	fmt.Fprintf(vs.Output, "   root: '%s' absolute, '%s' relative\n",
		vs.Props.AbsoluteRoot, vs.Props.RelativeRoot)
	fmt.Fprintf(vs.Output, "   paths: %d scanned, of which %d directories, %d source, %d copied, %d meta, %d unexpected\n",
		vs.Stats.Scanned, vs.Stats.Dir, vs.Stats.Source, vs.Stats.Generated, vs.Stats.Meta, vs.Stats.Unexpected)
	fmt.Fprintf(vs.Output, "\n")
	fmt.Fprintf(vs.Output, "Resource statistcs:\n")
	fmt.Fprintf(vs.Output, "   requested: %d file/dir resources, %d targets, %d users\n", vs.Stats.ResourceRequested, vs.Stats.ResourceTargetRequested, vs.Stats.ResourceRequestedUsers)
	fmt.Fprintf(vs.Output, "   unused: %d file/dir resources, %d targets\n", vs.Stats.ResourceUnused, vs.Stats.ResourceTargetUnused)
	fmt.Fprintf(vs.Output, "   total: %d file/dir resources, %d targets\n", vs.Stats.ResourceUnused+vs.Stats.ResourceRequested, vs.Stats.ResourceTargetRequested+vs.Stats.ResourceTargetUnused)
	fmt.Fprintf(vs.Output, "\n")
	fmt.Fprintf(vs.Output, "JHTML Statistics:\n")
	fmt.Fprintf(vs.Output, "   Text: %d blocks for %d bytes\n", vs.Stats.HTMLTextBlocks, vs.Stats.HTMLTextBytes)
	fmt.Fprintf(vs.Output, "   Tags: %d opened, %d closed, in a %d elements max stack\n", vs.Stats.HTMLTagOpen, vs.Stats.HTMLTagClosed, vs.Stats.HTMLTagStackMaxDepth)
	fmt.Fprintf(vs.Output, "   Attributes: %d href, %d id, %d class (%d classes specified)\n", vs.Stats.HTMLHrefAttr, vs.Stats.HTMLIdAttr, vs.Stats.HTMLClassAttr, vs.Stats.HTMLClassesUsed)
	fmt.Fprintf(vs.Output, "\n")
	fmt.Fprintf(vs.Output, "CLASS Attributes seen (%d different ones):\n", len(vs.Stats.Classes))

	classes, _ := misc.StringKeys(vs.Stats.Classes)
	sort.Strings(classes)
	for i := 0; i < len(classes); {
		for j := i + 5; i < j && i < len(classes); i++ {
			fmt.Fprintf(vs.Output, "%8d:%-20s", vs.Stats.Classes[classes[i]], classes[i])
		}
		fmt.Fprintf(vs.Output, "\n")
	}
}

func NewStats() Stats {
	return Stats{Classes: make(map[string]int)}
}

func NewProperties() Properties {
	return Properties{Requested: make(map[string]*Resource), Provided: make(map[string]*Resource)}
}

func NewValidator(root string) *Validator {
	return &Validator{
		Root:   root,
		Output: os.Stdout,
		Stats:  NewStats(),
		Props:  NewProperties(),
	}
}

func main() {
	flag.Parse()
	v := NewValidator(*root)

	fmt.Printf("==== WARNINGS FOR %s\n", *root)
	v.Scan()
	if *pstats {
		fmt.Printf("==== STATISTICS FOR %s\n", *root)
		v.PrintStats(os.Stdout)
	}
}
