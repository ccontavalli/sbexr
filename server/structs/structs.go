package structs


type SymbolData struct {
	Data []*SymbolObject `json:"data"`
        Error string `json:"error,omitempty"`
}
type SymbolProvider struct {
	Href     string `json:"href"`
	Location string `json:"location"`
	Snippet  string `json:"snippet"`
}

type SymbolKindLinkage struct {
	Linkage int              `json:"linkage"`
	Access  int              `json:"access"`
	Kind    string           `json:"kind"`
        Details string           `json:"details"`
	Defs    []SymbolProvider `json:"defs"`
	Decls   []SymbolProvider `json:"decls"`
}
type SymbolObject struct {
	Name  string              `json:"name"`
        Hash  string              `json:"hash"`
	Kinds []SymbolKindLinkage `json:"kinds"`
}
