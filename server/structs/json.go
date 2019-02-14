package structs

type JLink struct {
	Name string `json:"name"`
	Href string `json:"href"`
}

type JNavData struct {
	Name    string   `json:"name"`
	Path    string   `json:"path"`
	Root    string   `json:"root"`

	Project string   `json:"project"`
	Tag     string   `json:"tag"`
	Tags    []string `json:"tags"`

	Parents []JLink  `json:"parents"`
}

type JFile struct {
	JLink
	Type  string `json:"type"`
	Mtime string `json:"mtime"`
	Size  int    `json:"size"`
}

type JDir struct {
	JNavData
	Files []JFile `json:"files"`
	Dirs  []JFile `json:"dirs"`
}
