// CodeYAML2Go auto-generated Go source from TeX.yaml (Pascal-W AST)
// This is a transliteration of Knuth's TeX82, not a port.
// Many original TeX macros and WEB sections are stubbed out.

package main

import (
	"fmt"
	"math"
	"os"
	"time"
)

// ── Basic type aliases ──
type boolean = bool

const (
	true  = true
	false = false
)

// ── TeX constants (may be overridden in the code below) ──
const (
	MAXPRINTLINE = 79
	MAXBUFSTACK  = 20
	TEXTWIDTH    = 65
	TEXTHEIGHT   = 53
	DRAFTMODE    = false
	MAXCOMPRESS  = 35
	MAXSAVE      = 1000
	MAXDEPTH     = 500
	STACKSIZE    = 200
	MEMMAX       = 30000
	BUFSIZE      = 500
	MAXINOPEN    = 6
	FONTMAX      = 75
	FONTMEMSIZE  = 20000
	PARAMSIZE    = 60
	NESTSIZE     = 40
	MAXSTRINGS   = 3000
	POOLSIZE     = 65535
	MAXWIDTH     = 100000000
	MAXHEIGHT    = 100000000
)

// ── I/O helpers ──
func print_(s string) { fmt.Print(s) }
func println_()       { fmt.Println() }
func printc(c byte)   { fmt.Printf("%c", c) }
func newline()        { fmt.Println() }
func flush_out()      {}

// ── Pascal standard function stubs ──
func abs_(x int) int {
	if x < 0 {
		return -x
	}
	return x
}
func sqr_(x int) int                 { return x * x }
func round_(x float64) int           { return int(math.Floor(x + 0.5)) }
func trunc_(x float64) int           { return int(x) }
func eof_(args ...interface{}) bool  { return false }
func eoln_(args ...interface{}) bool { return false }
func max_(args ...int) int {
	m := 0
	for _, v := range args {
		if v > m {
			m = v
		}
	}
	return m
}
func min_(args ...int) int {
	if len(args) == 0 {
		return 0
	}
	m := args[0]
	for _, v := range args {
		if v < m {
			m = v
		}
	}
	return m
}
func copy_(s string, idx, n int) string {
	if idx < 1 {
		idx = 1
	}
	if idx > len(s) {
		return ""
	}
	end := idx - 1 + n
	if end > len(s) {
		end = len(s)
	}
	return s[idx-1 : end]
}
func concat_(args ...string) string {
	var b []byte
	for _, s := range args {
		b = append(b, s...)
	}
	return string(b)
}
func pos_(sub, s string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return i + 1
		}
	}
	return 0
}
func upcase_(c byte) byte {
	if c >= 'a' && c <= 'z' {
		return c - 32
	}
	return c
}
func lo_(x int) int                { return x & 0xFFFF }
func hi_(x int) int                { return (x >> 16) & 0xFFFF }
func mem_(args ...interface{}) int { return 0 }

// ── Pascal standard procedure stubs (variadic to match any arity) ──
func get_(args ...interface{})     {}
func put_(args ...interface{})     {}
func reset_(args ...interface{})   {}
func rewrite_(args ...interface{}) {}
func read_(args ...interface{})    {}
func readln_(args ...interface{})  {}
func write_(args ...interface{}) {
	for _, a := range args {
		fmt.Print(a)
	}
}
func writeln_(args ...interface{}) {
	for _, a := range args {
		fmt.Print(a)
	}
	fmt.Println()
}
func page_(args ...interface{})    {}
func break_(args ...interface{})   {}
func pack_(args ...interface{})    {}
func unpack_(args ...interface{})  {}
func dispose_(args ...interface{}) {}
func new_(args ...interface{})     {}
func seek_(args ...interface{})    {}
func close_(args ...interface{})   {}
func flush_(args ...interface{})   {}

// ── Forward declarations (forward stubs) ──

/* ── Type definitions ── */
type ASCIIcode = int
type eightbits = int
type alphafile = *os.File
type bytefile = *os.File
type poolpointer = int
type strnumber = int
type packedASCIIcode = int
type scaled = int
type nonnegativeinteger = int
type smallnumber = int
type glueratio = float64
type quarterword = int
type halfword = int
type twochoices = int
type fourchoices = int
type twohalves_t struct {
	rh int
	lh int
	b0 int
	b1 int
}

type fourquarters_t struct {
	b0 int
	b1 int
	b2 int
	b3 int
}

type memoryword_t struct {
	int  int
	gr   *glueratio_t
	hh   *twohalves_t
	qqqq *fourquarters_t
}

type wordfile = *os.File
type glueord = int
type liststaterecord_t struct {
	modefield int
	headfield int
	tailfield int
	pgfield   int
	mlfield   int
	auxfield  *memoryword_t
}

type groupcode = int
type instaterecord_t struct {
	statefield int
	indexfield int
	startfield int
	locfield   int
	limitfield int
	namefield  int
}

type internalfontnumber = int
type fontindex = int
type dviindex = int
type triepointer = int
type hyphpointer = int
type ASCIIcode = int
type eightbits = int
type alphafile = *os.File
type bytefile = *os.File
type poolpointer = int
type strnumber = int
type packedASCIIcode = int
type scaled = int
type nonnegativeinteger = int
type smallnumber = int
type glueratio = float64
type quarterword = int
type halfword = int
type twochoices = int
type fourchoices = int
type twohalves_t struct {
	rh int
	lh int
	b0 int
	b1 int
}

type fourquarters_t struct {
	b0 int
	b1 int
	b2 int
	b3 int
}

type memoryword_t struct {
	int  int
	gr   *glueratio_t
	hh   *twohalves_t
	qqqq *fourquarters_t
}

type wordfile = *os.File
type glueord = int
type liststaterecord_t struct {
	modefield int
	headfield int
	tailfield int
	pgfield   int
	mlfield   int
	auxfield  *memoryword_t
}

type groupcode = int
type instaterecord_t struct {
	statefield int
	indexfield int
	startfield int
	locfield   int
	limitfield int
	namefield  int
}

type internalfontnumber = int
type fontindex = int
type dviindex = int
type triepointer = int
type hyphpointer = int

/* ── Constants ── */
const (
	memmax          = 30000
	memmin          = 0
	bufsize         = 500
	errorline       = 72
	halferrorline   = 42
	maxprintline    = 79
	stacksize       = 200
	maxinopen       = 6
	fontmax         = 75
	fontmemsize     = 20000
	paramsize       = 60
	nestsize        = 40
	maxstrings      = 3000
	stringvacancies = 8000
	poolsize        = 32000
	savesize        = 600
	triesize        = 8000
	trieopsize      = 500
	dvibufsize      = 800
	filenamesize    = 40
	poolname        = "TeXformats:TEX.POOL                     "
)

/* ── Global variables ── */
var (
	bad                  int
	xord                 []byte
	xchr                 []byte
	nameoffile           []byte
	namelength           int
	buffer               []byte
	first                int
	last                 int
	maxbufstack          int
	termin               *alphafile_t
	termout              *alphafile_t
	strpool              []byte
	strstart             []byte
	poolptr              int
	strptr               int
	initpoolptr          int
	initstrptr           int
	poolfile             *alphafile_t
	logfile              *alphafile_t
	selector             int
	dig                  []byte
	tally                int
	termoffset           int
	fileoffset           int
	trickbuf             []byte
	trickcount           int
	firstcount           int
	interaction          int
	deletionsallowed     bool
	setboxallowed        bool
	history              int
	errorcount           int
	helpline             []byte
	helpptr              int
	useerrhelp           bool
	interrupt            int
	OKtointerrupt        bool
	aritherror           bool
	remainder            int
	tempptr              int
	mem                  []byte
	lomemmax             int
	himemmin             int
	varused              int
	avail                int
	memend               int
	rover                int
	fontinshortdisplay   int
	depththreshold       int
	breadthmax           int
	nest                 []byte
	nestptr              int
	maxneststack         int
	curlist              *liststaterecord_t
	shownmode            int
	oldsetting           int
	systime              int
	eqtb                 []byte
	xeqlevel             []byte
	hash                 []byte
	hashused             int
	nonewcontrolsequence bool
	cscount              int
	savestack            []byte
	saveptr              int
	maxsavestack         int
	curlevel             int
	curgroup             int
	curboundary          int
	magset               int
	curcmd               byte
	curchr               int
	curcs                int
	curtok               int
	inputstack           []byte
	inputptr             int
	maxinstack           int
	curinput             *instaterecord_t
	inopen               int
	openparens           int
	inputfile            []byte
	line                 int
	linestack            []byte
	scannerstatus        int
	warningindex         int
	defref               int
	paramstack           []byte
	paramptr             int
	maxparamstack        int
	alignstate           int
	baseptr              int
	parloc               int
	partoken             int
	forceeof             bool
	curmark              []byte
	longstate            int
	pstack               []byte
	curval               int
	curvallevel          int
	radix                int
	curorder             int
	readfile             []byte
	readopen             []byte
	condptr              int
	iflimit              int
	curif                int
	ifline               int
	skipline             int
	curname              int
	curarea              int
	curext               int
	areadelimiter        int
	extdelimiter         int
	TEXformatdefault     []byte
	nameinprogress       bool
	jobname              int
	logopened            bool
	dvifile              *bytefile_t
	outputfilename       int
	logname              int
	tfmfile              *bytefile_t
	fontinfo             []byte
	fmemptr              int
	fontptr              int
	fontcheck            []byte
	fontsize             []byte
	fontdsize            []byte
	fontparams           []byte
	fontname             []byte
	fontarea             []byte
	fontbc               []byte
	fontec               []byte
	fontglue             []byte
	fontused             []byte
	hyphenchar           []byte
	skewchar             []byte
	bcharlabel           []byte
	fontbchar            []byte
	fontfalsebchar       []byte
	charbase             []byte
	widthbase            []byte
	heightbase           []byte
	depthbase            []byte
	italicbase           []byte
	ligkernbase          []byte
	kernbase             []byte
	extenbase            []byte
	parambase            []byte
	nullcharacter        *fourquarters_t
	totalpages           int
	maxv                 int
	maxh                 int
	maxpush              int
	lastbop              int
	deadcycles           int
	doingleaders         bool
	c                    int
	ruleht               int
	g                    int
	lq                   int
	dvibuf               []byte
	halfbuf              int
	dvilimit             int
	dviptr               int
	dvioffset            int
	dvigone              int
	downptr              int
	dvih                 int
	curh                 int
	dvif                 int
	curs                 int
	totalstretch         []byte
	lastbadness          int
	adjusttail           int
	packbeginline        int
	emptyfield           *twohalves_t
	nulldelimiter        *fourquarters_t
	curmlist             int
	curstyle             int
	cursize              int
	curmu                int
	mlistpenalties       bool
	curf                 int
	curc                 int
	curi                 *fourquarters_t
	magicoffset          int
	curalign             int
	curspan              int
	curloop              int
	alignptr             int
	curhead              int
	justbox              int
	passive              int
	printednode          int
	passnumber           int
	activewidth          []byte
	curactivewidth       []byte
	background           []byte
	breakwidth           []byte
	noshrinkerroryet     bool
	curp                 int
	secondpass           bool
	finalpass            bool
	threshold            int
	minimaldemerits      []byte
	minimumdemerits      int
	bestplace            []byte
	bestplline           []byte
	discwidth            int
	easyline             int
	lastspecialline      int
	firstwidth           int
	secondwidth          int
	firstindent          int
	secondindent         int
	bestbet              int
	fewestdemerits       int
	bestline             int
	actuallooseness      int
	linediff             int
	hc                   []byte
	hn                   int
	ha                   int
	hf                   int
	hu                   []byte
	hyfchar              int
	curlang              byte
	lhyf                 int
	hyfbchar             int
	hyf                  []byte
	initlist             int
	initlig              bool
	initlft              bool
	hyphenpassed         int
	curl                 int
	curq                 int
	ligstack             int
	ligaturepresent      bool
	lfthit               bool
	trie                 []byte
	hyfdistance          []byte
	hyfnum               []byte
	hyfnext              []byte
	opstart              []byte
	hyphword             []byte
	hyphlist             []byte
	hyphcount            int
	trieophash           []byte
	trieused             []byte
	trieoplang           []byte
	trieopval            []byte
	trieopptr            int
	triec                []byte
	trieo                []byte
	triel                []byte
	trier                []byte
	trieptr              *triepointer_t
	triehash             []byte
	trietaken            []byte
	triemin              []byte
	triemax              *triepointer_t
	trienotready         bool
	bestheightplusdepth  int
	pagetail             int
	pagecontents         int
	pagemaxdepth         int
	bestpagebreak        int
	leastpagecost        int
	bestsize             int
	pagesofar            []byte
	lastglue             int
	lastpenalty          int
	lastkern             int
	insertpenalties      int
	outputactive         bool
	mainf                int
	maini                *fourquarters_t
	mainj                *fourquarters_t
	maink                int
	mainp                int
	mains                int
	bchar                int
	falsebchar           int
	cancelboundary       bool
	insdisc              bool
	curbox               int
	aftertoken           int
	longhelpseen         bool
	formatident          int
	fmtfile              *wordfile_t
	readyalready         int
	writefile            []byte
	writeopen            []byte
	writeloc             int
)

/* ── Forward declarations ── */
func normalizeselector() { /* forward stub */ }

func gettoken() { /* forward stub */ }

func terminput() { /* forward stub */ }

func showcontext() { /* forward stub */ }

func beginfilereading() { /* forward stub */ }

func openlogfile() { /* forward stub */ }

func closefilesandterminate() { /* forward stub */ }

func clearforerrorprompt() { /* forward stub */ }

func giveerrhelp() { /* forward stub */ }

func showinfo() { /* forward stub */ }

func printtotals() { /* forward stub */ }

func backinput() { /* forward stub */ }

func firmuptheline() { /* forward stub */ }

func passtext() { /* forward stub */ }

func startinput() { /* forward stub */ }

func conditional() { /* forward stub */ }

func getxtoken() { /* forward stub */ }

func convtoks() { /* forward stub */ }

func insthetoks() { /* forward stub */ }

func scanint() { /* forward stub */ }

func vlistout() { /* forward stub */ }

func mlisttohlist() { /* forward stub */ }

func alignpeek() { /* forward stub */ }

func normalparagraph() { /* forward stub */ }

func doassignments() { /* forward stub */ }

func resumeafterdisplay() { /* forward stub */ }

func buildpage() { /* forward stub */ }

/* ── Subprograms ── */

/* procedure: initialize */
func initialize() {
	var (
		i int
		k int
		z int
	)
	xchr[32] = " "
	xchr[33] = "!"
	xchr[34] = "\""
	xchr[35] = "#"
	xchr[36] = "$"
	xchr[37] = "%"
	xchr[38] = "&"
	xchr[39] = "'"
	xchr[40] = "("
	xchr[41] = ")"
	xchr[42] = "*"
	xchr[43] = "+"
	xchr[44] = ","
	xchr[45] = "-"
	xchr[46] = "."
	xchr[47] = "/"
	xchr[48] = "0"
	xchr[49] = "1"
	xchr[50] = "2"
	xchr[51] = "3"
	xchr[52] = "4"
	xchr[53] = "5"
	xchr[54] = "6"
	xchr[55] = "7"
	xchr[56] = "8"
	xchr[57] = "9"
	xchr[58] = ":"
	xchr[59] = ";"
	xchr[60] = "<"
	xchr[61] = "="
	xchr[62] = ">"
	xchr[63] = "?"
	xchr[64] = "@"
	xchr[65] = "A"
	xchr[66] = "B"
	xchr[67] = "C"
	xchr[68] = "D"
	xchr[69] = "E"
	xchr[70] = "F"
	xchr[71] = "G"
	xchr[72] = "H"
	xchr[73] = "I"
	xchr[74] = "J"
	xchr[75] = "K"
	xchr[76] = "L"
	xchr[77] = "M"
	xchr[78] = "N"
	xchr[79] = "O"
	xchr[80] = "P"
	xchr[81] = "Q"
	xchr[82] = "R"
	xchr[83] = "S"
	xchr[84] = "T"
	xchr[85] = "U"
	xchr[86] = "V"
	xchr[87] = "W"
	xchr[88] = "X"
	xchr[89] = "Y"
	xchr[90] = "Z"
	xchr[91] = "["
	xchr[92] = "\\"
	xchr[93] = "]"
	xchr[94] = "^"
	xchr[95] = "_"
	xchr[96] = "`"
	xchr[97] = "a"
	xchr[98] = "b"
	xchr[99] = "c"
	xchr[100] = "d"
	xchr[101] = "e"
	xchr[102] = "f"
	xchr[103] = "g"
	xchr[104] = "h"
	xchr[105] = "i"
	xchr[106] = "j"
	xchr[107] = "k"
	xchr[108] = "l"
	xchr[109] = "m"
	xchr[110] = "n"
	xchr[111] = "o"
	xchr[112] = "p"
	xchr[113] = "q"
	xchr[114] = "r"
	xchr[115] = "s"
	xchr[116] = "t"
	xchr[117] = "u"
	xchr[118] = "v"
	xchr[119] = "w"
	xchr[120] = "x"
	xchr[121] = "y"
	xchr[122] = "z"
	xchr[123] = "{"
	xchr[124] = "|"
	xchr[125] = "}"
	xchr[126] = "~"
	for i := 0; i <= 31; i++ {
		xchr[i] = " "
	}
	for i := 127; i <= 255; i++ {
		xchr[i] = " "
	}
	for i := 0; i <= 255; i++ {
		xord[byte(i)] = 127
	}
	for i := 128; i <= 255; i++ {
		xord[xchr[i]] = i
	}
	for i := 0; i <= 126; i++ {
		xord[xchr[i]] = i
	}
	interaction = 3
	deletionsallowed = true
	setboxallowed = true
	errorcount = 0
	helpptr = 0
	useerrhelp = false
	interrupt = 0
	OKtointerrupt = true
	nestptr = 0
	maxneststack = 0
	curlist.modefield = 1
	curlist.headfield = 29999
	curlist.tailfield = 29999
	curlist.auxfield.int = (-65536000)
	curlist.mlfield = 0
	curlist.pgfield = 0
	shownmode = 0
	pagecontents = 0
	pagetail = 29998
	mem[29998].hh.rh = 0
	lastglue = 65535
	lastpenalty = 0
	lastkern = 0
	pagesofar[7] = 0
	pagemaxdepth = 0
	for k := 5263; k <= 6106; k++ {
		xeqlevel[k] = 1
	}
	nonewcontrolsequence = true
	hash[514].lh = 0
	hash[514].rh = 0
	for k := 515; k <= 2880; k++ {
		hash[k] = hash[514]
	}
	saveptr = 0
	curlevel = 1
	curgroup = 0
	curboundary = 0
	maxsavestack = 0
	magset = 0
	curmark[0] = 0
	curmark[1] = 0
	curmark[2] = 0
	curmark[3] = 0
	curmark[4] = 0
	curval = 0
	curvallevel = 0
	radix = 0
	curorder = 0
	for k := 0; k <= 16; k++ {
		readopen[k] = 2
	}
	condptr = 0
	iflimit = 0
	curif = 0
	ifline = 0
	TEXformatdefault = "TeXformats:plain.fmt"
	for k := 0; k <= fontmax; k++ {
		fontused[k] = false
	}
	nullcharacter.b0 = 0
	nullcharacter.b1 = 0
	nullcharacter.b2 = 0
	nullcharacter.b3 = 0
	totalpages = 0
	maxv = 0
	maxh = 0
	maxpush = 0
	lastbop = (-1)
	doingleaders = false
	deadcycles = 0
	curs = (-1)
	halfbuf = (dvibufsize / 2)
	dvilimit = dvibufsize
	dviptr = 0
	dvioffset = 0
	dvigone = 0
	downptr = 0
	rightptr = 0
	adjusttail = 0
	lastbadness = 0
	packbeginline = 0
	emptyfield.rh = 0
	emptyfield.lh = 0
	nulldelimiter.b0 = 0
	nulldelimiter.b1 = 0
	nulldelimiter.b2 = 0
	nulldelimiter.b3 = 0
	alignptr = 0
	curalign = 0
	curspan = 0
	curloop = 0
	curhead = 0
	curtail = 0
	for z := 0; z <= 307; z++ {
		{
			hyphword[z] = 0
			hyphlist[z] = 0
		}
	}
	hyphcount = 0
	outputactive = false
	insertpenalties = 0
	ligaturepresent = false
	cancelboundary = false
	lfthit = false
	rthit = false
	insdisc = false
	aftertoken = 0
	longhelpseen = false
	formatident = 0
	for k := 0; k <= 17; k++ {
		writeopen[k] = false
	}
	for k := 1; k <= 19; k++ {
		mem[k].int = 0
	}
	k = 0
	for k <= 19 {
		{
			mem[k].hh.rh = 1
			mem[k].hh.b0 = 0
			mem[k].hh.b1 = 0
			k = (k + 4)
		}
	}
	mem[6].int = 65536
	mem[4].hh.b0 = 1
	mem[10].int = 65536
	mem[8].hh.b0 = 2
	mem[14].int = 65536
	mem[12].hh.b0 = 1
	mem[15].int = 65536
	mem[12].hh.b1 = 1
	mem[18].int = (-65536)
	mem[16].hh.b0 = 1
	rover = 20
	mem[rover].hh.rh = 65535
	mem[rover].hh.lh = 1000
	mem[(rover + 1)].hh.lh = rover
	mem[(rover + 1)].hh.rh = rover
	lomemmax = (rover + 1000)
	mem[lomemmax].hh.rh = 0
	mem[lomemmax].hh.lh = 0
	for k := 29987; k <= 30000; k++ {
		mem[k] = mem[lomemmax]
	}
	mem[29990].hh.lh = 6714
	mem[29991].hh.rh = 256
	mem[29991].hh.lh = 0
	mem[29993].hh.b0 = 1
	mem[29994].hh.lh = 65535
	mem[29993].hh.b1 = 0
	mem[30000].hh.b1 = 255
	mem[30000].hh.b0 = 1
	mem[30000].hh.rh = 30000
	mem[29998].hh.b0 = 10
	mem[29998].hh.b1 = 0
	avail = 0
	memend = 30000
	himemmin = 29987
	varused = 20
	dynused = 14
	eqtb[2881].hh.b0 = 101
	eqtb[2881].hh.rh = 0
	eqtb[2881].hh.b1 = 0
	for k := 1; k <= 2880; k++ {
		eqtb[k] = eqtb[2881]
	}
	eqtb[2882].hh.rh = 0
	eqtb[2882].hh.b1 = 1
	eqtb[2882].hh.b0 = 117
	for k := 2883; k <= 3411; k++ {
		eqtb[k] = eqtb[2882]
	}
	mem[0].hh.rh = (mem[0].hh.rh + 530)
	eqtb[3412].hh.rh = 0
	eqtb[3412].hh.b0 = 118
	eqtb[3412].hh.b1 = 1
	for k := 3413; k <= 3677; k++ {
		eqtb[k] = eqtb[2881]
	}
	eqtb[3678].hh.rh = 0
	eqtb[3678].hh.b0 = 119
	eqtb[3678].hh.b1 = 1
	for k := 3679; k <= 3933; k++ {
		eqtb[k] = eqtb[3678]
	}
	eqtb[3934].hh.rh = 0
	eqtb[3934].hh.b0 = 120
	eqtb[3934].hh.b1 = 1
	for k := 3935; k <= 3982; k++ {
		eqtb[k] = eqtb[3934]
	}
	eqtb[3983].hh.rh = 0
	eqtb[3983].hh.b0 = 120
	eqtb[3983].hh.b1 = 1
	for k := 3984; k <= 5262; k++ {
		eqtb[k] = eqtb[3983]
	}
	for k := 0; k <= 255; k++ {
		{
			eqtb[(3983 + k)].hh.rh = 12
			eqtb[(5007 + k)].hh.rh = (k + 0)
			eqtb[(4751 + k)].hh.rh = 1000
		}
	}
	eqtb[3996].hh.rh = 5
	eqtb[4015].hh.rh = 10
	eqtb[4075].hh.rh = 0
	eqtb[4020].hh.rh = 14
	eqtb[4110].hh.rh = 15
	eqtb[3983].hh.rh = 9
	for k := 48; k <= 57; k++ {
		eqtb[(5007 + k)].hh.rh = (k + 28672)
	}
	for k := 65; k <= 90; k++ {
		{
			eqtb[(3983 + k)].hh.rh = 11
			eqtb[((3983 + k) + 32)].hh.rh = 11
			eqtb[(5007 + k)].hh.rh = (k + 28928)
			eqtb[((5007 + k) + 32)].hh.rh = (k + 28960)
			eqtb[(4239 + k)].hh.rh = (k + 32)
			eqtb[((4239 + k) + 32)].hh.rh = (k + 32)
			eqtb[(4495 + k)].hh.rh = k
			eqtb[((4495 + k) + 32)].hh.rh = k
			eqtb[(4751 + k)].hh.rh = 999
		}
	}
	for k := 5263; k <= 5573; k++ {
		eqtb[k].int = 0
	}
	eqtb[5280].int = 1000
	eqtb[5264].int = 10000
	eqtb[5304].int = 1
	eqtb[5303].int = 25
	eqtb[5308].int = 92
	eqtb[5311].int = 13
	for k := 0; k <= 255; k++ {
		eqtb[(5574 + k)].int = (-1)
	}
	eqtb[5620].int = 0
	for k := 5830; k <= 6106; k++ {
		eqtb[k].int = 0
	}
	hashused = 2614
	cscount = 0
	eqtb[2623].hh.b0 = 116
	hash[2623].rh = 502
	fontptr = 0
	fmemptr = 7
	fontname[0] = 801
	fontarea[0] = 338
	hyphenchar[0] = 45
	skewchar[0] = (-1)
	bcharlabel[0] = 0
	fontbchar[0] = 256
	fontfalsebchar[0] = 256
	fontbc[0] = 1
	fontec[0] = 0
	fontsize[0] = 0
	fontdsize[0] = 0
	charbase[0] = 0
	widthbase[0] = 0
	heightbase[0] = 0
	depthbase[0] = 0
	italicbase[0] = 0
	ligkernbase[0] = 0
	kernbase[0] = 0
	extenbase[0] = 0
	fontglue[0] = 0
	fontparams[0] = 7
	parambase[0] = (-1)
	for k := 0; k <= 6; k++ {
		fontinfo[k].int = 0
	}
	for k := (-trieopsize); k <= trieopsize; k++ {
		trieophash[k] = 0
	}
	for k := 0; k <= 255; k++ {
		trieused[k] = 0
	}
	trieopptr = 0
	trienotready = true
	triel[0] = 0
	triec[0] = 0
	trieptr = 0
	hash[2614].rh = 1190
	formatident = 1257
	hash[2622].rh = 1296
	eqtb[2622].hh.b1 = 1
	eqtb[2622].hh.b0 = 113
	eqtb[2622].hh.rh = 0
}

/* procedure: println */
func println_() {
	switch selector {
	case 19:
		{
			writeln_(termout)
			writeln_(logfile)
			termoffset = 0
			fileoffset = 0
		}
	case 18:
		{
			writeln_(logfile)
			fileoffset = 0
		}
	case 17:
		{
			writeln_(termout)
			termoffset = 0
		}
	case 16:
		// empty
	case 20:
		// empty
	case 21:
		// empty
	default:
		writeln_(writefile[selector])
	}
}

/* procedure: printchar */
func printchar(s byte) {
	if s == eqtb[5312].int {
		if selector < 20 {
			{
				println_()
				goto L10
			}
		}
	}
	switch selector {
	case 19:
		{
			write_(termout, xchr[s])
			write_(logfile, xchr[s])
			termoffset = (termoffset + 1)
			fileoffset = (fileoffset + 1)
			if termoffset == maxprintline {
				{
					writeln_(termout)
					termoffset = 0
				}
			}
			if fileoffset == maxprintline {
				{
					writeln_(logfile)
					fileoffset = 0
				}
			}
		}
	case 18:
		{
			write_(logfile, xchr[s])
			fileoffset = (fileoffset + 1)
			if fileoffset == maxprintline {
				println_()
			}
		}
	case 17:
		{
			write_(termout, xchr[s])
			termoffset = (termoffset + 1)
			if termoffset == maxprintline {
				println_()
			}
		}
	case 16:
		// empty
	case 20:
		if tally < trickcount {
			trickbuf[(tally % errorline)] = s
		}
	case 21:
		{
			if poolptr < poolsize {
				{
					strpool[poolptr] = s
					poolptr = (poolptr + 1)
				}
			}
		}
	default:
		write_(writefile[selector], xchr[s])
	}
	tally = (tally + 1)
L10:
	// empty
}

/* procedure: print */
func print_(s int) {
	var (
		j  int
		nl int
	)
	if s >= strptr {
		s = 259
	} else {
		if s < 256 {
			if s < 0 {
				s = 259
			} else {
				{
					if selector > 20 {
						{
							printchar(s)
							goto L10
						}
					}
					if s == eqtb[5312].int {
						if selector < 20 {
							{
								println_()
								goto L10
							}
						}
					}
					nl = eqtb[5312].int
					eqtb[5312].int = (-1)
					j = strstart[s]
					for j < strstart[(s+1)] {
						{
							printchar(strpool[j])
							j = (j + 1)
						}
					}
					eqtb[5312].int = nl
					goto L10
				}
			}
		}
	}
	j = strstart[s]
	for j < strstart[(s+1)] {
		{
			printchar(strpool[j])
			j = (j + 1)
		}
	}
L10:
	// empty
}

/* procedure: slowprint */
func slowprint(s int) {
	var (
		j int
	)
	if (s >= strptr) || (s < 256) {
		print_(s)
	} else {
		{
			j = strstart[s]
			for j < strstart[(s+1)] {
				{
					print_(strpool[j])
					j = (j + 1)
				}
			}
		}
	}
}

/* procedure: printnl */
func printnl(s int) {
	if ((termoffset > 0) && ((selector & 1) != 0)) || ((fileoffset > 0) && (selector >= 18)) {
		println_()
	}
	print_(s)
}

/* procedure: printesc */
func printesc(s int) {
	var (
		c int
	)
	c = eqtb[5308].int
	if c >= 0 {
		if c < 256 {
			print_(c)
		}
	}
	slowprint(s)
}

/* procedure: printthedigs */
func printthedigs(k byte) {
	for k > 0 {
		{
			k = (k - 1)
			if dig[k] < 10 {
				printchar((48 + dig[k]))
			} else {
				printchar((55 + dig[k]))
			}
		}
	}
}

/* procedure: printint */
func printint(n int) {
	var (
		k int
		m int
	)
	k = 0
	if n < 0 {
		{
			printchar(45)
			if n > (-100000000) {
				n = (-n)
			} else {
				{
					m = ((-1) - n)
					n = (m / 10)
					m = ((m % 10) + 1)
					k = 1
					if m < 10 {
						dig[0] = m
					} else {
						{
							dig[0] = 0
							n = (n + 1)
						}
					}
				}
			}
		}
	}
	for {
		dig[k] = (n % 10)
		n = (n / 10)
		k = (k + 1)
		if !(n == 0) {
			break
		}
	}
	printthedigs(k)
}

/* procedure: printcs */
func printcs(p int) {
	if p < 514 {
		if p >= 257 {
			if p == 513 {
				{
					printesc(504)
					printesc(505)
					printchar(32)
				}
			} else {
				{
					printesc((p - 257))
					if eqtb[((3983+p)-257)].hh.rh == 11 {
						printchar(32)
					}
				}
			}
		} else {
			if p < 1 {
				printesc(506)
			} else {
				print_((p - 1))
			}
		}
	} else {
		if p >= 2881 {
			printesc(506)
		} else {
			if (hash[p].rh < 0) || (hash[p].rh >= strptr) {
				printesc(507)
			} else {
				{
					printesc(hash[p].rh)
					printchar(32)
				}
			}
		}
	}
}

/* procedure: sprintcs */
func sprintcs(p int) {
	if p < 514 {
		if p < 257 {
			print_((p - 1))
		} else {
			if p < 513 {
				printesc((p - 257))
			} else {
				{
					printesc(504)
					printesc(505)
				}
			}
		}
	} else {
		printesc(hash[p].rh)
	}
}

/* procedure: printfilename */
func printfilename(n int, a int, e int) {
	slowprint(a)
	slowprint(n)
	slowprint(e)
}

/* procedure: printsize */
func printsize(s int) {
	if s == 0 {
		printesc(412)
	} else {
		if s == 16 {
			printesc(413)
		} else {
			printesc(414)
		}
	}
}

/* procedure: printwritewhatsit */
func printwritewhatsit(s int, p int) {
	printesc(s)
	if mem[(p+1)].hh.lh < 16 {
		printint(mem[(p + 1)].hh.lh)
	} else {
		if mem[(p+1)].hh.lh == 16 {
			printchar(42)
		} else {
			printchar(45)
		}
	}
}

/* procedure: jumpout */
func jumpout() {
	goto L9998
}

/* procedure: error */
func error_() {
	var (
		c  byte
		s1 int
		s2 int
		s3 int
		s4 int
	)
	if history < 2 {
		history = 2
	}
	printchar(46)
	showcontext()
	if interaction == 3 {
		for true {
			{
			L22:
				if interaction != 3 {
					goto L10
				}
				clearforerrorprompt()
				{
					print_(264)
					terminput()
				}
				if last == first {
					goto L10
				}
				c = buffer[first]
				if c >= 97 {
					c = (c - 32)
				}
				switch c {
				case 48:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 49:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 50:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 51:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 52:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 53:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 54:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 55:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 56:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 57:
					if deletionsallowed {
						{
							s1 = curtok
							s2 = curcmd
							s3 = curchr
							s4 = alignstate
							alignstate = 1000000
							OKtointerrupt = false
							if ((last > (first + 1)) && (buffer[(first+1)] >= 48)) && (buffer[(first+1)] <= 57) {
								c = (((c * 10) + buffer[(first+1)]) - (48 * 11))
							} else {
								c = (c - 48)
							}
							for c > 0 {
								{
									gettoken()
									c = (c - 1)
								}
							}
							curtok = s1
							curcmd = s2
							curchr = s3
							alignstate = s4
							OKtointerrupt = true
							{
								helpptr = 2
								helpline[1] = 279
								helpline[0] = 280
							}
							showcontext()
							goto L22
						}
					}
				case 69:
					if baseptr > 0 {
						if inputstack[baseptr].namefield >= 256 {
							{
								printnl(265)
								slowprint(inputstack[baseptr].namefield)
								print_(266)
								printint(line)
								interaction = 2
								jumpout()
							}
						}
					}
				case 72:
					{
						if useerrhelp {
							{
								giveerrhelp()
								useerrhelp = false
							}
						} else {
							{
								if helpptr == 0 {
									{
										helpptr = 2
										helpline[1] = 281
										helpline[0] = 282
									}
								}
								for {
									helpptr = (helpptr - 1)
									print_(helpline[helpptr])
									println_()
									if !(helpptr == 0) {
										break
									}
								}
							}
						}
						{
							helpptr = 4
							helpline[3] = 283
							helpline[2] = 282
							helpline[1] = 284
							helpline[0] = 285
						}
						goto L22
					}
				case 73:
					{
						beginfilereading()
						if last > (first + 1) {
							{
								curinput.locfield = (first + 1)
								buffer[first] = 32
							}
						} else {
							{
								{
									print_(278)
									terminput()
								}
								curinput.locfield = first
							}
						}
						first = last
						curinput.limitfield = (last - 1)
						goto L10
					}
				case 81:
					{
						errorcount = 0
						interaction = ((0 + c) - 81)
						print_(273)
						switch c {
						case 81:
							{
								printesc(274)
								selector = (selector - 1)
							}
						case 82:
							printesc(275)
						case 83:
							printesc(276)
						}
						print_(277)
						println_()
						break_(termout)
						goto L10
					}
				case 82:
					{
						errorcount = 0
						interaction = ((0 + c) - 81)
						print_(273)
						switch c {
						case 81:
							{
								printesc(274)
								selector = (selector - 1)
							}
						case 82:
							printesc(275)
						case 83:
							printesc(276)
						}
						print_(277)
						println_()
						break_(termout)
						goto L10
					}
				case 83:
					{
						errorcount = 0
						interaction = ((0 + c) - 81)
						print_(273)
						switch c {
						case 81:
							{
								printesc(274)
								selector = (selector - 1)
							}
						case 82:
							printesc(275)
						case 83:
							printesc(276)
						}
						print_(277)
						println_()
						break_(termout)
						goto L10
					}
				case 88:
					{
						interaction = 2
						jumpout()
					}
				default:
					// empty
				}
				{
					print_(267)
					printnl(268)
					printnl(269)
					if baseptr > 0 {
						if inputstack[baseptr].namefield >= 256 {
							print_(270)
						}
					}
					if deletionsallowed {
						printnl(271)
					}
					printnl(272)
				}
			}
		}
	}
	errorcount = (errorcount + 1)
	if errorcount == 100 {
		{
			printnl(263)
			history = 3
			jumpout()
		}
	}
	if interaction > 0 {
		selector = (selector - 1)
	}
	if useerrhelp {
		{
			println_()
			giveerrhelp()
		}
	} else {
		for helpptr > 0 {
			{
				helpptr = (helpptr - 1)
				printnl(helpline[helpptr])
			}
		}
	}
	println_()
	if interaction > 0 {
		selector = (selector + 1)
	}
	println_()
L10:
	// empty
}

/* procedure: fatalerror */
func fatalerror(s int) {
	normalizeselector()
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(287)
	}
	{
		helpptr = 1
		helpline[0] = s
	}
	{
		if interaction == 3 {
			interaction = 2
		}
		if logopened {
			error_()
		}
		history = 3
		jumpout()
	}
}

/* procedure: overflow */
func overflow(s int, n int) {
	normalizeselector()
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(288)
	}
	print_(s)
	printchar(61)
	printint(n)
	printchar(93)
	{
		helpptr = 2
		helpline[1] = 289
		helpline[0] = 290
	}
	{
		if interaction == 3 {
			interaction = 2
		}
		if logopened {
			error_()
		}
		history = 3
		jumpout()
	}
}

/* procedure: confusion */
func confusion(s int) {
	normalizeselector()
	if history < 2 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(291)
			}
			print_(s)
			printchar(41)
			{
				helpptr = 1
				helpline[0] = 292
			}
		}
	} else {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(293)
			}
			{
				helpptr = 2
				helpline[1] = 294
				helpline[0] = 295
			}
		}
	}
	{
		if interaction == 3 {
			interaction = 2
		}
		if logopened {
			error_()
		}
		history = 3
		jumpout()
	}
}

/* function: aopenin */
func aopenin(f **alphafile_t) bool {
	reset_(f, nameoffile, "/O")
	aopenin = (erstat(f) == 0)
}

/* function: aopenout */
func aopenout(f **alphafile_t) bool {
	rewrite_(f, nameoffile, "/O")
	aopenout = (erstat(f) == 0)
}

/* function: bopenin */
func bopenin(f **bytefile_t) bool {
	reset_(f, nameoffile, "/O")
	bopenin = (erstat(f) == 0)
}

/* function: bopenout */
func bopenout(f **bytefile_t) bool {
	rewrite_(f, nameoffile, "/O")
	bopenout = (erstat(f) == 0)
}

/* function: wopenin */
func wopenin(f **wordfile_t) bool {
	reset_(f, nameoffile, "/O")
	wopenin = (erstat(f) == 0)
}

/* function: wopenout */
func wopenout(f **wordfile_t) bool {
	rewrite_(f, nameoffile, "/O")
	wopenout = (erstat(f) == 0)
}

/* procedure: aclose */
func aclose(f **alphafile_t) {
	close_(f)
}

/* procedure: bclose */
func bclose(f **bytefile_t) {
	close_(f)
}

/* procedure: wclose */
func wclose(f **wordfile_t) {
	close_(f)
}

/* function: inputln */
func inputln(f **alphafile_t, bypasseoln bool) bool {
	var (
		lastnonblank int
	)
	if bypasseoln {
		if !eof_(f) {
			get_(f)
		}
	}
	last = first
	if eof_(f) {
		inputln = false
	} else {
		{
			lastnonblank = first
			for !eoln_(f) {
				{
					if last >= maxbufstack {
						{
							maxbufstack = (last + 1)
							if maxbufstack == bufsize {
								if formatident == 0 {
									{
										writeln_(termout, "Buffer size exceeded!")
										goto L9999
									}
								} else {
									{
										curinput.locfield = first
										curinput.limitfield = (last - 1)
										overflow(256, bufsize)
									}
								}
							}
						}
					}
					buffer[last] = xord[*f]
					get_(f)
					last = (last + 1)
					if buffer[(last-1)] != 32 {
						lastnonblank = last
					}
				}
			}
			last = lastnonblank
			inputln = true
		}
	}
}

/* function: initterminal */
func initterminal() bool {
	reset_(termin, "TTY:", "/O/I")
	for true {
		{
			write_(termout, "**")
			break_(termout)
			if !inputln(termin, true) {
				{
					writeln_(termout)
					write_(termout, "! End of file on the terminal... why?")
					initterminal = false
					goto L10
				}
			}
			curinput.locfield = first
			for (curinput.locfield < last) && (buffer[curinput.locfield] == 32) {
				curinput.locfield = (curinput.locfield + 1)
			}
			if curinput.locfield < last {
				{
					initterminal = true
					goto L10
				}
			}
			writeln_(termout, "Please type the name of your input file.")
		}
	}
L10:
	// empty
}

/* function: makestring */
func makestring() int {
	if strptr == maxstrings {
		overflow(258, (maxstrings - initstrptr))
	}
	strptr = (strptr + 1)
	strstart[strptr] = poolptr
	makestring = (strptr - 1)
}

/* function: streqbuf */
func streqbuf(s int, k int) bool {
	var (
		j      int
		result bool
	)
	j = strstart[s]
	for j < strstart[(s+1)] {
		{
			if strpool[j] != buffer[k] {
				{
					result = false
					goto L45
				}
			}
			j = (j + 1)
			k = (k + 1)
		}
	}
	result = true
L45:
	streqbuf = result
}

/* function: streqstr */
func streqstr(s int, t int) bool {
	var (
		j      int
		k      int
		result bool
	)
	result = false
	if (strstart[(s+1)] - strstart[s]) != (strstart[(t+1)] - strstart[t]) {
		goto L45
	}
	j = strstart[s]
	k = strstart[t]
	for j < strstart[(s+1)] {
		{
			if strpool[j] != strpool[k] {
				goto L45
			}
			j = (j + 1)
			k = (k + 1)
		}
	}
	result = true
L45:
	streqstr = result
}

/* function: getstringsstarted */
func getstringsstarted() bool {
	var (
		k int
		l int
		m byte
		n byte
		g int
		a int
		c bool
	)
	poolptr = 0
	strptr = 0
	strstart[0] = 0
	for k := 0; k <= 255; k++ {
		{
			if (k < 32) || (k > 126) {
				{
					{
						strpool[poolptr] = 94
						poolptr = (poolptr + 1)
					}
					{
						strpool[poolptr] = 94
						poolptr = (poolptr + 1)
					}
					if k < 64 {
						{
							strpool[poolptr] = (k + 64)
							poolptr = (poolptr + 1)
						}
					} else {
						if k < 128 {
							{
								strpool[poolptr] = (k - 64)
								poolptr = (poolptr + 1)
							}
						} else {
							{
								l = (k / 16)
								if l < 10 {
									{
										strpool[poolptr] = (l + 48)
										poolptr = (poolptr + 1)
									}
								} else {
									{
										strpool[poolptr] = (l + 87)
										poolptr = (poolptr + 1)
									}
								}
								l = (k % 16)
								if l < 10 {
									{
										strpool[poolptr] = (l + 48)
										poolptr = (poolptr + 1)
									}
								} else {
									{
										strpool[poolptr] = (l + 87)
										poolptr = (poolptr + 1)
									}
								}
							}
						}
					}
				}
			} else {
				{
					strpool[poolptr] = k
					poolptr = (poolptr + 1)
				}
			}
			g = makestring
		}
	}
	nameoffile = poolname
	if aopenin(poolfile) {
		{
			c = false
			for {
				{
					if eof_(poolfile) {
						{
							writeln_(termout, "! TEX.POOL has no check sum.")
							aclose(poolfile)
							getstringsstarted = false
							goto L10
						}
					}
					read_(poolfile, m, n)
					if m == "*" {
						{
							a = 0
							k = 1
							for true {
								{
									if (xord[n] < 48) || (xord[n] > 57) {
										{
											writeln_(termout, "! TEX.POOL check sum doesn't have nine digits.")
											aclose(poolfile)
											getstringsstarted = false
											goto L10
										}
									}
									a = (((10 * a) + xord[n]) - 48)
									if k == 9 {
										goto L30
									}
									k = (k + 1)
									read_(poolfile, n)
								}
							}
						L30:
							if a != 504454778 {
								{
									writeln_(termout, "! TEX.POOL doesn't match; TANGLE me again.")
									aclose(poolfile)
									getstringsstarted = false
									goto L10
								}
							}
							c = true
						}
					} else {
						{
							if (((xord[m] < 48) || (xord[m] > 57)) || (xord[n] < 48)) || (xord[n] > 57) {
								{
									writeln_(termout, "! TEX.POOL line doesn't begin with two digits.")
									aclose(poolfile)
									getstringsstarted = false
									goto L10
								}
							}
							l = (((xord[m] * 10) + xord[n]) - (48 * 11))
							if ((poolptr + l) + stringvacancies) > poolsize {
								{
									writeln_(termout, "! You have to increase POOLSIZE.")
									aclose(poolfile)
									getstringsstarted = false
									goto L10
								}
							}
							for k := 1; k <= l; k++ {
								{
									if eoln_(poolfile) {
										m = " "
									} else {
										read_(poolfile, m)
									}
									{
										strpool[poolptr] = xord[m]
										poolptr = (poolptr + 1)
									}
								}
							}
							readln_(poolfile)
							g = makestring
						}
					}
				}
				if !(c) {
					break
				}
			}
			aclose(poolfile)
			getstringsstarted = true
		}
	} else {
		{
			writeln_(termout, "! I can't read TEX.POOL.")
			aclose(poolfile)
			getstringsstarted = false
			goto L10
		}
	}
L10:
	// empty
}

/* procedure: printtwo */
func printtwo(n int) {
	n = (abs_(n) % 100)
	printchar((48 + (n / 10)))
	printchar((48 + (n % 10)))
}

/* procedure: printhex */
func printhex(n int) {
	var (
		k int
	)
	k = 0
	printchar(34)
	for {
		dig[k] = (n % 16)
		n = (n / 16)
		k = (k + 1)
		if !(n == 0) {
			break
		}
	}
	printthedigs(k)
}

/* procedure: printromanint */
func printromanint(n int) {
	var (
		j int
		k int
		u int
		v int
	)
	j = strstart[260]
	v = 1000
	for true {
		{
			for n >= v {
				{
					printchar(strpool[j])
					n = (n - v)
				}
			}
			if n <= 0 {
				goto L10
			}
			k = (j + 2)
			u = (v / (strpool[(k-1)] - 48))
			if strpool[(k-1)] == 50 {
				{
					k = (k + 2)
					u = (u / (strpool[(k-1)] - 48))
				}
			}
			if (n + u) >= v {
				{
					printchar(strpool[k])
					n = (n + u)
				}
			} else {
				{
					j = (j + 2)
					v = (v / (strpool[(j-1)] - 48))
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: printcurrentstring */
func printcurrentstring() {
	var (
		j int
	)
	j = strstart[strptr]
	for j < poolptr {
		{
			printchar(strpool[j])
			j = (j + 1)
		}
	}
}

/* procedure: terminput */
func terminput() {
	var (
		k int
	)
	break_(termout)
	if !inputln(termin, true) {
		fatalerror(261)
	}
	termoffset = 0
	selector = (selector - 1)
	if last != first {
		for k := first; k <= (last - 1); k++ {
			print_(buffer[k])
		}
	}
	println_()
	selector = (selector + 1)
}

/* procedure: interror */
func interror(n int) {
	print_(286)
	printint(n)
	printchar(41)
	error_()
}

/* procedure: normalizeselector */
func normalizeselector() {
	if logopened {
		selector = 19
	} else {
		selector = 17
	}
	if jobname == 0 {
		openlogfile()
	}
	if interaction == 0 {
		selector = (selector - 1)
	}
}

/* procedure: pauseforinstructions */
func pauseforinstructions() {
	if OKtointerrupt {
		{
			interaction = 3
			if (selector == 18) || (selector == 16) {
				selector = (selector + 1)
			}
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(296)
			}
			{
				helpptr = 3
				helpline[2] = 297
				helpline[1] = 298
				helpline[0] = 299
			}
			deletionsallowed = false
			error_()
			deletionsallowed = true
			interrupt = 0
		}
	}
}

/* function: half */
func half(x int) int {
	if (x & 1) != 0 {
		half = ((x + 1) / 2)
	} else {
		half = (x / 2)
	}
}

/* function: rounddecimals */
func rounddecimals(k int) int {
	var (
		a int
	)
	a = 0
	for k > 0 {
		{
			k = (k - 1)
			a = ((a + (dig[k] * 131072)) / 10)
		}
	}
	rounddecimals = ((a + 1) / 2)
}

/* procedure: printscaled */
func printscaled(s int) {
	var (
		delta int
	)
	if s < 0 {
		{
			printchar(45)
			s = (-s)
		}
	}
	printint((s / 65536))
	printchar(46)
	s = ((10 * (s % 65536)) + 5)
	delta = 10
	for {
		if delta > 65536 {
			s = (s - 17232)
		}
		printchar((48 + (s / 65536)))
		s = (10 * (s % 65536))
		delta = (delta * 10)
		if !(s <= delta) {
			break
		}
	}
}

/* function: multandadd */
func multandadd(n int, x int, y int, maxanswer int) int {
	if n < 0 {
		{
			x = (-x)
			n = (-n)
		}
	}
	if n == 0 {
		multandadd = y
	} else {
		if (x <= ((maxanswer - y) / n)) && ((-x) <= ((maxanswer + y) / n)) {
			multandadd = ((n * x) + y)
		} else {
			{
				aritherror = true
				multandadd = 0
			}
		}
	}
}

/* function: xovern */
func xovern(x int, n int) int {
	var (
		negative bool
	)
	negative = false
	if n == 0 {
		{
			aritherror = true
			xovern = 0
			remainder = x
		}
	} else {
		{
			if n < 0 {
				{
					x = (-x)
					n = (-n)
					negative = true
				}
			}
			if x >= 0 {
				{
					xovern = (x / n)
					remainder = (x % n)
				}
			} else {
				{
					xovern = (-((-x) / n))
					remainder = (-((-x) % n))
				}
			}
		}
	}
	if negative {
		remainder = (-remainder)
	}
}

/* function: xnoverd */
func xnoverd(x int, n int, d int) int {
	var (
		positive bool
		t        int
		u        int
		v        int
	)
	if x >= 0 {
		positive = true
	} else {
		{
			x = (-x)
			positive = false
		}
	}
	t = ((x % 32768) * n)
	u = (((x / 32768) * n) + (t / 32768))
	v = (((u % d) * 32768) + (t % 32768))
	if (u / d) >= 32768 {
		aritherror = true
	} else {
		u = ((32768 * (u / d)) + (v / d))
	}
	if positive {
		{
			xnoverd = u
			remainder = (v % d)
		}
	} else {
		{
			xnoverd = (-u)
			remainder = (-(v % d))
		}
	}
}

/* function: badness */
func badness(t int, s int) int {
	var (
		r int
	)
	if t == 0 {
		badness = 0
	} else {
		if s <= 0 {
			badness = 10000
		} else {
			{
				if t <= 7230584 {
					r = ((t * 297) / s)
				} else {
					if s >= 1663497 {
						r = (t / (s / 297))
					} else {
						r = t
					}
				}
				if r > 1290 {
					badness = 10000
				} else {
					badness = ((((r * r) * r) + 131072) / 262144)
				}
			}
		}
	}
}

/* procedure: showtokenlist */
func showtokenlist(p int, q int, l int) {
	var (
		m        int
		c        int
		matchchr byte
		n        byte
	)
	matchchr = 35
	n = 48
	tally = 0
	for (p != 0) && (tally < l) {
		{
			if p == q {
				{
					firstcount = tally
					trickcount = (((tally + 1) + errorline) - halferrorline)
					if trickcount < errorline {
						trickcount = errorline
					}
				}
			}
			if (p < himemmin) || (p > memend) {
				{
					printesc(309)
					goto L10
				}
			}
			if mem[p].hh.lh >= 4095 {
				printcs((mem[p].hh.lh - 4095))
			} else {
				{
					m = (mem[p].hh.lh / 256)
					c = (mem[p].hh.lh % 256)
					if mem[p].hh.lh < 0 {
						printesc(555)
					} else {
						switch m {
						case 1:
							print_(c)
						case 2:
							print_(c)
						case 3:
							print_(c)
						case 4:
							print_(c)
						case 7:
							print_(c)
						case 8:
							print_(c)
						case 10:
							print_(c)
						case 11:
							print_(c)
						case 12:
							print_(c)
						case 6:
							{
								print_(c)
								print_(c)
							}
						case 5:
							{
								print_(matchchr)
								if c <= 9 {
									printchar((c + 48))
								} else {
									{
										printchar(33)
										goto L10
									}
								}
							}
						case 13:
							{
								matchchr = c
								print_(c)
								n = (n + 1)
								printchar(n)
								if n > 57 {
									goto L10
								}
							}
						case 14:
							print_(556)
						default:
							printesc(555)
						}
					}
				}
			}
			p = mem[p].hh.rh
		}
	}
	if p != 0 {
		printesc(554)
	}
L10:
	// empty
}

/* procedure: runaway */
func runaway() {
	var (
		p int
	)
	if scannerstatus > 1 {
		{
			printnl(569)
			switch scannerstatus {
			case 2:
				{
					print_(570)
					p = defref
				}
			case 3:
				{
					print_(571)
					p = 29997
				}
			case 4:
				{
					print_(572)
					p = 29996
				}
			case 5:
				{
					print_(573)
					p = defref
				}
			}
			printchar(63)
			println_()
			showtokenlist(mem[p].hh.rh, 0, (errorline - 10))
		}
	}
}

/* function: getavail */
func getavail() int {
	var (
		p int
	)
	p = avail
	if p != 0 {
		avail = mem[avail].hh.rh
	} else {
		if memend < memmax {
			{
				memend = (memend + 1)
				p = memend
			}
		} else {
			{
				himemmin = (himemmin - 1)
				p = himemmin
				if himemmin <= lomemmax {
					{
						runaway()
						overflow(300, ((memmax + 1) - memmin))
					}
				}
			}
		}
	}
	mem[p].hh.rh = 0
	getavail = p
}

/* procedure: flushlist */
func flushlist(p int) {
	var (
		q int
		r int
	)
	if p != 0 {
		{
			r = p
			for {
				q = r
				r = mem[r].hh.rh
				if !(r == 0) {
					break
				}
			}
			mem[q].hh.rh = avail
			avail = p
		}
	}
}

/* function: getnode */
func getnode(s int) int {
	var (
		p int
		q int
		r int
		t int
	)
L20:
	p = rover
	for {
		q = (p + mem[p].hh.lh)
		for mem[q].hh.rh == 65535 {
			{
				t = mem[(q + 1)].hh.rh
				if q == rover {
					rover = t
				}
				mem[(t + 1)].hh.lh = mem[(q + 1)].hh.lh
				mem[(mem[(q+1)].hh.lh + 1)].hh.rh = t
				q = (q + mem[q].hh.lh)
			}
		}
		r = (q - s)
		if r > (p + 1) {
			{
				mem[p].hh.lh = (r - p)
				rover = p
				goto L40
			}
		}
		if r == p {
			if mem[(p+1)].hh.rh != p {
				{
					rover = mem[(p + 1)].hh.rh
					t = mem[(p + 1)].hh.lh
					mem[(rover + 1)].hh.lh = t
					mem[(t + 1)].hh.rh = rover
					goto L40
				}
			}
		}
		mem[p].hh.lh = (q - p)
		p = mem[(p + 1)].hh.rh
		if !(p == rover) {
			break
		}
	}
	if s == 1073741824 {
		{
			getnode = 65535
			goto L10
		}
	}
	if (lomemmax + 2) < himemmin {
		if (lomemmax + 2) <= 65535 {
			{
				if (himemmin - lomemmax) >= 1998 {
					t = (lomemmax + 1000)
				} else {
					t = ((lomemmax + 1) + ((himemmin - lomemmax) / 2))
				}
				p = mem[(rover + 1)].hh.lh
				q = lomemmax
				mem[(p + 1)].hh.rh = q
				mem[(rover + 1)].hh.lh = q
				if t > 65535 {
					t = 65535
				}
				mem[(q + 1)].hh.rh = rover
				mem[(q + 1)].hh.lh = p
				mem[q].hh.rh = 65535
				mem[q].hh.lh = (t - lomemmax)
				lomemmax = t
				mem[lomemmax].hh.rh = 0
				mem[lomemmax].hh.lh = 0
				rover = q
				goto L20
			}
		}
	}
	overflow(300, ((memmax + 1) - memmin))
L40:
	mem[r].hh.rh = 0
	getnode = r
L10:
	// empty
}

/* procedure: freenode */
func freenode(p int, s int) {
	var (
		q int
	)
	mem[p].hh.lh = s
	mem[p].hh.rh = 65535
	q = mem[(rover + 1)].hh.lh
	mem[(p + 1)].hh.lh = q
	mem[(p + 1)].hh.rh = rover
	mem[(rover + 1)].hh.lh = p
	mem[(q + 1)].hh.rh = p
}

/* procedure: sortavail */
func sortavail() {
	var (
		p        int
		q        int
		r        int
		oldrover int
	)
	p = getnode(1073741824)
	p = mem[(rover + 1)].hh.rh
	mem[(rover + 1)].hh.rh = 65535
	oldrover = rover
	for p != oldrover {
		if p < rover {
			{
				q = p
				p = mem[(q + 1)].hh.rh
				mem[(q + 1)].hh.rh = rover
				rover = q
			}
		} else {
			{
				q = rover
				for mem[(q+1)].hh.rh < p {
					q = mem[(q + 1)].hh.rh
				}
				r = mem[(p + 1)].hh.rh
				mem[(p + 1)].hh.rh = mem[(q + 1)].hh.rh
				mem[(q + 1)].hh.rh = p
				p = r
			}
		}
	}
	p = rover
	for mem[(p+1)].hh.rh != 65535 {
		{
			mem[(mem[(p+1)].hh.rh + 1)].hh.lh = p
			p = mem[(p + 1)].hh.rh
		}
	}
	mem[(p + 1)].hh.rh = rover
	mem[(rover + 1)].hh.lh = p
}

/* function: newnullbox */
func newnullbox() int {
	var (
		p int
	)
	p = getnode(7)
	mem[p].hh.b0 = 0
	mem[p].hh.b1 = 0
	mem[(p + 1)].int = 0
	mem[(p + 2)].int = 0
	mem[(p + 3)].int = 0
	mem[(p + 4)].int = 0
	mem[(p + 5)].hh.rh = 0
	mem[(p + 5)].hh.b0 = 0
	mem[(p + 5)].hh.b1 = 0
	mem[(p + 6)].gr = 0
	newnullbox = p
}

/* function: newrule */
func newrule() int {
	var (
		p int
	)
	p = getnode(4)
	mem[p].hh.b0 = 2
	mem[p].hh.b1 = 0
	mem[(p + 1)].int = (-1073741824)
	mem[(p + 2)].int = (-1073741824)
	mem[(p + 3)].int = (-1073741824)
	newrule = p
}

/* function: newligature */
func newligature(f int, c int, q int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 6
	mem[(p + 1)].hh.b0 = f
	mem[(p + 1)].hh.b1 = c
	mem[(p + 1)].hh.rh = q
	mem[p].hh.b1 = 0
	newligature = p
}

/* function: newligitem */
func newligitem(c int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b1 = c
	mem[(p + 1)].hh.rh = 0
	newligitem = p
}

/* function: newdisc */
func newdisc() int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 7
	mem[p].hh.b1 = 0
	mem[(p + 1)].hh.lh = 0
	mem[(p + 1)].hh.rh = 0
	newdisc = p
}

/* function: newmath */
func newmath(w int, s int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 9
	mem[p].hh.b1 = s
	mem[(p + 1)].int = w
	newmath = p
}

/* function: newspec */
func newspec(p int) int {
	var (
		q int
	)
	q = getnode(4)
	mem[q] = mem[p]
	mem[q].hh.rh = 0
	mem[(q + 1)].int = mem[(p + 1)].int
	mem[(q + 2)].int = mem[(p + 2)].int
	mem[(q + 3)].int = mem[(p + 3)].int
	newspec = q
}

/* function: newparamglue */
func newparamglue(n int) int {
	var (
		p int
		q int
	)
	p = getnode(2)
	mem[p].hh.b0 = 10
	mem[p].hh.b1 = (n + 1)
	mem[(p + 1)].hh.rh = 0
	q = eqtb[(2882 + n)].hh.rh
	mem[(p + 1)].hh.lh = q
	mem[q].hh.rh = (mem[q].hh.rh + 1)
	newparamglue = p
}

/* function: newglue */
func newglue(q int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 10
	mem[p].hh.b1 = 0
	mem[(p + 1)].hh.rh = 0
	mem[(p + 1)].hh.lh = q
	mem[q].hh.rh = (mem[q].hh.rh + 1)
	newglue = p
}

/* function: newskipparam */
func newskipparam(n int) int {
	var (
		p int
	)
	tempptr = newspec(eqtb[(2882 + n)].hh.rh)
	p = newglue(tempptr)
	mem[tempptr].hh.rh = 0
	mem[p].hh.b1 = (n + 1)
	newskipparam = p
}

/* function: newkern */
func newkern(w int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 11
	mem[p].hh.b1 = 0
	mem[(p + 1)].int = w
	newkern = p
}

/* function: newpenalty */
func newpenalty(m int) int {
	var (
		p int
	)
	p = getnode(2)
	mem[p].hh.b0 = 12
	mem[p].hh.b1 = 0
	mem[(p + 1)].int = m
	newpenalty = p
}

/* procedure: shortdisplay */
func shortdisplay(p int) {
	var (
		n int
	)
	for p > memmin {
		{
			if p >= himemmin {
				{
					if p <= memend {
						{
							if mem[p].hh.b0 != fontinshortdisplay {
								{
									if (mem[p].hh.b0 < 0) || (mem[p].hh.b0 > fontmax) {
										printchar(42)
									} else {
										printesc(hash[(2624 + mem[p].hh.b0)].rh)
									}
									printchar(32)
									fontinshortdisplay = mem[p].hh.b0
								}
							}
							print_((mem[p].hh.b1 - 0))
						}
					}
				}
			} else {
				switch mem[p].hh.b0 {
				case 0:
					print_(308)
				case 1:
					print_(308)
				case 3:
					print_(308)
				case 8:
					print_(308)
				case 4:
					print_(308)
				case 5:
					print_(308)
				case 13:
					print_(308)
				case 2:
					printchar(124)
				case 10:
					if mem[(p+1)].hh.lh != 0 {
						printchar(32)
					}
				case 9:
					printchar(36)
				case 6:
					shortdisplay(mem[(p + 1)].hh.rh)
				case 7:
					{
						shortdisplay(mem[(p + 1)].hh.lh)
						shortdisplay(mem[(p + 1)].hh.rh)
						n = mem[p].hh.b1
						for n > 0 {
							{
								if mem[p].hh.rh != 0 {
									p = mem[p].hh.rh
								}
								n = (n - 1)
							}
						}
					}
				default:
					// empty
				}
			}
			p = mem[p].hh.rh
		}
	}
}

/* procedure: printfontandchar */
func printfontandchar(p int) {
	if p > memend {
		printesc(309)
	} else {
		{
			if (mem[p].hh.b0 < 0) || (mem[p].hh.b0 > fontmax) {
				printchar(42)
			} else {
				printesc(hash[(2624 + mem[p].hh.b0)].rh)
			}
			printchar(32)
			print_((mem[p].hh.b1 - 0))
		}
	}
}

/* procedure: printmark */
func printmark(p int) {
	printchar(123)
	if (p < himemmin) || (p > memend) {
		printesc(309)
	} else {
		showtokenlist(mem[p].hh.rh, 0, (maxprintline - 10))
	}
	printchar(125)
}

/* procedure: printruledimen */
func printruledimen(d int) {
	if d == (-1073741824) {
		printchar(42)
	} else {
		printscaled(d)
	}
}

/* procedure: printglue */
func printglue(d int, order int, s int) {
	printscaled(d)
	if (order < 0) || (order > 3) {
		print_(310)
	} else {
		if order > 0 {
			{
				print_(311)
				for order > 1 {
					{
						printchar(108)
						order = (order - 1)
					}
				}
			}
		} else {
			if s != 0 {
				print_(s)
			}
		}
	}
}

/* procedure: printspec */
func printspec(p int, s int) {
	if (p < memmin) || (p >= lomemmax) {
		printchar(42)
	} else {
		{
			printscaled(mem[(p + 1)].int)
			if s != 0 {
				print_(s)
			}
			if mem[(p+2)].int != 0 {
				{
					print_(312)
					printglue(mem[(p+2)].int, mem[p].hh.b0, s)
				}
			}
			if mem[(p+3)].int != 0 {
				{
					print_(313)
					printglue(mem[(p+3)].int, mem[p].hh.b1, s)
				}
			}
		}
	}
}

/* procedure: printfamandchar */
func printfamandchar(p int) {
	printesc(464)
	printint(mem[p].hh.b0)
	printchar(32)
	print_((mem[p].hh.b1 - 0))
}

/* procedure: printdelimiter */
func printdelimiter(p int) {
	var (
		a int
	)
	a = (((mem[p].qqqq.b0 * 256) + mem[p].qqqq.b1) - 0)
	a = ((((a * 4096) + (mem[p].qqqq.b2 * 256)) + mem[p].qqqq.b3) - 0)
	if a < 0 {
		printint(a)
	} else {
		printhex(a)
	}
}

/* procedure: printsubsidiarydata */
func printsubsidiarydata(p int, c byte) {
	if (poolptr - strstart[strptr]) >= depththreshold {
		{
			if mem[p].hh.rh != 0 {
				print_(314)
			}
		}
	} else {
		{
			{
				strpool[poolptr] = c
				poolptr = (poolptr + 1)
			}
			tempptr = p
			switch mem[p].hh.rh {
			case 1:
				{
					println_()
					printcurrentstring()
					printfamandchar(p)
				}
			case 2:
				showinfo()
			case 3:
				if mem[p].hh.lh == 0 {
					{
						println_()
						printcurrentstring()
						print_(860)
					}
				} else {
					showinfo()
				}
			default:
				// empty
			}
			poolptr = (poolptr - 1)
		}
	}
}

/* procedure: printstyle */
func printstyle(c int) {
	switch c / 2 {
	case 0:
		printesc(861)
	case 1:
		printesc(862)
	case 2:
		printesc(863)
	case 3:
		printesc(864)
	default:
		print_(865)
	}
}

/* procedure: printskipparam */
func printskipparam(n int) {
	switch n {
	case 0:
		printesc(376)
	case 1:
		printesc(377)
	case 2:
		printesc(378)
	case 3:
		printesc(379)
	case 4:
		printesc(380)
	case 5:
		printesc(381)
	case 6:
		printesc(382)
	case 7:
		printesc(383)
	case 8:
		printesc(384)
	case 9:
		printesc(385)
	case 10:
		printesc(386)
	case 11:
		printesc(387)
	case 12:
		printesc(388)
	case 13:
		printesc(389)
	case 14:
		printesc(390)
	case 15:
		printesc(391)
	case 16:
		printesc(392)
	case 17:
		printesc(393)
	default:
		print_(394)
	}
}

/* procedure: shownodelist */
func shownodelist(p int) {
	var (
		n int
		g float64
	)
	if (poolptr - strstart[strptr]) > depththreshold {
		{
			if p > 0 {
				print_(314)
			}
			goto L10
		}
	}
	n = 0
	for p > memmin {
		{
			println_()
			printcurrentstring()
			if p > memend {
				{
					print_(315)
					goto L10
				}
			}
			n = (n + 1)
			if n > breadthmax {
				{
					print_(316)
					goto L10
				}
			}
			if p >= himemmin {
				printfontandchar(p)
			} else {
				switch mem[p].hh.b0 {
				case 0:
					{
						if mem[p].hh.b0 == 0 {
							printesc(104)
						} else {
							if mem[p].hh.b0 == 1 {
								printesc(118)
							} else {
								printesc(318)
							}
						}
						print_(319)
						printscaled(mem[(p + 3)].int)
						printchar(43)
						printscaled(mem[(p + 2)].int)
						print_(320)
						printscaled(mem[(p + 1)].int)
						if mem[p].hh.b0 == 13 {
							{
								if mem[p].hh.b1 != 0 {
									{
										print_(286)
										printint((mem[p].hh.b1 + 1))
										print_(322)
									}
								}
								if mem[(p+6)].int != 0 {
									{
										print_(323)
										printglue(mem[(p+6)].int, mem[(p+5)].hh.b1, 0)
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(324)
										printglue(mem[(p+4)].int, mem[(p+5)].hh.b0, 0)
									}
								}
							}
						} else {
							{
								g = mem[(p + 6)].gr
								if (g != 0) && (mem[(p+5)].hh.b0 != 0) {
									{
										print_(325)
										if mem[(p+5)].hh.b0 == 2 {
											print_(326)
										}
										if abs_(mem[(p+6)].int) < 1048576 {
											print_(327)
										} else {
											if abs_(g) > 20000 {
												{
													if g > 0 {
														printchar(62)
													} else {
														print_(328)
													}
													printglue((20000 * 65536), mem[(p+5)].hh.b1, 0)
												}
											} else {
												printglue(round_((65536 * g)), mem[(p+5)].hh.b1, 0)
											}
										}
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(321)
										printscaled(mem[(p + 4)].int)
									}
								}
							}
						}
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 5)].hh.rh)
							poolptr = (poolptr - 1)
						}
					}
				case 1:
					{
						if mem[p].hh.b0 == 0 {
							printesc(104)
						} else {
							if mem[p].hh.b0 == 1 {
								printesc(118)
							} else {
								printesc(318)
							}
						}
						print_(319)
						printscaled(mem[(p + 3)].int)
						printchar(43)
						printscaled(mem[(p + 2)].int)
						print_(320)
						printscaled(mem[(p + 1)].int)
						if mem[p].hh.b0 == 13 {
							{
								if mem[p].hh.b1 != 0 {
									{
										print_(286)
										printint((mem[p].hh.b1 + 1))
										print_(322)
									}
								}
								if mem[(p+6)].int != 0 {
									{
										print_(323)
										printglue(mem[(p+6)].int, mem[(p+5)].hh.b1, 0)
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(324)
										printglue(mem[(p+4)].int, mem[(p+5)].hh.b0, 0)
									}
								}
							}
						} else {
							{
								g = mem[(p + 6)].gr
								if (g != 0) && (mem[(p+5)].hh.b0 != 0) {
									{
										print_(325)
										if mem[(p+5)].hh.b0 == 2 {
											print_(326)
										}
										if abs_(mem[(p+6)].int) < 1048576 {
											print_(327)
										} else {
											if abs_(g) > 20000 {
												{
													if g > 0 {
														printchar(62)
													} else {
														print_(328)
													}
													printglue((20000 * 65536), mem[(p+5)].hh.b1, 0)
												}
											} else {
												printglue(round_((65536 * g)), mem[(p+5)].hh.b1, 0)
											}
										}
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(321)
										printscaled(mem[(p + 4)].int)
									}
								}
							}
						}
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 5)].hh.rh)
							poolptr = (poolptr - 1)
						}
					}
				case 13:
					{
						if mem[p].hh.b0 == 0 {
							printesc(104)
						} else {
							if mem[p].hh.b0 == 1 {
								printesc(118)
							} else {
								printesc(318)
							}
						}
						print_(319)
						printscaled(mem[(p + 3)].int)
						printchar(43)
						printscaled(mem[(p + 2)].int)
						print_(320)
						printscaled(mem[(p + 1)].int)
						if mem[p].hh.b0 == 13 {
							{
								if mem[p].hh.b1 != 0 {
									{
										print_(286)
										printint((mem[p].hh.b1 + 1))
										print_(322)
									}
								}
								if mem[(p+6)].int != 0 {
									{
										print_(323)
										printglue(mem[(p+6)].int, mem[(p+5)].hh.b1, 0)
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(324)
										printglue(mem[(p+4)].int, mem[(p+5)].hh.b0, 0)
									}
								}
							}
						} else {
							{
								g = mem[(p + 6)].gr
								if (g != 0) && (mem[(p+5)].hh.b0 != 0) {
									{
										print_(325)
										if mem[(p+5)].hh.b0 == 2 {
											print_(326)
										}
										if abs_(mem[(p+6)].int) < 1048576 {
											print_(327)
										} else {
											if abs_(g) > 20000 {
												{
													if g > 0 {
														printchar(62)
													} else {
														print_(328)
													}
													printglue((20000 * 65536), mem[(p+5)].hh.b1, 0)
												}
											} else {
												printglue(round_((65536 * g)), mem[(p+5)].hh.b1, 0)
											}
										}
									}
								}
								if mem[(p+4)].int != 0 {
									{
										print_(321)
										printscaled(mem[(p + 4)].int)
									}
								}
							}
						}
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 5)].hh.rh)
							poolptr = (poolptr - 1)
						}
					}
				case 2:
					{
						printesc(329)
						printruledimen(mem[(p + 3)].int)
						printchar(43)
						printruledimen(mem[(p + 2)].int)
						print_(320)
						printruledimen(mem[(p + 1)].int)
					}
				case 3:
					{
						printesc(330)
						printint((mem[p].hh.b1 - 0))
						print_(331)
						printscaled(mem[(p + 3)].int)
						print_(332)
						printspec(mem[(p+4)].hh.rh, 0)
						printchar(44)
						printscaled(mem[(p + 2)].int)
						print_(333)
						printint(mem[(p + 1)].int)
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 4)].hh.lh)
							poolptr = (poolptr - 1)
						}
					}
				case 8:
					switch mem[p].hh.b1 {
					case 0:
						{
							printwritewhatsit(1285, p)
							printchar(61)
							printfilename(mem[(p+1)].hh.rh, mem[(p+2)].hh.lh, mem[(p+2)].hh.rh)
						}
					case 1:
						{
							printwritewhatsit(594, p)
							printmark(mem[(p + 1)].hh.rh)
						}
					case 2:
						printwritewhatsit(1286, p)
					case 3:
						{
							printesc(1287)
							printmark(mem[(p + 1)].hh.rh)
						}
					case 4:
						{
							printesc(1289)
							printint(mem[(p + 1)].hh.rh)
							print_(1292)
							printint(mem[(p + 1)].hh.b0)
							printchar(44)
							printint(mem[(p + 1)].hh.b1)
							printchar(41)
						}
					default:
						print_(1293)
					}
				case 10:
					if mem[p].hh.b1 >= 100 {
						{
							printesc(338)
							if mem[p].hh.b1 == 101 {
								printchar(99)
							} else {
								if mem[p].hh.b1 == 102 {
									printchar(120)
								}
							}
							print_(339)
							printspec(mem[(p+1)].hh.lh, 0)
							{
								{
									strpool[poolptr] = 46
									poolptr = (poolptr + 1)
								}
								shownodelist(mem[(p + 1)].hh.rh)
								poolptr = (poolptr - 1)
							}
						}
					} else {
						{
							printesc(334)
							if mem[p].hh.b1 != 0 {
								{
									printchar(40)
									if mem[p].hh.b1 < 98 {
										printskipparam((mem[p].hh.b1 - 1))
									} else {
										if mem[p].hh.b1 == 98 {
											printesc(335)
										} else {
											printesc(336)
										}
									}
									printchar(41)
								}
							}
							if mem[p].hh.b1 != 98 {
								{
									printchar(32)
									if mem[p].hh.b1 < 98 {
										printspec(mem[(p+1)].hh.lh, 0)
									} else {
										printspec(mem[(p+1)].hh.lh, 337)
									}
								}
							}
						}
					}
				case 11:
					if mem[p].hh.b1 != 99 {
						{
							printesc(340)
							if mem[p].hh.b1 != 0 {
								printchar(32)
							}
							printscaled(mem[(p + 1)].int)
							if mem[p].hh.b1 == 2 {
								print_(341)
							}
						}
					} else {
						{
							printesc(342)
							printscaled(mem[(p + 1)].int)
							print_(337)
						}
					}
				case 9:
					{
						printesc(343)
						if mem[p].hh.b1 == 0 {
							print_(344)
						} else {
							print_(345)
						}
						if mem[(p+1)].int != 0 {
							{
								print_(346)
								printscaled(mem[(p + 1)].int)
							}
						}
					}
				case 6:
					{
						printfontandchar((p + 1))
						print_(347)
						if mem[p].hh.b1 > 1 {
							printchar(124)
						}
						fontinshortdisplay = mem[(p + 1)].hh.b0
						shortdisplay(mem[(p + 1)].hh.rh)
						if (mem[p].hh.b1 & 1) != 0 {
							printchar(124)
						}
						printchar(41)
					}
				case 12:
					{
						printesc(348)
						printint(mem[(p + 1)].int)
					}
				case 7:
					{
						printesc(349)
						if mem[p].hh.b1 > 0 {
							{
								print_(350)
								printint(mem[p].hh.b1)
							}
						}
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 1)].hh.lh)
							poolptr = (poolptr - 1)
						}
						{
							strpool[poolptr] = 124
							poolptr = (poolptr + 1)
						}
						shownodelist(mem[(p + 1)].hh.rh)
						poolptr = (poolptr - 1)
					}
				case 4:
					{
						printesc(351)
						printmark(mem[(p + 1)].int)
					}
				case 5:
					{
						printesc(352)
						{
							{
								strpool[poolptr] = 46
								poolptr = (poolptr + 1)
							}
							shownodelist(mem[(p + 1)].int)
							poolptr = (poolptr - 1)
						}
					}
				case 14:
					printstyle(mem[p].hh.b1)
				case 15:
					{
						printesc(525)
						{
							strpool[poolptr] = 68
							poolptr = (poolptr + 1)
						}
						shownodelist(mem[(p + 1)].hh.lh)
						poolptr = (poolptr - 1)
						{
							strpool[poolptr] = 84
							poolptr = (poolptr + 1)
						}
						shownodelist(mem[(p + 1)].hh.rh)
						poolptr = (poolptr - 1)
						{
							strpool[poolptr] = 83
							poolptr = (poolptr + 1)
						}
						shownodelist(mem[(p + 2)].hh.lh)
						poolptr = (poolptr - 1)
						{
							strpool[poolptr] = 115
							poolptr = (poolptr + 1)
						}
						shownodelist(mem[(p + 2)].hh.rh)
						poolptr = (poolptr - 1)
					}
				case 16:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 17:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 18:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 19:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 20:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 21:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 22:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 23:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 24:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 27:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 26:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 29:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 28:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 30:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 31:
					{
						switch mem[p].hh.b0 {
						case 16:
							printesc(866)
						case 17:
							printesc(867)
						case 18:
							printesc(868)
						case 19:
							printesc(869)
						case 20:
							printesc(870)
						case 21:
							printesc(871)
						case 22:
							printesc(872)
						case 23:
							printesc(873)
						case 27:
							printesc(874)
						case 26:
							printesc(875)
						case 29:
							printesc(539)
						case 24:
							{
								printesc(533)
								printdelimiter((p + 4))
							}
						case 28:
							{
								printesc(508)
								printfamandchar((p + 4))
							}
						case 30:
							{
								printesc(876)
								printdelimiter((p + 1))
							}
						case 31:
							{
								printesc(877)
								printdelimiter((p + 1))
							}
						}
						if mem[p].hh.b1 != 0 {
							if mem[p].hh.b1 == 1 {
								printesc(878)
							} else {
								printesc(879)
							}
						}
						if mem[p].hh.b0 < 30 {
							printsubsidiarydata((p + 1), 46)
						}
						printsubsidiarydata((p + 2), 94)
						printsubsidiarydata((p + 3), 95)
					}
				case 25:
					{
						printesc(880)
						if mem[(p+1)].int == 1073741824 {
							print_(881)
						} else {
							printscaled(mem[(p + 1)].int)
						}
						if (((mem[(p+4)].qqqq.b0 != 0) || (mem[(p+4)].qqqq.b1 != 0)) || (mem[(p+4)].qqqq.b2 != 0)) || (mem[(p+4)].qqqq.b3 != 0) {
							{
								print_(882)
								printdelimiter((p + 4))
							}
						}
						if (((mem[(p+5)].qqqq.b0 != 0) || (mem[(p+5)].qqqq.b1 != 0)) || (mem[(p+5)].qqqq.b2 != 0)) || (mem[(p+5)].qqqq.b3 != 0) {
							{
								print_(883)
								printdelimiter((p + 5))
							}
						}
						printsubsidiarydata((p + 2), 92)
						printsubsidiarydata((p + 3), 47)
					}
				default:
					print_(317)
				}
			}
			p = mem[p].hh.rh
		}
	}
L10:
	// empty
}

/* procedure: showbox */
func showbox(p int) {
	depththreshold = eqtb[5288].int
	breadthmax = eqtb[5287].int
	if breadthmax <= 0 {
		breadthmax = 5
	}
	if (poolptr + depththreshold) >= poolsize {
		depththreshold = ((poolsize - poolptr) - 1)
	}
	shownodelist(p)
	println_()
}

/* procedure: deletetokenref */
func deletetokenref(p int) {
	if mem[p].hh.lh == 0 {
		flushlist(p)
	} else {
		mem[p].hh.lh = (mem[p].hh.lh - 1)
	}
}

/* procedure: deleteglueref */
func deleteglueref(p int) {
	if mem[p].hh.rh == 0 {
		freenode(p, 4)
	} else {
		mem[p].hh.rh = (mem[p].hh.rh - 1)
	}
}

/* procedure: flushnodelist */
func flushnodelist(p int) {
	var (
		q int
	)
	for p != 0 {
		{
			q = mem[p].hh.rh
			if p >= himemmin {
				{
					mem[p].hh.rh = avail
					avail = p
				}
			} else {
				{
					switch mem[p].hh.b0 {
					case 0:
						{
							flushnodelist(mem[(p + 5)].hh.rh)
							freenode(p, 7)
							goto L30
						}
					case 1:
						{
							flushnodelist(mem[(p + 5)].hh.rh)
							freenode(p, 7)
							goto L30
						}
					case 13:
						{
							flushnodelist(mem[(p + 5)].hh.rh)
							freenode(p, 7)
							goto L30
						}
					case 2:
						{
							freenode(p, 4)
							goto L30
						}
					case 3:
						{
							flushnodelist(mem[(p + 4)].hh.lh)
							deleteglueref(mem[(p + 4)].hh.rh)
							freenode(p, 5)
							goto L30
						}
					case 8:
						{
							switch mem[p].hh.b1 {
							case 0:
								freenode(p, 3)
							case 1:
								{
									deletetokenref(mem[(p + 1)].hh.rh)
									freenode(p, 2)
									goto L30
								}
							case 3:
								{
									deletetokenref(mem[(p + 1)].hh.rh)
									freenode(p, 2)
									goto L30
								}
							case 2:
								freenode(p, 2)
							case 4:
								freenode(p, 2)
							default:
								confusion(1295)
							}
							goto L30
						}
					case 10:
						{
							{
								if mem[mem[(p+1)].hh.lh].hh.rh == 0 {
									freenode(mem[(p+1)].hh.lh, 4)
								} else {
									mem[mem[(p+1)].hh.lh].hh.rh = (mem[mem[(p+1)].hh.lh].hh.rh - 1)
								}
							}
							if mem[(p+1)].hh.rh != 0 {
								flushnodelist(mem[(p + 1)].hh.rh)
							}
						}
					case 11:
						// empty
					case 9:
						// empty
					case 12:
						// empty
					case 6:
						flushnodelist(mem[(p + 1)].hh.rh)
					case 4:
						deletetokenref(mem[(p + 1)].int)
					case 7:
						{
							flushnodelist(mem[(p + 1)].hh.lh)
							flushnodelist(mem[(p + 1)].hh.rh)
						}
					case 5:
						flushnodelist(mem[(p + 1)].int)
					case 14:
						{
							freenode(p, 3)
							goto L30
						}
					case 15:
						{
							flushnodelist(mem[(p + 1)].hh.lh)
							flushnodelist(mem[(p + 1)].hh.rh)
							flushnodelist(mem[(p + 2)].hh.lh)
							flushnodelist(mem[(p + 2)].hh.rh)
							freenode(p, 3)
							goto L30
						}
					case 16:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 17:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 18:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 19:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 20:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 21:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 22:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 23:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 24:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 27:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 26:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 29:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 28:
						{
							if mem[(p+1)].hh.rh >= 2 {
								flushnodelist(mem[(p + 1)].hh.lh)
							}
							if mem[(p+2)].hh.rh >= 2 {
								flushnodelist(mem[(p + 2)].hh.lh)
							}
							if mem[(p+3)].hh.rh >= 2 {
								flushnodelist(mem[(p + 3)].hh.lh)
							}
							if mem[p].hh.b0 == 24 {
								freenode(p, 5)
							} else {
								if mem[p].hh.b0 == 28 {
									freenode(p, 5)
								} else {
									freenode(p, 4)
								}
							}
							goto L30
						}
					case 30:
						{
							freenode(p, 4)
							goto L30
						}
					case 31:
						{
							freenode(p, 4)
							goto L30
						}
					case 25:
						{
							flushnodelist(mem[(p + 2)].hh.lh)
							flushnodelist(mem[(p + 3)].hh.lh)
							freenode(p, 6)
							goto L30
						}
					default:
						confusion(353)
					}
					freenode(p, 2)
				L30:
					// empty
				}
			}
			p = q
		}
	}
}

/* function: copynodelist */
func copynodelist(p int) int {
	var (
		h     int
		q     int
		r     int
		words int
	)
	h = getavail
	q = h
	for p != 0 {
		{
			words = 1
			if p >= himemmin {
				r = getavail
			} else {
				switch mem[p].hh.b0 {
				case 0:
					{
						r = getnode(7)
						mem[(r + 6)] = mem[(p + 6)]
						mem[(r + 5)] = mem[(p + 5)]
						mem[(r + 5)].hh.rh = copynodelist(mem[(p + 5)].hh.rh)
						words = 5
					}
				case 1:
					{
						r = getnode(7)
						mem[(r + 6)] = mem[(p + 6)]
						mem[(r + 5)] = mem[(p + 5)]
						mem[(r + 5)].hh.rh = copynodelist(mem[(p + 5)].hh.rh)
						words = 5
					}
				case 13:
					{
						r = getnode(7)
						mem[(r + 6)] = mem[(p + 6)]
						mem[(r + 5)] = mem[(p + 5)]
						mem[(r + 5)].hh.rh = copynodelist(mem[(p + 5)].hh.rh)
						words = 5
					}
				case 2:
					{
						r = getnode(4)
						words = 4
					}
				case 3:
					{
						r = getnode(5)
						mem[(r + 4)] = mem[(p + 4)]
						mem[mem[(p+4)].hh.rh].hh.rh = (mem[mem[(p+4)].hh.rh].hh.rh + 1)
						mem[(r + 4)].hh.lh = copynodelist(mem[(p + 4)].hh.lh)
						words = 4
					}
				case 8:
					switch mem[p].hh.b1 {
					case 0:
						{
							r = getnode(3)
							words = 3
						}
					case 1:
						{
							r = getnode(2)
							mem[mem[(p+1)].hh.rh].hh.lh = (mem[mem[(p+1)].hh.rh].hh.lh + 1)
							words = 2
						}
					case 3:
						{
							r = getnode(2)
							mem[mem[(p+1)].hh.rh].hh.lh = (mem[mem[(p+1)].hh.rh].hh.lh + 1)
							words = 2
						}
					case 2:
						{
							r = getnode(2)
							words = 2
						}
					case 4:
						{
							r = getnode(2)
							words = 2
						}
					default:
						confusion(1294)
					}
				case 10:
					{
						r = getnode(2)
						mem[mem[(p+1)].hh.lh].hh.rh = (mem[mem[(p+1)].hh.lh].hh.rh + 1)
						mem[(r + 1)].hh.lh = mem[(p + 1)].hh.lh
						mem[(r + 1)].hh.rh = copynodelist(mem[(p + 1)].hh.rh)
					}
				case 11:
					{
						r = getnode(2)
						words = 2
					}
				case 9:
					{
						r = getnode(2)
						words = 2
					}
				case 12:
					{
						r = getnode(2)
						words = 2
					}
				case 6:
					{
						r = getnode(2)
						mem[(r + 1)] = mem[(p + 1)]
						mem[(r + 1)].hh.rh = copynodelist(mem[(p + 1)].hh.rh)
					}
				case 7:
					{
						r = getnode(2)
						mem[(r + 1)].hh.lh = copynodelist(mem[(p + 1)].hh.lh)
						mem[(r + 1)].hh.rh = copynodelist(mem[(p + 1)].hh.rh)
					}
				case 4:
					{
						r = getnode(2)
						mem[mem[(p+1)].int].hh.lh = (mem[mem[(p+1)].int].hh.lh + 1)
						words = 2
					}
				case 5:
					{
						r = getnode(2)
						mem[(r + 1)].int = copynodelist(mem[(p + 1)].int)
					}
				default:
					confusion(354)
				}
			}
			for words > 0 {
				{
					words = (words - 1)
					mem[(r + words)] = mem[(p + words)]
				}
			}
			mem[q].hh.rh = r
			q = r
			p = mem[p].hh.rh
		}
	}
	mem[q].hh.rh = 0
	q = mem[h].hh.rh
	{
		mem[h].hh.rh = avail
		avail = h
	}
	copynodelist = q
}

/* procedure: printmode */
func printmode(m int) {
	if m > 0 {
		switch m / 101 {
		case 0:
			print_(355)
		case 1:
			print_(356)
		case 2:
			print_(357)
		}
	} else {
		if m == 0 {
			print_(358)
		} else {
			switch (-m) / 101 {
			case 0:
				print_(359)
			case 1:
				print_(360)
			case 2:
				print_(343)
			}
		}
	}
	print_(361)
}

/* procedure: pushnest */
func pushnest() {
	if nestptr > maxneststack {
		{
			maxneststack = nestptr
			if nestptr == nestsize {
				overflow(362, nestsize)
			}
		}
	}
	nest[nestptr] = curlist
	nestptr = (nestptr + 1)
	curlist.headfield = getavail
	curlist.tailfield = curlist.headfield
	curlist.pgfield = 0
	curlist.mlfield = line
}

/* procedure: popnest */
func popnest() {
	{
		mem[curlist.headfield].hh.rh = avail
		avail = curlist.headfield
	}
	nestptr = (nestptr - 1)
	curlist = nest[nestptr]
}

/* procedure: showactivities */
func showactivities() {
	var (
		p int
		m int
		a *memoryword_t
		q int
		r int
		t int
	)
	nest[nestptr] = curlist
	printnl(338)
	println_()
	for p := nestptr; p >= 0; p-- {
		{
			m = nest[p].modefield
			a = nest[p].auxfield
			printnl(363)
			printmode(m)
			print_(364)
			printint(abs_(nest[p].mlfield))
			if m == 102 {
				if nest[p].pgfield != 8585216 {
					{
						print_(365)
						printint((nest[p].pgfield % 65536))
						print_(366)
						printint((nest[p].pgfield / 4194304))
						printchar(44)
						printint(((nest[p].pgfield / 65536) % 64))
						printchar(41)
					}
				}
			}
			if nest[p].mlfield < 0 {
				print_(367)
			}
			if p == 0 {
				{
					if 29998 != pagetail {
						{
							printnl(980)
							if outputactive {
								print_(981)
							}
							showbox(mem[29998].hh.rh)
							if pagecontents > 0 {
								{
									printnl(982)
									printtotals()
									printnl(983)
									printscaled(pagesofar[0])
									r = mem[30000].hh.rh
									for r != 30000 {
										{
											println_()
											printesc(330)
											t = (mem[r].hh.b1 - 0)
											printint(t)
											print_(984)
											if eqtb[(5318+t)].int == 1000 {
												t = mem[(r + 3)].int
											} else {
												t = (xovern(mem[(r+3)].int, 1000) * eqtb[(5318+t)].int)
											}
											printscaled(t)
											if mem[r].hh.b0 == 1 {
												{
													q = 29998
													t = 0
													for {
														q = mem[q].hh.rh
														if (mem[q].hh.b0 == 3) && (mem[q].hh.b1 == mem[r].hh.b1) {
															t = (t + 1)
														}
														if !(q == mem[(r+1)].hh.lh) {
															break
														}
													}
													print_(985)
													printint(t)
													print_(986)
												}
											}
											r = mem[r].hh.rh
										}
									}
								}
							}
						}
					}
					if mem[29999].hh.rh != 0 {
						printnl(368)
					}
				}
			}
			showbox(mem[nest[p].headfield].hh.rh)
			switch abs_(m) / 101 {
			case 0:
				{
					printnl(369)
					if a.int <= (-65536000) {
						print_(370)
					} else {
						printscaled(a.int)
					}
					if nest[p].pgfield != 0 {
						{
							print_(371)
							printint(nest[p].pgfield)
							print_(372)
							if nest[p].pgfield != 1 {
								printchar(115)
							}
						}
					}
				}
			case 1:
				{
					printnl(373)
					printint(a.hh.lh)
					if m > 0 {
						if a.hh.rh > 0 {
							{
								print_(374)
								printint(a.hh.rh)
							}
						}
					}
				}
			case 2:
				if a.int != 0 {
					{
						print_(375)
						showbox(a.int)
					}
				}
			}
		}
	}
}

/* procedure: printparam */
func printparam(n int) {
	switch n {
	case 0:
		printesc(420)
	case 1:
		printesc(421)
	case 2:
		printesc(422)
	case 3:
		printesc(423)
	case 4:
		printesc(424)
	case 5:
		printesc(425)
	case 6:
		printesc(426)
	case 7:
		printesc(427)
	case 8:
		printesc(428)
	case 9:
		printesc(429)
	case 10:
		printesc(430)
	case 11:
		printesc(431)
	case 12:
		printesc(432)
	case 13:
		printesc(433)
	case 14:
		printesc(434)
	case 15:
		printesc(435)
	case 16:
		printesc(436)
	case 17:
		printesc(437)
	case 18:
		printesc(438)
	case 19:
		printesc(439)
	case 20:
		printesc(440)
	case 21:
		printesc(441)
	case 22:
		printesc(442)
	case 23:
		printesc(443)
	case 24:
		printesc(444)
	case 25:
		printesc(445)
	case 26:
		printesc(446)
	case 27:
		printesc(447)
	case 28:
		printesc(448)
	case 29:
		printesc(449)
	case 30:
		printesc(450)
	case 31:
		printesc(451)
	case 32:
		printesc(452)
	case 33:
		printesc(453)
	case 34:
		printesc(454)
	case 35:
		printesc(455)
	case 36:
		printesc(456)
	case 37:
		printesc(457)
	case 38:
		printesc(458)
	case 39:
		printesc(459)
	case 40:
		printesc(460)
	case 41:
		printesc(461)
	case 42:
		printesc(462)
	case 43:
		printesc(463)
	case 44:
		printesc(464)
	case 45:
		printesc(465)
	case 46:
		printesc(466)
	case 47:
		printesc(467)
	case 48:
		printesc(468)
	case 49:
		printesc(469)
	case 50:
		printesc(470)
	case 51:
		printesc(471)
	case 52:
		printesc(472)
	case 53:
		printesc(473)
	case 54:
		printesc(474)
	default:
		print_(475)
	}
}

/* procedure: fixdateandtime */
func fixdateandtime() {
	systime = (12 * 60)
	sysday = 4
	sysmonth = 7
	sysyear = 1776
	eqtb[5283].int = systime
	eqtb[5284].int = sysday
	eqtb[5285].int = sysmonth
	eqtb[5286].int = sysyear
}

/* procedure: begindiagnostic */
func begindiagnostic() {
	oldsetting = selector
	if (eqtb[5292].int <= 0) && (selector == 19) {
		{
			selector = (selector - 1)
			if history == 0 {
				history = 1
			}
		}
	}
}

/* procedure: enddiagnostic */
func enddiagnostic(blankline bool) {
	printnl(338)
	if blankline {
		println_()
	}
	selector = oldsetting
}

/* procedure: printlengthparam */
func printlengthparam(n int) {
	switch n {
	case 0:
		printesc(478)
	case 1:
		printesc(479)
	case 2:
		printesc(480)
	case 3:
		printesc(481)
	case 4:
		printesc(482)
	case 5:
		printesc(483)
	case 6:
		printesc(484)
	case 7:
		printesc(485)
	case 8:
		printesc(486)
	case 9:
		printesc(487)
	case 10:
		printesc(488)
	case 11:
		printesc(489)
	case 12:
		printesc(490)
	case 13:
		printesc(491)
	case 14:
		printesc(492)
	case 15:
		printesc(493)
	case 16:
		printesc(494)
	case 17:
		printesc(495)
	case 18:
		printesc(496)
	case 19:
		printesc(497)
	case 20:
		printesc(498)
	default:
		print_(499)
	}
}

/* procedure: printcmdchr */
func printcmdchr(cmd int, chrcode int) {
	switch cmd {
	case 1:
		{
			print_(557)
			print_(chrcode)
		}
	case 2:
		{
			print_(558)
			print_(chrcode)
		}
	case 3:
		{
			print_(559)
			print_(chrcode)
		}
	case 6:
		{
			print_(560)
			print_(chrcode)
		}
	case 7:
		{
			print_(561)
			print_(chrcode)
		}
	case 8:
		{
			print_(562)
			print_(chrcode)
		}
	case 9:
		print_(563)
	case 10:
		{
			print_(564)
			print_(chrcode)
		}
	case 11:
		{
			print_(565)
			print_(chrcode)
		}
	case 12:
		{
			print_(566)
			print_(chrcode)
		}
	case 75:
		if chrcode < 2900 {
			printskipparam((chrcode - 2882))
		} else {
			if chrcode < 3156 {
				{
					printesc(395)
					printint((chrcode - 2900))
				}
			} else {
				{
					printesc(396)
					printint((chrcode - 3156))
				}
			}
		}
	case 76:
		if chrcode < 2900 {
			printskipparam((chrcode - 2882))
		} else {
			if chrcode < 3156 {
				{
					printesc(395)
					printint((chrcode - 2900))
				}
			} else {
				{
					printesc(396)
					printint((chrcode - 3156))
				}
			}
		}
	case 72:
		if chrcode >= 3422 {
			{
				printesc(407)
				printint((chrcode - 3422))
			}
		} else {
			switch chrcode {
			case 3413:
				printesc(398)
			case 3414:
				printesc(399)
			case 3415:
				printesc(400)
			case 3416:
				printesc(401)
			case 3417:
				printesc(402)
			case 3418:
				printesc(403)
			case 3419:
				printesc(404)
			case 3420:
				printesc(405)
			default:
				printesc(406)
			}
		}
	case 73:
		if chrcode < 5318 {
			printparam((chrcode - 5263))
		} else {
			{
				printesc(476)
				printint((chrcode - 5318))
			}
		}
	case 74:
		if chrcode < 5851 {
			printlengthparam((chrcode - 5830))
		} else {
			{
				printesc(500)
				printint((chrcode - 5851))
			}
		}
	case 45:
		printesc(508)
	case 90:
		printesc(509)
	case 40:
		printesc(510)
	case 41:
		printesc(511)
	case 77:
		printesc(519)
	case 61:
		printesc(512)
	case 42:
		printesc(531)
	case 16:
		printesc(513)
	case 107:
		printesc(504)
	case 88:
		printesc(518)
	case 15:
		printesc(514)
	case 92:
		printesc(515)
	case 67:
		printesc(505)
	case 62:
		printesc(516)
	case 64:
		printesc(32)
	case 102:
		printesc(517)
	case 32:
		printesc(520)
	case 36:
		printesc(521)
	case 39:
		printesc(522)
	case 37:
		printesc(330)
	case 44:
		printesc(47)
	case 18:
		printesc(351)
	case 46:
		printesc(523)
	case 17:
		printesc(524)
	case 54:
		printesc(525)
	case 91:
		printesc(526)
	case 34:
		printesc(527)
	case 65:
		printesc(528)
	case 103:
		printesc(529)
	case 55:
		printesc(335)
	case 63:
		printesc(530)
	case 66:
		printesc(533)
	case 96:
		printesc(534)
	case 0:
		printesc(535)
	case 98:
		printesc(536)
	case 80:
		printesc(532)
	case 84:
		printesc(408)
	case 109:
		printesc(537)
	case 71:
		printesc(407)
	case 38:
		printesc(352)
	case 33:
		printesc(538)
	case 56:
		printesc(539)
	case 35:
		printesc(540)
	case 13:
		printesc(597)
	case 104:
		if chrcode == 0 {
			printesc(629)
		} else {
			printesc(630)
		}
	case 110:
		switch chrcode {
		case 1:
			printesc(632)
		case 2:
			printesc(633)
		case 3:
			printesc(634)
		case 4:
			printesc(635)
		default:
			printesc(631)
		}
	case 89:
		if chrcode == 0 {
			printesc(476)
		} else {
			if chrcode == 1 {
				printesc(500)
			} else {
				if chrcode == 2 {
					printesc(395)
				} else {
					printesc(396)
				}
			}
		}
	case 79:
		if chrcode == 1 {
			printesc(669)
		} else {
			printesc(668)
		}
	case 82:
		if chrcode == 0 {
			printesc(670)
		} else {
			printesc(671)
		}
	case 83:
		if chrcode == 1 {
			printesc(672)
		} else {
			if chrcode == 3 {
				printesc(673)
			} else {
				printesc(674)
			}
		}
	case 70:
		switch chrcode {
		case 0:
			printesc(675)
		case 1:
			printesc(676)
		case 2:
			printesc(677)
		case 3:
			printesc(678)
		default:
			printesc(679)
		}
	case 108:
		switch chrcode {
		case 0:
			printesc(735)
		case 1:
			printesc(736)
		case 2:
			printesc(737)
		case 3:
			printesc(738)
		case 4:
			printesc(739)
		default:
			printesc(740)
		}
	case 105:
		switch chrcode {
		case 1:
			printesc(758)
		case 2:
			printesc(759)
		case 3:
			printesc(760)
		case 4:
			printesc(761)
		case 5:
			printesc(762)
		case 6:
			printesc(763)
		case 7:
			printesc(764)
		case 8:
			printesc(765)
		case 9:
			printesc(766)
		case 10:
			printesc(767)
		case 11:
			printesc(768)
		case 12:
			printesc(769)
		case 13:
			printesc(770)
		case 14:
			printesc(771)
		case 15:
			printesc(772)
		case 16:
			printesc(773)
		default:
			printesc(757)
		}
	case 106:
		if chrcode == 2 {
			printesc(774)
		} else {
			if chrcode == 4 {
				printesc(775)
			} else {
				printesc(776)
			}
		}
	case 4:
		if chrcode == 256 {
			printesc(898)
		} else {
			{
				print_(902)
				print_(chrcode)
			}
		}
	case 5:
		if chrcode == 257 {
			printesc(899)
		} else {
			printesc(900)
		}
	case 81:
		switch chrcode {
		case 0:
			printesc(970)
		case 1:
			printesc(971)
		case 2:
			printesc(972)
		case 3:
			printesc(973)
		case 4:
			printesc(974)
		case 5:
			printesc(975)
		case 6:
			printesc(976)
		default:
			printesc(977)
		}
	case 14:
		if chrcode == 1 {
			printesc(1026)
		} else {
			printesc(1025)
		}
	case 26:
		switch chrcode {
		case 4:
			printesc(1027)
		case 0:
			printesc(1028)
		case 1:
			printesc(1029)
		case 2:
			printesc(1030)
		default:
			printesc(1031)
		}
	case 27:
		switch chrcode {
		case 4:
			printesc(1032)
		case 0:
			printesc(1033)
		case 1:
			printesc(1034)
		case 2:
			printesc(1035)
		default:
			printesc(1036)
		}
	case 28:
		printesc(336)
	case 29:
		printesc(340)
	case 30:
		printesc(342)
	case 21:
		if chrcode == 1 {
			printesc(1054)
		} else {
			printesc(1055)
		}
	case 22:
		if chrcode == 1 {
			printesc(1056)
		} else {
			printesc(1057)
		}
	case 20:
		switch chrcode {
		case 0:
			printesc(409)
		case 1:
			printesc(1058)
		case 2:
			printesc(1059)
		case 3:
			printesc(965)
		case 4:
			printesc(1060)
		case 5:
			printesc(967)
		default:
			printesc(1061)
		}
	case 31:
		if chrcode == 100 {
			printesc(1063)
		} else {
			if chrcode == 101 {
				printesc(1064)
			} else {
				if chrcode == 102 {
					printesc(1065)
				} else {
					printesc(1062)
				}
			}
		}
	case 43:
		if chrcode == 0 {
			printesc(1081)
		} else {
			printesc(1080)
		}
	case 25:
		if chrcode == 10 {
			printesc(1092)
		} else {
			if chrcode == 11 {
				printesc(1091)
			} else {
				printesc(1090)
			}
		}
	case 23:
		if chrcode == 1 {
			printesc(1094)
		} else {
			printesc(1093)
		}
	case 24:
		if chrcode == 1 {
			printesc(1096)
		} else {
			printesc(1095)
		}
	case 47:
		if chrcode == 1 {
			printesc(45)
		} else {
			printesc(349)
		}
	case 48:
		if chrcode == 1 {
			printesc(1128)
		} else {
			printesc(1127)
		}
	case 50:
		switch chrcode {
		case 16:
			printesc(866)
		case 17:
			printesc(867)
		case 18:
			printesc(868)
		case 19:
			printesc(869)
		case 20:
			printesc(870)
		case 21:
			printesc(871)
		case 22:
			printesc(872)
		case 23:
			printesc(873)
		case 26:
			printesc(875)
		default:
			printesc(874)
		}
	case 51:
		if chrcode == 1 {
			printesc(878)
		} else {
			if chrcode == 2 {
				printesc(879)
			} else {
				printesc(1129)
			}
		}
	case 53:
		printstyle(chrcode)
	case 52:
		switch chrcode {
		case 1:
			printesc(1148)
		case 2:
			printesc(1149)
		case 3:
			printesc(1150)
		case 4:
			printesc(1151)
		case 5:
			printesc(1152)
		default:
			printesc(1147)
		}
	case 49:
		if chrcode == 30 {
			printesc(876)
		} else {
			printesc(877)
		}
	case 93:
		if chrcode == 1 {
			printesc(1171)
		} else {
			if chrcode == 2 {
				printesc(1172)
			} else {
				printesc(1173)
			}
		}
	case 97:
		if chrcode == 0 {
			printesc(1174)
		} else {
			if chrcode == 1 {
				printesc(1175)
			} else {
				if chrcode == 2 {
					printesc(1176)
				} else {
					printesc(1177)
				}
			}
		}
	case 94:
		if chrcode != 0 {
			printesc(1192)
		} else {
			printesc(1191)
		}
	case 95:
		switch chrcode {
		case 0:
			printesc(1193)
		case 1:
			printesc(1194)
		case 2:
			printesc(1195)
		case 3:
			printesc(1196)
		case 4:
			printesc(1197)
		case 5:
			printesc(1198)
		default:
			printesc(1199)
		}
	case 68:
		{
			printesc(513)
			printhex(chrcode)
		}
	case 69:
		{
			printesc(524)
			printhex(chrcode)
		}
	case 85:
		if chrcode == 3983 {
			printesc(415)
		} else {
			if chrcode == 5007 {
				printesc(419)
			} else {
				if chrcode == 4239 {
					printesc(416)
				} else {
					if chrcode == 4495 {
						printesc(417)
					} else {
						if chrcode == 4751 {
							printesc(418)
						} else {
							printesc(477)
						}
					}
				}
			}
		}
	case 86:
		printsize((chrcode - 3935))
	case 99:
		if chrcode == 1 {
			printesc(953)
		} else {
			printesc(941)
		}
	case 78:
		if chrcode == 0 {
			printesc(1217)
		} else {
			printesc(1218)
		}
	case 87:
		{
			print_(1226)
			slowprint(fontname[chrcode])
			if fontsize[chrcode] != fontdsize[chrcode] {
				{
					print_(741)
					printscaled(fontsize[chrcode])
					print_(397)
				}
			}
		}
	case 100:
		switch chrcode {
		case 0:
			printesc(274)
		case 1:
			printesc(275)
		case 2:
			printesc(276)
		default:
			printesc(1227)
		}
	case 60:
		if chrcode == 0 {
			printesc(1229)
		} else {
			printesc(1228)
		}
	case 58:
		if chrcode == 0 {
			printesc(1230)
		} else {
			printesc(1231)
		}
	case 57:
		if chrcode == 4239 {
			printesc(1237)
		} else {
			printesc(1238)
		}
	case 19:
		switch chrcode {
		case 1:
			printesc(1240)
		case 2:
			printesc(1241)
		case 3:
			printesc(1242)
		default:
			printesc(1239)
		}
	case 101:
		print_(1249)
	case 111:
		print_(1250)
	case 112:
		printesc(1251)
	case 113:
		printesc(1252)
	case 114:
		{
			printesc(1171)
			printesc(1252)
		}
	case 115:
		printesc(1253)
	case 59:
		switch chrcode {
		case 0:
			printesc(1285)
		case 1:
			printesc(594)
		case 2:
			printesc(1286)
		case 3:
			printesc(1287)
		case 4:
			printesc(1288)
		case 5:
			printesc(1289)
		default:
			print_(1290)
		}
	default:
		print_(567)
	}
}

/* function: idlookup */
func idlookup(j int, l int) int {
	var (
		h int
		d int
		p int
		k int
	)
	h = buffer[j]
	for k := (j + 1); k <= ((j + l) - 1); k++ {
		{
			h = ((h + h) + buffer[k])
			for h >= 1777 {
				h = (h - 1777)
			}
		}
	}
	p = (h + 514)
	for true {
		{
			if hash[p].rh > 0 {
				if (strstart[(hash[p].rh+1)] - strstart[hash[p].rh]) == l {
					if streqbuf(hash[p].rh, j) {
						goto L40
					}
				}
			}
			if hash[p].lh == 0 {
				{
					if nonewcontrolsequence {
						p = 2881
					} else {
						{
							if hash[p].rh > 0 {
								{
									for {
										if hashused == 514 {
											overflow(503, 2100)
										}
										hashused = (hashused - 1)
										if !(hash[hashused].rh == 0) {
											break
										}
									}
									hash[p].lh = hashused
									p = hashused
								}
							}
							{
								if (poolptr + l) > poolsize {
									overflow(257, (poolsize - initpoolptr))
								}
							}
							d = (poolptr - strstart[strptr])
							for poolptr > strstart[strptr] {
								{
									poolptr = (poolptr - 1)
									strpool[(poolptr + l)] = strpool[poolptr]
								}
							}
							for k := j; k <= ((j + l) - 1); k++ {
								{
									strpool[poolptr] = buffer[k]
									poolptr = (poolptr + 1)
								}
							}
							hash[p].rh = makestring
							poolptr = (poolptr + d)
						}
					}
					goto L40
				}
			}
			p = hash[p].lh
		}
	}
L40:
	idlookup = p
}

/* procedure: primitive */
func primitive(s int, c int, o int) {
	var (
		k int
		j int
		l int
	)
	if s < 256 {
		curval = (s + 257)
	} else {
		{
			k = strstart[s]
			l = (strstart[(s+1)] - k)
			for j := 0; j <= (l - 1); j++ {
				buffer[j] = strpool[(k + j)]
			}
			curval = idlookup(0, l)
			{
				strptr = (strptr - 1)
				poolptr = strstart[strptr]
			}
			hash[curval].rh = s
		}
	}
	eqtb[curval].hh.b1 = 1
	eqtb[curval].hh.b0 = c
	eqtb[curval].hh.rh = o
}

/* procedure: newsavelevel */
func newsavelevel(c int) {
	if saveptr > maxsavestack {
		{
			maxsavestack = saveptr
			if maxsavestack > (savesize - 6) {
				overflow(541, savesize)
			}
		}
	}
	savestack[saveptr].hh.b0 = 3
	savestack[saveptr].hh.b1 = curgroup
	savestack[saveptr].hh.rh = curboundary
	if curlevel == 255 {
		overflow(542, 255)
	}
	curboundary = saveptr
	curlevel = (curlevel + 1)
	saveptr = (saveptr + 1)
	curgroup = c
}

/* procedure: eqdestroy */
func eqdestroy(w *memoryword_t) {
	var (
		q int
	)
	switch w.hh.b0 {
	case 111:
		deletetokenref(w.hh.rh)
	case 112:
		deletetokenref(w.hh.rh)
	case 113:
		deletetokenref(w.hh.rh)
	case 114:
		deletetokenref(w.hh.rh)
	case 117:
		deleteglueref(w.hh.rh)
	case 118:
		{
			q = w.hh.rh
			if q != 0 {
				freenode(q, ((mem[q].hh.lh + mem[q].hh.lh) + 1))
			}
		}
	case 119:
		flushnodelist(w.hh.rh)
	default:
		// empty
	}
}

/* procedure: eqsave */
func eqsave(p int, l int) {
	if saveptr > maxsavestack {
		{
			maxsavestack = saveptr
			if maxsavestack > (savesize - 6) {
				overflow(541, savesize)
			}
		}
	}
	if l == 0 {
		savestack[saveptr].hh.b0 = 1
	} else {
		{
			savestack[saveptr] = eqtb[p]
			saveptr = (saveptr + 1)
			savestack[saveptr].hh.b0 = 0
		}
	}
	savestack[saveptr].hh.b1 = l
	savestack[saveptr].hh.rh = p
	saveptr = (saveptr + 1)
}

/* procedure: eqdefine */
func eqdefine(p int, t int, e int) {
	if eqtb[p].hh.b1 == curlevel {
		eqdestroy(eqtb[p])
	} else {
		if curlevel > 1 {
			eqsave(p, eqtb[p].hh.b1)
		}
	}
	eqtb[p].hh.b1 = curlevel
	eqtb[p].hh.b0 = t
	eqtb[p].hh.rh = e
}

/* procedure: eqworddefine */
func eqworddefine(p int, w int) {
	if xeqlevel[p] != curlevel {
		{
			eqsave(p, xeqlevel[p])
			xeqlevel[p] = curlevel
		}
	}
	eqtb[p].int = w
}

/* procedure: geqdefine */
func geqdefine(p int, t int, e int) {
	eqdestroy(eqtb[p])
	eqtb[p].hh.b1 = 1
	eqtb[p].hh.b0 = t
	eqtb[p].hh.rh = e
}

/* procedure: geqworddefine */
func geqworddefine(p int, w int) {
	eqtb[p].int = w
	xeqlevel[p] = 1
}

/* procedure: saveforafter */
func saveforafter(t int) {
	if curlevel > 1 {
		{
			if saveptr > maxsavestack {
				{
					maxsavestack = saveptr
					if maxsavestack > (savesize - 6) {
						overflow(541, savesize)
					}
				}
			}
			savestack[saveptr].hh.b0 = 2
			savestack[saveptr].hh.b1 = 0
			savestack[saveptr].hh.rh = t
			saveptr = (saveptr + 1)
		}
	}
}

/* procedure: unsave */
func unsave() {
	var (
		p int
		l int
		t int
	)
	if curlevel > 1 {
		{
			curlevel = (curlevel - 1)
			for true {
				{
					saveptr = (saveptr - 1)
					if savestack[saveptr].hh.b0 == 3 {
						goto L30
					}
					p = savestack[saveptr].hh.rh
					if savestack[saveptr].hh.b0 == 2 {
						{
							t = curtok
							curtok = p
							backinput()
							curtok = t
						}
					} else {
						{
							if savestack[saveptr].hh.b0 == 0 {
								{
									l = savestack[saveptr].hh.b1
									saveptr = (saveptr - 1)
								}
							} else {
								savestack[saveptr] = eqtb[2881]
							}
							if p < 5263 {
								if eqtb[p].hh.b1 == 1 {
									{
										eqdestroy(savestack[saveptr])
									}
								} else {
									{
										eqdestroy(eqtb[p])
										eqtb[p] = savestack[saveptr]
									}
								}
							} else {
								if xeqlevel[p] != 1 {
									{
										eqtb[p] = savestack[saveptr]
										xeqlevel[p] = l
									}
								} else {
									{
									}
								}
							}
						}
					}
				}
			}
		L30:
			curgroup = savestack[saveptr].hh.b1
			curboundary = savestack[saveptr].hh.rh
		}
	} else {
		confusion(543)
	}
}

/* procedure: preparemag */
func preparemag() {
	if (magset > 0) && (eqtb[5280].int != magset) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(547)
			}
			printint(eqtb[5280].int)
			print_(548)
			printnl(549)
			{
				helpptr = 2
				helpline[1] = 550
				helpline[0] = 551
			}
			interror(magset)
			geqworddefine(5280, magset)
		}
	}
	if (eqtb[5280].int <= 0) || (eqtb[5280].int > 32768) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(552)
			}
			{
				helpptr = 1
				helpline[0] = 553
			}
			interror(eqtb[5280].int)
			geqworddefine(5280, 1000)
		}
	}
	magset = eqtb[5280].int
}

/* procedure: tokenshow */
func tokenshow(p int) {
	if p != 0 {
		showtokenlist(mem[p].hh.rh, 0, 10000000)
	}
}

/* procedure: printmeaning */
func printmeaning() {
	printcmdchr(curcmd, curchr)
	if curcmd >= 111 {
		{
			printchar(58)
			println_()
			tokenshow(curchr)
		}
	} else {
		if curcmd == 110 {
			{
				printchar(58)
				println_()
				tokenshow(curmark[curchr])
			}
		}
	}
}

/* procedure: showcurcmdchr */
func showcurcmdchr() {
	begindiagnostic()
	printnl(123)
	if curlist.modefield != shownmode {
		{
			printmode(curlist.modefield)
			print_(568)
			shownmode = curlist.modefield
		}
	}
	printcmdchr(curcmd, curchr)
	printchar(125)
	enddiagnostic(false)
}

/* procedure: showcontext */
func showcontext() {
	var (
		oldsetting int
		nn         int
		bottomline bool
		i          int
		j          int
		l          int
		m          int
		n          int
		p          int
		q          int
	)
	baseptr = inputptr
	inputstack[baseptr] = curinput
	nn = (-1)
	bottomline = false
	for true {
		{
			curinput = inputstack[baseptr]
			if curinput.statefield != 0 {
				if (curinput.namefield > 17) || (baseptr == 0) {
					bottomline = true
				}
			}
			if ((baseptr == inputptr) || bottomline) || (nn < eqtb[5317].int) {
				{
					if (((baseptr == inputptr) || (curinput.statefield != 0)) || (curinput.indexfield != 3)) || (curinput.locfield != 0) {
						{
							tally = 0
							oldsetting = selector
							if curinput.statefield != 0 {
								{
									if curinput.namefield <= 17 {
										if curinput.namefield == 0 {
											if baseptr == 0 {
												printnl(574)
											} else {
												printnl(575)
											}
										} else {
											{
												printnl(576)
												if curinput.namefield == 17 {
													printchar(42)
												} else {
													printint((curinput.namefield - 1))
												}
												printchar(62)
											}
										}
									} else {
										{
											printnl(577)
											printint(line)
										}
									}
									printchar(32)
									{
										l = tally
										tally = 0
										selector = 20
										trickcount = 1000000
									}
									if buffer[curinput.limitfield] == eqtb[5311].int {
										j = curinput.limitfield
									} else {
										j = (curinput.limitfield + 1)
									}
									if j > 0 {
										for i := curinput.startfield; i <= (j - 1); i++ {
											{
												if i == curinput.locfield {
													{
														firstcount = tally
														trickcount = (((tally + 1) + errorline) - halferrorline)
														if trickcount < errorline {
															trickcount = errorline
														}
													}
												}
												print_(buffer[i])
											}
										}
									}
								}
							} else {
								{
									switch curinput.indexfield {
									case 0:
										printnl(578)
									case 1:
										printnl(579)
									case 2:
										printnl(579)
									case 3:
										if curinput.locfield == 0 {
											printnl(580)
										} else {
											printnl(581)
										}
									case 4:
										printnl(582)
									case 5:
										{
											println_()
											printcs(curinput.namefield)
										}
									case 6:
										printnl(583)
									case 7:
										printnl(584)
									case 8:
										printnl(585)
									case 9:
										printnl(586)
									case 10:
										printnl(587)
									case 11:
										printnl(588)
									case 12:
										printnl(589)
									case 13:
										printnl(590)
									case 14:
										printnl(591)
									case 15:
										printnl(592)
									default:
										printnl(63)
									}
									{
										l = tally
										tally = 0
										selector = 20
										trickcount = 1000000
									}
									if curinput.indexfield < 5 {
										showtokenlist(curinput.startfield, curinput.locfield, 100000)
									} else {
										showtokenlist(mem[curinput.startfield].hh.rh, curinput.locfield, 100000)
									}
								}
							}
							selector = oldsetting
							if trickcount == 1000000 {
								{
									firstcount = tally
									trickcount = (((tally + 1) + errorline) - halferrorline)
									if trickcount < errorline {
										trickcount = errorline
									}
								}
							}
							if tally < trickcount {
								m = (tally - firstcount)
							} else {
								m = (trickcount - firstcount)
							}
							if (l + firstcount) <= halferrorline {
								{
									p = 0
									n = (l + firstcount)
								}
							} else {
								{
									print_(277)
									p = (((l + firstcount) - halferrorline) + 3)
									n = halferrorline
								}
							}
							for q := p; q <= (firstcount - 1); q++ {
								printchar(trickbuf[(q % errorline)])
							}
							println_()
							for q := 1; q <= n; q++ {
								printchar(32)
							}
							if (m + n) <= errorline {
								p = (firstcount + m)
							} else {
								p = (firstcount + ((errorline - n) - 3))
							}
							for q := firstcount; q <= (p - 1); q++ {
								printchar(trickbuf[(q % errorline)])
							}
							if (m + n) > errorline {
								print_(277)
							}
							nn = (nn + 1)
						}
					}
				}
			} else {
				if nn == eqtb[5317].int {
					{
						printnl(277)
						nn = (nn + 1)
					}
				}
			}
			if bottomline {
				goto L30
			}
			baseptr = (baseptr - 1)
		}
	}
L30:
	curinput = inputstack[inputptr]
}

/* procedure: begintokenlist */
func begintokenlist(p int, t int) {
	{
		if inputptr > maxinstack {
			{
				maxinstack = inputptr
				if inputptr == stacksize {
					overflow(593, stacksize)
				}
			}
		}
		inputstack[inputptr] = curinput
		inputptr = (inputptr + 1)
	}
	curinput.statefield = 0
	curinput.startfield = p
	curinput.indexfield = t
	if t >= 5 {
		{
			mem[p].hh.lh = (mem[p].hh.lh + 1)
			if t == 5 {
				curinput.limitfield = paramptr
			} else {
				{
					curinput.locfield = mem[p].hh.rh
					if eqtb[5293].int > 1 {
						{
							begindiagnostic()
							printnl(338)
							switch t {
							case 14:
								printesc(351)
							case 15:
								printesc(594)
							default:
								printcmdchr(72, (t + 3407))
							}
							print_(556)
							tokenshow(p)
							enddiagnostic(false)
						}
					}
				}
			}
		}
	} else {
		curinput.locfield = p
	}
}

/* procedure: endtokenlist */
func endtokenlist() {
	if curinput.indexfield >= 3 {
		{
			if curinput.indexfield <= 4 {
				flushlist(curinput.startfield)
			} else {
				{
					deletetokenref(curinput.startfield)
					if curinput.indexfield == 5 {
						for paramptr > curinput.limitfield {
							{
								paramptr = (paramptr - 1)
								flushlist(paramstack[paramptr])
							}
						}
					}
				}
			}
		}
	} else {
		if curinput.indexfield == 1 {
			if alignstate > 500000 {
				alignstate = 0
			} else {
				fatalerror(595)
			}
		}
	}
	{
		inputptr = (inputptr - 1)
		curinput = inputstack[inputptr]
	}
	{
		if interrupt != 0 {
			pauseforinstructions()
		}
	}
}

/* procedure: backinput */
func backinput() {
	var (
		p int
	)
	for ((curinput.statefield == 0) && (curinput.locfield == 0)) && (curinput.indexfield != 2) {
		endtokenlist()
	}
	p = getavail
	mem[p].hh.lh = curtok
	if curtok < 768 {
		if curtok < 512 {
			alignstate = (alignstate - 1)
		} else {
			alignstate = (alignstate + 1)
		}
	}
	{
		if inputptr > maxinstack {
			{
				maxinstack = inputptr
				if inputptr == stacksize {
					overflow(593, stacksize)
				}
			}
		}
		inputstack[inputptr] = curinput
		inputptr = (inputptr + 1)
	}
	curinput.statefield = 0
	curinput.startfield = p
	curinput.indexfield = 3
	curinput.locfield = p
}

/* procedure: backerror */
func backerror() {
	OKtointerrupt = false
	backinput()
	OKtointerrupt = true
	error_()
}

/* procedure: inserror */
func inserror() {
	OKtointerrupt = false
	backinput()
	curinput.indexfield = 4
	OKtointerrupt = true
	error_()
}

/* procedure: beginfilereading */
func beginfilereading() {
	if inopen == maxinopen {
		overflow(596, maxinopen)
	}
	if first == bufsize {
		overflow(256, bufsize)
	}
	inopen = (inopen + 1)
	{
		if inputptr > maxinstack {
			{
				maxinstack = inputptr
				if inputptr == stacksize {
					overflow(593, stacksize)
				}
			}
		}
		inputstack[inputptr] = curinput
		inputptr = (inputptr + 1)
	}
	curinput.indexfield = inopen
	linestack[curinput.indexfield] = line
	curinput.startfield = first
	curinput.statefield = 1
	curinput.namefield = 0
}

/* procedure: endfilereading */
func endfilereading() {
	first = curinput.startfield
	line = linestack[curinput.indexfield]
	if curinput.namefield > 17 {
		aclose(inputfile[curinput.indexfield])
	}
	{
		inputptr = (inputptr - 1)
		curinput = inputstack[inputptr]
	}
	inopen = (inopen - 1)
}

/* procedure: clearforerrorprompt */
func clearforerrorprompt() {
	for (((curinput.statefield != 0) && (curinput.namefield == 0)) && (inputptr > 0)) && (curinput.locfield > curinput.limitfield) {
		endfilereading()
	}
	println_()
	breakin(termin, true)
}

/* procedure: checkoutervalidity */
func checkoutervalidity() {
	var (
		p int
		q int
	)
	if scannerstatus != 0 {
		{
			deletionsallowed = false
			if curcs != 0 {
				{
					if ((curinput.statefield == 0) || (curinput.namefield < 1)) || (curinput.namefield > 17) {
						{
							p = getavail
							mem[p].hh.lh = (4095 + curcs)
							begintokenlist(p, 3)
						}
					}
					curcmd = 10
					curchr = 32
				}
			}
			if scannerstatus > 1 {
				{
					runaway()
					if curcs == 0 {
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(604)
						}
					} else {
						{
							curcs = 0
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(605)
							}
						}
					}
					print_(606)
					p = getavail
					switch scannerstatus {
					case 2:
						{
							print_(570)
							mem[p].hh.lh = 637
						}
					case 3:
						{
							print_(612)
							mem[p].hh.lh = partoken
							longstate = 113
						}
					case 4:
						{
							print_(572)
							mem[p].hh.lh = 637
							q = p
							p = getavail
							mem[p].hh.rh = q
							mem[p].hh.lh = 6710
							alignstate = (-1000000)
						}
					case 5:
						{
							print_(573)
							mem[p].hh.lh = 637
						}
					}
					begintokenlist(p, 4)
					print_(607)
					sprintcs(warningindex)
					{
						helpptr = 4
						helpline[3] = 608
						helpline[2] = 609
						helpline[1] = 610
						helpline[0] = 611
					}
					error_()
				}
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(598)
					}
					printcmdchr(105, curif)
					print_(599)
					printint(skipline)
					{
						helpptr = 3
						helpline[2] = 600
						helpline[1] = 601
						helpline[0] = 602
					}
					if curcs != 0 {
						curcs = 0
					} else {
						helpline[2] = 603
					}
					curtok = 6713
					inserror()
				}
			}
			deletionsallowed = true
		}
	}
}

/* procedure: getnext */
func getnext() {
	var (
		k   int
		t   int
		cat int
		c   byte
		cc  byte
		d   int
	)
L20:
	curcs = 0
	if curinput.statefield != 0 {
		{
		L25:
			if curinput.locfield <= curinput.limitfield {
				{
					curchr = buffer[curinput.locfield]
					curinput.locfield = (curinput.locfield + 1)
				L21:
					curcmd = eqtb[(3983 + curchr)].hh.rh
					switch curinput.statefield + curcmd {
					case 10:
						goto L25
					case 26:
						goto L25
					case 42:
						goto L25
					case 27:
						goto L25
					case 43:
						goto L25
					case 1:
						{
							if curinput.locfield > curinput.limitfield {
								curcs = 513
							} else {
								{
								L26:
									k = curinput.locfield
									curchr = buffer[k]
									cat = eqtb[(3983 + curchr)].hh.rh
									k = (k + 1)
									if cat == 11 {
										curinput.statefield = 17
									} else {
										if cat == 10 {
											curinput.statefield = 17
										} else {
											curinput.statefield = 1
										}
									}
									if (cat == 11) && (k <= curinput.limitfield) {
										{
											for {
												curchr = buffer[k]
												cat = eqtb[(3983 + curchr)].hh.rh
												k = (k + 1)
												if !((cat != 11) || (k > curinput.limitfield)) {
													break
												}
											}
											{
												if buffer[k] == curchr {
													if cat == 7 {
														if k < curinput.limitfield {
															{
																c = buffer[(k + 1)]
																if c < 128 {
																	{
																		d = 2
																		if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																			if (k + 2) <= curinput.limitfield {
																				{
																					cc = buffer[(k + 2)]
																					if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																						d = (d + 1)
																					}
																				}
																			}
																		}
																		if d > 2 {
																			{
																				if c <= 57 {
																					curchr = (c - 48)
																				} else {
																					curchr = (c - 87)
																				}
																				if cc <= 57 {
																					curchr = (((16 * curchr) + cc) - 48)
																				} else {
																					curchr = (((16 * curchr) + cc) - 87)
																				}
																				buffer[(k - 1)] = curchr
																			}
																		} else {
																			if c < 64 {
																				buffer[(k - 1)] = (c + 64)
																			} else {
																				buffer[(k - 1)] = (c - 64)
																			}
																		}
																		curinput.limitfield = (curinput.limitfield - d)
																		first = (first - d)
																		for k <= curinput.limitfield {
																			{
																				buffer[k] = buffer[(k + d)]
																				k = (k + 1)
																			}
																		}
																		goto L26
																	}
																}
															}
														}
													}
												}
											}
											if cat != 11 {
												k = (k - 1)
											}
											if k > (curinput.locfield + 1) {
												{
													curcs = idlookup(curinput.locfield, (k - curinput.locfield))
													curinput.locfield = k
													goto L40
												}
											}
										}
									} else {
										{
											if buffer[k] == curchr {
												if cat == 7 {
													if k < curinput.limitfield {
														{
															c = buffer[(k + 1)]
															if c < 128 {
																{
																	d = 2
																	if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																		if (k + 2) <= curinput.limitfield {
																			{
																				cc = buffer[(k + 2)]
																				if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																					d = (d + 1)
																				}
																			}
																		}
																	}
																	if d > 2 {
																		{
																			if c <= 57 {
																				curchr = (c - 48)
																			} else {
																				curchr = (c - 87)
																			}
																			if cc <= 57 {
																				curchr = (((16 * curchr) + cc) - 48)
																			} else {
																				curchr = (((16 * curchr) + cc) - 87)
																			}
																			buffer[(k - 1)] = curchr
																		}
																	} else {
																		if c < 64 {
																			buffer[(k - 1)] = (c + 64)
																		} else {
																			buffer[(k - 1)] = (c - 64)
																		}
																	}
																	curinput.limitfield = (curinput.limitfield - d)
																	first = (first - d)
																	for k <= curinput.limitfield {
																		{
																			buffer[k] = buffer[(k + d)]
																			k = (k + 1)
																		}
																	}
																	goto L26
																}
															}
														}
													}
												}
											}
										}
									}
									curcs = (257 + buffer[curinput.locfield])
									curinput.locfield = (curinput.locfield + 1)
								}
							}
						L40:
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 17:
						{
							if curinput.locfield > curinput.limitfield {
								curcs = 513
							} else {
								{
								L26:
									k = curinput.locfield
									curchr = buffer[k]
									cat = eqtb[(3983 + curchr)].hh.rh
									k = (k + 1)
									if cat == 11 {
										curinput.statefield = 17
									} else {
										if cat == 10 {
											curinput.statefield = 17
										} else {
											curinput.statefield = 1
										}
									}
									if (cat == 11) && (k <= curinput.limitfield) {
										{
											for {
												curchr = buffer[k]
												cat = eqtb[(3983 + curchr)].hh.rh
												k = (k + 1)
												if !((cat != 11) || (k > curinput.limitfield)) {
													break
												}
											}
											{
												if buffer[k] == curchr {
													if cat == 7 {
														if k < curinput.limitfield {
															{
																c = buffer[(k + 1)]
																if c < 128 {
																	{
																		d = 2
																		if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																			if (k + 2) <= curinput.limitfield {
																				{
																					cc = buffer[(k + 2)]
																					if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																						d = (d + 1)
																					}
																				}
																			}
																		}
																		if d > 2 {
																			{
																				if c <= 57 {
																					curchr = (c - 48)
																				} else {
																					curchr = (c - 87)
																				}
																				if cc <= 57 {
																					curchr = (((16 * curchr) + cc) - 48)
																				} else {
																					curchr = (((16 * curchr) + cc) - 87)
																				}
																				buffer[(k - 1)] = curchr
																			}
																		} else {
																			if c < 64 {
																				buffer[(k - 1)] = (c + 64)
																			} else {
																				buffer[(k - 1)] = (c - 64)
																			}
																		}
																		curinput.limitfield = (curinput.limitfield - d)
																		first = (first - d)
																		for k <= curinput.limitfield {
																			{
																				buffer[k] = buffer[(k + d)]
																				k = (k + 1)
																			}
																		}
																		goto L26
																	}
																}
															}
														}
													}
												}
											}
											if cat != 11 {
												k = (k - 1)
											}
											if k > (curinput.locfield + 1) {
												{
													curcs = idlookup(curinput.locfield, (k - curinput.locfield))
													curinput.locfield = k
													goto L40
												}
											}
										}
									} else {
										{
											if buffer[k] == curchr {
												if cat == 7 {
													if k < curinput.limitfield {
														{
															c = buffer[(k + 1)]
															if c < 128 {
																{
																	d = 2
																	if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																		if (k + 2) <= curinput.limitfield {
																			{
																				cc = buffer[(k + 2)]
																				if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																					d = (d + 1)
																				}
																			}
																		}
																	}
																	if d > 2 {
																		{
																			if c <= 57 {
																				curchr = (c - 48)
																			} else {
																				curchr = (c - 87)
																			}
																			if cc <= 57 {
																				curchr = (((16 * curchr) + cc) - 48)
																			} else {
																				curchr = (((16 * curchr) + cc) - 87)
																			}
																			buffer[(k - 1)] = curchr
																		}
																	} else {
																		if c < 64 {
																			buffer[(k - 1)] = (c + 64)
																		} else {
																			buffer[(k - 1)] = (c - 64)
																		}
																	}
																	curinput.limitfield = (curinput.limitfield - d)
																	first = (first - d)
																	for k <= curinput.limitfield {
																		{
																			buffer[k] = buffer[(k + d)]
																			k = (k + 1)
																		}
																	}
																	goto L26
																}
															}
														}
													}
												}
											}
										}
									}
									curcs = (257 + buffer[curinput.locfield])
									curinput.locfield = (curinput.locfield + 1)
								}
							}
						L40:
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 33:
						{
							if curinput.locfield > curinput.limitfield {
								curcs = 513
							} else {
								{
								L26:
									k = curinput.locfield
									curchr = buffer[k]
									cat = eqtb[(3983 + curchr)].hh.rh
									k = (k + 1)
									if cat == 11 {
										curinput.statefield = 17
									} else {
										if cat == 10 {
											curinput.statefield = 17
										} else {
											curinput.statefield = 1
										}
									}
									if (cat == 11) && (k <= curinput.limitfield) {
										{
											for {
												curchr = buffer[k]
												cat = eqtb[(3983 + curchr)].hh.rh
												k = (k + 1)
												if !((cat != 11) || (k > curinput.limitfield)) {
													break
												}
											}
											{
												if buffer[k] == curchr {
													if cat == 7 {
														if k < curinput.limitfield {
															{
																c = buffer[(k + 1)]
																if c < 128 {
																	{
																		d = 2
																		if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																			if (k + 2) <= curinput.limitfield {
																				{
																					cc = buffer[(k + 2)]
																					if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																						d = (d + 1)
																					}
																				}
																			}
																		}
																		if d > 2 {
																			{
																				if c <= 57 {
																					curchr = (c - 48)
																				} else {
																					curchr = (c - 87)
																				}
																				if cc <= 57 {
																					curchr = (((16 * curchr) + cc) - 48)
																				} else {
																					curchr = (((16 * curchr) + cc) - 87)
																				}
																				buffer[(k - 1)] = curchr
																			}
																		} else {
																			if c < 64 {
																				buffer[(k - 1)] = (c + 64)
																			} else {
																				buffer[(k - 1)] = (c - 64)
																			}
																		}
																		curinput.limitfield = (curinput.limitfield - d)
																		first = (first - d)
																		for k <= curinput.limitfield {
																			{
																				buffer[k] = buffer[(k + d)]
																				k = (k + 1)
																			}
																		}
																		goto L26
																	}
																}
															}
														}
													}
												}
											}
											if cat != 11 {
												k = (k - 1)
											}
											if k > (curinput.locfield + 1) {
												{
													curcs = idlookup(curinput.locfield, (k - curinput.locfield))
													curinput.locfield = k
													goto L40
												}
											}
										}
									} else {
										{
											if buffer[k] == curchr {
												if cat == 7 {
													if k < curinput.limitfield {
														{
															c = buffer[(k + 1)]
															if c < 128 {
																{
																	d = 2
																	if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
																		if (k + 2) <= curinput.limitfield {
																			{
																				cc = buffer[(k + 2)]
																				if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																					d = (d + 1)
																				}
																			}
																		}
																	}
																	if d > 2 {
																		{
																			if c <= 57 {
																				curchr = (c - 48)
																			} else {
																				curchr = (c - 87)
																			}
																			if cc <= 57 {
																				curchr = (((16 * curchr) + cc) - 48)
																			} else {
																				curchr = (((16 * curchr) + cc) - 87)
																			}
																			buffer[(k - 1)] = curchr
																		}
																	} else {
																		if c < 64 {
																			buffer[(k - 1)] = (c + 64)
																		} else {
																			buffer[(k - 1)] = (c - 64)
																		}
																	}
																	curinput.limitfield = (curinput.limitfield - d)
																	first = (first - d)
																	for k <= curinput.limitfield {
																		{
																			buffer[k] = buffer[(k + d)]
																			k = (k + 1)
																		}
																	}
																	goto L26
																}
															}
														}
													}
												}
											}
										}
									}
									curcs = (257 + buffer[curinput.locfield])
									curinput.locfield = (curinput.locfield + 1)
								}
							}
						L40:
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 14:
						{
							curcs = (curchr + 1)
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							curinput.statefield = 1
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 30:
						{
							curcs = (curchr + 1)
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							curinput.statefield = 1
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 46:
						{
							curcs = (curchr + 1)
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							curinput.statefield = 1
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 8:
						{
							if curchr == buffer[curinput.locfield] {
								if curinput.locfield < curinput.limitfield {
									{
										c = buffer[(curinput.locfield + 1)]
										if c < 128 {
											{
												curinput.locfield = (curinput.locfield + 2)
												if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
													if curinput.locfield <= curinput.limitfield {
														{
															cc = buffer[curinput.locfield]
															if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																{
																	curinput.locfield = (curinput.locfield + 1)
																	if c <= 57 {
																		curchr = (c - 48)
																	} else {
																		curchr = (c - 87)
																	}
																	if cc <= 57 {
																		curchr = (((16 * curchr) + cc) - 48)
																	} else {
																		curchr = (((16 * curchr) + cc) - 87)
																	}
																	goto L21
																}
															}
														}
													}
												}
												if c < 64 {
													curchr = (c + 64)
												} else {
													curchr = (c - 64)
												}
												goto L21
											}
										}
									}
								}
							}
							curinput.statefield = 1
						}
					case 24:
						{
							if curchr == buffer[curinput.locfield] {
								if curinput.locfield < curinput.limitfield {
									{
										c = buffer[(curinput.locfield + 1)]
										if c < 128 {
											{
												curinput.locfield = (curinput.locfield + 2)
												if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
													if curinput.locfield <= curinput.limitfield {
														{
															cc = buffer[curinput.locfield]
															if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																{
																	curinput.locfield = (curinput.locfield + 1)
																	if c <= 57 {
																		curchr = (c - 48)
																	} else {
																		curchr = (c - 87)
																	}
																	if cc <= 57 {
																		curchr = (((16 * curchr) + cc) - 48)
																	} else {
																		curchr = (((16 * curchr) + cc) - 87)
																	}
																	goto L21
																}
															}
														}
													}
												}
												if c < 64 {
													curchr = (c + 64)
												} else {
													curchr = (c - 64)
												}
												goto L21
											}
										}
									}
								}
							}
							curinput.statefield = 1
						}
					case 40:
						{
							if curchr == buffer[curinput.locfield] {
								if curinput.locfield < curinput.limitfield {
									{
										c = buffer[(curinput.locfield + 1)]
										if c < 128 {
											{
												curinput.locfield = (curinput.locfield + 2)
												if ((c >= 48) && (c <= 57)) || ((c >= 97) && (c <= 102)) {
													if curinput.locfield <= curinput.limitfield {
														{
															cc = buffer[curinput.locfield]
															if ((cc >= 48) && (cc <= 57)) || ((cc >= 97) && (cc <= 102)) {
																{
																	curinput.locfield = (curinput.locfield + 1)
																	if c <= 57 {
																		curchr = (c - 48)
																	} else {
																		curchr = (c - 87)
																	}
																	if cc <= 57 {
																		curchr = (((16 * curchr) + cc) - 48)
																	} else {
																		curchr = (((16 * curchr) + cc) - 87)
																	}
																	goto L21
																}
															}
														}
													}
												}
												if c < 64 {
													curchr = (c + 64)
												} else {
													curchr = (c - 64)
												}
												goto L21
											}
										}
									}
								}
							}
							curinput.statefield = 1
						}
					case 16:
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(613)
							}
							{
								helpptr = 2
								helpline[1] = 614
								helpline[0] = 615
							}
							deletionsallowed = false
							error_()
							deletionsallowed = true
							goto L20
						}
					case 32:
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(613)
							}
							{
								helpptr = 2
								helpline[1] = 614
								helpline[0] = 615
							}
							deletionsallowed = false
							error_()
							deletionsallowed = true
							goto L20
						}
					case 48:
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(613)
							}
							{
								helpptr = 2
								helpline[1] = 614
								helpline[0] = 615
							}
							deletionsallowed = false
							error_()
							deletionsallowed = true
							goto L20
						}
					case 11:
						{
							curinput.statefield = 17
							curchr = 32
						}
					case 6:
						{
							curinput.locfield = (curinput.limitfield + 1)
							curcmd = 10
							curchr = 32
						}
					case 22:
						{
							curinput.locfield = (curinput.limitfield + 1)
							goto L25
						}
					case 15:
						{
							curinput.locfield = (curinput.limitfield + 1)
							goto L25
						}
					case 31:
						{
							curinput.locfield = (curinput.limitfield + 1)
							goto L25
						}
					case 47:
						{
							curinput.locfield = (curinput.limitfield + 1)
							goto L25
						}
					case 38:
						{
							curinput.locfield = (curinput.limitfield + 1)
							curcs = parloc
							curcmd = eqtb[curcs].hh.b0
							curchr = eqtb[curcs].hh.rh
							if curcmd >= 113 {
								checkoutervalidity()
							}
						}
					case 2:
						alignstate = (alignstate + 1)
					case 18:
						{
							curinput.statefield = 1
							alignstate = (alignstate + 1)
						}
					case 34:
						{
							curinput.statefield = 1
							alignstate = (alignstate + 1)
						}
					case 3:
						alignstate = (alignstate - 1)
					case 19:
						{
							curinput.statefield = 1
							alignstate = (alignstate - 1)
						}
					case 35:
						{
							curinput.statefield = 1
							alignstate = (alignstate - 1)
						}
					case 20:
						curinput.statefield = 1
					case 21:
						curinput.statefield = 1
					case 23:
						curinput.statefield = 1
					case 25:
						curinput.statefield = 1
					case 28:
						curinput.statefield = 1
					case 29:
						curinput.statefield = 1
					case 36:
						curinput.statefield = 1
					case 37:
						curinput.statefield = 1
					case 39:
						curinput.statefield = 1
					case 41:
						curinput.statefield = 1
					case 44:
						curinput.statefield = 1
					case 45:
						curinput.statefield = 1
					default:
						// empty
					}
				}
			} else {
				{
					curinput.statefield = 33
					if curinput.namefield > 17 {
						{
							line = (line + 1)
							first = curinput.startfield
							if !forceeof {
								{
									if inputln(inputfile[curinput.indexfield], true) {
										firmuptheline()
									} else {
										forceeof = true
									}
								}
							}
							if forceeof {
								{
									printchar(41)
									openparens = (openparens - 1)
									break_(termout)
									forceeof = false
									endfilereading()
									checkoutervalidity()
									goto L20
								}
							}
							if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
								curinput.limitfield = (curinput.limitfield - 1)
							} else {
								buffer[curinput.limitfield] = eqtb[5311].int
							}
							first = (curinput.limitfield + 1)
							curinput.locfield = curinput.startfield
						}
					} else {
						{
							if !(curinput.namefield == 0) {
								{
									curcmd = 0
									curchr = 0
									goto L10
								}
							}
							if inputptr > 0 {
								{
									endfilereading()
									goto L20
								}
							}
							if selector < 18 {
								openlogfile()
							}
							if interaction > 1 {
								{
									if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
										curinput.limitfield = (curinput.limitfield + 1)
									}
									if curinput.limitfield == curinput.startfield {
										printnl(616)
									}
									println_()
									first = curinput.startfield
									{
										print_(42)
										terminput()
									}
									curinput.limitfield = last
									if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
										curinput.limitfield = (curinput.limitfield - 1)
									} else {
										buffer[curinput.limitfield] = eqtb[5311].int
									}
									first = (curinput.limitfield + 1)
									curinput.locfield = curinput.startfield
								}
							} else {
								fatalerror(617)
							}
						}
					}
					{
						if interrupt != 0 {
							pauseforinstructions()
						}
					}
					goto L25
				}
			}
		}
	} else {
		if curinput.locfield != 0 {
			{
				t = mem[curinput.locfield].hh.lh
				curinput.locfield = mem[curinput.locfield].hh.rh
				if t >= 4095 {
					{
						curcs = (t - 4095)
						curcmd = eqtb[curcs].hh.b0
						curchr = eqtb[curcs].hh.rh
						if curcmd >= 113 {
							if curcmd == 116 {
								{
									curcs = (mem[curinput.locfield].hh.lh - 4095)
									curinput.locfield = 0
									curcmd = eqtb[curcs].hh.b0
									curchr = eqtb[curcs].hh.rh
									if curcmd > 100 {
										{
											curcmd = 0
											curchr = 257
										}
									}
								}
							} else {
								checkoutervalidity()
							}
						}
					}
				} else {
					{
						curcmd = (t / 256)
						curchr = (t % 256)
						switch curcmd {
						case 1:
							alignstate = (alignstate + 1)
						case 2:
							alignstate = (alignstate - 1)
						case 5:
							{
								begintokenlist(paramstack[((curinput.limitfield+curchr)-1)], 0)
								goto L20
							}
						default:
							// empty
						}
					}
				}
			}
		} else {
			{
				endtokenlist()
				goto L20
			}
		}
	}
	if curcmd <= 5 {
		if curcmd >= 4 {
			if alignstate == 0 {
				{
					if (scannerstatus == 4) || (curalign == 0) {
						fatalerror(595)
					}
					curcmd = mem[(curalign + 5)].hh.lh
					mem[(curalign + 5)].hh.lh = curchr
					if curcmd == 63 {
						begintokenlist(29990, 2)
					} else {
						begintokenlist(mem[(curalign+2)].int, 2)
					}
					alignstate = 1000000
					goto L20
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: firmuptheline */
func firmuptheline() {
	var (
		k int
	)
	curinput.limitfield = last
	if eqtb[5291].int > 0 {
		if interaction > 1 {
			{
				println_()
				if curinput.startfield < curinput.limitfield {
					for k := curinput.startfield; k <= (curinput.limitfield - 1); k++ {
						print_(buffer[k])
					}
				}
				first = curinput.limitfield
				{
					print_(618)
					terminput()
				}
				if last > first {
					{
						for k := first; k <= (last - 1); k++ {
							buffer[((k + curinput.startfield) - first)] = buffer[k]
						}
						curinput.limitfield = ((curinput.startfield + last) - first)
					}
				}
			}
		}
	}
}

/* procedure: gettoken */
func gettoken() {
	nonewcontrolsequence = false
	getnext()
	nonewcontrolsequence = true
	if curcs == 0 {
		curtok = ((curcmd * 256) + curchr)
	} else {
		curtok = (4095 + curcs)
	}
}

/* procedure: macrocall */
func macrocall() {
	var (
		r                 int
		p                 int
		q                 int
		s                 int
		t                 int
		u                 int
		v                 int
		rbraceptr         int
		n                 int
		unbalance         int
		m                 int
		refcount          int
		savescannerstatus int
		savewarningindex  int
		matchchr          byte
	)
	savescannerstatus = scannerstatus
	savewarningindex = warningindex
	warningindex = curcs
	refcount = curchr
	r = mem[refcount].hh.rh
	n = 0
	if eqtb[5293].int > 0 {
		{
			begindiagnostic()
			println_()
			printcs(warningindex)
			tokenshow(refcount)
			enddiagnostic(false)
		}
	}
	if mem[r].hh.lh != 3584 {
		{
			scannerstatus = 3
			unbalance = 0
			longstate = eqtb[curcs].hh.b0
			if longstate >= 113 {
				longstate = (longstate - 2)
			}
			for {
				mem[29997].hh.rh = 0
				if (mem[r].hh.lh > 3583) || (mem[r].hh.lh < 3328) {
					s = 0
				} else {
					{
						matchchr = (mem[r].hh.lh - 3328)
						s = mem[r].hh.rh
						r = s
						p = 29997
						m = 0
					}
				}
			L22:
				gettoken()
				if curtok == mem[r].hh.lh {
					{
						r = mem[r].hh.rh
						if (mem[r].hh.lh >= 3328) && (mem[r].hh.lh <= 3584) {
							{
								if curtok < 512 {
									alignstate = (alignstate - 1)
								}
								goto L40
							}
						} else {
							goto L22
						}
					}
				}
				if s != r {
					if s == 0 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(650)
							}
							sprintcs(warningindex)
							print_(651)
							{
								helpptr = 4
								helpline[3] = 652
								helpline[2] = 653
								helpline[1] = 654
								helpline[0] = 655
							}
							error_()
							goto L10
						}
					} else {
						{
							t = s
							for {
								{
									q = getavail
									mem[p].hh.rh = q
									mem[q].hh.lh = mem[t].hh.lh
									p = q
								}
								m = (m + 1)
								u = mem[t].hh.rh
								v = s
								for true {
									{
										if u == r {
											if curtok != mem[v].hh.lh {
												goto L30
											} else {
												{
													r = mem[v].hh.rh
													goto L22
												}
											}
										}
										if mem[u].hh.lh != mem[v].hh.lh {
											goto L30
										}
										u = mem[u].hh.rh
										v = mem[v].hh.rh
									}
								}
							L30:
								t = mem[t].hh.rh
								if !(t == r) {
									break
								}
							}
							r = s
						}
					}
				}
				if curtok == partoken {
					if longstate != 112 {
						{
							if longstate == 111 {
								{
									runaway()
									{
										if interaction == 3 {
											// empty
										}
										printnl(262)
										print_(645)
									}
									sprintcs(warningindex)
									print_(646)
									{
										helpptr = 3
										helpline[2] = 647
										helpline[1] = 648
										helpline[0] = 649
									}
									backerror()
								}
							}
							pstack[n] = mem[29997].hh.rh
							alignstate = (alignstate - unbalance)
							for m := 0; m <= n; m++ {
								flushlist(pstack[m])
							}
							goto L10
						}
					}
				}
				if curtok < 768 {
					if curtok < 512 {
						{
							unbalance = 1
							for true {
								{
									{
										{
											q = avail
											if q == 0 {
												q = getavail
											} else {
												{
													avail = mem[q].hh.rh
													mem[q].hh.rh = 0
												}
											}
										}
										mem[p].hh.rh = q
										mem[q].hh.lh = curtok
										p = q
									}
									gettoken()
									if curtok == partoken {
										if longstate != 112 {
											{
												if longstate == 111 {
													{
														runaway()
														{
															if interaction == 3 {
																// empty
															}
															printnl(262)
															print_(645)
														}
														sprintcs(warningindex)
														print_(646)
														{
															helpptr = 3
															helpline[2] = 647
															helpline[1] = 648
															helpline[0] = 649
														}
														backerror()
													}
												}
												pstack[n] = mem[29997].hh.rh
												alignstate = (alignstate - unbalance)
												for m := 0; m <= n; m++ {
													flushlist(pstack[m])
												}
												goto L10
											}
										}
									}
									if curtok < 768 {
										if curtok < 512 {
											unbalance = (unbalance + 1)
										} else {
											{
												unbalance = (unbalance - 1)
												if unbalance == 0 {
													goto L31
												}
											}
										}
									}
								}
							}
						L31:
							rbraceptr = p
							{
								q = getavail
								mem[p].hh.rh = q
								mem[q].hh.lh = curtok
								p = q
							}
						}
					} else {
						{
							backinput()
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(637)
							}
							sprintcs(warningindex)
							print_(638)
							{
								helpptr = 6
								helpline[5] = 639
								helpline[4] = 640
								helpline[3] = 641
								helpline[2] = 642
								helpline[1] = 643
								helpline[0] = 644
							}
							alignstate = (alignstate + 1)
							longstate = 111
							curtok = partoken
							inserror()
							goto L22
						}
					}
				} else {
					{
						if curtok == 2592 {
							if mem[r].hh.lh <= 3584 {
								if mem[r].hh.lh >= 3328 {
									goto L22
								}
							}
						}
						{
							q = getavail
							mem[p].hh.rh = q
							mem[q].hh.lh = curtok
							p = q
						}
					}
				}
				m = (m + 1)
				if mem[r].hh.lh > 3584 {
					goto L22
				}
				if mem[r].hh.lh < 3328 {
					goto L22
				}
			L40:
				if s != 0 {
					{
						if (m == 1) && (mem[p].hh.lh < 768) {
							{
								mem[rbraceptr].hh.rh = 0
								{
									mem[p].hh.rh = avail
									avail = p
								}
								p = mem[29997].hh.rh
								pstack[n] = mem[p].hh.rh
								{
									mem[p].hh.rh = avail
									avail = p
								}
							}
						} else {
							pstack[n] = mem[29997].hh.rh
						}
						n = (n + 1)
						if eqtb[5293].int > 0 {
							{
								begindiagnostic()
								printnl(matchchr)
								printint(n)
								print_(656)
								showtokenlist(pstack[(n-1)], 0, 1000)
								enddiagnostic(false)
							}
						}
					}
				}
				if !(mem[r].hh.lh == 3584) {
					break
				}
			}
		}
	}
	for ((curinput.statefield == 0) && (curinput.locfield == 0)) && (curinput.indexfield != 2) {
		endtokenlist()
	}
	begintokenlist(refcount, 5)
	curinput.namefield = warningindex
	curinput.locfield = mem[r].hh.rh
	if n > 0 {
		{
			if (paramptr + n) > maxparamstack {
				{
					maxparamstack = (paramptr + n)
					if maxparamstack > paramsize {
						overflow(636, paramsize)
					}
				}
			}
			for m := 0; m <= (n - 1); m++ {
				paramstack[(paramptr + m)] = pstack[m]
			}
			paramptr = (paramptr + n)
		}
	}
L10:
	scannerstatus = savescannerstatus
	warningindex = savewarningindex
}

/* procedure: insertrelax */
func insertrelax() {
	curtok = (4095 + curcs)
	backinput()
	curtok = 6716
	backinput()
	curinput.indexfield = 4
}

/* procedure: expand */
func expand() {
	var (
		t                 int
		p                 int
		q                 int
		r                 int
		j                 int
		cvbackup          int
		cvlbackup         int
		radixbackup       int
		cobackup          int
		backupbackup      int
		savescannerstatus int
	)
	cvbackup = curval
	cvlbackup = curvallevel
	radixbackup = radix
	cobackup = curorder
	backupbackup = mem[29987].hh.rh
	if curcmd < 111 {
		{
			if eqtb[5299].int > 1 {
				showcurcmdchr()
			}
			switch curcmd {
			case 110:
				{
					if curmark[curchr] != 0 {
						begintokenlist(curmark[curchr], 14)
					}
				}
			case 102:
				{
					gettoken()
					t = curtok
					gettoken()
					if curcmd > 100 {
						expand()
					} else {
						backinput()
					}
					curtok = t
					backinput()
				}
			case 103:
				{
					savescannerstatus = scannerstatus
					scannerstatus = 0
					gettoken()
					scannerstatus = savescannerstatus
					t = curtok
					backinput()
					if t >= 4095 {
						{
							p = getavail
							mem[p].hh.lh = 6718
							mem[p].hh.rh = curinput.locfield
							curinput.startfield = p
							curinput.locfield = p
						}
					}
				}
			case 107:
				{
					r = getavail
					p = r
					for {
						getxtoken()
						if curcs == 0 {
							{
								q = getavail
								mem[p].hh.rh = q
								mem[q].hh.lh = curtok
								p = q
							}
						}
						if !(curcs != 0) {
							break
						}
					}
					if curcmd != 67 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(625)
							}
							printesc(505)
							print_(626)
							{
								helpptr = 2
								helpline[1] = 627
								helpline[0] = 628
							}
							backerror()
						}
					}
					j = first
					p = mem[r].hh.rh
					for p != 0 {
						{
							if j >= maxbufstack {
								{
									maxbufstack = (j + 1)
									if maxbufstack == bufsize {
										overflow(256, bufsize)
									}
								}
							}
							buffer[j] = (mem[p].hh.lh % 256)
							j = (j + 1)
							p = mem[p].hh.rh
						}
					}
					if j > (first + 1) {
						{
							nonewcontrolsequence = false
							curcs = idlookup(first, (j - first))
							nonewcontrolsequence = true
						}
					} else {
						if j == first {
							curcs = 513
						} else {
							curcs = (257 + buffer[first])
						}
					}
					flushlist(r)
					if eqtb[curcs].hh.b0 == 101 {
						{
							eqdefine(curcs, 0, 256)
						}
					}
					curtok = (curcs + 4095)
					backinput()
				}
			case 108:
				convtoks()
			case 109:
				insthetoks()
			case 105:
				conditional()
			case 106:
				if curchr > iflimit {
					if iflimit == 1 {
						insertrelax()
					} else {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(777)
							}
							printcmdchr(106, curchr)
							{
								helpptr = 1
								helpline[0] = 778
							}
							error_()
						}
					}
				} else {
					{
						for curchr != 2 {
							passtext()
						}
						{
							p = condptr
							ifline = mem[(p + 1)].int
							curif = mem[p].hh.b1
							iflimit = mem[p].hh.b0
							condptr = mem[p].hh.rh
							freenode(p, 2)
						}
					}
				}
			case 104:
				if curchr > 0 {
					forceeof = true
				} else {
					if nameinprogress {
						insertrelax()
					} else {
						startinput()
					}
				}
			default:
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(619)
					}
					{
						helpptr = 5
						helpline[4] = 620
						helpline[3] = 621
						helpline[2] = 622
						helpline[1] = 623
						helpline[0] = 624
					}
					error_()
				}
			}
		}
	} else {
		if curcmd < 115 {
			macrocall()
		} else {
			{
				curtok = 6715
				backinput()
			}
		}
	}
	curval = cvbackup
	curvallevel = cvlbackup
	radix = radixbackup
	curorder = cobackup
	mem[29987].hh.rh = backupbackup
}

/* procedure: getxtoken */
func getxtoken() {
L20:
	getnext()
	if curcmd <= 100 {
		goto L30
	}
	if curcmd >= 111 {
		if curcmd < 115 {
			macrocall()
		} else {
			{
				curcs = 2620
				curcmd = 9
				goto L30
			}
		}
	} else {
		expand()
	}
	goto L20
L30:
	if curcs == 0 {
		curtok = ((curcmd * 256) + curchr)
	} else {
		curtok = (4095 + curcs)
	}
}

/* procedure: xtoken */
func xtoken() {
	for curcmd > 100 {
		{
			expand()
			getnext()
		}
	}
	if curcs == 0 {
		curtok = ((curcmd * 256) + curchr)
	} else {
		curtok = (4095 + curcs)
	}
}

/* procedure: scanleftbrace */
func scanleftbrace() {
	for {
		getxtoken()
		if !((curcmd != 10) && (curcmd != 0)) {
			break
		}
	}
	if curcmd != 1 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(657)
			}
			{
				helpptr = 4
				helpline[3] = 658
				helpline[2] = 659
				helpline[1] = 660
				helpline[0] = 661
			}
			backerror()
			curtok = 379
			curcmd = 1
			curchr = 123
			alignstate = (alignstate + 1)
		}
	}
}

/* procedure: scanoptionalequals */
func scanoptionalequals() {
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	if curtok != 3133 {
		backinput()
	}
}

/* function: scankeyword */
func scankeyword(s int) bool {
	var (
		p int
		q int
		k int
	)
	p = 29987
	mem[p].hh.rh = 0
	k = strstart[s]
	for k < strstart[(s+1)] {
		{
			getxtoken()
			if (curcs == 0) && ((curchr == strpool[k]) || (curchr == (strpool[k] - 32))) {
				{
					{
						q = getavail
						mem[p].hh.rh = q
						mem[q].hh.lh = curtok
						p = q
					}
					k = (k + 1)
				}
			} else {
				if (curcmd != 10) || (p != 29987) {
					{
						backinput()
						if p != 29987 {
							begintokenlist(mem[29987].hh.rh, 3)
						}
						scankeyword = false
						goto L10
					}
				}
			}
		}
	}
	flushlist(mem[29987].hh.rh)
	scankeyword = true
L10:
	// empty
}

/* procedure: muerror */
func muerror() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(662)
	}
	{
		helpptr = 1
		helpline[0] = 663
	}
	error_()
}

/* procedure: scaneightbitint */
func scaneightbitint() {
	scanint()
	if (curval < 0) || (curval > 255) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(687)
			}
			{
				helpptr = 2
				helpline[1] = 688
				helpline[0] = 689
			}
			interror(curval)
			curval = 0
		}
	}
}

/* procedure: scancharnum */
func scancharnum() {
	scanint()
	if (curval < 0) || (curval > 255) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(690)
			}
			{
				helpptr = 2
				helpline[1] = 691
				helpline[0] = 689
			}
			interror(curval)
			curval = 0
		}
	}
}

/* procedure: scanfourbitint */
func scanfourbitint() {
	scanint()
	if (curval < 0) || (curval > 15) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(692)
			}
			{
				helpptr = 2
				helpline[1] = 693
				helpline[0] = 689
			}
			interror(curval)
			curval = 0
		}
	}
}

/* procedure: scanfifteenbitint */
func scanfifteenbitint() {
	scanint()
	if (curval < 0) || (curval > 32767) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(694)
			}
			{
				helpptr = 2
				helpline[1] = 695
				helpline[0] = 689
			}
			interror(curval)
			curval = 0
		}
	}
}

/* procedure: scantwentysevenbitint */
func scantwentysevenbitint() {
	scanint()
	if (curval < 0) || (curval > 134217727) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(696)
			}
			{
				helpptr = 2
				helpline[1] = 697
				helpline[0] = 689
			}
			interror(curval)
			curval = 0
		}
	}
}

/* procedure: scanfontident */
func scanfontident() {
	var (
		f int
		m int
	)
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	if curcmd == 88 {
		f = eqtb[3934].hh.rh
	} else {
		if curcmd == 87 {
			f = curchr
		} else {
			if curcmd == 86 {
				{
					m = curchr
					scanfourbitint()
					f = eqtb[(m + curval)].hh.rh
				}
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(817)
					}
					{
						helpptr = 2
						helpline[1] = 818
						helpline[0] = 819
					}
					backerror()
					f = 0
				}
			}
		}
	}
	curval = f
}

/* procedure: findfontdimen */
func findfontdimen(writing bool) {
	var (
		f int
		n int
	)
	scanint()
	n = curval
	scanfontident()
	f = curval
	if n <= 0 {
		curval = fmemptr
	} else {
		{
			if ((writing && (n <= 4)) && (n >= 2)) && (fontglue[f] != 0) {
				{
					deleteglueref(fontglue[f])
					fontglue[f] = 0
				}
			}
			if n > fontparams[f] {
				if f < fontptr {
					curval = fmemptr
				} else {
					{
						for {
							if fmemptr == fontmemsize {
								overflow(824, fontmemsize)
							}
							fontinfo[fmemptr].int = 0
							fmemptr = (fmemptr + 1)
							fontparams[f] = (fontparams[f] + 1)
							if !(n == fontparams[f]) {
								break
							}
						}
						curval = (fmemptr - 1)
					}
				}
			} else {
				curval = (n + parambase[f])
			}
		}
	}
	if curval == fmemptr {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(802)
			}
			printesc(hash[(2624 + f)].rh)
			print_(820)
			printint(fontparams[f])
			print_(821)
			{
				helpptr = 2
				helpline[1] = 822
				helpline[0] = 823
			}
			error_()
		}
	}
}

/* procedure: scansomethinginternal */
func scansomethinginternal(level int, negative bool) {
	var (
		m int
		p int
	)
	m = curchr
	switch curcmd {
	case 85:
		{
			scancharnum()
			if m == 5007 {
				{
					curval = (eqtb[(5007+curval)].hh.rh - 0)
					curvallevel = 0
				}
			} else {
				if m < 5007 {
					{
						curval = eqtb[(m + curval)].hh.rh
						curvallevel = 0
					}
				} else {
					{
						curval = eqtb[(m + curval)].int
						curvallevel = 0
					}
				}
			}
		}
	case 71:
		if level != 5 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(664)
				}
				{
					helpptr = 3
					helpline[2] = 665
					helpline[1] = 666
					helpline[0] = 667
				}
				backerror()
				{
					curval = 0
					curvallevel = 1
				}
			}
		} else {
			if curcmd <= 72 {
				{
					if curcmd < 72 {
						{
							scaneightbitint()
							m = (3422 + curval)
						}
					}
					{
						curval = eqtb[m].hh.rh
						curvallevel = 5
					}
				}
			} else {
				{
					backinput()
					scanfontident()
					{
						curval = (2624 + curval)
						curvallevel = 4
					}
				}
			}
		}
	case 72:
		if level != 5 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(664)
				}
				{
					helpptr = 3
					helpline[2] = 665
					helpline[1] = 666
					helpline[0] = 667
				}
				backerror()
				{
					curval = 0
					curvallevel = 1
				}
			}
		} else {
			if curcmd <= 72 {
				{
					if curcmd < 72 {
						{
							scaneightbitint()
							m = (3422 + curval)
						}
					}
					{
						curval = eqtb[m].hh.rh
						curvallevel = 5
					}
				}
			} else {
				{
					backinput()
					scanfontident()
					{
						curval = (2624 + curval)
						curvallevel = 4
					}
				}
			}
		}
	case 86:
		if level != 5 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(664)
				}
				{
					helpptr = 3
					helpline[2] = 665
					helpline[1] = 666
					helpline[0] = 667
				}
				backerror()
				{
					curval = 0
					curvallevel = 1
				}
			}
		} else {
			if curcmd <= 72 {
				{
					if curcmd < 72 {
						{
							scaneightbitint()
							m = (3422 + curval)
						}
					}
					{
						curval = eqtb[m].hh.rh
						curvallevel = 5
					}
				}
			} else {
				{
					backinput()
					scanfontident()
					{
						curval = (2624 + curval)
						curvallevel = 4
					}
				}
			}
		}
	case 87:
		if level != 5 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(664)
				}
				{
					helpptr = 3
					helpline[2] = 665
					helpline[1] = 666
					helpline[0] = 667
				}
				backerror()
				{
					curval = 0
					curvallevel = 1
				}
			}
		} else {
			if curcmd <= 72 {
				{
					if curcmd < 72 {
						{
							scaneightbitint()
							m = (3422 + curval)
						}
					}
					{
						curval = eqtb[m].hh.rh
						curvallevel = 5
					}
				}
			} else {
				{
					backinput()
					scanfontident()
					{
						curval = (2624 + curval)
						curvallevel = 4
					}
				}
			}
		}
	case 88:
		if level != 5 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(664)
				}
				{
					helpptr = 3
					helpline[2] = 665
					helpline[1] = 666
					helpline[0] = 667
				}
				backerror()
				{
					curval = 0
					curvallevel = 1
				}
			}
		} else {
			if curcmd <= 72 {
				{
					if curcmd < 72 {
						{
							scaneightbitint()
							m = (3422 + curval)
						}
					}
					{
						curval = eqtb[m].hh.rh
						curvallevel = 5
					}
				}
			} else {
				{
					backinput()
					scanfontident()
					{
						curval = (2624 + curval)
						curvallevel = 4
					}
				}
			}
		}
	case 73:
		{
			curval = eqtb[m].int
			curvallevel = 0
		}
	case 74:
		{
			curval = eqtb[m].int
			curvallevel = 1
		}
	case 75:
		{
			curval = eqtb[m].hh.rh
			curvallevel = 2
		}
	case 76:
		{
			curval = eqtb[m].hh.rh
			curvallevel = 3
		}
	case 79:
		if abs_(curlist.modefield) != m {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(680)
				}
				printcmdchr(79, m)
				{
					helpptr = 4
					helpline[3] = 681
					helpline[2] = 682
					helpline[1] = 683
					helpline[0] = 684
				}
				error_()
				if level != 5 {
					{
						curval = 0
						curvallevel = 1
					}
				} else {
					{
						curval = 0
						curvallevel = 0
					}
				}
			}
		} else {
			if m == 1 {
				{
					curval = curlist.auxfield.int
					curvallevel = 1
				}
			} else {
				{
					curval = curlist.auxfield.hh.lh
					curvallevel = 0
				}
			}
		}
	case 80:
		if curlist.modefield == 0 {
			{
				curval = 0
				curvallevel = 0
			}
		} else {
			{
				nest[nestptr] = curlist
				p = nestptr
				for abs_(nest[p].modefield) != 1 {
					p = (p - 1)
				}
				{
					curval = nest[p].pgfield
					curvallevel = 0
				}
			}
		}
	case 82:
		{
			if m == 0 {
				curval = deadcycles
			} else {
				curval = insertpenalties
			}
			curvallevel = 0
		}
	case 81:
		{
			if (pagecontents == 0) && (!outputactive) {
				if m == 0 {
					curval = 1073741823
				} else {
					curval = 0
				}
			} else {
				curval = pagesofar[m]
			}
			curvallevel = 1
		}
	case 84:
		{
			if eqtb[3412].hh.rh == 0 {
				curval = 0
			} else {
				curval = mem[eqtb[3412].hh.rh].hh.lh
			}
			curvallevel = 0
		}
	case 83:
		{
			scaneightbitint()
			if eqtb[(3678+curval)].hh.rh == 0 {
				curval = 0
			} else {
				curval = mem[(eqtb[(3678+curval)].hh.rh + m)].int
			}
			curvallevel = 1
		}
	case 68:
		{
			curval = curchr
			curvallevel = 0
		}
	case 69:
		{
			curval = curchr
			curvallevel = 0
		}
	case 77:
		{
			findfontdimen(false)
			fontinfo[fmemptr].int = 0
			{
				curval = fontinfo[curval].int
				curvallevel = 1
			}
		}
	case 78:
		{
			scanfontident()
			if m == 0 {
				{
					curval = hyphenchar[curval]
					curvallevel = 0
				}
			} else {
				{
					curval = skewchar[curval]
					curvallevel = 0
				}
			}
		}
	case 89:
		{
			scaneightbitint()
			switch m {
			case 0:
				curval = eqtb[(5318 + curval)].int
			case 1:
				curval = eqtb[(5851 + curval)].int
			case 2:
				curval = eqtb[(2900 + curval)].hh.rh
			case 3:
				curval = eqtb[(3156 + curval)].hh.rh
			}
			curvallevel = m
		}
	case 70:
		if curchr > 2 {
			{
				if curchr == 3 {
					curval = line
				} else {
					curval = lastbadness
				}
				curvallevel = 0
			}
		} else {
			{
				if curchr == 2 {
					curval = 0
				} else {
					curval = 0
				}
				curvallevel = curchr
				if (!(curlist.tailfield >= himemmin)) && (curlist.modefield != 0) {
					switch curchr {
					case 0:
						if mem[curlist.tailfield].hh.b0 == 12 {
							curval = mem[(curlist.tailfield + 1)].int
						}
					case 1:
						if mem[curlist.tailfield].hh.b0 == 11 {
							curval = mem[(curlist.tailfield + 1)].int
						}
					case 2:
						if mem[curlist.tailfield].hh.b0 == 10 {
							{
								curval = mem[(curlist.tailfield + 1)].hh.lh
								if mem[curlist.tailfield].hh.b1 == 99 {
									curvallevel = 3
								}
							}
						}
					}
				} else {
					if (curlist.modefield == 1) && (curlist.tailfield == curlist.headfield) {
						switch curchr {
						case 0:
							curval = lastpenalty
						case 1:
							curval = lastkern
						case 2:
							if lastglue != 65535 {
								curval = lastglue
							}
						}
					}
				}
			}
		}
	default:
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(685)
			}
			printcmdchr(curcmd, curchr)
			print_(686)
			printesc(537)
			{
				helpptr = 1
				helpline[0] = 684
			}
			error_()
			if level != 5 {
				{
					curval = 0
					curvallevel = 1
				}
			} else {
				{
					curval = 0
					curvallevel = 0
				}
			}
		}
	}
	for curvallevel > level {
		{
			if curvallevel == 2 {
				curval = mem[(curval + 1)].int
			} else {
				if curvallevel == 3 {
					muerror()
				}
			}
			curvallevel = (curvallevel - 1)
		}
	}
	if negative {
		if curvallevel >= 2 {
			{
				curval = newspec(curval)
				{
					mem[(curval + 1)].int = (-mem[(curval + 1)].int)
					mem[(curval + 2)].int = (-mem[(curval + 2)].int)
					mem[(curval + 3)].int = (-mem[(curval + 3)].int)
				}
			}
		} else {
			curval = (-curval)
		}
	} else {
		if (curvallevel >= 2) && (curvallevel <= 3) {
			mem[curval].hh.rh = (mem[curval].hh.rh + 1)
		}
	}
}

/* procedure: scanint */
func scanint() {
	var (
		negative bool
		m        int
		d        int
		vacuous  bool
		OKsofar  bool
	)
	radix = 0
	OKsofar = true
	negative = false
	for {
		for {
			getxtoken()
			if !(curcmd != 10) {
				break
			}
		}
		if curtok == 3117 {
			{
				negative = (!negative)
				curtok = 3115
			}
		}
		if !(curtok != 3115) {
			break
		}
	}
	if curtok == 3168 {
		{
			gettoken()
			if curtok < 4095 {
				{
					curval = curchr
					if curcmd <= 2 {
						if curcmd == 2 {
							alignstate = (alignstate + 1)
						} else {
							alignstate = (alignstate - 1)
						}
					}
				}
			} else {
				if curtok < 4352 {
					curval = (curtok - 4096)
				} else {
					curval = (curtok - 4352)
				}
			}
			if curval > 255 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(698)
					}
					{
						helpptr = 2
						helpline[1] = 699
						helpline[0] = 700
					}
					curval = 48
					backerror()
				}
			} else {
				{
					getxtoken()
					if curcmd != 10 {
						backinput()
					}
				}
			}
		}
	} else {
		if (curcmd >= 68) && (curcmd <= 89) {
			scansomethinginternal(0, false)
		} else {
			{
				radix = 10
				m = 214748364
				if curtok == 3111 {
					{
						radix = 8
						m = 268435456
						getxtoken()
					}
				} else {
					if curtok == 3106 {
						{
							radix = 16
							m = 134217728
							getxtoken()
						}
					}
				}
				vacuous = true
				curval = 0
				for true {
					{
						if ((curtok < (3120 + radix)) && (curtok >= 3120)) && (curtok <= 3129) {
							d = (curtok - 3120)
						} else {
							if radix == 16 {
								if (curtok <= 2886) && (curtok >= 2881) {
									d = (curtok - 2871)
								} else {
									if (curtok <= 3142) && (curtok >= 3137) {
										d = (curtok - 3127)
									} else {
										goto L30
									}
								}
							} else {
								goto L30
							}
						}
						vacuous = false
						if (curval >= m) && (((curval > m) || (d > 7)) || (radix != 10)) {
							{
								if OKsofar {
									{
										{
											if interaction == 3 {
												// empty
											}
											printnl(262)
											print_(701)
										}
										{
											helpptr = 2
											helpline[1] = 702
											helpline[0] = 703
										}
										error_()
										curval = 2147483647
										OKsofar = false
									}
								}
							}
						} else {
							curval = ((curval * radix) + d)
						}
						getxtoken()
					}
				}
			L30:
				// empty
				if vacuous {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(664)
						}
						{
							helpptr = 3
							helpline[2] = 665
							helpline[1] = 666
							helpline[0] = 667
						}
						backerror()
					}
				} else {
					if curcmd != 10 {
						backinput()
					}
				}
			}
		}
	}
	if negative {
		curval = (-curval)
	}
}

/* procedure: scandimen */
func scandimen(mu bool, inf bool, shortcut bool) {
	var (
		negative   bool
		f          int
		num        int
		denom      int
		k          int
		kk         int
		p          int
		q          int
		v          int
		savecurval int
	)
	f = 0
	aritherror = false
	curorder = 0
	negative = false
	if !shortcut {
		{
			negative = false
			for {
				for {
					getxtoken()
					if !(curcmd != 10) {
						break
					}
				}
				if curtok == 3117 {
					{
						negative = (!negative)
						curtok = 3115
					}
				}
				if !(curtok != 3115) {
					break
				}
			}
			if (curcmd >= 68) && (curcmd <= 89) {
				if mu {
					{
						scansomethinginternal(3, false)
						if curvallevel >= 2 {
							{
								v = mem[(curval + 1)].int
								deleteglueref(curval)
								curval = v
							}
						}
						if curvallevel == 3 {
							goto L89
						}
						if curvallevel != 0 {
							muerror()
						}
					}
				} else {
					{
						scansomethinginternal(1, false)
						if curvallevel == 1 {
							goto L89
						}
					}
				}
			} else {
				{
					backinput()
					if curtok == 3116 {
						curtok = 3118
					}
					if curtok != 3118 {
						scanint()
					} else {
						{
							radix = 10
							curval = 0
						}
					}
					if curtok == 3116 {
						curtok = 3118
					}
					if (radix == 10) && (curtok == 3118) {
						{
							k = 0
							p = 0
							gettoken()
							for true {
								{
									getxtoken()
									if (curtok > 3129) || (curtok < 3120) {
										goto L31
									}
									if k < 17 {
										{
											q = getavail
											mem[q].hh.rh = p
											mem[q].hh.lh = (curtok - 3120)
											p = q
											k = (k + 1)
										}
									}
								}
							}
						L31:
							for kk := k; kk >= 1; kk-- {
								{
									dig[(kk - 1)] = mem[p].hh.lh
									q = p
									p = mem[p].hh.rh
									{
										mem[q].hh.rh = avail
										avail = q
									}
								}
							}
							f = rounddecimals(k)
							if curcmd != 10 {
								backinput()
							}
						}
					}
				}
			}
		}
	}
	if curval < 0 {
		{
			negative = (!negative)
			curval = (-curval)
		}
	}
	if inf {
		if scankeyword(311) {
			{
				curorder = 1
				for scankeyword(108) {
					{
						if curorder == 3 {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(705)
								}
								print_(706)
								{
									helpptr = 1
									helpline[0] = 707
								}
								error_()
							}
						} else {
							curorder = (curorder + 1)
						}
					}
				}
				goto L88
			}
		}
	}
	savecurval = curval
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	if (curcmd < 68) || (curcmd > 89) {
		backinput()
	} else {
		{
			if mu {
				{
					scansomethinginternal(3, false)
					if curvallevel >= 2 {
						{
							v = mem[(curval + 1)].int
							deleteglueref(curval)
							curval = v
						}
					}
					if curvallevel != 3 {
						muerror()
					}
				}
			} else {
				scansomethinginternal(1, false)
			}
			v = curval
			goto L40
		}
	}
	if mu {
		goto L45
	}
	if scankeyword(708) {
		v = fontinfo[(6 + parambase[eqtb[3934].hh.rh])].int
	} else {
		if scankeyword(709) {
			v = fontinfo[(5 + parambase[eqtb[3934].hh.rh])].int
		} else {
			goto L45
		}
	}
	{
		getxtoken()
		if curcmd != 10 {
			backinput()
		}
	}
L40:
	curval = multandadd(savecurval, v, xnoverd(v, f, 65536), 1073741823)
	goto L89
L45:
	// empty
	if mu {
		if scankeyword(337) {
			goto L88
		} else {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(705)
				}
				print_(710)
				{
					helpptr = 4
					helpline[3] = 711
					helpline[2] = 712
					helpline[1] = 713
					helpline[0] = 714
				}
				error_()
				goto L88
			}
		}
	}
	if scankeyword(704) {
		{
			preparemag()
			if eqtb[5280].int != 1000 {
				{
					curval = xnoverd(curval, 1000, eqtb[5280].int)
					f = (((1000 * f) + (65536 * remainder)) / eqtb[5280].int)
					curval = (curval + (f / 65536))
					f = (f % 65536)
				}
			}
		}
	}
	if scankeyword(397) {
		goto L88
	}
	if scankeyword(715) {
		{
			num = 7227
			denom = 100
		}
	} else {
		if scankeyword(716) {
			{
				num = 12
				denom = 1
			}
		} else {
			if scankeyword(717) {
				{
					num = 7227
					denom = 254
				}
			} else {
				if scankeyword(718) {
					{
						num = 7227
						denom = 2540
					}
				} else {
					if scankeyword(719) {
						{
							num = 7227
							denom = 7200
						}
					} else {
						if scankeyword(720) {
							{
								num = 1238
								denom = 1157
							}
						} else {
							if scankeyword(721) {
								{
									num = 14856
									denom = 1157
								}
							} else {
								if scankeyword(722) {
									goto L30
								} else {
									{
										{
											if interaction == 3 {
												// empty
											}
											printnl(262)
											print_(705)
										}
										print_(723)
										{
											helpptr = 6
											helpline[5] = 724
											helpline[4] = 725
											helpline[3] = 726
											helpline[2] = 712
											helpline[1] = 713
											helpline[0] = 714
										}
										error_()
										goto L32
									}
								}
							}
						}
					}
				}
			}
		}
	}
	curval = xnoverd(curval, num, denom)
	f = (((num * f) + (65536 * remainder)) / denom)
	curval = (curval + (f / 65536))
	f = (f % 65536)
L32:
	// empty
L88:
	if curval >= 16384 {
		aritherror = true
	} else {
		curval = ((curval * 65536) + f)
	}
L30:
	// empty
	{
		getxtoken()
		if curcmd != 10 {
			backinput()
		}
	}
L89:
	if aritherror || (abs_(curval) >= 1073741824) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(727)
			}
			{
				helpptr = 2
				helpline[1] = 728
				helpline[0] = 729
			}
			error_()
			curval = 1073741823
			aritherror = false
		}
	}
	if negative {
		curval = (-curval)
	}
}

/* procedure: scanglue */
func scanglue(level int) {
	var (
		negative bool
		q        int
		mu       bool
	)
	mu = (level == 3)
	negative = false
	for {
		for {
			getxtoken()
			if !(curcmd != 10) {
				break
			}
		}
		if curtok == 3117 {
			{
				negative = (!negative)
				curtok = 3115
			}
		}
		if !(curtok != 3115) {
			break
		}
	}
	if (curcmd >= 68) && (curcmd <= 89) {
		{
			scansomethinginternal(level, negative)
			if curvallevel >= 2 {
				{
					if curvallevel != level {
						muerror()
					}
					goto L10
				}
			}
			if curvallevel == 0 {
				scandimen(mu, false, true)
			} else {
				if level == 3 {
					muerror()
				}
			}
		}
	} else {
		{
			backinput()
			scandimen(mu, false, false)
			if negative {
				curval = (-curval)
			}
		}
	}
	q = newspec(0)
	mem[(q + 1)].int = curval
	if scankeyword(730) {
		{
			scandimen(mu, true, false)
			mem[(q + 2)].int = curval
			mem[q].hh.b0 = curorder
		}
	}
	if scankeyword(731) {
		{
			scandimen(mu, true, false)
			mem[(q + 3)].int = curval
			mem[q].hh.b1 = curorder
		}
	}
	curval = q
L10:
	// empty
}

/* function: scanrulespec */
func scanrulespec() int {
	var (
		q int
	)
	q = newrule
	if curcmd == 35 {
		mem[(q + 1)].int = 26214
	} else {
		{
			mem[(q + 3)].int = 26214
			mem[(q + 2)].int = 0
		}
	}
L21:
	if scankeyword(732) {
		{
			scandimen(false, false, false)
			mem[(q + 1)].int = curval
			goto L21
		}
	}
	if scankeyword(733) {
		{
			scandimen(false, false, false)
			mem[(q + 3)].int = curval
			goto L21
		}
	}
	if scankeyword(734) {
		{
			scandimen(false, false, false)
			mem[(q + 2)].int = curval
			goto L21
		}
	}
	scanrulespec = q
}

/* function: strtoks */
func strtoks(b int) int {
	var (
		p int
		q int
		t int
		k int
	)
	{
		if (poolptr + 1) > poolsize {
			overflow(257, (poolsize - initpoolptr))
		}
	}
	p = 29997
	mem[p].hh.rh = 0
	k = b
	for k < poolptr {
		{
			t = strpool[k]
			if t == 32 {
				t = 2592
			} else {
				t = (3072 + t)
			}
			{
				{
					q = avail
					if q == 0 {
						q = getavail
					} else {
						{
							avail = mem[q].hh.rh
							mem[q].hh.rh = 0
						}
					}
				}
				mem[p].hh.rh = q
				mem[q].hh.lh = t
				p = q
			}
			k = (k + 1)
		}
	}
	poolptr = b
	strtoks = p
}

/* function: thetoks */
func thetoks() int {
	var (
		oldsetting int
		p          int
		q          int
		r          int
		b          int
	)
	getxtoken()
	scansomethinginternal(5, false)
	if curvallevel >= 4 {
		{
			p = 29997
			mem[p].hh.rh = 0
			if curvallevel == 4 {
				{
					q = getavail
					mem[p].hh.rh = q
					mem[q].hh.lh = (4095 + curval)
					p = q
				}
			} else {
				if curval != 0 {
					{
						r = mem[curval].hh.rh
						for r != 0 {
							{
								{
									{
										q = avail
										if q == 0 {
											q = getavail
										} else {
											{
												avail = mem[q].hh.rh
												mem[q].hh.rh = 0
											}
										}
									}
									mem[p].hh.rh = q
									mem[q].hh.lh = mem[r].hh.lh
									p = q
								}
								r = mem[r].hh.rh
							}
						}
					}
				}
			}
			thetoks = p
		}
	} else {
		{
			oldsetting = selector
			selector = 21
			b = poolptr
			switch curvallevel {
			case 0:
				printint(curval)
			case 1:
				{
					printscaled(curval)
					print_(397)
				}
			case 2:
				{
					printspec(curval, 397)
					deleteglueref(curval)
				}
			case 3:
				{
					printspec(curval, 337)
					deleteglueref(curval)
				}
			}
			selector = oldsetting
			thetoks = strtoks(b)
		}
	}
}

/* procedure: insthetoks */
func insthetoks() {
	mem[29988].hh.rh = thetoks
	begintokenlist(mem[29997].hh.rh, 4)
}

/* procedure: convtoks */
func convtoks() {
	var (
		oldsetting        int
		c                 int
		savescannerstatus int
		b                 int
	)
	c = curchr
	switch c {
	case 0:
		scanint()
	case 1:
		scanint()
	case 2:
		{
			savescannerstatus = scannerstatus
			scannerstatus = 0
			gettoken()
			scannerstatus = savescannerstatus
		}
	case 3:
		{
			savescannerstatus = scannerstatus
			scannerstatus = 0
			gettoken()
			scannerstatus = savescannerstatus
		}
	case 4:
		scanfontident()
	case 5:
		if jobname == 0 {
			openlogfile()
		}
	}
	oldsetting = selector
	selector = 21
	b = poolptr
	switch c {
	case 0:
		printint(curval)
	case 1:
		printromanint(curval)
	case 2:
		if curcs != 0 {
			sprintcs(curcs)
		} else {
			printchar(curchr)
		}
	case 3:
		printmeaning()
	case 4:
		{
			print_(fontname[curval])
			if fontsize[curval] != fontdsize[curval] {
				{
					print_(741)
					printscaled(fontsize[curval])
					print_(397)
				}
			}
		}
	case 5:
		print_(jobname)
	}
	selector = oldsetting
	mem[29988].hh.rh = strtoks(b)
	begintokenlist(mem[29997].hh.rh, 4)
}

/* function: scantoks */
func scantoks(macrodef bool, xpand bool) int {
	var (
		t         int
		s         int
		p         int
		q         int
		unbalance int
		hashbrace int
	)
	if macrodef {
		scannerstatus = 2
	} else {
		scannerstatus = 5
	}
	warningindex = curcs
	defref = getavail
	mem[defref].hh.lh = 0
	p = defref
	hashbrace = 0
	t = 3120
	if macrodef {
		{
			for true {
				{
				L22:
					gettoken()
					if curtok < 768 {
						goto L31
					}
					if curcmd == 6 {
						{
							s = (3328 + curchr)
							gettoken()
							if curtok < 512 {
								{
									hashbrace = curtok
									{
										q = getavail
										mem[p].hh.rh = q
										mem[q].hh.lh = curtok
										p = q
									}
									{
										q = getavail
										mem[p].hh.rh = q
										mem[q].hh.lh = 3584
										p = q
									}
									goto L30
								}
							}
							if t == 3129 {
								{
									{
										if interaction == 3 {
											// empty
										}
										printnl(262)
										print_(744)
									}
									{
										helpptr = 2
										helpline[1] = 745
										helpline[0] = 746
									}
									error_()
									goto L22
								}
							} else {
								{
									t = (t + 1)
									if curtok != t {
										{
											{
												if interaction == 3 {
													// empty
												}
												printnl(262)
												print_(747)
											}
											{
												helpptr = 2
												helpline[1] = 748
												helpline[0] = 749
											}
											backerror()
										}
									}
									curtok = s
								}
							}
						}
					}
					{
						q = getavail
						mem[p].hh.rh = q
						mem[q].hh.lh = curtok
						p = q
					}
				}
			}
		L31:
			{
				q = getavail
				mem[p].hh.rh = q
				mem[q].hh.lh = 3584
				p = q
			}
			if curcmd == 2 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(657)
					}
					alignstate = (alignstate + 1)
					{
						helpptr = 2
						helpline[1] = 742
						helpline[0] = 743
					}
					error_()
					goto L40
				}
			}
		L30:
			// empty
		}
	} else {
		scanleftbrace()
	}
	unbalance = 1
	for true {
		{
			if xpand {
				{
					for true {
						{
							getnext()
							if curcmd <= 100 {
								goto L32
							}
							if curcmd != 109 {
								expand()
							} else {
								{
									q = thetoks
									if mem[29997].hh.rh != 0 {
										{
											mem[p].hh.rh = mem[29997].hh.rh
											p = q
										}
									}
								}
							}
						}
					}
				L32:
					xtoken()
				}
			} else {
				gettoken()
			}
			if curtok < 768 {
				if curcmd < 2 {
					unbalance = (unbalance + 1)
				} else {
					{
						unbalance = (unbalance - 1)
						if unbalance == 0 {
							goto L40
						}
					}
				}
			} else {
				if curcmd == 6 {
					if macrodef {
						{
							s = curtok
							if xpand {
								getxtoken()
							} else {
								gettoken()
							}
							if curcmd != 6 {
								if (curtok <= 3120) || (curtok > t) {
									{
										{
											if interaction == 3 {
												// empty
											}
											printnl(262)
											print_(750)
										}
										sprintcs(warningindex)
										{
											helpptr = 3
											helpline[2] = 751
											helpline[1] = 752
											helpline[0] = 753
										}
										backerror()
										curtok = s
									}
								} else {
									curtok = (1232 + curchr)
								}
							}
						}
					}
				}
			}
			{
				q = getavail
				mem[p].hh.rh = q
				mem[q].hh.lh = curtok
				p = q
			}
		}
	}
L40:
	scannerstatus = 0
	if hashbrace != 0 {
		{
			q = getavail
			mem[p].hh.rh = q
			mem[q].hh.lh = hashbrace
			p = q
		}
	}
	scantoks = p
}

/* procedure: readtoks */
func readtoks(n int, r int) {
	var (
		p int
		q int
		s int
		m int
	)
	scannerstatus = 2
	warningindex = r
	defref = getavail
	mem[defref].hh.lh = 0
	p = defref
	{
		q = getavail
		mem[p].hh.rh = q
		mem[q].hh.lh = 3584
		p = q
	}
	if (n < 0) || (n > 15) {
		m = 16
	} else {
		m = n
	}
	s = alignstate
	alignstate = 1000000
	for {
		beginfilereading()
		curinput.namefield = (m + 1)
		if readopen[m] == 2 {
			if interaction > 1 {
				if n < 0 {
					{
						print_(338)
						terminput()
					}
				} else {
					{
						println_()
						sprintcs(r)
						{
							print_(61)
							terminput()
						}
						n = (-1)
					}
				}
			} else {
				fatalerror(754)
			}
		} else {
			if readopen[m] == 1 {
				if inputln(readfile[m], false) {
					readopen[m] = 0
				} else {
					{
						aclose(readfile[m])
						readopen[m] = 2
					}
				}
			} else {
				{
					if !inputln(readfile[m], true) {
						{
							aclose(readfile[m])
							readopen[m] = 2
							if alignstate != 1000000 {
								{
									runaway()
									{
										if interaction == 3 {
											// empty
										}
										printnl(262)
										print_(755)
									}
									printesc(534)
									{
										helpptr = 1
										helpline[0] = 756
									}
									alignstate = 1000000
									curinput.limitfield = 0
									error_()
								}
							}
						}
					}
				}
			}
		}
		curinput.limitfield = last
		if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
			curinput.limitfield = (curinput.limitfield - 1)
		} else {
			buffer[curinput.limitfield] = eqtb[5311].int
		}
		first = (curinput.limitfield + 1)
		curinput.locfield = curinput.startfield
		curinput.statefield = 33
		for true {
			{
				gettoken()
				if curtok == 0 {
					goto L30
				}
				if alignstate < 1000000 {
					{
						for {
							gettoken()
							if !(curtok == 0) {
								break
							}
						}
						alignstate = 1000000
						goto L30
					}
				}
				{
					q = getavail
					mem[p].hh.rh = q
					mem[q].hh.lh = curtok
					p = q
				}
			}
		}
	L30:
		endfilereading()
		if !(alignstate == 1000000) {
			break
		}
	}
	curval = defref
	scannerstatus = 0
	alignstate = s
}

/* procedure: passtext */
func passtext() {
	var (
		l                 int
		savescannerstatus int
	)
	savescannerstatus = scannerstatus
	scannerstatus = 1
	l = 0
	skipline = line
	for true {
		{
			getnext()
			if curcmd == 106 {
				{
					if l == 0 {
						goto L30
					}
					if curchr == 2 {
						l = (l - 1)
					}
				}
			} else {
				if curcmd == 105 {
					l = (l + 1)
				}
			}
		}
	}
L30:
	scannerstatus = savescannerstatus
}

/* procedure: changeiflimit */
func changeiflimit(l int, p int) {
	var (
		q int
	)
	if p == condptr {
		iflimit = l
	} else {
		{
			q = condptr
			for true {
				{
					if q == 0 {
						confusion(757)
					}
					if mem[q].hh.rh == p {
						{
							mem[q].hh.b0 = l
							goto L10
						}
					}
					q = mem[q].hh.rh
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: conditional */
func conditional() {
	var (
		b                 bool
		r                 int
		m                 int
		n                 int
		p                 int
		q                 int
		savescannerstatus int
		savecondptr       int
		thisif            int
	)
	{
		p = getnode(2)
		mem[p].hh.rh = condptr
		mem[p].hh.b0 = iflimit
		mem[p].hh.b1 = curif
		mem[(p + 1)].int = ifline
		condptr = p
		curif = curchr
		iflimit = 1
		ifline = line
	}
	savecondptr = condptr
	thisif = curchr
	switch thisif {
	case 0:
		{
			{
				getxtoken()
				if curcmd == 0 {
					if curchr == 257 {
						{
							curcmd = 13
							curchr = (curtok - 4096)
						}
					}
				}
			}
			if (curcmd > 13) || (curchr > 255) {
				{
					m = 0
					n = 256
				}
			} else {
				{
					m = curcmd
					n = curchr
				}
			}
			{
				getxtoken()
				if curcmd == 0 {
					if curchr == 257 {
						{
							curcmd = 13
							curchr = (curtok - 4096)
						}
					}
				}
			}
			if (curcmd > 13) || (curchr > 255) {
				{
					curcmd = 0
					curchr = 256
				}
			}
			if thisif == 0 {
				b = (n == curchr)
			} else {
				b = (m == curcmd)
			}
		}
	case 1:
		{
			{
				getxtoken()
				if curcmd == 0 {
					if curchr == 257 {
						{
							curcmd = 13
							curchr = (curtok - 4096)
						}
					}
				}
			}
			if (curcmd > 13) || (curchr > 255) {
				{
					m = 0
					n = 256
				}
			} else {
				{
					m = curcmd
					n = curchr
				}
			}
			{
				getxtoken()
				if curcmd == 0 {
					if curchr == 257 {
						{
							curcmd = 13
							curchr = (curtok - 4096)
						}
					}
				}
			}
			if (curcmd > 13) || (curchr > 255) {
				{
					curcmd = 0
					curchr = 256
				}
			}
			if thisif == 0 {
				b = (n == curchr)
			} else {
				b = (m == curcmd)
			}
		}
	case 2:
		{
			if thisif == 2 {
				scanint()
			} else {
				scandimen(false, false, false)
			}
			n = curval
			for {
				getxtoken()
				if !(curcmd != 10) {
					break
				}
			}
			if (curtok >= 3132) && (curtok <= 3134) {
				r = (curtok - 3072)
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(781)
					}
					printcmdchr(105, thisif)
					{
						helpptr = 1
						helpline[0] = 782
					}
					backerror()
					r = 61
				}
			}
			if thisif == 2 {
				scanint()
			} else {
				scandimen(false, false, false)
			}
			switch r {
			case 60:
				b = (n < curval)
			case 61:
				b = (n == curval)
			case 62:
				b = (n > curval)
			}
		}
	case 3:
		{
			if thisif == 2 {
				scanint()
			} else {
				scandimen(false, false, false)
			}
			n = curval
			for {
				getxtoken()
				if !(curcmd != 10) {
					break
				}
			}
			if (curtok >= 3132) && (curtok <= 3134) {
				r = (curtok - 3072)
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(781)
					}
					printcmdchr(105, thisif)
					{
						helpptr = 1
						helpline[0] = 782
					}
					backerror()
					r = 61
				}
			}
			if thisif == 2 {
				scanint()
			} else {
				scandimen(false, false, false)
			}
			switch r {
			case 60:
				b = (n < curval)
			case 61:
				b = (n == curval)
			case 62:
				b = (n > curval)
			}
		}
	case 4:
		{
			scanint()
			b = ((curval & 1) != 0)
		}
	case 5:
		b = (abs_(curlist.modefield) == 1)
	case 6:
		b = (abs_(curlist.modefield) == 102)
	case 7:
		b = (abs_(curlist.modefield) == 203)
	case 8:
		b = (curlist.modefield < 0)
	case 9:
		{
			scaneightbitint()
			p = eqtb[(3678 + curval)].hh.rh
			if thisif == 9 {
				b = (p == 0)
			} else {
				if p == 0 {
					b = false
				} else {
					if thisif == 10 {
						b = (mem[p].hh.b0 == 0)
					} else {
						b = (mem[p].hh.b0 == 1)
					}
				}
			}
		}
	case 10:
		{
			scaneightbitint()
			p = eqtb[(3678 + curval)].hh.rh
			if thisif == 9 {
				b = (p == 0)
			} else {
				if p == 0 {
					b = false
				} else {
					if thisif == 10 {
						b = (mem[p].hh.b0 == 0)
					} else {
						b = (mem[p].hh.b0 == 1)
					}
				}
			}
		}
	case 11:
		{
			scaneightbitint()
			p = eqtb[(3678 + curval)].hh.rh
			if thisif == 9 {
				b = (p == 0)
			} else {
				if p == 0 {
					b = false
				} else {
					if thisif == 10 {
						b = (mem[p].hh.b0 == 0)
					} else {
						b = (mem[p].hh.b0 == 1)
					}
				}
			}
		}
	case 12:
		{
			savescannerstatus = scannerstatus
			scannerstatus = 0
			getnext()
			n = curcs
			p = curcmd
			q = curchr
			getnext()
			if curcmd != p {
				b = false
			} else {
				if curcmd < 111 {
					b = (curchr == q)
				} else {
					{
						p = mem[curchr].hh.rh
						q = mem[eqtb[n].hh.rh].hh.rh
						if p == q {
							b = true
						} else {
							{
								for (p != 0) && (q != 0) {
									if mem[p].hh.lh != mem[q].hh.lh {
										p = 0
									} else {
										{
											p = mem[p].hh.rh
											q = mem[q].hh.rh
										}
									}
								}
								b = ((p == 0) && (q == 0))
							}
						}
					}
				}
			}
			scannerstatus = savescannerstatus
		}
	case 13:
		{
			scanfourbitint()
			b = (readopen[curval] == 2)
		}
	case 14:
		b = true
	case 15:
		b = false
	case 16:
		{
			scanint()
			n = curval
			if eqtb[5299].int > 1 {
				{
					begindiagnostic()
					print_(783)
					printint(n)
					printchar(125)
					enddiagnostic(false)
				}
			}
			for n != 0 {
				{
					passtext()
					if condptr == savecondptr {
						if curchr == 4 {
							n = (n - 1)
						} else {
							goto L50
						}
					} else {
						if curchr == 2 {
							{
								p = condptr
								ifline = mem[(p + 1)].int
								curif = mem[p].hh.b1
								iflimit = mem[p].hh.b0
								condptr = mem[p].hh.rh
								freenode(p, 2)
							}
						}
					}
				}
			}
			changeiflimit(4, savecondptr)
			goto L10
		}
	}
	if eqtb[5299].int > 1 {
		{
			begindiagnostic()
			if b {
				print_(779)
			} else {
				print_(780)
			}
			enddiagnostic(false)
		}
	}
	if b {
		{
			changeiflimit(3, savecondptr)
			goto L10
		}
	}
	for true {
		{
			passtext()
			if condptr == savecondptr {
				{
					if curchr != 4 {
						goto L50
					}
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(777)
					}
					printesc(775)
					{
						helpptr = 1
						helpline[0] = 778
					}
					error_()
				}
			} else {
				if curchr == 2 {
					{
						p = condptr
						ifline = mem[(p + 1)].int
						curif = mem[p].hh.b1
						iflimit = mem[p].hh.b0
						condptr = mem[p].hh.rh
						freenode(p, 2)
					}
				}
			}
		}
	}
L50:
	if curchr == 2 {
		{
			p = condptr
			ifline = mem[(p + 1)].int
			curif = mem[p].hh.b1
			iflimit = mem[p].hh.b0
			condptr = mem[p].hh.rh
			freenode(p, 2)
		}
	} else {
		iflimit = 2
	}
L10:
	// empty
}

/* procedure: beginname */
func beginname() {
	areadelimiter = 0
	extdelimiter = 0
}

/* function: morename */
func morename(c byte) bool {
	if c == 32 {
		morename = false
	} else {
		{
			{
				if (poolptr + 1) > poolsize {
					overflow(257, (poolsize - initpoolptr))
				}
			}
			{
				strpool[poolptr] = c
				poolptr = (poolptr + 1)
			}
			if (c == 62) || (c == 58) {
				{
					areadelimiter = (poolptr - strstart[strptr])
					extdelimiter = 0
				}
			} else {
				if (c == 46) && (extdelimiter == 0) {
					extdelimiter = (poolptr - strstart[strptr])
				}
			}
			morename = true
		}
	}
}

/* procedure: endname */
func endname() {
	if (strptr + 3) > maxstrings {
		overflow(258, (maxstrings - initstrptr))
	}
	if areadelimiter == 0 {
		curarea = 338
	} else {
		{
			curarea = strptr
			strstart[(strptr + 1)] = (strstart[strptr] + areadelimiter)
			strptr = (strptr + 1)
		}
	}
	if extdelimiter == 0 {
		{
			curext = 338
			curname = makestring
		}
	} else {
		{
			curname = strptr
			strstart[(strptr + 1)] = (((strstart[strptr] + extdelimiter) - areadelimiter) - 1)
			strptr = (strptr + 1)
			curext = makestring
		}
	}
}

/* procedure: packfilename */
func packfilename(n int, a int, e int) {
	var (
		k int
		c byte
		j int
	)
	k = 0
	for j := strstart[a]; j <= (strstart[(a+1)] - 1); j++ {
		{
			c = strpool[j]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	for j := strstart[n]; j <= (strstart[(n+1)] - 1); j++ {
		{
			c = strpool[j]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	for j := strstart[e]; j <= (strstart[(e+1)] - 1); j++ {
		{
			c = strpool[j]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	if k <= filenamesize {
		namelength = k
	} else {
		namelength = filenamesize
	}
	for k := (namelength + 1); k <= filenamesize; k++ {
		nameoffile[k] = " "
	}
}

/* procedure: packbufferedname */
func packbufferedname(n int, a int, b int) {
	var (
		k int
		c byte
		j int
	)
	if (((n + b) - a) + 5) > filenamesize {
		b = (((a + filenamesize) - n) - 5)
	}
	k = 0
	for j := 1; j <= n; j++ {
		{
			c = xord[TEXformatdefault[j]]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	for j := a; j <= b; j++ {
		{
			c = buffer[j]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	for j := 17; j <= 20; j++ {
		{
			c = xord[TEXformatdefault[j]]
			k = (k + 1)
			if k <= filenamesize {
				nameoffile[k] = xchr[c]
			}
		}
	}
	if k <= filenamesize {
		namelength = k
	} else {
		namelength = filenamesize
	}
	for k := (namelength + 1); k <= filenamesize; k++ {
		nameoffile[k] = " "
	}
}

/* function: makenamestring */
func makenamestring() int {
	var (
		k int
	)
	if (((poolptr + namelength) > poolsize) || (strptr == maxstrings)) || ((poolptr - strstart[strptr]) > 0) {
		makenamestring = 63
	} else {
		{
			for k := 1; k <= namelength; k++ {
				{
					strpool[poolptr] = xord[nameoffile[k]]
					poolptr = (poolptr + 1)
				}
			}
			makenamestring = makestring
		}
	}
}

/* function: amakenamestring */
func amakenamestring(f **alphafile_t) int {
	amakenamestring = makenamestring
}

/* function: bmakenamestring */
func bmakenamestring(f **bytefile_t) int {
	bmakenamestring = makenamestring
}

/* function: wmakenamestring */
func wmakenamestring(f **wordfile_t) int {
	wmakenamestring = makenamestring
}

/* procedure: scanfilename */
func scanfilename() {
	nameinprogress = true
	beginname()
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	for true {
		{
			if (curcmd > 12) || (curchr > 255) {
				{
					backinput()
					goto L30
				}
			}
			if !morename(curchr) {
				goto L30
			}
			getxtoken()
		}
	}
L30:
	endname()
	nameinprogress = false
}

/* procedure: packjobname */
func packjobname(s int) {
	curarea = 338
	curext = s
	curname = jobname
	packfilename(curname, curarea, curext)
}

/* procedure: promptfilename */
func promptfilename(s int, e int) {
	var (
		k int
	)
	if interaction == 2 {
		// empty
	}
	if s == 787 {
		{
			if interaction == 3 {
				// empty
			}
			printnl(262)
			print_(788)
		}
	} else {
		{
			if interaction == 3 {
				// empty
			}
			printnl(262)
			print_(789)
		}
	}
	printfilename(curname, curarea, curext)
	print_(790)
	if e == 791 {
		showcontext()
	}
	printnl(792)
	print_(s)
	if interaction < 2 {
		fatalerror(793)
	}
	breakin(termin, true)
	{
		print_(568)
		terminput()
	}
	{
		beginname()
		k = first
		for (buffer[k] == 32) && (k < last) {
			k = (k + 1)
		}
		for true {
			{
				if k == last {
					goto L30
				}
				if !morename(buffer[k]) {
					goto L30
				}
				k = (k + 1)
			}
		}
	L30:
		endname()
	}
	if curext == 338 {
		curext = e
	}
	packfilename(curname, curarea, curext)
}

/* procedure: openlogfile */
func openlogfile() {
	var (
		oldsetting int
		k          int
		l          int
		months     []byte
	)
	oldsetting = selector
	if jobname == 0 {
		jobname = 796
	}
	packjobname(797)
	for !aopenout(logfile) {
		{
			selector = 17
			promptfilename(799, 797)
		}
	}
	logname = amakenamestring(logfile)
	selector = 18
	logopened = true
	{
		write_(logfile, "This is TeX, Version 3.141592653")
		slowprint(formatident)
		print_(800)
		printint(sysday)
		printchar(32)
		months = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC"
		for k := ((3 * sysmonth) - 2); k <= (3 * sysmonth); k++ {
			write_(logfile, months[k])
		}
		printchar(32)
		printint(sysyear)
		printchar(32)
		printtwo((systime / 60))
		printchar(58)
		printtwo((systime % 60))
	}
	inputstack[inputptr] = curinput
	printnl(798)
	l = inputstack[0].limitfield
	if buffer[l] == eqtb[5311].int {
		l = (l - 1)
	}
	for k := 1; k <= l; k++ {
		print_(buffer[k])
	}
	println_()
	selector = (oldsetting + 2)
}

/* procedure: startinput */
func startinput() {
	scanfilename()
	if curext == 338 {
		curext = 791
	}
	packfilename(curname, curarea, curext)
	for true {
		{
			beginfilereading()
			if aopenin(inputfile[curinput.indexfield]) {
				goto L30
			}
			if curarea == 338 {
				{
					packfilename(curname, 784, curext)
					if aopenin(inputfile[curinput.indexfield]) {
						goto L30
					}
				}
			}
			endfilereading()
			promptfilename(787, 791)
		}
	}
L30:
	curinput.namefield = amakenamestring(inputfile[curinput.indexfield])
	if jobname == 0 {
		{
			jobname = curname
			openlogfile()
		}
	}
	if (termoffset + (strstart[(curinput.namefield+1)] - strstart[curinput.namefield])) > (maxprintline - 2) {
		println_()
	} else {
		if (termoffset > 0) || (fileoffset > 0) {
			printchar(32)
		}
	}
	printchar(40)
	openparens = (openparens + 1)
	slowprint(curinput.namefield)
	break_(termout)
	curinput.statefield = 33
	if curinput.namefield == (strptr - 1) {
		{
			{
				strptr = (strptr - 1)
				poolptr = strstart[strptr]
			}
			curinput.namefield = curname
		}
	}
	{
		line = 1
		if inputln(inputfile[curinput.indexfield], false) {
			// empty
		}
		firmuptheline()
		if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
			curinput.limitfield = (curinput.limitfield - 1)
		} else {
			buffer[curinput.limitfield] = eqtb[5311].int
		}
		first = (curinput.limitfield + 1)
		curinput.locfield = curinput.startfield
	}
}

/* function: readfontinfo */
func readfontinfo(u int, nom int, aire int, s int) int {
	var (
		k          int
		fileopened bool
		lf         int
		lh         int
		bc         int
		ec         int
		nw         int
		nh         int
		nd         int
		ni         int
		nl         int
		nk         int
		ne         int
		np         int
		f          int
		g          int
		a          byte
		b          byte
		c          byte
		d          byte
		qw         *fourquarters_t
		sw         int
		bchlabel   int
		bchar      int
		z          int
		alpha      int
		beta       int
	)
	g = 0
	fileopened = false
	if aire == 338 {
		packfilename(nom, 785, 811)
	} else {
		packfilename(nom, aire, 811)
	}
	if !bopenin(tfmfile) {
		goto L11
	}
	fileopened = true
	{
		{
			lf = *tfmfile
			if lf > 127 {
				goto L11
			}
			get_(tfmfile)
			lf = ((lf * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			lh = *tfmfile
			if lh > 127 {
				goto L11
			}
			get_(tfmfile)
			lh = ((lh * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			bc = *tfmfile
			if bc > 127 {
				goto L11
			}
			get_(tfmfile)
			bc = ((bc * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			ec = *tfmfile
			if ec > 127 {
				goto L11
			}
			get_(tfmfile)
			ec = ((ec * 256) + *tfmfile)
		}
		if (bc > (ec + 1)) || (ec > 255) {
			goto L11
		}
		if bc > 255 {
			{
				bc = 1
				ec = 0
			}
		}
		get_(tfmfile)
		{
			nw = *tfmfile
			if nw > 127 {
				goto L11
			}
			get_(tfmfile)
			nw = ((nw * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			nh = *tfmfile
			if nh > 127 {
				goto L11
			}
			get_(tfmfile)
			nh = ((nh * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			nd = *tfmfile
			if nd > 127 {
				goto L11
			}
			get_(tfmfile)
			nd = ((nd * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			ni = *tfmfile
			if ni > 127 {
				goto L11
			}
			get_(tfmfile)
			ni = ((ni * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			nl = *tfmfile
			if nl > 127 {
				goto L11
			}
			get_(tfmfile)
			nl = ((nl * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			nk = *tfmfile
			if nk > 127 {
				goto L11
			}
			get_(tfmfile)
			nk = ((nk * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			ne = *tfmfile
			if ne > 127 {
				goto L11
			}
			get_(tfmfile)
			ne = ((ne * 256) + *tfmfile)
		}
		get_(tfmfile)
		{
			np = *tfmfile
			if np > 127 {
				goto L11
			}
			get_(tfmfile)
			np = ((np * 256) + *tfmfile)
		}
		if lf != ((((((((((6 + lh) + ((ec - bc) + 1)) + nw) + nh) + nd) + ni) + nl) + nk) + ne) + np) {
			goto L11
		}
		if (((nw == 0) || (nh == 0)) || (nd == 0)) || (ni == 0) {
			goto L11
		}
	}
	lf = ((lf - 6) - lh)
	if np < 7 {
		lf = ((lf + 7) - np)
	}
	if (fontptr == fontmax) || ((fmemptr + lf) > fontmemsize) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(802)
			}
			sprintcs(u)
			printchar(61)
			printfilename(nom, aire, 338)
			if s >= 0 {
				{
					print_(741)
					printscaled(s)
					print_(397)
				}
			} else {
				if s != (-1000) {
					{
						print_(803)
						printint((-s))
					}
				}
			}
			print_(812)
			{
				helpptr = 4
				helpline[3] = 813
				helpline[2] = 814
				helpline[1] = 815
				helpline[0] = 816
			}
			error_()
			goto L30
		}
	}
	f = (fontptr + 1)
	charbase[f] = (fmemptr - bc)
	widthbase[f] = ((charbase[f] + ec) + 1)
	heightbase[f] = (widthbase[f] + nw)
	depthbase[f] = (heightbase[f] + nh)
	italicbase[f] = (depthbase[f] + nd)
	ligkernbase[f] = (italicbase[f] + ni)
	kernbase[f] = ((ligkernbase[f] + nl) - (256 * 128))
	extenbase[f] = ((kernbase[f] + (256 * 128)) + nk)
	parambase[f] = (extenbase[f] + ne)
	{
		if lh < 2 {
			goto L11
		}
		{
			get_(tfmfile)
			a = *tfmfile
			qw.b0 = (a + 0)
			get_(tfmfile)
			b = *tfmfile
			qw.b1 = (b + 0)
			get_(tfmfile)
			c = *tfmfile
			qw.b2 = (c + 0)
			get_(tfmfile)
			d = *tfmfile
			qw.b3 = (d + 0)
			fontcheck[f] = qw
		}
		get_(tfmfile)
		{
			z = *tfmfile
			if z > 127 {
				goto L11
			}
			get_(tfmfile)
			z = ((z * 256) + *tfmfile)
		}
		get_(tfmfile)
		z = ((z * 256) + *tfmfile)
		get_(tfmfile)
		z = ((z * 16) + (*tfmfile / 16))
		if z < 65536 {
			goto L11
		}
		for lh > 2 {
			{
				get_(tfmfile)
				get_(tfmfile)
				get_(tfmfile)
				get_(tfmfile)
				lh = (lh - 1)
			}
		}
		fontdsize[f] = z
		if s != (-1000) {
			if s >= 0 {
				z = s
			} else {
				z = xnoverd(z, (-s), 1000)
			}
		}
		fontsize[f] = z
	}
	for k := fmemptr; k <= (widthbase[f] - 1); k++ {
		{
			{
				get_(tfmfile)
				a = *tfmfile
				qw.b0 = (a + 0)
				get_(tfmfile)
				b = *tfmfile
				qw.b1 = (b + 0)
				get_(tfmfile)
				c = *tfmfile
				qw.b2 = (c + 0)
				get_(tfmfile)
				d = *tfmfile
				qw.b3 = (d + 0)
				fontinfo[k].qqqq = qw
			}
			if (((a >= nw) || ((b / 16) >= nh)) || ((b % 16) >= nd)) || ((c / 4) >= ni) {
				goto L11
			}
			switch c % 4 {
			case 1:
				if d >= nl {
					goto L11
				}
			case 3:
				if d >= ne {
					goto L11
				}
			case 2:
				{
					{
						if (d < bc) || (d > ec) {
							goto L11
						}
					}
					for d < ((k + bc) - fmemptr) {
						{
							qw = fontinfo[(charbase[f] + d)].qqqq
							if ((qw.b2 - 0) % 4) != 2 {
								goto L45
							}
							d = (qw.b3 - 0)
						}
					}
					if d == ((k + bc) - fmemptr) {
						goto L11
					}
				L45:
					// empty
				}
			default:
				// empty
			}
		}
	}
	{
		{
			alpha = 16
			for z >= 8388608 {
				{
					z = (z / 2)
					alpha = (alpha + alpha)
				}
			}
			beta = (256 / alpha)
			alpha = (alpha * z)
		}
		for k := widthbase[f]; k <= (ligkernbase[f] - 1); k++ {
			{
				get_(tfmfile)
				a = *tfmfile
				get_(tfmfile)
				b = *tfmfile
				get_(tfmfile)
				c = *tfmfile
				get_(tfmfile)
				d = *tfmfile
				sw = ((((((d * z) / 256) + (c * z)) / 256) + (b * z)) / beta)
				if a == 0 {
					fontinfo[k].int = sw
				} else {
					if a == 255 {
						fontinfo[k].int = (sw - alpha)
					} else {
						goto L11
					}
				}
			}
		}
		if fontinfo[widthbase[f]].int != 0 {
			goto L11
		}
		if fontinfo[heightbase[f]].int != 0 {
			goto L11
		}
		if fontinfo[depthbase[f]].int != 0 {
			goto L11
		}
		if fontinfo[italicbase[f]].int != 0 {
			goto L11
		}
	}
	bchlabel = 32767
	bchar = 256
	if nl > 0 {
		{
			for k := ligkernbase[f]; k <= ((kernbase[f] + (256 * 128)) - 1); k++ {
				{
					{
						get_(tfmfile)
						a = *tfmfile
						qw.b0 = (a + 0)
						get_(tfmfile)
						b = *tfmfile
						qw.b1 = (b + 0)
						get_(tfmfile)
						c = *tfmfile
						qw.b2 = (c + 0)
						get_(tfmfile)
						d = *tfmfile
						qw.b3 = (d + 0)
						fontinfo[k].qqqq = qw
					}
					if a > 128 {
						{
							if ((256 * c) + d) >= nl {
								goto L11
							}
							if a == 255 {
								if k == ligkernbase[f] {
									bchar = b
								}
							}
						}
					} else {
						{
							if b != bchar {
								{
									{
										if (b < bc) || (b > ec) {
											goto L11
										}
									}
									qw = fontinfo[(charbase[f] + b)].qqqq
									if !(qw.b0 > 0) {
										goto L11
									}
								}
							}
							if c < 128 {
								{
									{
										if (d < bc) || (d > ec) {
											goto L11
										}
									}
									qw = fontinfo[(charbase[f] + d)].qqqq
									if !(qw.b0 > 0) {
										goto L11
									}
								}
							} else {
								if ((256 * (c - 128)) + d) >= nk {
									goto L11
								}
							}
							if a < 128 {
								if (((k - ligkernbase[f]) + a) + 1) >= nl {
									goto L11
								}
							}
						}
					}
				}
			}
			if a == 255 {
				bchlabel = ((256 * c) + d)
			}
		}
	}
	for k := (kernbase[f] + (256 * 128)); k <= (extenbase[f] - 1); k++ {
		{
			get_(tfmfile)
			a = *tfmfile
			get_(tfmfile)
			b = *tfmfile
			get_(tfmfile)
			c = *tfmfile
			get_(tfmfile)
			d = *tfmfile
			sw = ((((((d * z) / 256) + (c * z)) / 256) + (b * z)) / beta)
			if a == 0 {
				fontinfo[k].int = sw
			} else {
				if a == 255 {
					fontinfo[k].int = (sw - alpha)
				} else {
					goto L11
				}
			}
		}
	}
	for k := extenbase[f]; k <= (parambase[f] - 1); k++ {
		{
			{
				get_(tfmfile)
				a = *tfmfile
				qw.b0 = (a + 0)
				get_(tfmfile)
				b = *tfmfile
				qw.b1 = (b + 0)
				get_(tfmfile)
				c = *tfmfile
				qw.b2 = (c + 0)
				get_(tfmfile)
				d = *tfmfile
				qw.b3 = (d + 0)
				fontinfo[k].qqqq = qw
			}
			if a != 0 {
				{
					{
						if (a < bc) || (a > ec) {
							goto L11
						}
					}
					qw = fontinfo[(charbase[f] + a)].qqqq
					if !(qw.b0 > 0) {
						goto L11
					}
				}
			}
			if b != 0 {
				{
					{
						if (b < bc) || (b > ec) {
							goto L11
						}
					}
					qw = fontinfo[(charbase[f] + b)].qqqq
					if !(qw.b0 > 0) {
						goto L11
					}
				}
			}
			if c != 0 {
				{
					{
						if (c < bc) || (c > ec) {
							goto L11
						}
					}
					qw = fontinfo[(charbase[f] + c)].qqqq
					if !(qw.b0 > 0) {
						goto L11
					}
				}
			}
			{
				{
					if (d < bc) || (d > ec) {
						goto L11
					}
				}
				qw = fontinfo[(charbase[f] + d)].qqqq
				if !(qw.b0 > 0) {
					goto L11
				}
			}
		}
	}
	{
		for k := 1; k <= np; k++ {
			if k == 1 {
				{
					get_(tfmfile)
					sw = *tfmfile
					if sw > 127 {
						sw = (sw - 256)
					}
					get_(tfmfile)
					sw = ((sw * 256) + *tfmfile)
					get_(tfmfile)
					sw = ((sw * 256) + *tfmfile)
					get_(tfmfile)
					fontinfo[parambase[f]].int = ((sw * 16) + (*tfmfile / 16))
				}
			} else {
				{
					get_(tfmfile)
					a = *tfmfile
					get_(tfmfile)
					b = *tfmfile
					get_(tfmfile)
					c = *tfmfile
					get_(tfmfile)
					d = *tfmfile
					sw = ((((((d * z) / 256) + (c * z)) / 256) + (b * z)) / beta)
					if a == 0 {
						fontinfo[((parambase[f] + k) - 1)].int = sw
					} else {
						if a == 255 {
							fontinfo[((parambase[f] + k) - 1)].int = (sw - alpha)
						} else {
							goto L11
						}
					}
				}
			}
		}
		if eof_(tfmfile) {
			goto L11
		}
		for k := (np + 1); k <= 7; k++ {
			fontinfo[((parambase[f] + k) - 1)].int = 0
		}
	}
	if np >= 7 {
		fontparams[f] = np
	} else {
		fontparams[f] = 7
	}
	hyphenchar[f] = eqtb[5309].int
	skewchar[f] = eqtb[5310].int
	if bchlabel < nl {
		bcharlabel[f] = (bchlabel + ligkernbase[f])
	} else {
		bcharlabel[f] = 0
	}
	fontbchar[f] = (bchar + 0)
	fontfalsebchar[f] = (bchar + 0)
	if bchar <= ec {
		if bchar >= bc {
			{
				qw = fontinfo[(charbase[f] + bchar)].qqqq
				if qw.b0 > 0 {
					fontfalsebchar[f] = 256
				}
			}
		}
	}
	fontname[f] = nom
	fontarea[f] = aire
	fontbc[f] = bc
	fontec[f] = ec
	fontglue[f] = 0
	charbase[f] = (charbase[f] - 0)
	widthbase[f] = (widthbase[f] - 0)
	ligkernbase[f] = (ligkernbase[f] - 0)
	kernbase[f] = (kernbase[f] - 0)
	extenbase[f] = (extenbase[f] - 0)
	parambase[f] = (parambase[f] - 1)
	fmemptr = (fmemptr + lf)
	fontptr = f
	g = f
	goto L30
L11:
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(802)
	}
	sprintcs(u)
	printchar(61)
	printfilename(nom, aire, 338)
	if s >= 0 {
		{
			print_(741)
			printscaled(s)
			print_(397)
		}
	} else {
		if s != (-1000) {
			{
				print_(803)
				printint((-s))
			}
		}
	}
	if fileopened {
		print_(804)
	} else {
		print_(805)
	}
	{
		helpptr = 5
		helpline[4] = 806
		helpline[3] = 807
		helpline[2] = 808
		helpline[1] = 809
		helpline[0] = 810
	}
	error_()
L30:
	if fileopened {
		bclose(tfmfile)
	}
	readfontinfo = g
}

/* procedure: charwarning */
func charwarning(f int, c byte) {
	if eqtb[5298].int > 0 {
		{
			begindiagnostic()
			printnl(825)
			print_(c)
			print_(826)
			slowprint(fontname[f])
			printchar(33)
			enddiagnostic(false)
		}
	}
}

/* function: newcharacter */
func newcharacter(f int, c byte) int {
	var (
		p int
	)
	if fontbc[f] <= c {
		if fontec[f] >= c {
			if fontinfo[((charbase[f]+c)+0)].qqqq.b0 > 0 {
				{
					p = getavail
					mem[p].hh.b0 = f
					mem[p].hh.b1 = (c + 0)
					newcharacter = p
					goto L10
				}
			}
		}
	}
	charwarning(f, c)
	newcharacter = 0
L10:
	// empty
}

/* procedure: writedvi */
func writedvi(a int, b int) {
	var (
		k int
	)
	for k := a; k <= b; k++ {
		write_(dvifile, dvibuf[k])
	}
}

/* procedure: dviswap */
func dviswap() {
	if dvilimit == dvibufsize {
		{
			writedvi(0, (halfbuf - 1))
			dvilimit = halfbuf
			dvioffset = (dvioffset + dvibufsize)
			dviptr = 0
		}
	} else {
		{
			writedvi(halfbuf, (dvibufsize - 1))
			dvilimit = dvibufsize
		}
	}
	dvigone = (dvigone + halfbuf)
}

/* procedure: dvifour */
func dvifour(x int) {
	if x >= 0 {
		{
			dvibuf[dviptr] = (x / 16777216)
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	} else {
		{
			x = (x + 1073741824)
			x = (x + 1073741824)
			{
				dvibuf[dviptr] = ((x / 16777216) + 128)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
		}
	}
	x = (x % 16777216)
	{
		dvibuf[dviptr] = (x / 65536)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	x = (x % 65536)
	{
		dvibuf[dviptr] = (x / 256)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (x % 256)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
}

/* procedure: dvipop */
func dvipop(l int) {
	if (l == (dvioffset + dviptr)) && (dviptr > 0) {
		dviptr = (dviptr - 1)
	} else {
		{
			dvibuf[dviptr] = 142
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
}

/* procedure: dvifontdef */
func dvifontdef(f int) {
	var (
		k int
	)
	{
		dvibuf[dviptr] = 243
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (f - 1)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (fontcheck[f].b0 - 0)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (fontcheck[f].b1 - 0)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (fontcheck[f].b2 - 0)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (fontcheck[f].b3 - 0)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	dvifour(fontsize[f])
	dvifour(fontdsize[f])
	{
		dvibuf[dviptr] = (strstart[(fontarea[f]+1)] - strstart[fontarea[f]])
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	{
		dvibuf[dviptr] = (strstart[(fontname[f]+1)] - strstart[fontname[f]])
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	for k := strstart[fontarea[f]]; k <= (strstart[(fontarea[f]+1)] - 1); k++ {
		{
			dvibuf[dviptr] = strpool[k]
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
	for k := strstart[fontname[f]]; k <= (strstart[(fontname[f]+1)] - 1); k++ {
		{
			dvibuf[dviptr] = strpool[k]
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
}

/* procedure: movement */
func movement(w int, o byte) {
	var (
		mstate int
		p      int
		q      int
		k      int
	)
	q = getnode(3)
	mem[(q + 1)].int = w
	mem[(q + 2)].int = (dvioffset + dviptr)
	if o == 157 {
		{
			mem[q].hh.rh = downptr
			downptr = q
		}
	} else {
		{
			mem[q].hh.rh = rightptr
			rightptr = q
		}
	}
	p = mem[q].hh.rh
	mstate = 0
	for p != 0 {
		{
			if mem[(p+1)].int == w {
				switch mstate + mem[p].hh.lh {
				case 3:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 5)
							mem[p].hh.lh = 1
							goto L40
						}
					}
				case 4:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 5)
							mem[p].hh.lh = 1
							goto L40
						}
					}
				case 15:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 5)
							mem[p].hh.lh = 1
							goto L40
						}
					}
				case 16:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 5)
							mem[p].hh.lh = 1
							goto L40
						}
					}
				case 5:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 10)
							mem[p].hh.lh = 2
							goto L40
						}
					}
				case 9:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 10)
							mem[p].hh.lh = 2
							goto L40
						}
					}
				case 11:
					if mem[(p+2)].int < dvigone {
						goto L45
					} else {
						{
							k = (mem[(p+2)].int - dvioffset)
							if k < 0 {
								k = (k + dvibufsize)
							}
							dvibuf[k] = (dvibuf[k] + 10)
							mem[p].hh.lh = 2
							goto L40
						}
					}
				case 1:
					goto L40
				case 2:
					goto L40
				case 8:
					goto L40
				case 13:
					goto L40
				default:
					// empty
				}
			} else {
				switch mstate + mem[p].hh.lh {
				case 1:
					mstate = 6
				case 2:
					mstate = 12
				case 8:
					goto L45
				case 13:
					goto L45
				default:
					// empty
				}
			}
			p = mem[p].hh.rh
		}
	}
L45:
	// empty
	mem[q].hh.lh = 3
	if abs_(w) >= 8388608 {
		{
			{
				dvibuf[dviptr] = (o + 3)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			dvifour(w)
			goto L10
		}
	}
	if abs_(w) >= 32768 {
		{
			{
				dvibuf[dviptr] = (o + 2)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			if w < 0 {
				w = (w + 16777216)
			}
			{
				dvibuf[dviptr] = (w / 65536)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			w = (w % 65536)
			goto L2
		}
	}
	if abs_(w) >= 128 {
		{
			{
				dvibuf[dviptr] = (o + 1)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			if w < 0 {
				w = (w + 65536)
			}
			goto L2
		}
	}
	{
		dvibuf[dviptr] = o
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	if w < 0 {
		w = (w + 256)
	}
	goto L1
L2:
	{
		dvibuf[dviptr] = (w / 256)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
L1:
	{
		dvibuf[dviptr] = (w % 256)
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	goto L10
L40:
	mem[q].hh.lh = mem[p].hh.lh
	if mem[q].hh.lh == 1 {
		{
			{
				dvibuf[dviptr] = (o + 4)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			for mem[q].hh.rh != p {
				{
					q = mem[q].hh.rh
					switch mem[q].hh.lh {
					case 3:
						mem[q].hh.lh = 5
					case 4:
						mem[q].hh.lh = 6
					default:
						// empty
					}
				}
			}
		}
	} else {
		{
			{
				dvibuf[dviptr] = (o + 9)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			for mem[q].hh.rh != p {
				{
					q = mem[q].hh.rh
					switch mem[q].hh.lh {
					case 3:
						mem[q].hh.lh = 4
					case 5:
						mem[q].hh.lh = 6
					default:
						// empty
					}
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: prunemovements */
func prunemovements(l int) {
	var (
		p int
	)
	for downptr != 0 {
		{
			if mem[(downptr+2)].int < l {
				goto L30
			}
			p = downptr
			downptr = mem[p].hh.rh
			freenode(p, 3)
		}
	}
L30:
	for rightptr != 0 {
		{
			if mem[(rightptr+2)].int < l {
				goto L10
			}
			p = rightptr
			rightptr = mem[p].hh.rh
			freenode(p, 3)
		}
	}
L10:
	// empty
}

/* procedure: specialout */
func specialout(p int) {
	var (
		oldsetting int
		k          int
	)
	if curh != dvih {
		{
			movement((curh - dvih), 143)
			dvih = curh
		}
	}
	if curv != dviv {
		{
			movement((curv - dviv), 157)
			dviv = curv
		}
	}
	oldsetting = selector
	selector = 21
	showtokenlist(mem[mem[(p+1)].hh.rh].hh.rh, 0, (poolsize - poolptr))
	selector = oldsetting
	{
		if (poolptr + 1) > poolsize {
			overflow(257, (poolsize - initpoolptr))
		}
	}
	if (poolptr - strstart[strptr]) < 256 {
		{
			{
				dvibuf[dviptr] = 239
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			{
				dvibuf[dviptr] = (poolptr - strstart[strptr])
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
		}
	} else {
		{
			{
				dvibuf[dviptr] = 242
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			dvifour((poolptr - strstart[strptr]))
		}
	}
	for k := strstart[strptr]; k <= (poolptr - 1); k++ {
		{
			dvibuf[dviptr] = strpool[k]
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
	poolptr = strstart[strptr]
}

/* procedure: writeout */
func writeout(p int) {
	var (
		oldsetting int
		oldmode    int
		j          int
		q          int
		r          int
	)
	q = getavail
	mem[q].hh.lh = 637
	r = getavail
	mem[q].hh.rh = r
	mem[r].hh.lh = 6717
	begintokenlist(q, 4)
	begintokenlist(mem[(p+1)].hh.rh, 15)
	q = getavail
	mem[q].hh.lh = 379
	begintokenlist(q, 4)
	oldmode = curlist.modefield
	curlist.modefield = 0
	curcs = writeloc
	q = scantoks(false, true)
	gettoken()
	if curtok != 6717 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1297)
			}
			{
				helpptr = 2
				helpline[1] = 1298
				helpline[0] = 1012
			}
			error_()
			for {
				gettoken()
				if !(curtok == 6717) {
					break
				}
			}
		}
	}
	curlist.modefield = oldmode
	endtokenlist()
	oldsetting = selector
	j = mem[(p + 1)].hh.lh
	if writeopen[j] {
		selector = j
	} else {
		{
			if (j == 17) && (selector == 19) {
				selector = 18
			}
			printnl(338)
		}
	}
	tokenshow(defref)
	println_()
	flushlist(defref)
	selector = oldsetting
}

/* procedure: outwhat */
func outwhat(p int) {
	var (
		j int
	)
	switch mem[p].hh.b1 {
	case 0:
		if !doingleaders {
			{
				j = mem[(p + 1)].hh.lh
				if mem[p].hh.b1 == 1 {
					writeout(p)
				} else {
					{
						if writeopen[j] {
							aclose(writefile[j])
						}
						if mem[p].hh.b1 == 2 {
							writeopen[j] = false
						} else {
							if j < 16 {
								{
									curname = mem[(p + 1)].hh.rh
									curarea = mem[(p + 2)].hh.lh
									curext = mem[(p + 2)].hh.rh
									if curext == 338 {
										curext = 791
									}
									packfilename(curname, curarea, curext)
									for !aopenout(writefile[j]) {
										promptfilename(1300, 791)
									}
									writeopen[j] = true
								}
							}
						}
					}
				}
			}
		}
	case 1:
		if !doingleaders {
			{
				j = mem[(p + 1)].hh.lh
				if mem[p].hh.b1 == 1 {
					writeout(p)
				} else {
					{
						if writeopen[j] {
							aclose(writefile[j])
						}
						if mem[p].hh.b1 == 2 {
							writeopen[j] = false
						} else {
							if j < 16 {
								{
									curname = mem[(p + 1)].hh.rh
									curarea = mem[(p + 2)].hh.lh
									curext = mem[(p + 2)].hh.rh
									if curext == 338 {
										curext = 791
									}
									packfilename(curname, curarea, curext)
									for !aopenout(writefile[j]) {
										promptfilename(1300, 791)
									}
									writeopen[j] = true
								}
							}
						}
					}
				}
			}
		}
	case 2:
		if !doingleaders {
			{
				j = mem[(p + 1)].hh.lh
				if mem[p].hh.b1 == 1 {
					writeout(p)
				} else {
					{
						if writeopen[j] {
							aclose(writefile[j])
						}
						if mem[p].hh.b1 == 2 {
							writeopen[j] = false
						} else {
							if j < 16 {
								{
									curname = mem[(p + 1)].hh.rh
									curarea = mem[(p + 2)].hh.lh
									curext = mem[(p + 2)].hh.rh
									if curext == 338 {
										curext = 791
									}
									packfilename(curname, curarea, curext)
									for !aopenout(writefile[j]) {
										promptfilename(1300, 791)
									}
									writeopen[j] = true
								}
							}
						}
					}
				}
			}
		}
	case 3:
		specialout(p)
	case 4:
		// empty
	default:
		confusion(1299)
	}
}

/* procedure: hlistout */
func hlistout() {
	var (
		baseline          int
		leftedge          int
		saveh             int
		savev             int
		thisbox           int
		gorder            int
		gsign             int
		p                 int
		saveloc           int
		leaderbox         int
		leaderwd          int
		lx                int
		outerdoingleaders bool
		edge              int
		gluetemp          float64
		curglue           float64
		curg              int
	)
	curg = 0
	curglue = 0
	thisbox = tempptr
	gorder = mem[(thisbox + 5)].hh.b1
	gsign = mem[(thisbox + 5)].hh.b0
	p = mem[(thisbox + 5)].hh.rh
	curs = (curs + 1)
	if curs > 0 {
		{
			dvibuf[dviptr] = 141
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
	if curs > maxpush {
		maxpush = curs
	}
	saveloc = (dvioffset + dviptr)
	baseline = curv
	leftedge = curh
	for p != 0 {
	L21:
		if p >= himemmin {
			{
				if curh != dvih {
					{
						movement((curh - dvih), 143)
						dvih = curh
					}
				}
				if curv != dviv {
					{
						movement((curv - dviv), 157)
						dviv = curv
					}
				}
				for {
					f = mem[p].hh.b0
					c = mem[p].hh.b1
					if f != dvif {
						{
							if !fontused[f] {
								{
									dvifontdef(f)
									fontused[f] = true
								}
							}
							if f <= 64 {
								{
									dvibuf[dviptr] = (f + 170)
									dviptr = (dviptr + 1)
									if dviptr == dvilimit {
										dviswap()
									}
								}
							} else {
								{
									{
										dvibuf[dviptr] = 235
										dviptr = (dviptr + 1)
										if dviptr == dvilimit {
											dviswap()
										}
									}
									{
										dvibuf[dviptr] = (f - 1)
										dviptr = (dviptr + 1)
										if dviptr == dvilimit {
											dviswap()
										}
									}
								}
							}
							dvif = f
						}
					}
					if c >= 128 {
						{
							dvibuf[dviptr] = 128
							dviptr = (dviptr + 1)
							if dviptr == dvilimit {
								dviswap()
							}
						}
					}
					{
						dvibuf[dviptr] = (c - 0)
						dviptr = (dviptr + 1)
						if dviptr == dvilimit {
							dviswap()
						}
					}
					curh = (curh + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+c)].qqqq.b0)].int)
					p = mem[p].hh.rh
					if !(!(p >= himemmin)) {
						break
					}
				}
				dvih = curh
			}
		} else {
			{
				switch mem[p].hh.b0 {
				case 0:
					if mem[(p+5)].hh.rh == 0 {
						curh = (curh + mem[(p+1)].int)
					} else {
						{
							saveh = dvih
							savev = dviv
							curv = (baseline + mem[(p+4)].int)
							tempptr = p
							edge = curh
							if mem[p].hh.b0 == 1 {
								vlistout()
							} else {
								hlistout()
							}
							dvih = saveh
							dviv = savev
							curh = (edge + mem[(p+1)].int)
							curv = baseline
						}
					}
				case 1:
					if mem[(p+5)].hh.rh == 0 {
						curh = (curh + mem[(p+1)].int)
					} else {
						{
							saveh = dvih
							savev = dviv
							curv = (baseline + mem[(p+4)].int)
							tempptr = p
							edge = curh
							if mem[p].hh.b0 == 1 {
								vlistout()
							} else {
								hlistout()
							}
							dvih = saveh
							dviv = savev
							curh = (edge + mem[(p+1)].int)
							curv = baseline
						}
					}
				case 2:
					{
						ruleht = mem[(p + 3)].int
						ruledp = mem[(p + 2)].int
						rulewd = mem[(p + 1)].int
						goto L14
					}
				case 8:
					outwhat(p)
				case 10:
					{
						g = mem[(p + 1)].hh.lh
						rulewd = (mem[(g+1)].int - curg)
						if gsign != 0 {
							{
								if gsign == 1 {
									{
										if mem[g].hh.b0 == gorder {
											{
												curglue = (curglue + mem[(g+2)].int)
												gluetemp = (mem[(thisbox+6)].gr * curglue)
												if gluetemp > 1000000000 {
													gluetemp = 1000000000
												} else {
													if gluetemp < (-1000000000) {
														gluetemp = (-1000000000)
													}
												}
												curg = round_(gluetemp)
											}
										}
									}
								} else {
									if mem[g].hh.b1 == gorder {
										{
											curglue = (curglue - mem[(g+3)].int)
											gluetemp = (mem[(thisbox+6)].gr * curglue)
											if gluetemp > 1000000000 {
												gluetemp = 1000000000
											} else {
												if gluetemp < (-1000000000) {
													gluetemp = (-1000000000)
												}
											}
											curg = round_(gluetemp)
										}
									}
								}
							}
						}
						rulewd = (rulewd + curg)
						if mem[p].hh.b1 >= 100 {
							{
								leaderbox = mem[(p + 1)].hh.rh
								if mem[leaderbox].hh.b0 == 2 {
									{
										ruleht = mem[(leaderbox + 3)].int
										ruledp = mem[(leaderbox + 2)].int
										goto L14
									}
								}
								leaderwd = mem[(leaderbox + 1)].int
								if (leaderwd > 0) && (rulewd > 0) {
									{
										rulewd = (rulewd + 10)
										edge = (curh + rulewd)
										lx = 0
										if mem[p].hh.b1 == 100 {
											{
												saveh = curh
												curh = (leftedge + (leaderwd * ((curh - leftedge) / leaderwd)))
												if curh < saveh {
													curh = (curh + leaderwd)
												}
											}
										} else {
											{
												lq = (rulewd / leaderwd)
												lr = (rulewd % leaderwd)
												if mem[p].hh.b1 == 101 {
													curh = (curh + (lr / 2))
												} else {
													{
														lx = (lr / (lq + 1))
														curh = (curh + ((lr - ((lq - 1) * lx)) / 2))
													}
												}
											}
										}
										for (curh + leaderwd) <= edge {
											{
												curv = (baseline + mem[(leaderbox+4)].int)
												if curv != dviv {
													{
														movement((curv - dviv), 157)
														dviv = curv
													}
												}
												savev = dviv
												if curh != dvih {
													{
														movement((curh - dvih), 143)
														dvih = curh
													}
												}
												saveh = dvih
												tempptr = leaderbox
												outerdoingleaders = doingleaders
												doingleaders = true
												if mem[leaderbox].hh.b0 == 1 {
													vlistout()
												} else {
													hlistout()
												}
												doingleaders = outerdoingleaders
												dviv = savev
												dvih = saveh
												curv = baseline
												curh = ((saveh + leaderwd) + lx)
											}
										}
										curh = (edge - 10)
										goto L15
									}
								}
							}
						}
						goto L13
					}
				case 11:
					curh = (curh + mem[(p+1)].int)
				case 9:
					curh = (curh + mem[(p+1)].int)
				case 6:
					{
						mem[29988] = mem[(p + 1)]
						mem[29988].hh.rh = mem[p].hh.rh
						p = 29988
						goto L21
					}
				default:
					// empty
				}
				goto L15
			L14:
				if ruleht == (-1073741824) {
					ruleht = mem[(thisbox + 3)].int
				}
				if ruledp == (-1073741824) {
					ruledp = mem[(thisbox + 2)].int
				}
				ruleht = (ruleht + ruledp)
				if (ruleht > 0) && (rulewd > 0) {
					{
						if curh != dvih {
							{
								movement((curh - dvih), 143)
								dvih = curh
							}
						}
						curv = (baseline + ruledp)
						if curv != dviv {
							{
								movement((curv - dviv), 157)
								dviv = curv
							}
						}
						{
							dvibuf[dviptr] = 132
							dviptr = (dviptr + 1)
							if dviptr == dvilimit {
								dviswap()
							}
						}
						dvifour(ruleht)
						dvifour(rulewd)
						curv = baseline
						dvih = (dvih + rulewd)
					}
				}
			L13:
				curh = (curh + rulewd)
			L15:
				p = mem[p].hh.rh
			}
		}
	}
	prunemovements(saveloc)
	if curs > 0 {
		dvipop(saveloc)
	}
	curs = (curs - 1)
}

/* procedure: vlistout */
func vlistout() {
	var (
		leftedge          int
		topedge           int
		saveh             int
		savev             int
		thisbox           int
		gorder            int
		gsign             int
		p                 int
		saveloc           int
		leaderbox         int
		leaderht          int
		lx                int
		outerdoingleaders bool
		edge              int
		gluetemp          float64
		curglue           float64
		curg              int
	)
	curg = 0
	curglue = 0
	thisbox = tempptr
	gorder = mem[(thisbox + 5)].hh.b1
	gsign = mem[(thisbox + 5)].hh.b0
	p = mem[(thisbox + 5)].hh.rh
	curs = (curs + 1)
	if curs > 0 {
		{
			dvibuf[dviptr] = 141
			dviptr = (dviptr + 1)
			if dviptr == dvilimit {
				dviswap()
			}
		}
	}
	if curs > maxpush {
		maxpush = curs
	}
	saveloc = (dvioffset + dviptr)
	leftedge = curh
	curv = (curv - mem[(thisbox+3)].int)
	topedge = curv
	for p != 0 {
		{
			if p >= himemmin {
				confusion(828)
			} else {
				{
					switch mem[p].hh.b0 {
					case 0:
						if mem[(p+5)].hh.rh == 0 {
							curv = ((curv + mem[(p+3)].int) + mem[(p+2)].int)
						} else {
							{
								curv = (curv + mem[(p+3)].int)
								if curv != dviv {
									{
										movement((curv - dviv), 157)
										dviv = curv
									}
								}
								saveh = dvih
								savev = dviv
								curh = (leftedge + mem[(p+4)].int)
								tempptr = p
								if mem[p].hh.b0 == 1 {
									vlistout()
								} else {
									hlistout()
								}
								dvih = saveh
								dviv = savev
								curv = (savev + mem[(p+2)].int)
								curh = leftedge
							}
						}
					case 1:
						if mem[(p+5)].hh.rh == 0 {
							curv = ((curv + mem[(p+3)].int) + mem[(p+2)].int)
						} else {
							{
								curv = (curv + mem[(p+3)].int)
								if curv != dviv {
									{
										movement((curv - dviv), 157)
										dviv = curv
									}
								}
								saveh = dvih
								savev = dviv
								curh = (leftedge + mem[(p+4)].int)
								tempptr = p
								if mem[p].hh.b0 == 1 {
									vlistout()
								} else {
									hlistout()
								}
								dvih = saveh
								dviv = savev
								curv = (savev + mem[(p+2)].int)
								curh = leftedge
							}
						}
					case 2:
						{
							ruleht = mem[(p + 3)].int
							ruledp = mem[(p + 2)].int
							rulewd = mem[(p + 1)].int
							goto L14
						}
					case 8:
						outwhat(p)
					case 10:
						{
							g = mem[(p + 1)].hh.lh
							ruleht = (mem[(g+1)].int - curg)
							if gsign != 0 {
								{
									if gsign == 1 {
										{
											if mem[g].hh.b0 == gorder {
												{
													curglue = (curglue + mem[(g+2)].int)
													gluetemp = (mem[(thisbox+6)].gr * curglue)
													if gluetemp > 1000000000 {
														gluetemp = 1000000000
													} else {
														if gluetemp < (-1000000000) {
															gluetemp = (-1000000000)
														}
													}
													curg = round_(gluetemp)
												}
											}
										}
									} else {
										if mem[g].hh.b1 == gorder {
											{
												curglue = (curglue - mem[(g+3)].int)
												gluetemp = (mem[(thisbox+6)].gr * curglue)
												if gluetemp > 1000000000 {
													gluetemp = 1000000000
												} else {
													if gluetemp < (-1000000000) {
														gluetemp = (-1000000000)
													}
												}
												curg = round_(gluetemp)
											}
										}
									}
								}
							}
							ruleht = (ruleht + curg)
							if mem[p].hh.b1 >= 100 {
								{
									leaderbox = mem[(p + 1)].hh.rh
									if mem[leaderbox].hh.b0 == 2 {
										{
											rulewd = mem[(leaderbox + 1)].int
											ruledp = 0
											goto L14
										}
									}
									leaderht = (mem[(leaderbox+3)].int + mem[(leaderbox+2)].int)
									if (leaderht > 0) && (ruleht > 0) {
										{
											ruleht = (ruleht + 10)
											edge = (curv + ruleht)
											lx = 0
											if mem[p].hh.b1 == 100 {
												{
													savev = curv
													curv = (topedge + (leaderht * ((curv - topedge) / leaderht)))
													if curv < savev {
														curv = (curv + leaderht)
													}
												}
											} else {
												{
													lq = (ruleht / leaderht)
													lr = (ruleht % leaderht)
													if mem[p].hh.b1 == 101 {
														curv = (curv + (lr / 2))
													} else {
														{
															lx = (lr / (lq + 1))
															curv = (curv + ((lr - ((lq - 1) * lx)) / 2))
														}
													}
												}
											}
											for (curv + leaderht) <= edge {
												{
													curh = (leftedge + mem[(leaderbox+4)].int)
													if curh != dvih {
														{
															movement((curh - dvih), 143)
															dvih = curh
														}
													}
													saveh = dvih
													curv = (curv + mem[(leaderbox+3)].int)
													if curv != dviv {
														{
															movement((curv - dviv), 157)
															dviv = curv
														}
													}
													savev = dviv
													tempptr = leaderbox
													outerdoingleaders = doingleaders
													doingleaders = true
													if mem[leaderbox].hh.b0 == 1 {
														vlistout()
													} else {
														hlistout()
													}
													doingleaders = outerdoingleaders
													dviv = savev
													dvih = saveh
													curh = leftedge
													curv = (((savev - mem[(leaderbox+3)].int) + leaderht) + lx)
												}
											}
											curv = (edge - 10)
											goto L15
										}
									}
								}
							}
							goto L13
						}
					case 11:
						curv = (curv + mem[(p+1)].int)
					default:
						// empty
					}
					goto L15
				L14:
					if rulewd == (-1073741824) {
						rulewd = mem[(thisbox + 1)].int
					}
					ruleht = (ruleht + ruledp)
					curv = (curv + ruleht)
					if (ruleht > 0) && (rulewd > 0) {
						{
							if curh != dvih {
								{
									movement((curh - dvih), 143)
									dvih = curh
								}
							}
							if curv != dviv {
								{
									movement((curv - dviv), 157)
									dviv = curv
								}
							}
							{
								dvibuf[dviptr] = 137
								dviptr = (dviptr + 1)
								if dviptr == dvilimit {
									dviswap()
								}
							}
							dvifour(ruleht)
							dvifour(rulewd)
						}
					}
					goto L15
				L13:
					curv = (curv + ruleht)
				}
			}
		L15:
			p = mem[p].hh.rh
		}
	}
	prunemovements(saveloc)
	if curs > 0 {
		dvipop(saveloc)
	}
	curs = (curs - 1)
}

/* procedure: shipout */
func shipout(p int) {
	var (
		pageloc    int
		j          int
		k          int
		s          int
		oldsetting int
	)
	if eqtb[5297].int > 0 {
		{
			printnl(338)
			println_()
			print_(829)
		}
	}
	if termoffset > (maxprintline - 9) {
		println_()
	} else {
		if (termoffset > 0) || (fileoffset > 0) {
			printchar(32)
		}
	}
	printchar(91)
	j = 9
	for (eqtb[(5318+j)].int == 0) && (j > 0) {
		j = (j - 1)
	}
	for k := 0; k <= j; k++ {
		{
			printint(eqtb[(5318 + k)].int)
			if k < j {
				printchar(46)
			}
		}
	}
	break_(termout)
	if eqtb[5297].int > 0 {
		{
			printchar(93)
			begindiagnostic()
			showbox(p)
			enddiagnostic(true)
		}
	}
	if (((mem[(p+3)].int > 1073741823) || (mem[(p+2)].int > 1073741823)) || (((mem[(p+3)].int + mem[(p+2)].int) + eqtb[5849].int) > 1073741823)) || ((mem[(p+1)].int + eqtb[5848].int) > 1073741823) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(833)
			}
			{
				helpptr = 2
				helpline[1] = 834
				helpline[0] = 835
			}
			error_()
			if eqtb[5297].int <= 0 {
				{
					begindiagnostic()
					printnl(836)
					showbox(p)
					enddiagnostic(true)
				}
			}
			goto L30
		}
	}
	if ((mem[(p+3)].int + mem[(p+2)].int) + eqtb[5849].int) > maxv {
		maxv = ((mem[(p+3)].int + mem[(p+2)].int) + eqtb[5849].int)
	}
	if (mem[(p+1)].int + eqtb[5848].int) > maxh {
		maxh = (mem[(p+1)].int + eqtb[5848].int)
	}
	dvih = 0
	dviv = 0
	curh = eqtb[5848].int
	dvif = 0
	if outputfilename == 0 {
		{
			if jobname == 0 {
				openlogfile()
			}
			packjobname(794)
			for !bopenout(dvifile) {
				promptfilename(795, 794)
			}
			outputfilename = bmakenamestring(dvifile)
		}
	}
	if totalpages == 0 {
		{
			{
				dvibuf[dviptr] = 247
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			{
				dvibuf[dviptr] = 2
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			dvifour(25400000)
			dvifour(473628672)
			preparemag()
			dvifour(eqtb[5280].int)
			oldsetting = selector
			selector = 21
			print_(827)
			printint(eqtb[5286].int)
			printchar(46)
			printtwo(eqtb[5285].int)
			printchar(46)
			printtwo(eqtb[5284].int)
			printchar(58)
			printtwo((eqtb[5283].int / 60))
			printtwo((eqtb[5283].int % 60))
			selector = oldsetting
			{
				dvibuf[dviptr] = (poolptr - strstart[strptr])
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			for s := strstart[strptr]; s <= (poolptr - 1); s++ {
				{
					dvibuf[dviptr] = strpool[s]
					dviptr = (dviptr + 1)
					if dviptr == dvilimit {
						dviswap()
					}
				}
			}
			poolptr = strstart[strptr]
		}
	}
	pageloc = (dvioffset + dviptr)
	{
		dvibuf[dviptr] = 139
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	for k := 0; k <= 9; k++ {
		dvifour(eqtb[(5318 + k)].int)
	}
	dvifour(lastbop)
	lastbop = pageloc
	curv = (mem[(p+3)].int + eqtb[5849].int)
	tempptr = p
	if mem[p].hh.b0 == 1 {
		vlistout()
	} else {
		hlistout()
	}
	{
		dvibuf[dviptr] = 140
		dviptr = (dviptr + 1)
		if dviptr == dvilimit {
			dviswap()
		}
	}
	totalpages = (totalpages + 1)
	curs = (-1)
L30:
	// empty
	if eqtb[5297].int <= 0 {
		printchar(93)
	}
	deadcycles = 0
	break_(termout)
	flushnodelist(p)
}

/* procedure: scanspec */
func scanspec(c int, threecodes bool) {
	var (
		s        int
		speccode int
	)
	if threecodes {
		s = savestack[(saveptr + 0)].int
	}
	if scankeyword(842) {
		speccode = 0
	} else {
		if scankeyword(843) {
			speccode = 1
		} else {
			{
				speccode = 1
				curval = 0
				goto L40
			}
		}
	}
	scandimen(false, false, false)
L40:
	if threecodes {
		{
			savestack[(saveptr + 0)].int = s
			saveptr = (saveptr + 1)
		}
	}
	savestack[(saveptr + 0)].int = speccode
	savestack[(saveptr + 1)].int = curval
	saveptr = (saveptr + 2)
	newsavelevel(c)
	scanleftbrace()
}

/* function: hpack */
func hpack(p int, w int, m int) int {
	var (
		r  int
		q  int
		h  int
		d  int
		x  int
		s  int
		g  int
		o  int
		f  int
		i  *fourquarters_t
		hd byte
	)
	lastbadness = 0
	r = getnode(7)
	mem[r].hh.b0 = 0
	mem[r].hh.b1 = 0
	mem[(r + 4)].int = 0
	q = (r + 5)
	mem[q].hh.rh = p
	h = 0
	d = 0
	x = 0
	totalstretch[0] = 0
	totalshrink[0] = 0
	totalstretch[1] = 0
	totalshrink[1] = 0
	totalstretch[2] = 0
	totalshrink[2] = 0
	totalstretch[3] = 0
	totalshrink[3] = 0
	for p != 0 {
		{
		L21:
			for p >= himemmin {
				{
					f = mem[p].hh.b0
					i = fontinfo[(charbase[f] + mem[p].hh.b1)].qqqq
					hd = (i.b1 - 0)
					x = (x + fontinfo[(widthbase[f]+i.b0)].int)
					s = fontinfo[(heightbase[f] + (hd / 16))].int
					if s > h {
						h = s
					}
					s = fontinfo[(depthbase[f] + (hd % 16))].int
					if s > d {
						d = s
					}
					p = mem[p].hh.rh
				}
			}
			if p != 0 {
				{
					switch mem[p].hh.b0 {
					case 0:
						{
							x = (x + mem[(p+1)].int)
							if mem[p].hh.b0 >= 2 {
								s = 0
							} else {
								s = mem[(p + 4)].int
							}
							if (mem[(p+3)].int - s) > h {
								h = (mem[(p+3)].int - s)
							}
							if (mem[(p+2)].int + s) > d {
								d = (mem[(p+2)].int + s)
							}
						}
					case 1:
						{
							x = (x + mem[(p+1)].int)
							if mem[p].hh.b0 >= 2 {
								s = 0
							} else {
								s = mem[(p + 4)].int
							}
							if (mem[(p+3)].int - s) > h {
								h = (mem[(p+3)].int - s)
							}
							if (mem[(p+2)].int + s) > d {
								d = (mem[(p+2)].int + s)
							}
						}
					case 2:
						{
							x = (x + mem[(p+1)].int)
							if mem[p].hh.b0 >= 2 {
								s = 0
							} else {
								s = mem[(p + 4)].int
							}
							if (mem[(p+3)].int - s) > h {
								h = (mem[(p+3)].int - s)
							}
							if (mem[(p+2)].int + s) > d {
								d = (mem[(p+2)].int + s)
							}
						}
					case 13:
						{
							x = (x + mem[(p+1)].int)
							if mem[p].hh.b0 >= 2 {
								s = 0
							} else {
								s = mem[(p + 4)].int
							}
							if (mem[(p+3)].int - s) > h {
								h = (mem[(p+3)].int - s)
							}
							if (mem[(p+2)].int + s) > d {
								d = (mem[(p+2)].int + s)
							}
						}
					case 3:
						if adjusttail != 0 {
							{
								for mem[q].hh.rh != p {
									q = mem[q].hh.rh
								}
								if mem[p].hh.b0 == 5 {
									{
										mem[adjusttail].hh.rh = mem[(p + 1)].int
										for mem[adjusttail].hh.rh != 0 {
											adjusttail = mem[adjusttail].hh.rh
										}
										p = mem[p].hh.rh
										freenode(mem[q].hh.rh, 2)
									}
								} else {
									{
										mem[adjusttail].hh.rh = p
										adjusttail = p
										p = mem[p].hh.rh
									}
								}
								mem[q].hh.rh = p
								p = q
							}
						}
					case 4:
						if adjusttail != 0 {
							{
								for mem[q].hh.rh != p {
									q = mem[q].hh.rh
								}
								if mem[p].hh.b0 == 5 {
									{
										mem[adjusttail].hh.rh = mem[(p + 1)].int
										for mem[adjusttail].hh.rh != 0 {
											adjusttail = mem[adjusttail].hh.rh
										}
										p = mem[p].hh.rh
										freenode(mem[q].hh.rh, 2)
									}
								} else {
									{
										mem[adjusttail].hh.rh = p
										adjusttail = p
										p = mem[p].hh.rh
									}
								}
								mem[q].hh.rh = p
								p = q
							}
						}
					case 5:
						if adjusttail != 0 {
							{
								for mem[q].hh.rh != p {
									q = mem[q].hh.rh
								}
								if mem[p].hh.b0 == 5 {
									{
										mem[adjusttail].hh.rh = mem[(p + 1)].int
										for mem[adjusttail].hh.rh != 0 {
											adjusttail = mem[adjusttail].hh.rh
										}
										p = mem[p].hh.rh
										freenode(mem[q].hh.rh, 2)
									}
								} else {
									{
										mem[adjusttail].hh.rh = p
										adjusttail = p
										p = mem[p].hh.rh
									}
								}
								mem[q].hh.rh = p
								p = q
							}
						}
					case 8:
						// empty
					case 10:
						{
							g = mem[(p + 1)].hh.lh
							x = (x + mem[(g+1)].int)
							o = mem[g].hh.b0
							totalstretch[o] = (totalstretch[o] + mem[(g+2)].int)
							o = mem[g].hh.b1
							totalshrink[o] = (totalshrink[o] + mem[(g+3)].int)
							if mem[p].hh.b1 >= 100 {
								{
									g = mem[(p + 1)].hh.rh
									if mem[(g+3)].int > h {
										h = mem[(g + 3)].int
									}
									if mem[(g+2)].int > d {
										d = mem[(g + 2)].int
									}
								}
							}
						}
					case 11:
						x = (x + mem[(p+1)].int)
					case 9:
						x = (x + mem[(p+1)].int)
					case 6:
						{
							mem[29988] = mem[(p + 1)]
							mem[29988].hh.rh = mem[p].hh.rh
							p = 29988
							goto L21
						}
					default:
						// empty
					}
					p = mem[p].hh.rh
				}
			}
		}
	}
	if adjusttail != 0 {
		mem[adjusttail].hh.rh = 0
	}
	mem[(r + 3)].int = h
	mem[(r + 2)].int = d
	if m == 1 {
		w = (x + w)
	}
	mem[(r + 1)].int = w
	x = (w - x)
	if x == 0 {
		{
			mem[(r + 5)].hh.b0 = 0
			mem[(r + 5)].hh.b1 = 0
			mem[(r + 6)].gr = 0
			goto L10
		}
	} else {
		if x > 0 {
			{
				if totalstretch[3] != 0 {
					o = 3
				} else {
					if totalstretch[2] != 0 {
						o = 2
					} else {
						if totalstretch[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(r + 5)].hh.b1 = o
				mem[(r + 5)].hh.b0 = 1
				if totalstretch[o] != 0 {
					mem[(r + 6)].gr = (x / totalstretch[o])
				} else {
					{
						mem[(r + 5)].hh.b0 = 0
						mem[(r + 6)].gr = 0
					}
				}
				if o == 0 {
					if mem[(r+5)].hh.rh != 0 {
						{
							lastbadness = badness(x, totalstretch[0])
							if lastbadness > eqtb[5289].int {
								{
									println_()
									if lastbadness > 100 {
										printnl(844)
									} else {
										printnl(845)
									}
									print_(846)
									printint(lastbadness)
									goto L50
								}
							}
						}
					}
				}
				goto L10
			}
		} else {
			{
				if totalshrink[3] != 0 {
					o = 3
				} else {
					if totalshrink[2] != 0 {
						o = 2
					} else {
						if totalshrink[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(r + 5)].hh.b1 = o
				mem[(r + 5)].hh.b0 = 2
				if totalshrink[o] != 0 {
					mem[(r + 6)].gr = ((-x) / totalshrink[o])
				} else {
					{
						mem[(r + 5)].hh.b0 = 0
						mem[(r + 6)].gr = 0
					}
				}
				if ((totalshrink[o] < (-x)) && (o == 0)) && (mem[(r+5)].hh.rh != 0) {
					{
						lastbadness = 1000000
						mem[(r + 6)].gr = 1
						if (((-x) - totalshrink[0]) > eqtb[5838].int) || (eqtb[5289].int < 100) {
							{
								if (eqtb[5846].int > 0) && (((-x) - totalshrink[0]) > eqtb[5838].int) {
									{
										for mem[q].hh.rh != 0 {
											q = mem[q].hh.rh
										}
										mem[q].hh.rh = newrule
										mem[(mem[q].hh.rh + 1)].int = eqtb[5846].int
									}
								}
								println_()
								printnl(852)
								printscaled(((-x) - totalshrink[0]))
								print_(853)
								goto L50
							}
						}
					}
				} else {
					if o == 0 {
						if mem[(r+5)].hh.rh != 0 {
							{
								lastbadness = badness((-x), totalshrink[0])
								if lastbadness > eqtb[5289].int {
									{
										println_()
										printnl(854)
										printint(lastbadness)
										goto L50
									}
								}
							}
						}
					}
				}
				goto L10
			}
		}
	}
L50:
	if outputactive {
		print_(847)
	} else {
		{
			if packbeginline != 0 {
				{
					if packbeginline > 0 {
						print_(848)
					} else {
						print_(849)
					}
					printint(abs_(packbeginline))
					print_(850)
				}
			} else {
				print_(851)
			}
			printint(line)
		}
	}
	println_()
	fontinshortdisplay = 0
	shortdisplay(mem[(r + 5)].hh.rh)
	println_()
	begindiagnostic()
	showbox(r)
	enddiagnostic(true)
L10:
	hpack = r
}

/* function: vpackage */
func vpackage(p int, h int, m int, l int) int {
	var (
		r int
		w int
		d int
		x int
		s int
		g int
		o int
	)
	lastbadness = 0
	r = getnode(7)
	mem[r].hh.b0 = 1
	mem[r].hh.b1 = 0
	mem[(r + 4)].int = 0
	mem[(r + 5)].hh.rh = p
	w = 0
	d = 0
	x = 0
	totalstretch[0] = 0
	totalshrink[0] = 0
	totalstretch[1] = 0
	totalshrink[1] = 0
	totalstretch[2] = 0
	totalshrink[2] = 0
	totalstretch[3] = 0
	totalshrink[3] = 0
	for p != 0 {
		{
			if p >= himemmin {
				confusion(855)
			} else {
				switch mem[p].hh.b0 {
				case 0:
					{
						x = ((x + d) + mem[(p+3)].int)
						d = mem[(p + 2)].int
						if mem[p].hh.b0 >= 2 {
							s = 0
						} else {
							s = mem[(p + 4)].int
						}
						if (mem[(p+1)].int + s) > w {
							w = (mem[(p+1)].int + s)
						}
					}
				case 1:
					{
						x = ((x + d) + mem[(p+3)].int)
						d = mem[(p + 2)].int
						if mem[p].hh.b0 >= 2 {
							s = 0
						} else {
							s = mem[(p + 4)].int
						}
						if (mem[(p+1)].int + s) > w {
							w = (mem[(p+1)].int + s)
						}
					}
				case 2:
					{
						x = ((x + d) + mem[(p+3)].int)
						d = mem[(p + 2)].int
						if mem[p].hh.b0 >= 2 {
							s = 0
						} else {
							s = mem[(p + 4)].int
						}
						if (mem[(p+1)].int + s) > w {
							w = (mem[(p+1)].int + s)
						}
					}
				case 13:
					{
						x = ((x + d) + mem[(p+3)].int)
						d = mem[(p + 2)].int
						if mem[p].hh.b0 >= 2 {
							s = 0
						} else {
							s = mem[(p + 4)].int
						}
						if (mem[(p+1)].int + s) > w {
							w = (mem[(p+1)].int + s)
						}
					}
				case 8:
					// empty
				case 10:
					{
						x = (x + d)
						d = 0
						g = mem[(p + 1)].hh.lh
						x = (x + mem[(g+1)].int)
						o = mem[g].hh.b0
						totalstretch[o] = (totalstretch[o] + mem[(g+2)].int)
						o = mem[g].hh.b1
						totalshrink[o] = (totalshrink[o] + mem[(g+3)].int)
						if mem[p].hh.b1 >= 100 {
							{
								g = mem[(p + 1)].hh.rh
								if mem[(g+1)].int > w {
									w = mem[(g + 1)].int
								}
							}
						}
					}
				case 11:
					{
						x = ((x + d) + mem[(p+1)].int)
						d = 0
					}
				default:
					// empty
				}
			}
			p = mem[p].hh.rh
		}
	}
	mem[(r + 1)].int = w
	if d > l {
		{
			x = ((x + d) - l)
			mem[(r + 2)].int = l
		}
	} else {
		mem[(r + 2)].int = d
	}
	if m == 1 {
		h = (x + h)
	}
	mem[(r + 3)].int = h
	x = (h - x)
	if x == 0 {
		{
			mem[(r + 5)].hh.b0 = 0
			mem[(r + 5)].hh.b1 = 0
			mem[(r + 6)].gr = 0
			goto L10
		}
	} else {
		if x > 0 {
			{
				if totalstretch[3] != 0 {
					o = 3
				} else {
					if totalstretch[2] != 0 {
						o = 2
					} else {
						if totalstretch[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(r + 5)].hh.b1 = o
				mem[(r + 5)].hh.b0 = 1
				if totalstretch[o] != 0 {
					mem[(r + 6)].gr = (x / totalstretch[o])
				} else {
					{
						mem[(r + 5)].hh.b0 = 0
						mem[(r + 6)].gr = 0
					}
				}
				if o == 0 {
					if mem[(r+5)].hh.rh != 0 {
						{
							lastbadness = badness(x, totalstretch[0])
							if lastbadness > eqtb[5290].int {
								{
									println_()
									if lastbadness > 100 {
										printnl(844)
									} else {
										printnl(845)
									}
									print_(856)
									printint(lastbadness)
									goto L50
								}
							}
						}
					}
				}
				goto L10
			}
		} else {
			{
				if totalshrink[3] != 0 {
					o = 3
				} else {
					if totalshrink[2] != 0 {
						o = 2
					} else {
						if totalshrink[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(r + 5)].hh.b1 = o
				mem[(r + 5)].hh.b0 = 2
				if totalshrink[o] != 0 {
					mem[(r + 6)].gr = ((-x) / totalshrink[o])
				} else {
					{
						mem[(r + 5)].hh.b0 = 0
						mem[(r + 6)].gr = 0
					}
				}
				if ((totalshrink[o] < (-x)) && (o == 0)) && (mem[(r+5)].hh.rh != 0) {
					{
						lastbadness = 1000000
						mem[(r + 6)].gr = 1
						if (((-x) - totalshrink[0]) > eqtb[5839].int) || (eqtb[5290].int < 100) {
							{
								println_()
								printnl(857)
								printscaled(((-x) - totalshrink[0]))
								print_(858)
								goto L50
							}
						}
					}
				} else {
					if o == 0 {
						if mem[(r+5)].hh.rh != 0 {
							{
								lastbadness = badness((-x), totalshrink[0])
								if lastbadness > eqtb[5290].int {
									{
										println_()
										printnl(859)
										printint(lastbadness)
										goto L50
									}
								}
							}
						}
					}
				}
				goto L10
			}
		}
	}
L50:
	if outputactive {
		print_(847)
	} else {
		{
			if packbeginline != 0 {
				{
					print_(849)
					printint(abs_(packbeginline))
					print_(850)
				}
			} else {
				print_(851)
			}
			printint(line)
			println_()
		}
	}
	begindiagnostic()
	showbox(r)
	enddiagnostic(true)
L10:
	vpackage = r
}

/* procedure: appendtovlist */
func appendtovlist(b int) {
	var (
		d int
		p int
	)
	if curlist.auxfield.int > (-65536000) {
		{
			d = ((mem[(eqtb[2883].hh.rh+1)].int - curlist.auxfield.int) - mem[(b+3)].int)
			if d < eqtb[5832].int {
				p = newparamglue(0)
			} else {
				{
					p = newskipparam(1)
					mem[(tempptr + 1)].int = d
				}
			}
			mem[curlist.tailfield].hh.rh = p
			curlist.tailfield = p
		}
	}
	mem[curlist.tailfield].hh.rh = b
	curlist.tailfield = b
	curlist.auxfield.int = mem[(b + 2)].int
}

/* function: newnoad */
func newnoad() int {
	var (
		p int
	)
	p = getnode(4)
	mem[p].hh.b0 = 16
	mem[p].hh.b1 = 0
	mem[(p + 1)].hh = emptyfield
	mem[(p + 3)].hh = emptyfield
	mem[(p + 2)].hh = emptyfield
	newnoad = p
}

/* function: newstyle */
func newstyle(s int) int {
	var (
		p int
	)
	p = getnode(3)
	mem[p].hh.b0 = 14
	mem[p].hh.b1 = s
	mem[(p + 1)].int = 0
	mem[(p + 2)].int = 0
	newstyle = p
}

/* function: newchoice */
func newchoice() int {
	var (
		p int
	)
	p = getnode(3)
	mem[p].hh.b0 = 15
	mem[p].hh.b1 = 0
	mem[(p + 1)].hh.lh = 0
	mem[(p + 1)].hh.rh = 0
	mem[(p + 2)].hh.lh = 0
	mem[(p + 2)].hh.rh = 0
	newchoice = p
}

/* procedure: showinfo */
func showinfo() {
	shownodelist(mem[tempptr].hh.lh)
}

/* function: fractionrule */
func fractionrule(t int) int {
	var (
		p int
	)
	p = newrule
	mem[(p + 3)].int = t
	mem[(p + 2)].int = 0
	fractionrule = p
}

/* function: overbar */
func overbar(b int, k int, t int) int {
	var (
		p int
		q int
	)
	p = newkern(k)
	mem[p].hh.rh = b
	q = fractionrule(t)
	mem[q].hh.rh = p
	p = newkern(t)
	mem[p].hh.rh = q
	overbar = vpackage(p, 0, 1, 1073741823)
}

/* function: charbox */
func charbox(f int, c int) int {
	var (
		q  *fourquarters_t
		hd byte
		b  int
		p  int
	)
	q = fontinfo[(charbase[f] + c)].qqqq
	hd = (q.b1 - 0)
	b = newnullbox
	mem[(b + 1)].int = (fontinfo[(widthbase[f]+q.b0)].int + fontinfo[(italicbase[f]+((q.b2-0)/4))].int)
	mem[(b + 3)].int = fontinfo[(heightbase[f] + (hd / 16))].int
	mem[(b + 2)].int = fontinfo[(depthbase[f] + (hd % 16))].int
	p = getavail
	mem[p].hh.b1 = c
	mem[p].hh.b0 = f
	mem[(b + 5)].hh.rh = p
	charbox = b
}

/* procedure: stackintobox */
func stackintobox(b int, f int, c int) {
	var (
		p int
	)
	p = charbox(f, c)
	mem[p].hh.rh = mem[(b + 5)].hh.rh
	mem[(b + 5)].hh.rh = p
	mem[(b + 3)].int = mem[(p + 3)].int
}

/* function: heightplusdepth */
func heightplusdepth(f int, c int) int {
	var (
		q  *fourquarters_t
		hd byte
	)
	q = fontinfo[(charbase[f] + c)].qqqq
	hd = (q.b1 - 0)
	heightplusdepth = (fontinfo[(heightbase[f]+(hd/16))].int + fontinfo[(depthbase[f]+(hd%16))].int)
}

/* function: vardelimiter */
func vardelimiter(d int, s int, v int) int {
	var (
		b            int
		f            int
		g            int
		c            int
		x            int
		y            int
		m            int
		n            int
		u            int
		w            int
		q            *fourquarters_t
		hd           byte
		r            *fourquarters_t
		z            int
		largeattempt bool
	)
	f = 0
	w = 0
	largeattempt = false
	z = mem[d].qqqq.b0
	x = mem[d].qqqq.b1
	for true {
		{
			if (z != 0) || (x != 0) {
				{
					z = ((z + s) + 16)
					for {
						z = (z - 16)
						g = eqtb[(3935 + z)].hh.rh
						if g != 0 {
							{
								y = x
								if ((y - 0) >= fontbc[g]) && ((y - 0) <= fontec[g]) {
									{
									L22:
										q = fontinfo[(charbase[g] + y)].qqqq
										if q.b0 > 0 {
											{
												if ((q.b2 - 0) % 4) == 3 {
													{
														f = g
														c = y
														goto L40
													}
												}
												hd = (q.b1 - 0)
												u = (fontinfo[(heightbase[g]+(hd/16))].int + fontinfo[(depthbase[g]+(hd%16))].int)
												if u > w {
													{
														f = g
														c = y
														w = u
														if u >= v {
															goto L40
														}
													}
												}
												if ((q.b2 - 0) % 4) == 2 {
													{
														y = q.b3
														goto L22
													}
												}
											}
										}
									}
								}
							}
						}
						if !(z < 16) {
							break
						}
					}
				}
			}
			if largeattempt {
				goto L40
			}
			largeattempt = true
			z = mem[d].qqqq.b2
			x = mem[d].qqqq.b3
		}
	}
L40:
	if f != 0 {
		if ((q.b2 - 0) % 4) == 3 {
			{
				b = newnullbox
				mem[b].hh.b0 = 1
				r = fontinfo[(extenbase[f] + q.b3)].qqqq
				c = r.b3
				u = heightplusdepth(f, c)
				w = 0
				q = fontinfo[(charbase[f] + c)].qqqq
				mem[(b + 1)].int = (fontinfo[(widthbase[f]+q.b0)].int + fontinfo[(italicbase[f]+((q.b2-0)/4))].int)
				c = r.b2
				if c != 0 {
					w = (w + heightplusdepth(f, c))
				}
				c = r.b1
				if c != 0 {
					w = (w + heightplusdepth(f, c))
				}
				c = r.b0
				if c != 0 {
					w = (w + heightplusdepth(f, c))
				}
				n = 0
				if u > 0 {
					for w < v {
						{
							w = (w + u)
							n = (n + 1)
							if r.b1 != 0 {
								w = (w + u)
							}
						}
					}
				}
				c = r.b2
				if c != 0 {
					stackintobox(b, f, c)
				}
				c = r.b3
				for m := 1; m <= n; m++ {
					stackintobox(b, f, c)
				}
				c = r.b1
				if c != 0 {
					{
						stackintobox(b, f, c)
						c = r.b3
						for m := 1; m <= n; m++ {
							stackintobox(b, f, c)
						}
					}
				}
				c = r.b0
				if c != 0 {
					stackintobox(b, f, c)
				}
				mem[(b + 2)].int = (w - mem[(b+3)].int)
			}
		} else {
			b = charbox(f, c)
		}
	} else {
		{
			b = newnullbox
			mem[(b + 1)].int = eqtb[5841].int
		}
	}
	mem[(b + 4)].int = (half((mem[(b+3)].int - mem[(b+2)].int)) - fontinfo[(22+parambase[eqtb[(3937+s)].hh.rh])].int)
	vardelimiter = b
}

/* function: rebox */
func rebox(b int, w int) int {
	var (
		p int
		f int
		v int
	)
	if (mem[(b+1)].int != w) && (mem[(b+5)].hh.rh != 0) {
		{
			if mem[b].hh.b0 == 1 {
				b = hpack(b, 0, 1)
			}
			p = mem[(b + 5)].hh.rh
			if (p >= himemmin) && (mem[p].hh.rh == 0) {
				{
					f = mem[p].hh.b0
					v = fontinfo[(widthbase[f] + fontinfo[(charbase[f]+mem[p].hh.b1)].qqqq.b0)].int
					if v != mem[(b+1)].int {
						mem[p].hh.rh = newkern((mem[(b+1)].int - v))
					}
				}
			}
			freenode(b, 7)
			b = newglue(12)
			mem[b].hh.rh = p
			for mem[p].hh.rh != 0 {
				p = mem[p].hh.rh
			}
			mem[p].hh.rh = newglue(12)
			rebox = hpack(b, w, 0)
		}
	} else {
		{
			mem[(b + 1)].int = w
			rebox = b
		}
	}
}

/* function: mathglue */
func mathglue(g int, m int) int {
	var (
		p int
		n int
		f int
	)
	n = xovern(m, 65536)
	f = remainder
	if f < 0 {
		{
			n = (n - 1)
			f = (f + 65536)
		}
	}
	p = getnode(4)
	mem[(p + 1)].int = multandadd(n, mem[(g+1)].int, xnoverd(mem[(g+1)].int, f, 65536), 1073741823)
	mem[p].hh.b0 = mem[g].hh.b0
	if mem[p].hh.b0 == 0 {
		mem[(p + 2)].int = multandadd(n, mem[(g+2)].int, xnoverd(mem[(g+2)].int, f, 65536), 1073741823)
	} else {
		mem[(p + 2)].int = mem[(g + 2)].int
	}
	mem[p].hh.b1 = mem[g].hh.b1
	if mem[p].hh.b1 == 0 {
		mem[(p + 3)].int = multandadd(n, mem[(g+3)].int, xnoverd(mem[(g+3)].int, f, 65536), 1073741823)
	} else {
		mem[(p + 3)].int = mem[(g + 3)].int
	}
	mathglue = p
}

/* procedure: mathkern */
func mathkern(p int, m int) {
	var (
		n int
		f int
	)
	if mem[p].hh.b1 == 99 {
		{
			n = xovern(m, 65536)
			f = remainder
			if f < 0 {
				{
					n = (n - 1)
					f = (f + 65536)
				}
			}
			mem[(p + 1)].int = multandadd(n, mem[(p+1)].int, xnoverd(mem[(p+1)].int, f, 65536), 1073741823)
			mem[p].hh.b1 = 1
		}
	}
}

/* procedure: flushmath */
func flushmath() {
	flushnodelist(mem[curlist.headfield].hh.rh)
	flushnodelist(curlist.auxfield.int)
	mem[curlist.headfield].hh.rh = 0
	curlist.tailfield = curlist.headfield
	curlist.auxfield.int = 0
}

/* function: cleanbox */
func cleanbox(p int, s int) int {
	var (
		q         int
		savestyle int
		x         int
		r         int
	)
	switch mem[p].hh.rh {
	case 1:
		{
			curmlist = newnoad
			mem[(curmlist + 1)] = mem[p]
		}
	case 2:
		{
			q = mem[p].hh.lh
			goto L40
		}
	case 3:
		curmlist = mem[p].hh.lh
	default:
		{
			q = newnullbox
			goto L40
		}
	}
	savestyle = curstyle
	curstyle = s
	mlistpenalties = false
	mlisttohlist()
	q = mem[29997].hh.rh
	curstyle = savestyle
	{
		if curstyle < 4 {
			cursize = 0
		} else {
			cursize = (16 * ((curstyle - 2) / 2))
		}
		curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
	}
L40:
	if (q >= himemmin) || (q == 0) {
		x = hpack(q, 0, 1)
	} else {
		if ((mem[q].hh.rh == 0) && (mem[q].hh.b0 <= 1)) && (mem[(q+4)].int == 0) {
			x = q
		} else {
			x = hpack(q, 0, 1)
		}
	}
	q = mem[(x + 5)].hh.rh
	if q >= himemmin {
		{
			r = mem[q].hh.rh
			if r != 0 {
				if mem[r].hh.rh == 0 {
					if !(r >= himemmin) {
						if mem[r].hh.b0 == 11 {
							{
								freenode(r, 2)
								mem[q].hh.rh = 0
							}
						}
					}
				}
			}
		}
	}
	cleanbox = x
}

/* procedure: fetch */
func fetch(a int) {
	curc = mem[a].hh.b1
	curf = eqtb[((3935 + mem[a].hh.b0) + cursize)].hh.rh
	if curf == 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(338)
			}
			printsize(cursize)
			printchar(32)
			printint(mem[a].hh.b0)
			print_(884)
			print_((curc - 0))
			printchar(41)
			{
				helpptr = 4
				helpline[3] = 885
				helpline[2] = 886
				helpline[1] = 887
				helpline[0] = 888
			}
			error_()
			curi = nullcharacter
			mem[a].hh.rh = 0
		}
	} else {
		{
			if ((curc - 0) >= fontbc[curf]) && ((curc - 0) <= fontec[curf]) {
				curi = fontinfo[(charbase[curf] + curc)].qqqq
			} else {
				curi = nullcharacter
			}
			if !(curi.b0 > 0) {
				{
					charwarning(curf, (curc - 0))
					mem[a].hh.rh = 0
					curi = nullcharacter
				}
			}
		}
	}
}

/* procedure: makeover */
func makeover(q int) {
	mem[(q + 1)].hh.lh = overbar(cleanbox((q+1), ((2*(curstyle/2))+1)), (3 * fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int), fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int)
	mem[(q + 1)].hh.rh = 2
}

/* procedure: makeunder */
func makeunder(q int) {
	var (
		p     int
		x     int
		y     int
		delta int
	)
	x = cleanbox((q + 1), curstyle)
	p = newkern((3 * fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int))
	mem[x].hh.rh = p
	mem[p].hh.rh = fractionrule(fontinfo[(8 + parambase[eqtb[(3938+cursize)].hh.rh])].int)
	y = vpackage(x, 0, 1, 1073741823)
	delta = ((mem[(y+3)].int + mem[(y+2)].int) + fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int)
	mem[(y + 3)].int = mem[(x + 3)].int
	mem[(y + 2)].int = (delta - mem[(y+3)].int)
	mem[(q + 1)].hh.lh = y
	mem[(q + 1)].hh.rh = 2
}

/* procedure: makevcenter */
func makevcenter(q int) {
	var (
		v     int
		delta int
	)
	v = mem[(q + 1)].hh.lh
	if mem[v].hh.b0 != 1 {
		confusion(539)
	}
	delta = (mem[(v+3)].int + mem[(v+2)].int)
	mem[(v + 3)].int = (fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int + half(delta))
	mem[(v + 2)].int = (delta - mem[(v+3)].int)
}

/* procedure: makeradical */
func makeradical(q int) {
	var (
		x     int
		y     int
		delta int
		clr   int
	)
	x = cleanbox((q + 1), ((2 * (curstyle / 2)) + 1))
	if curstyle < 2 {
		clr = (fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int + (abs_(fontinfo[(5+parambase[eqtb[(3937+cursize)].hh.rh])].int) / 4))
	} else {
		{
			clr = fontinfo[(8 + parambase[eqtb[(3938+cursize)].hh.rh])].int
			clr = (clr + (abs_(clr) / 4))
		}
	}
	y = vardelimiter((q + 4), cursize, (((mem[(x+3)].int + mem[(x+2)].int) + clr) + fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int))
	delta = (mem[(y+2)].int - ((mem[(x+3)].int + mem[(x+2)].int) + clr))
	if delta > 0 {
		clr = (clr + half(delta))
	}
	mem[(y + 4)].int = (-(mem[(x+3)].int + clr))
	mem[y].hh.rh = overbar(x, clr, mem[(y+3)].int)
	mem[(q + 1)].hh.lh = hpack(y, 0, 1)
	mem[(q + 1)].hh.rh = 2
}

/* procedure: makemathaccent */
func makemathaccent(q int) {
	var (
		p     int
		x     int
		y     int
		a     int
		c     int
		f     int
		i     *fourquarters_t
		s     int
		h     int
		delta int
		w     int
	)
	fetch((q + 4))
	if curi.b0 > 0 {
		{
			i = curi
			c = curc
			f = curf
			s = 0
			if mem[(q+1)].hh.rh == 1 {
				{
					fetch((q + 1))
					if ((curi.b2 - 0) % 4) == 1 {
						{
							a = (ligkernbase[curf] + curi.b3)
							curi = fontinfo[a].qqqq
							if curi.b0 > 128 {
								{
									a = ((((ligkernbase[curf] + (256 * curi.b2)) + curi.b3) + 32768) - (256 * 128))
									curi = fontinfo[a].qqqq
								}
							}
							for true {
								{
									if (curi.b1 - 0) == skewchar[curf] {
										{
											if curi.b2 >= 128 {
												if curi.b0 <= 128 {
													s = fontinfo[((kernbase[curf] + (256 * curi.b2)) + curi.b3)].int
												}
											}
											goto L31
										}
									}
									if curi.b0 >= 128 {
										goto L31
									}
									a = ((a + curi.b0) + 1)
									curi = fontinfo[a].qqqq
								}
							}
						}
					}
				}
			}
		L31:
			// empty
			x = cleanbox((q + 1), ((2 * (curstyle / 2)) + 1))
			w = mem[(x + 1)].int
			h = mem[(x + 3)].int
			for true {
				{
					if ((i.b2 - 0) % 4) != 2 {
						goto L30
					}
					y = i.b3
					i = fontinfo[(charbase[f] + y)].qqqq
					if !(i.b0 > 0) {
						goto L30
					}
					if fontinfo[(widthbase[f]+i.b0)].int > w {
						goto L30
					}
					c = y
				}
			}
		L30:
			// empty
			if h < fontinfo[(5+parambase[f])].int {
				delta = h
			} else {
				delta = fontinfo[(5 + parambase[f])].int
			}
			if (mem[(q+2)].hh.rh != 0) || (mem[(q+3)].hh.rh != 0) {
				if mem[(q+1)].hh.rh == 1 {
					{
						flushnodelist(x)
						x = newnoad
						mem[(x + 1)] = mem[(q + 1)]
						mem[(x + 2)] = mem[(q + 2)]
						mem[(x + 3)] = mem[(q + 3)]
						mem[(q + 2)].hh = emptyfield
						mem[(q + 3)].hh = emptyfield
						mem[(q + 1)].hh.rh = 3
						mem[(q + 1)].hh.lh = x
						x = cleanbox((q + 1), curstyle)
						delta = ((delta + mem[(x+3)].int) - h)
						h = mem[(x + 3)].int
					}
				}
			}
			y = charbox(f, c)
			mem[(y + 4)].int = (s + half((w - mem[(y+1)].int)))
			mem[(y + 1)].int = 0
			p = newkern((-delta))
			mem[p].hh.rh = x
			mem[y].hh.rh = p
			y = vpackage(y, 0, 1, 1073741823)
			mem[(y + 1)].int = mem[(x + 1)].int
			if mem[(y+3)].int < h {
				{
					p = newkern((h - mem[(y+3)].int))
					mem[p].hh.rh = mem[(y + 5)].hh.rh
					mem[(y + 5)].hh.rh = p
					mem[(y + 3)].int = h
				}
			}
			mem[(q + 1)].hh.lh = y
			mem[(q + 1)].hh.rh = 2
		}
	}
}

/* procedure: makefraction */
func makefraction(q int) {
	var (
		p         int
		v         int
		x         int
		y         int
		z         int
		delta     int
		delta1    int
		delta2    int
		shiftup   int
		shiftdown int
		clr       int
	)
	if mem[(q+1)].int == 1073741824 {
		mem[(q + 1)].int = fontinfo[(8 + parambase[eqtb[(3938+cursize)].hh.rh])].int
	}
	x = cleanbox((q + 2), ((curstyle + 2) - (2 * (curstyle / 6))))
	z = cleanbox((q + 3), (((2 * (curstyle / 2)) + 3) - (2 * (curstyle / 6))))
	if mem[(x+1)].int < mem[(z+1)].int {
		x = rebox(x, mem[(z+1)].int)
	} else {
		z = rebox(z, mem[(x+1)].int)
	}
	if curstyle < 2 {
		{
			shiftup = fontinfo[(8 + parambase[eqtb[(3937+cursize)].hh.rh])].int
			shiftdown = fontinfo[(11 + parambase[eqtb[(3937+cursize)].hh.rh])].int
		}
	} else {
		{
			shiftdown = fontinfo[(12 + parambase[eqtb[(3937+cursize)].hh.rh])].int
			if mem[(q+1)].int != 0 {
				shiftup = fontinfo[(9 + parambase[eqtb[(3937+cursize)].hh.rh])].int
			} else {
				shiftup = fontinfo[(10 + parambase[eqtb[(3937+cursize)].hh.rh])].int
			}
		}
	}
	if mem[(q+1)].int == 0 {
		{
			if curstyle < 2 {
				clr = (7 * fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int)
			} else {
				clr = (3 * fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int)
			}
			delta = half((clr - ((shiftup - mem[(x+2)].int) - (mem[(z+3)].int - shiftdown))))
			if delta > 0 {
				{
					shiftup = (shiftup + delta)
					shiftdown = (shiftdown + delta)
				}
			}
		}
	} else {
		{
			if curstyle < 2 {
				clr = (3 * mem[(q+1)].int)
			} else {
				clr = mem[(q + 1)].int
			}
			delta = half(mem[(q + 1)].int)
			delta1 = (clr - ((shiftup - mem[(x+2)].int) - (fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int + delta)))
			delta2 = (clr - ((fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int - delta) - (mem[(z+3)].int - shiftdown)))
			if delta1 > 0 {
				shiftup = (shiftup + delta1)
			}
			if delta2 > 0 {
				shiftdown = (shiftdown + delta2)
			}
		}
	}
	v = newnullbox
	mem[v].hh.b0 = 1
	mem[(v + 3)].int = (shiftup + mem[(x+3)].int)
	mem[(v + 2)].int = (mem[(z+2)].int + shiftdown)
	mem[(v + 1)].int = mem[(x + 1)].int
	if mem[(q+1)].int == 0 {
		{
			p = newkern(((shiftup - mem[(x+2)].int) - (mem[(z+3)].int - shiftdown)))
			mem[p].hh.rh = z
		}
	} else {
		{
			y = fractionrule(mem[(q + 1)].int)
			p = newkern(((fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int - delta) - (mem[(z+3)].int - shiftdown)))
			mem[y].hh.rh = p
			mem[p].hh.rh = z
			p = newkern(((shiftup - mem[(x+2)].int) - (fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int + delta)))
			mem[p].hh.rh = y
		}
	}
	mem[x].hh.rh = p
	mem[(v + 5)].hh.rh = x
	if curstyle < 2 {
		delta = fontinfo[(20 + parambase[eqtb[(3937+cursize)].hh.rh])].int
	} else {
		delta = fontinfo[(21 + parambase[eqtb[(3937+cursize)].hh.rh])].int
	}
	x = vardelimiter((q + 4), cursize, delta)
	mem[x].hh.rh = v
	z = vardelimiter((q + 5), cursize, delta)
	mem[v].hh.rh = z
	mem[(q + 1)].int = hpack(x, 0, 1)
}

/* function: makeop */
func makeop(q int) int {
	var (
		delta     int
		p         int
		v         int
		x         int
		y         int
		z         int
		c         int
		i         *fourquarters_t
		shiftup   int
		shiftdown int
	)
	if (mem[q].hh.b1 == 0) && (curstyle < 2) {
		mem[q].hh.b1 = 1
	}
	if mem[(q+1)].hh.rh == 1 {
		{
			fetch((q + 1))
			if (curstyle < 2) && (((curi.b2 - 0) % 4) == 2) {
				{
					c = curi.b3
					i = fontinfo[(charbase[curf] + c)].qqqq
					if i.b0 > 0 {
						{
							curc = c
							curi = i
							mem[(q + 1)].hh.b1 = c
						}
					}
				}
			}
			delta = fontinfo[(italicbase[curf] + ((curi.b2 - 0) / 4))].int
			x = cleanbox((q + 1), curstyle)
			if (mem[(q+3)].hh.rh != 0) && (mem[q].hh.b1 != 1) {
				mem[(x + 1)].int = (mem[(x+1)].int - delta)
			}
			mem[(x + 4)].int = (half((mem[(x+3)].int - mem[(x+2)].int)) - fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int)
			mem[(q + 1)].hh.rh = 2
			mem[(q + 1)].hh.lh = x
		}
	} else {
		delta = 0
	}
	if mem[q].hh.b1 == 1 {
		{
			x = cleanbox((q + 2), (((2 * (curstyle / 4)) + 4) + (curstyle % 2)))
			y = cleanbox((q + 1), curstyle)
			z = cleanbox((q + 3), ((2 * (curstyle / 4)) + 5))
			v = newnullbox
			mem[v].hh.b0 = 1
			mem[(v + 1)].int = mem[(y + 1)].int
			if mem[(x+1)].int > mem[(v+1)].int {
				mem[(v + 1)].int = mem[(x + 1)].int
			}
			if mem[(z+1)].int > mem[(v+1)].int {
				mem[(v + 1)].int = mem[(z + 1)].int
			}
			x = rebox(x, mem[(v+1)].int)
			y = rebox(y, mem[(v+1)].int)
			z = rebox(z, mem[(v+1)].int)
			mem[(x + 4)].int = half(delta)
			mem[(z + 4)].int = (-mem[(x + 4)].int)
			mem[(v + 3)].int = mem[(y + 3)].int
			mem[(v + 2)].int = mem[(y + 2)].int
			if mem[(q+2)].hh.rh == 0 {
				{
					freenode(x, 7)
					mem[(v + 5)].hh.rh = y
				}
			} else {
				{
					shiftup = (fontinfo[(11+parambase[eqtb[(3938+cursize)].hh.rh])].int - mem[(x+2)].int)
					if shiftup < fontinfo[(9+parambase[eqtb[(3938+cursize)].hh.rh])].int {
						shiftup = fontinfo[(9 + parambase[eqtb[(3938+cursize)].hh.rh])].int
					}
					p = newkern(shiftup)
					mem[p].hh.rh = y
					mem[x].hh.rh = p
					p = newkern(fontinfo[(13 + parambase[eqtb[(3938+cursize)].hh.rh])].int)
					mem[p].hh.rh = x
					mem[(v + 5)].hh.rh = p
					mem[(v + 3)].int = ((((mem[(v+3)].int + fontinfo[(13+parambase[eqtb[(3938+cursize)].hh.rh])].int) + mem[(x+3)].int) + mem[(x+2)].int) + shiftup)
				}
			}
			if mem[(q+3)].hh.rh == 0 {
				freenode(z, 7)
			} else {
				{
					shiftdown = (fontinfo[(12+parambase[eqtb[(3938+cursize)].hh.rh])].int - mem[(z+3)].int)
					if shiftdown < fontinfo[(10+parambase[eqtb[(3938+cursize)].hh.rh])].int {
						shiftdown = fontinfo[(10 + parambase[eqtb[(3938+cursize)].hh.rh])].int
					}
					p = newkern(shiftdown)
					mem[y].hh.rh = p
					mem[p].hh.rh = z
					p = newkern(fontinfo[(13 + parambase[eqtb[(3938+cursize)].hh.rh])].int)
					mem[z].hh.rh = p
					mem[(v + 2)].int = ((((mem[(v+2)].int + fontinfo[(13+parambase[eqtb[(3938+cursize)].hh.rh])].int) + mem[(z+3)].int) + mem[(z+2)].int) + shiftdown)
				}
			}
			mem[(q + 1)].int = v
		}
	}
	makeop = delta
}

/* procedure: makeord */
func makeord(q int) {
	var (
		a int
		p int
		r int
	)
L20:
	if mem[(q+3)].hh.rh == 0 {
		if mem[(q+2)].hh.rh == 0 {
			if mem[(q+1)].hh.rh == 1 {
				{
					p = mem[q].hh.rh
					if p != 0 {
						if (mem[p].hh.b0 >= 16) && (mem[p].hh.b0 <= 22) {
							if mem[(p+1)].hh.rh == 1 {
								if mem[(p+1)].hh.b0 == mem[(q+1)].hh.b0 {
									{
										mem[(q + 1)].hh.rh = 4
										fetch((q + 1))
										if ((curi.b2 - 0) % 4) == 1 {
											{
												a = (ligkernbase[curf] + curi.b3)
												curc = mem[(p + 1)].hh.b1
												curi = fontinfo[a].qqqq
												if curi.b0 > 128 {
													{
														a = ((((ligkernbase[curf] + (256 * curi.b2)) + curi.b3) + 32768) - (256 * 128))
														curi = fontinfo[a].qqqq
													}
												}
												for true {
													{
														if curi.b1 == curc {
															if curi.b0 <= 128 {
																if curi.b2 >= 128 {
																	{
																		p = newkern(fontinfo[((kernbase[curf] + (256 * curi.b2)) + curi.b3)].int)
																		mem[p].hh.rh = mem[q].hh.rh
																		mem[q].hh.rh = p
																		goto L10
																	}
																} else {
																	{
																		{
																			if interrupt != 0 {
																				pauseforinstructions()
																			}
																		}
																		switch curi.b2 {
																		case 1:
																			mem[(q + 1)].hh.b1 = curi.b3
																		case 5:
																			mem[(q + 1)].hh.b1 = curi.b3
																		case 2:
																			mem[(p + 1)].hh.b1 = curi.b3
																		case 6:
																			mem[(p + 1)].hh.b1 = curi.b3
																		case 3:
																			{
																				r = newnoad
																				mem[(r + 1)].hh.b1 = curi.b3
																				mem[(r + 1)].hh.b0 = mem[(q + 1)].hh.b0
																				mem[q].hh.rh = r
																				mem[r].hh.rh = p
																				if curi.b2 < 11 {
																					mem[(r + 1)].hh.rh = 1
																				} else {
																					mem[(r + 1)].hh.rh = 4
																				}
																			}
																		case 7:
																			{
																				r = newnoad
																				mem[(r + 1)].hh.b1 = curi.b3
																				mem[(r + 1)].hh.b0 = mem[(q + 1)].hh.b0
																				mem[q].hh.rh = r
																				mem[r].hh.rh = p
																				if curi.b2 < 11 {
																					mem[(r + 1)].hh.rh = 1
																				} else {
																					mem[(r + 1)].hh.rh = 4
																				}
																			}
																		case 11:
																			{
																				r = newnoad
																				mem[(r + 1)].hh.b1 = curi.b3
																				mem[(r + 1)].hh.b0 = mem[(q + 1)].hh.b0
																				mem[q].hh.rh = r
																				mem[r].hh.rh = p
																				if curi.b2 < 11 {
																					mem[(r + 1)].hh.rh = 1
																				} else {
																					mem[(r + 1)].hh.rh = 4
																				}
																			}
																		default:
																			{
																				mem[q].hh.rh = mem[p].hh.rh
																				mem[(q + 1)].hh.b1 = curi.b3
																				mem[(q + 3)] = mem[(p + 3)]
																				mem[(q + 2)] = mem[(p + 2)]
																				freenode(p, 4)
																			}
																		}
																		if curi.b2 > 3 {
																			goto L10
																		}
																		mem[(q + 1)].hh.rh = 1
																		goto L20
																	}
																}
															}
														}
														if curi.b0 >= 128 {
															goto L10
														}
														a = ((a + curi.b0) + 1)
														curi = fontinfo[a].qqqq
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: makescripts */
func makescripts(q int, delta int) {
	var (
		p         int
		x         int
		y         int
		z         int
		shiftup   int
		shiftdown int
		clr       int
		t         int
	)
	p = mem[(q + 1)].int
	if p >= himemmin {
		{
			shiftup = 0
			shiftdown = 0
		}
	} else {
		{
			z = hpack(p, 0, 1)
			if curstyle < 4 {
				t = 16
			} else {
				t = 32
			}
			shiftup = (mem[(z+3)].int - fontinfo[(18+parambase[eqtb[(3937+t)].hh.rh])].int)
			shiftdown = (mem[(z+2)].int + fontinfo[(19+parambase[eqtb[(3937+t)].hh.rh])].int)
			freenode(z, 7)
		}
	}
	if mem[(q+2)].hh.rh == 0 {
		{
			x = cleanbox((q + 3), ((2 * (curstyle / 4)) + 5))
			mem[(x + 1)].int = (mem[(x+1)].int + eqtb[5842].int)
			if shiftdown < fontinfo[(16+parambase[eqtb[(3937+cursize)].hh.rh])].int {
				shiftdown = fontinfo[(16 + parambase[eqtb[(3937+cursize)].hh.rh])].int
			}
			clr = (mem[(x+3)].int - (abs_((fontinfo[(5+parambase[eqtb[(3937+cursize)].hh.rh])].int * 4)) / 5))
			if shiftdown < clr {
				shiftdown = clr
			}
			mem[(x + 4)].int = shiftdown
		}
	} else {
		{
			{
				x = cleanbox((q + 2), (((2 * (curstyle / 4)) + 4) + (curstyle % 2)))
				mem[(x + 1)].int = (mem[(x+1)].int + eqtb[5842].int)
				if (curstyle & 1) != 0 {
					clr = fontinfo[(15 + parambase[eqtb[(3937+cursize)].hh.rh])].int
				} else {
					if curstyle < 2 {
						clr = fontinfo[(13 + parambase[eqtb[(3937+cursize)].hh.rh])].int
					} else {
						clr = fontinfo[(14 + parambase[eqtb[(3937+cursize)].hh.rh])].int
					}
				}
				if shiftup < clr {
					shiftup = clr
				}
				clr = (mem[(x+2)].int + (abs_(fontinfo[(5+parambase[eqtb[(3937+cursize)].hh.rh])].int) / 4))
				if shiftup < clr {
					shiftup = clr
				}
			}
			if mem[(q+3)].hh.rh == 0 {
				mem[(x + 4)].int = (-shiftup)
			} else {
				{
					y = cleanbox((q + 3), ((2 * (curstyle / 4)) + 5))
					mem[(y + 1)].int = (mem[(y+1)].int + eqtb[5842].int)
					if shiftdown < fontinfo[(17+parambase[eqtb[(3937+cursize)].hh.rh])].int {
						shiftdown = fontinfo[(17 + parambase[eqtb[(3937+cursize)].hh.rh])].int
					}
					clr = ((4 * fontinfo[(8+parambase[eqtb[(3938+cursize)].hh.rh])].int) - ((shiftup - mem[(x+2)].int) - (mem[(y+3)].int - shiftdown)))
					if clr > 0 {
						{
							shiftdown = (shiftdown + clr)
							clr = ((abs_((fontinfo[(5+parambase[eqtb[(3937+cursize)].hh.rh])].int * 4)) / 5) - (shiftup - mem[(x+2)].int))
							if clr > 0 {
								{
									shiftup = (shiftup + clr)
									shiftdown = (shiftdown - clr)
								}
							}
						}
					}
					mem[(x + 4)].int = delta
					p = newkern(((shiftup - mem[(x+2)].int) - (mem[(y+3)].int - shiftdown)))
					mem[x].hh.rh = p
					mem[p].hh.rh = y
					x = vpackage(x, 0, 1, 1073741823)
					mem[(x + 4)].int = shiftdown
				}
			}
		}
	}
	if mem[(q+1)].int == 0 {
		mem[(q + 1)].int = x
	} else {
		{
			p = mem[(q + 1)].int
			for mem[p].hh.rh != 0 {
				p = mem[p].hh.rh
			}
			mem[p].hh.rh = x
		}
	}
}

/* function: makeleftright */
func makeleftright(q int, style int, maxd int, maxh int) int {
	var (
		delta  int
		delta1 int
		delta2 int
	)
	if style < 4 {
		cursize = 0
	} else {
		cursize = (16 * ((style - 2) / 2))
	}
	delta2 = (maxd + fontinfo[(22+parambase[eqtb[(3937+cursize)].hh.rh])].int)
	delta1 = ((maxh + maxd) - delta2)
	if delta2 > delta1 {
		delta1 = delta2
	}
	delta = ((delta1 / 500) * eqtb[5281].int)
	delta2 = ((delta1 + delta1) - eqtb[5840].int)
	if delta < delta2 {
		delta = delta2
	}
	mem[(q + 1)].int = vardelimiter((q + 1), cursize, delta)
	makeleftright = (mem[q].hh.b0 - 10)
}

/* procedure: mlisttohlist */
func mlisttohlist() {
	var (
		mlist     int
		penalties bool
		style     int
		savestyle int
		q         int
		r         int
		rtype     int
		t         int
		p         int
		x         int
		y         int
		z         int
		pen       int
		s         int
		maxh      int
		maxd      int
		delta     int
	)
	mlist = curmlist
	penalties = mlistpenalties
	style = curstyle
	q = mlist
	r = 0
	rtype = 17
	maxh = 0
	maxd = 0
	{
		if curstyle < 4 {
			cursize = 0
		} else {
			cursize = (16 * ((curstyle - 2) / 2))
		}
		curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
	}
	for q != 0 {
		{
		L21:
			delta = 0
			switch mem[q].hh.b0 {
			case 18:
				switch rtype {
				case 18:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				case 17:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				case 19:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				case 20:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				case 22:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				case 30:
					{
						mem[q].hh.b0 = 16
						goto L21
					}
				default:
					// empty
				}
			case 19:
				{
					if rtype == 18 {
						mem[r].hh.b0 = 16
					}
					if mem[q].hh.b0 == 31 {
						goto L80
					}
				}
			case 21:
				{
					if rtype == 18 {
						mem[r].hh.b0 = 16
					}
					if mem[q].hh.b0 == 31 {
						goto L80
					}
				}
			case 22:
				{
					if rtype == 18 {
						mem[r].hh.b0 = 16
					}
					if mem[q].hh.b0 == 31 {
						goto L80
					}
				}
			case 31:
				{
					if rtype == 18 {
						mem[r].hh.b0 = 16
					}
					if mem[q].hh.b0 == 31 {
						goto L80
					}
				}
			case 30:
				goto L80
			case 25:
				{
					makefraction(q)
					goto L82
				}
			case 17:
				{
					delta = makeop(q)
					if mem[q].hh.b1 == 1 {
						goto L82
					}
				}
			case 16:
				makeord(q)
			case 20:
				// empty
			case 23:
				// empty
			case 24:
				makeradical(q)
			case 27:
				makeover(q)
			case 26:
				makeunder(q)
			case 28:
				makemathaccent(q)
			case 29:
				makevcenter(q)
			case 14:
				{
					curstyle = mem[q].hh.b1
					{
						if curstyle < 4 {
							cursize = 0
						} else {
							cursize = (16 * ((curstyle - 2) / 2))
						}
						curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
					}
					goto L81
				}
			case 15:
				{
					switch curstyle / 2 {
					case 0:
						{
							p = mem[(q + 1)].hh.lh
							mem[(q + 1)].hh.lh = 0
						}
					case 1:
						{
							p = mem[(q + 1)].hh.rh
							mem[(q + 1)].hh.rh = 0
						}
					case 2:
						{
							p = mem[(q + 2)].hh.lh
							mem[(q + 2)].hh.lh = 0
						}
					case 3:
						{
							p = mem[(q + 2)].hh.rh
							mem[(q + 2)].hh.rh = 0
						}
					}
					flushnodelist(mem[(q + 1)].hh.lh)
					flushnodelist(mem[(q + 1)].hh.rh)
					flushnodelist(mem[(q + 2)].hh.lh)
					flushnodelist(mem[(q + 2)].hh.rh)
					mem[q].hh.b0 = 14
					mem[q].hh.b1 = curstyle
					mem[(q + 1)].int = 0
					mem[(q + 2)].int = 0
					if p != 0 {
						{
							z = mem[q].hh.rh
							mem[q].hh.rh = p
							for mem[p].hh.rh != 0 {
								p = mem[p].hh.rh
							}
							mem[p].hh.rh = z
						}
					}
					goto L81
				}
			case 3:
				goto L81
			case 4:
				goto L81
			case 5:
				goto L81
			case 8:
				goto L81
			case 12:
				goto L81
			case 7:
				goto L81
			case 2:
				{
					if mem[(q+3)].int > maxh {
						maxh = mem[(q + 3)].int
					}
					if mem[(q+2)].int > maxd {
						maxd = mem[(q + 2)].int
					}
					goto L81
				}
			case 10:
				{
					if mem[q].hh.b1 == 99 {
						{
							x = mem[(q + 1)].hh.lh
							y = mathglue(x, curmu)
							deleteglueref(x)
							mem[(q + 1)].hh.lh = y
							mem[q].hh.b1 = 0
						}
					} else {
						if (cursize != 0) && (mem[q].hh.b1 == 98) {
							{
								p = mem[q].hh.rh
								if p != 0 {
									if (mem[p].hh.b0 == 10) || (mem[p].hh.b0 == 11) {
										{
											mem[q].hh.rh = mem[p].hh.rh
											mem[p].hh.rh = 0
											flushnodelist(p)
										}
									}
								}
							}
						}
					}
					goto L81
				}
			case 11:
				{
					mathkern(q, curmu)
					goto L81
				}
			default:
				confusion(889)
			}
			switch mem[(q + 1)].hh.rh {
			case 1:
				{
					fetch((q + 1))
					if curi.b0 > 0 {
						{
							delta = fontinfo[(italicbase[curf] + ((curi.b2 - 0) / 4))].int
							p = newcharacter(curf, (curc - 0))
							if (mem[(q+1)].hh.rh == 4) && (fontinfo[(2+parambase[curf])].int != 0) {
								delta = 0
							}
							if (mem[(q+3)].hh.rh == 0) && (delta != 0) {
								{
									mem[p].hh.rh = newkern(delta)
									delta = 0
								}
							}
						}
					} else {
						p = 0
					}
				}
			case 4:
				{
					fetch((q + 1))
					if curi.b0 > 0 {
						{
							delta = fontinfo[(italicbase[curf] + ((curi.b2 - 0) / 4))].int
							p = newcharacter(curf, (curc - 0))
							if (mem[(q+1)].hh.rh == 4) && (fontinfo[(2+parambase[curf])].int != 0) {
								delta = 0
							}
							if (mem[(q+3)].hh.rh == 0) && (delta != 0) {
								{
									mem[p].hh.rh = newkern(delta)
									delta = 0
								}
							}
						}
					} else {
						p = 0
					}
				}
			case 0:
				p = 0
			case 2:
				p = mem[(q + 1)].hh.lh
			case 3:
				{
					curmlist = mem[(q + 1)].hh.lh
					savestyle = curstyle
					mlistpenalties = false
					mlisttohlist()
					curstyle = savestyle
					{
						if curstyle < 4 {
							cursize = 0
						} else {
							cursize = (16 * ((curstyle - 2) / 2))
						}
						curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
					}
					p = hpack(mem[29997].hh.rh, 0, 1)
				}
			default:
				confusion(890)
			}
			mem[(q + 1)].int = p
			if (mem[(q+3)].hh.rh == 0) && (mem[(q+2)].hh.rh == 0) {
				goto L82
			}
			makescripts(q, delta)
		L82:
			z = hpack(mem[(q+1)].int, 0, 1)
			if mem[(z+3)].int > maxh {
				maxh = mem[(z + 3)].int
			}
			if mem[(z+2)].int > maxd {
				maxd = mem[(z + 2)].int
			}
			freenode(z, 7)
		L80:
			r = q
			rtype = mem[r].hh.b0
		L81:
			q = mem[q].hh.rh
		}
	}
	if rtype == 18 {
		mem[r].hh.b0 = 16
	}
	p = 29997
	mem[p].hh.rh = 0
	q = mlist
	rtype = 0
	curstyle = style
	{
		if curstyle < 4 {
			cursize = 0
		} else {
			cursize = (16 * ((curstyle - 2) / 2))
		}
		curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
	}
	for q != 0 {
		{
			t = 16
			s = 4
			pen = 10000
			switch mem[q].hh.b0 {
			case 17:
				t = mem[q].hh.b0
			case 20:
				t = mem[q].hh.b0
			case 21:
				t = mem[q].hh.b0
			case 22:
				t = mem[q].hh.b0
			case 23:
				t = mem[q].hh.b0
			case 18:
				{
					t = 18
					pen = eqtb[5272].int
				}
			case 19:
				{
					t = 19
					pen = eqtb[5273].int
				}
			case 16:
				// empty
			case 29:
				// empty
			case 27:
				// empty
			case 26:
				// empty
			case 24:
				s = 5
			case 28:
				s = 5
			case 25:
				s = 6
			case 30:
				t = makeleftright(q, style, maxd, maxh)
			case 31:
				t = makeleftright(q, style, maxd, maxh)
			case 14:
				{
					curstyle = mem[q].hh.b1
					s = 3
					{
						if curstyle < 4 {
							cursize = 0
						} else {
							cursize = (16 * ((curstyle - 2) / 2))
						}
						curmu = xovern(fontinfo[(6+parambase[eqtb[(3937+cursize)].hh.rh])].int, 18)
					}
					goto L83
				}
			case 8:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 12:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 2:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 7:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 5:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 3:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 4:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 10:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			case 11:
				{
					mem[p].hh.rh = q
					p = q
					q = mem[q].hh.rh
					mem[p].hh.rh = 0
					goto L30
				}
			default:
				confusion(891)
			}
			if rtype > 0 {
				{
					switch strpool[(((rtype * 8) + t) + magicoffset)] {
					case 48:
						x = 0
					case 49:
						if curstyle < 4 {
							x = 15
						} else {
							x = 0
						}
					case 50:
						x = 15
					case 51:
						if curstyle < 4 {
							x = 16
						} else {
							x = 0
						}
					case 52:
						if curstyle < 4 {
							x = 17
						} else {
							x = 0
						}
					default:
						confusion(893)
					}
					if x != 0 {
						{
							y = mathglue(eqtb[(2882+x)].hh.rh, curmu)
							z = newglue(y)
							mem[y].hh.rh = 0
							mem[p].hh.rh = z
							p = z
							mem[z].hh.b1 = (x + 1)
						}
					}
				}
			}
			if mem[(q+1)].int != 0 {
				{
					mem[p].hh.rh = mem[(q + 1)].int
					for {
						p = mem[p].hh.rh
						if !(mem[p].hh.rh == 0) {
							break
						}
					}
				}
			}
			if penalties {
				if mem[q].hh.rh != 0 {
					if pen < 10000 {
						{
							rtype = mem[mem[q].hh.rh].hh.b0
							if rtype != 12 {
								if rtype != 19 {
									{
										z = newpenalty(pen)
										mem[p].hh.rh = z
										p = z
									}
								}
							}
						}
					}
				}
			}
			rtype = t
		L83:
			r = q
			q = mem[q].hh.rh
			freenode(r, s)
		L30:
			// empty
		}
	}
}

/* procedure: pushalignment */
func pushalignment() {
	var (
		p int
	)
	p = getnode(5)
	mem[p].hh.rh = alignptr
	mem[p].hh.lh = curalign
	mem[(p + 1)].hh.lh = mem[29992].hh.rh
	mem[(p + 1)].hh.rh = curspan
	mem[(p + 2)].int = curloop
	mem[(p + 3)].int = alignstate
	mem[(p + 4)].hh.lh = curhead
	mem[(p + 4)].hh.rh = curtail
	alignptr = p
	curhead = getavail
}

/* procedure: popalignment */
func popalignment() {
	var (
		p int
	)
	{
		mem[curhead].hh.rh = avail
		avail = curhead
	}
	p = alignptr
	curtail = mem[(p + 4)].hh.rh
	curhead = mem[(p + 4)].hh.lh
	alignstate = mem[(p + 3)].int
	curloop = mem[(p + 2)].int
	curspan = mem[(p + 1)].hh.rh
	mem[29992].hh.rh = mem[(p + 1)].hh.lh
	curalign = mem[p].hh.lh
	alignptr = mem[p].hh.rh
	freenode(p, 5)
}

/* procedure: getpreambletoken */
func getpreambletoken() {
L20:
	gettoken()
	for (curchr == 256) && (curcmd == 4) {
		{
			gettoken()
			if curcmd > 100 {
				{
					expand()
					gettoken()
				}
			}
		}
	}
	if curcmd == 9 {
		fatalerror(595)
	}
	if (curcmd == 75) && (curchr == 2893) {
		{
			scanoptionalequals()
			scanglue(2)
			if eqtb[5306].int > 0 {
				geqdefine(2893, 117, curval)
			} else {
				eqdefine(2893, 117, curval)
			}
			goto L20
		}
	}
}

/* procedure: initalign */
func initalign() {
	var (
		savecsptr int
		p         int
	)
	savecsptr = curcs
	pushalignment()
	alignstate = (-1000000)
	if (curlist.modefield == 203) && ((curlist.tailfield != curlist.headfield) || (curlist.auxfield.int != 0)) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(680)
			}
			printesc(520)
			print_(894)
			{
				helpptr = 3
				helpline[2] = 895
				helpline[1] = 896
				helpline[0] = 897
			}
			error_()
			flushmath()
		}
	}
	pushnest()
	if curlist.modefield == 203 {
		{
			curlist.modefield = (-1)
			curlist.auxfield.int = nest[(nestptr - 2)].auxfield.int
		}
	} else {
		if curlist.modefield > 0 {
			curlist.modefield = (-curlist.modefield)
		}
	}
	scanspec(6, false)
	mem[29992].hh.rh = 0
	curalign = 29992
	curloop = 0
	scannerstatus = 4
	warningindex = savecsptr
	alignstate = (-1000000)
	for true {
		{
			mem[curalign].hh.rh = newparamglue(11)
			curalign = mem[curalign].hh.rh
			if curcmd == 5 {
				goto L30
			}
			p = 29996
			mem[p].hh.rh = 0
			for true {
				{
					getpreambletoken()
					if curcmd == 6 {
						goto L31
					}
					if ((curcmd <= 5) && (curcmd >= 4)) && (alignstate == (-1000000)) {
						if ((p == 29996) && (curloop == 0)) && (curcmd == 4) {
							curloop = curalign
						} else {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(903)
								}
								{
									helpptr = 3
									helpline[2] = 904
									helpline[1] = 905
									helpline[0] = 906
								}
								backerror()
								goto L31
							}
						}
					} else {
						if (curcmd != 10) || (p != 29996) {
							{
								mem[p].hh.rh = getavail
								p = mem[p].hh.rh
								mem[p].hh.lh = curtok
							}
						}
					}
				}
			}
		L31:
			// empty
			mem[curalign].hh.rh = newnullbox
			curalign = mem[curalign].hh.rh
			mem[curalign].hh.lh = 29991
			mem[(curalign + 1)].int = (-1073741824)
			mem[(curalign + 3)].int = mem[29996].hh.rh
			p = 29996
			mem[p].hh.rh = 0
			for true {
				{
				L22:
					getpreambletoken()
					if ((curcmd <= 5) && (curcmd >= 4)) && (alignstate == (-1000000)) {
						goto L32
					}
					if curcmd == 6 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(907)
							}
							{
								helpptr = 3
								helpline[2] = 904
								helpline[1] = 905
								helpline[0] = 908
							}
							error_()
							goto L22
						}
					}
					mem[p].hh.rh = getavail
					p = mem[p].hh.rh
					mem[p].hh.lh = curtok
				}
			}
		L32:
			mem[p].hh.rh = getavail
			p = mem[p].hh.rh
			mem[p].hh.lh = 6714
			mem[(curalign + 2)].int = mem[29996].hh.rh
		}
	}
L30:
	scannerstatus = 0
	newsavelevel(6)
	if eqtb[3420].hh.rh != 0 {
		begintokenlist(eqtb[3420].hh.rh, 13)
	}
	alignpeek()
}

/* procedure: initspan */
func initspan(p int) {
	pushnest()
	if curlist.modefield == (-102) {
		curlist.auxfield.hh.lh = 1000
	} else {
		{
			curlist.auxfield.int = (-65536000)
			normalparagraph()
		}
	}
	curspan = p
}

/* procedure: initrow */
func initrow() {
	pushnest()
	curlist.modefield = ((-103) - curlist.modefield)
	if curlist.modefield == (-102) {
		curlist.auxfield.hh.lh = 0
	} else {
		curlist.auxfield.int = 0
	}
	{
		mem[curlist.tailfield].hh.rh = newglue(mem[(mem[29992].hh.rh + 1)].hh.lh)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	mem[curlist.tailfield].hh.b1 = 12
	curalign = mem[mem[29992].hh.rh].hh.rh
	curtail = curhead
	initspan(curalign)
}

/* procedure: initcol */
func initcol() {
	mem[(curalign + 5)].hh.lh = curcmd
	if curcmd == 63 {
		alignstate = 0
	} else {
		{
			backinput()
			begintokenlist(mem[(curalign+3)].int, 1)
		}
	}
}

/* function: fincol */
func fincol() bool {
	var (
		p int
		q int
		r int
		s int
		u int
		w int
		o int
		n int
	)
	if curalign == 0 {
		confusion(909)
	}
	q = mem[curalign].hh.rh
	if q == 0 {
		confusion(909)
	}
	if alignstate < 500000 {
		fatalerror(595)
	}
	p = mem[q].hh.rh
	if (p == 0) && (mem[(curalign+5)].hh.lh < 257) {
		if curloop != 0 {
			{
				mem[q].hh.rh = newnullbox
				p = mem[q].hh.rh
				mem[p].hh.lh = 29991
				mem[(p + 1)].int = (-1073741824)
				curloop = mem[curloop].hh.rh
				q = 29996
				r = mem[(curloop + 3)].int
				for r != 0 {
					{
						mem[q].hh.rh = getavail
						q = mem[q].hh.rh
						mem[q].hh.lh = mem[r].hh.lh
						r = mem[r].hh.rh
					}
				}
				mem[q].hh.rh = 0
				mem[(p + 3)].int = mem[29996].hh.rh
				q = 29996
				r = mem[(curloop + 2)].int
				for r != 0 {
					{
						mem[q].hh.rh = getavail
						q = mem[q].hh.rh
						mem[q].hh.lh = mem[r].hh.lh
						r = mem[r].hh.rh
					}
				}
				mem[q].hh.rh = 0
				mem[(p + 2)].int = mem[29996].hh.rh
				curloop = mem[curloop].hh.rh
				mem[p].hh.rh = newglue(mem[(curloop + 1)].hh.lh)
				mem[mem[p].hh.rh].hh.b1 = 12
			}
		} else {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(910)
				}
				printesc(899)
				{
					helpptr = 3
					helpline[2] = 911
					helpline[1] = 912
					helpline[0] = 913
				}
				mem[(curalign + 5)].hh.lh = 257
				error_()
			}
		}
	}
	if mem[(curalign+5)].hh.lh != 256 {
		{
			unsave()
			newsavelevel(6)
			{
				if curlist.modefield == (-102) {
					{
						adjusttail = curtail
						u = hpack(mem[curlist.headfield].hh.rh, 0, 1)
						w = mem[(u + 1)].int
						curtail = adjusttail
						adjusttail = 0
					}
				} else {
					{
						u = vpackage(mem[curlist.headfield].hh.rh, 0, 1, 0)
						w = mem[(u + 3)].int
					}
				}
				n = 0
				if curspan != curalign {
					{
						q = curspan
						for {
							n = (n + 1)
							q = mem[mem[q].hh.rh].hh.rh
							if !(q == curalign) {
								break
							}
						}
						if n > 255 {
							confusion(914)
						}
						q = curspan
						for mem[mem[q].hh.lh].hh.rh < n {
							q = mem[q].hh.lh
						}
						if mem[mem[q].hh.lh].hh.rh > n {
							{
								s = getnode(2)
								mem[s].hh.lh = mem[q].hh.lh
								mem[s].hh.rh = n
								mem[q].hh.lh = s
								mem[(s + 1)].int = w
							}
						} else {
							if mem[(mem[q].hh.lh+1)].int < w {
								mem[(mem[q].hh.lh + 1)].int = w
							}
						}
					}
				} else {
					if w > mem[(curalign+1)].int {
						mem[(curalign + 1)].int = w
					}
				}
				mem[u].hh.b0 = 13
				mem[u].hh.b1 = n
				if totalstretch[3] != 0 {
					o = 3
				} else {
					if totalstretch[2] != 0 {
						o = 2
					} else {
						if totalstretch[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(u + 5)].hh.b1 = o
				mem[(u + 6)].int = totalstretch[o]
				if totalshrink[3] != 0 {
					o = 3
				} else {
					if totalshrink[2] != 0 {
						o = 2
					} else {
						if totalshrink[1] != 0 {
							o = 1
						} else {
							o = 0
						}
					}
				}
				mem[(u + 5)].hh.b0 = o
				mem[(u + 4)].int = totalshrink[o]
				popnest()
				mem[curlist.tailfield].hh.rh = u
				curlist.tailfield = u
			}
			{
				mem[curlist.tailfield].hh.rh = newglue(mem[(mem[curalign].hh.rh + 1)].hh.lh)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.b1 = 12
			if mem[(curalign+5)].hh.lh >= 257 {
				{
					fincol = true
					goto L10
				}
			}
			initspan(p)
		}
	}
	alignstate = 1000000
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	curalign = p
	initcol()
	fincol = false
L10:
	// empty
}

/* procedure: finrow */
func finrow() {
	var (
		p int
	)
	if curlist.modefield == (-102) {
		{
			p = hpack(mem[curlist.headfield].hh.rh, 0, 1)
			popnest()
			appendtovlist(p)
			if curhead != curtail {
				{
					mem[curlist.tailfield].hh.rh = mem[curhead].hh.rh
					curlist.tailfield = curtail
				}
			}
		}
	} else {
		{
			p = vpackage(mem[curlist.headfield].hh.rh, 0, 1, 1073741823)
			popnest()
			mem[curlist.tailfield].hh.rh = p
			curlist.tailfield = p
			curlist.auxfield.hh.lh = 1000
		}
	}
	mem[p].hh.b0 = 13
	mem[(p + 6)].int = 0
	if eqtb[3420].hh.rh != 0 {
		begintokenlist(eqtb[3420].hh.rh, 13)
	}
	alignpeek()
}

/* procedure: finalign */
func finalign() {
	var (
		p        int
		q        int
		r        int
		s        int
		u        int
		v        int
		t        int
		w        int
		o        int
		n        int
		rulesave int
		auxsave  *memoryword_t
	)
	if curgroup != 6 {
		confusion(915)
	}
	unsave()
	if curgroup != 6 {
		confusion(916)
	}
	unsave()
	if nest[(nestptr-1)].modefield == 203 {
		o = eqtb[5845].int
	} else {
		o = 0
	}
	q = mem[mem[29992].hh.rh].hh.rh
	for {
		flushlist(mem[(q + 3)].int)
		flushlist(mem[(q + 2)].int)
		p = mem[mem[q].hh.rh].hh.rh
		if mem[(q+1)].int == (-1073741824) {
			{
				mem[(q + 1)].int = 0
				r = mem[q].hh.rh
				s = mem[(r + 1)].hh.lh
				if s != 0 {
					{
						mem[0].hh.rh = (mem[0].hh.rh + 1)
						deleteglueref(s)
						mem[(r + 1)].hh.lh = 0
					}
				}
			}
		}
		if mem[q].hh.lh != 29991 {
			{
				t = (mem[(q+1)].int + mem[(mem[(mem[q].hh.rh+1)].hh.lh+1)].int)
				r = mem[q].hh.lh
				s = 29991
				mem[s].hh.lh = p
				n = 1
				for {
					mem[(r + 1)].int = (mem[(r+1)].int - t)
					u = mem[r].hh.lh
					for mem[r].hh.rh > n {
						{
							s = mem[s].hh.lh
							n = (mem[mem[s].hh.lh].hh.rh + 1)
						}
					}
					if mem[r].hh.rh < n {
						{
							mem[r].hh.lh = mem[s].hh.lh
							mem[s].hh.lh = r
							mem[r].hh.rh = (mem[r].hh.rh - 1)
							s = r
						}
					} else {
						{
							if mem[(r+1)].int > mem[(mem[s].hh.lh+1)].int {
								mem[(mem[s].hh.lh + 1)].int = mem[(r + 1)].int
							}
							freenode(r, 2)
						}
					}
					r = u
					if !(r == 29991) {
						break
					}
				}
			}
		}
		mem[q].hh.b0 = 13
		mem[q].hh.b1 = 0
		mem[(q + 3)].int = 0
		mem[(q + 2)].int = 0
		mem[(q + 5)].hh.b1 = 0
		mem[(q + 5)].hh.b0 = 0
		mem[(q + 6)].int = 0
		mem[(q + 4)].int = 0
		q = p
		if !(q == 0) {
			break
		}
	}
	saveptr = (saveptr - 2)
	packbeginline = (-curlist.mlfield)
	if curlist.modefield == (-1) {
		{
			rulesave = eqtb[5846].int
			eqtb[5846].int = 0
			p = hpack(mem[29992].hh.rh, savestack[(saveptr+1)].int, savestack[(saveptr+0)].int)
			eqtb[5846].int = rulesave
		}
	} else {
		{
			q = mem[mem[29992].hh.rh].hh.rh
			for {
				mem[(q + 3)].int = mem[(q + 1)].int
				mem[(q + 1)].int = 0
				q = mem[mem[q].hh.rh].hh.rh
				if !(q == 0) {
					break
				}
			}
			p = vpackage(mem[29992].hh.rh, savestack[(saveptr+1)].int, savestack[(saveptr+0)].int, 1073741823)
			q = mem[mem[29992].hh.rh].hh.rh
			for {
				mem[(q + 1)].int = mem[(q + 3)].int
				mem[(q + 3)].int = 0
				q = mem[mem[q].hh.rh].hh.rh
				if !(q == 0) {
					break
				}
			}
		}
	}
	packbeginline = 0
	q = mem[curlist.headfield].hh.rh
	s = curlist.headfield
	for q != 0 {
		{
			if !(q >= himemmin) {
				if mem[q].hh.b0 == 13 {
					{
						if curlist.modefield == (-1) {
							{
								mem[q].hh.b0 = 0
								mem[(q + 1)].int = mem[(p + 1)].int
							}
						} else {
							{
								mem[q].hh.b0 = 1
								mem[(q + 3)].int = mem[(p + 3)].int
							}
						}
						mem[(q + 5)].hh.b1 = mem[(p + 5)].hh.b1
						mem[(q + 5)].hh.b0 = mem[(p + 5)].hh.b0
						mem[(q + 6)].gr = mem[(p + 6)].gr
						mem[(q + 4)].int = o
						r = mem[mem[(q+5)].hh.rh].hh.rh
						s = mem[mem[(p+5)].hh.rh].hh.rh
						for {
							n = mem[r].hh.b1
							t = mem[(s + 1)].int
							w = t
							u = 29996
							for n > 0 {
								{
									n = (n - 1)
									s = mem[s].hh.rh
									v = mem[(s + 1)].hh.lh
									mem[u].hh.rh = newglue(v)
									u = mem[u].hh.rh
									mem[u].hh.b1 = 12
									t = (t + mem[(v+1)].int)
									if mem[(p+5)].hh.b0 == 1 {
										{
											if mem[v].hh.b0 == mem[(p+5)].hh.b1 {
												t = (t + round_((mem[(p+6)].gr * mem[(v+2)].int)))
											}
										}
									} else {
										if mem[(p+5)].hh.b0 == 2 {
											{
												if mem[v].hh.b1 == mem[(p+5)].hh.b1 {
													t = (t - round_((mem[(p+6)].gr * mem[(v+3)].int)))
												}
											}
										}
									}
									s = mem[s].hh.rh
									mem[u].hh.rh = newnullbox
									u = mem[u].hh.rh
									t = (t + mem[(s+1)].int)
									if curlist.modefield == (-1) {
										mem[(u + 1)].int = mem[(s + 1)].int
									} else {
										{
											mem[u].hh.b0 = 1
											mem[(u + 3)].int = mem[(s + 1)].int
										}
									}
								}
							}
							if curlist.modefield == (-1) {
								{
									mem[(r + 3)].int = mem[(q + 3)].int
									mem[(r + 2)].int = mem[(q + 2)].int
									if t == mem[(r+1)].int {
										{
											mem[(r + 5)].hh.b0 = 0
											mem[(r + 5)].hh.b1 = 0
											mem[(r + 6)].gr = 0
										}
									} else {
										if t > mem[(r+1)].int {
											{
												mem[(r + 5)].hh.b0 = 1
												if mem[(r+6)].int == 0 {
													mem[(r + 6)].gr = 0
												} else {
													mem[(r + 6)].gr = ((t - mem[(r+1)].int) / mem[(r+6)].int)
												}
											}
										} else {
											{
												mem[(r + 5)].hh.b1 = mem[(r + 5)].hh.b0
												mem[(r + 5)].hh.b0 = 2
												if mem[(r+4)].int == 0 {
													mem[(r + 6)].gr = 0
												} else {
													if (mem[(r+5)].hh.b1 == 0) && ((mem[(r+1)].int - t) > mem[(r+4)].int) {
														mem[(r + 6)].gr = 1
													} else {
														mem[(r + 6)].gr = ((mem[(r+1)].int - t) / mem[(r+4)].int)
													}
												}
											}
										}
									}
									mem[(r + 1)].int = w
									mem[r].hh.b0 = 0
								}
							} else {
								{
									mem[(r + 1)].int = mem[(q + 1)].int
									if t == mem[(r+3)].int {
										{
											mem[(r + 5)].hh.b0 = 0
											mem[(r + 5)].hh.b1 = 0
											mem[(r + 6)].gr = 0
										}
									} else {
										if t > mem[(r+3)].int {
											{
												mem[(r + 5)].hh.b0 = 1
												if mem[(r+6)].int == 0 {
													mem[(r + 6)].gr = 0
												} else {
													mem[(r + 6)].gr = ((t - mem[(r+3)].int) / mem[(r+6)].int)
												}
											}
										} else {
											{
												mem[(r + 5)].hh.b1 = mem[(r + 5)].hh.b0
												mem[(r + 5)].hh.b0 = 2
												if mem[(r+4)].int == 0 {
													mem[(r + 6)].gr = 0
												} else {
													if (mem[(r+5)].hh.b1 == 0) && ((mem[(r+3)].int - t) > mem[(r+4)].int) {
														mem[(r + 6)].gr = 1
													} else {
														mem[(r + 6)].gr = ((mem[(r+3)].int - t) / mem[(r+4)].int)
													}
												}
											}
										}
									}
									mem[(r + 3)].int = w
									mem[r].hh.b0 = 1
								}
							}
							mem[(r + 4)].int = 0
							if u != 29996 {
								{
									mem[u].hh.rh = mem[r].hh.rh
									mem[r].hh.rh = mem[29996].hh.rh
									r = u
								}
							}
							r = mem[mem[r].hh.rh].hh.rh
							s = mem[mem[s].hh.rh].hh.rh
							if !(r == 0) {
								break
							}
						}
					}
				} else {
					if mem[q].hh.b0 == 2 {
						{
							if mem[(q+1)].int == (-1073741824) {
								mem[(q + 1)].int = mem[(p + 1)].int
							}
							if mem[(q+3)].int == (-1073741824) {
								mem[(q + 3)].int = mem[(p + 3)].int
							}
							if mem[(q+2)].int == (-1073741824) {
								mem[(q + 2)].int = mem[(p + 2)].int
							}
							if o != 0 {
								{
									r = mem[q].hh.rh
									mem[q].hh.rh = 0
									q = hpack(q, 0, 1)
									mem[(q + 4)].int = o
									mem[q].hh.rh = r
									mem[s].hh.rh = q
								}
							}
						}
					}
				}
			}
			s = q
			q = mem[q].hh.rh
		}
	}
	flushnodelist(p)
	popalignment()
	auxsave = curlist.auxfield
	p = mem[curlist.headfield].hh.rh
	q = curlist.tailfield
	popnest()
	if curlist.modefield == 203 {
		{
			doassignments()
			if curcmd != 3 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1170)
					}
					{
						helpptr = 2
						helpline[1] = 895
						helpline[0] = 896
					}
					backerror()
				}
			} else {
				{
					getxtoken()
					if curcmd != 3 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(1166)
							}
							{
								helpptr = 2
								helpline[1] = 1167
								helpline[0] = 1168
							}
							backerror()
						}
					}
				}
			}
			popnest()
			{
				mem[curlist.tailfield].hh.rh = newpenalty(eqtb[5274].int)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			{
				mem[curlist.tailfield].hh.rh = newparamglue(3)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.rh = p
			if p != 0 {
				curlist.tailfield = q
			}
			{
				mem[curlist.tailfield].hh.rh = newpenalty(eqtb[5275].int)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			{
				mem[curlist.tailfield].hh.rh = newparamglue(4)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			curlist.auxfield.int = auxsave.int
			resumeafterdisplay()
		}
	} else {
		{
			curlist.auxfield = auxsave
			mem[curlist.tailfield].hh.rh = p
			if p != 0 {
				curlist.tailfield = q
			}
			if curlist.modefield == 1 {
				buildpage()
			}
		}
	}
}

/* procedure: alignpeek */
func alignpeek() {
L20:
	alignstate = 1000000
	for {
		getxtoken()
		if !(curcmd != 10) {
			break
		}
	}
	if curcmd == 34 {
		{
			scanleftbrace()
			newsavelevel(7)
			if curlist.modefield == (-1) {
				normalparagraph()
			}
		}
	} else {
		if curcmd == 2 {
			finalign()
		} else {
			if (curcmd == 5) && (curchr == 258) {
				goto L20
			} else {
				{
					initrow()
					initcol()
				}
			}
		}
	}
}

/* function: finiteshrink */
func finiteshrink(p int) int {
	var (
		q int
	)
	if noshrinkerroryet {
		{
			noshrinkerroryet = false
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(917)
			}
			{
				helpptr = 5
				helpline[4] = 918
				helpline[3] = 919
				helpline[2] = 920
				helpline[1] = 921
				helpline[0] = 922
			}
			error_()
		}
	}
	q = newspec(p)
	mem[q].hh.b1 = 0
	deleteglueref(p)
	finiteshrink = q
}

/* procedure: trybreak */
func trybreak(pi int, breaktype int) {
	var (
		r                  int
		prevr              int
		oldl               int
		nobreakyet         bool
		prevprevr          int
		s                  int
		q                  int
		v                  int
		t                  int
		f                  int
		l                  int
		noderstaysactive   bool
		linewidth          int
		fitclass           int
		b                  int
		d                  int
		artificialdemerits bool
		savelink           int
		shortfall          int
	)
	if abs_(pi) >= 10000 {
		if pi > 0 {
			goto L10
		} else {
			pi = (-10000)
		}
	}
	nobreakyet = true
	prevr = 29993
	oldl = 0
	curactivewidth[1] = activewidth[1]
	curactivewidth[2] = activewidth[2]
	curactivewidth[3] = activewidth[3]
	curactivewidth[4] = activewidth[4]
	curactivewidth[5] = activewidth[5]
	curactivewidth[6] = activewidth[6]
	for true {
		{
		L22:
			r = mem[prevr].hh.rh
			if mem[r].hh.b0 == 2 {
				{
					curactivewidth[1] = (curactivewidth[1] + mem[(r+1)].int)
					curactivewidth[2] = (curactivewidth[2] + mem[(r+2)].int)
					curactivewidth[3] = (curactivewidth[3] + mem[(r+3)].int)
					curactivewidth[4] = (curactivewidth[4] + mem[(r+4)].int)
					curactivewidth[5] = (curactivewidth[5] + mem[(r+5)].int)
					curactivewidth[6] = (curactivewidth[6] + mem[(r+6)].int)
					prevprevr = prevr
					prevr = r
					goto L22
				}
			}
			{
				l = mem[(r + 1)].hh.lh
				if l > oldl {
					{
						if (minimumdemerits < 1073741823) && ((oldl != easyline) || (r == 29993)) {
							{
								if nobreakyet {
									{
										nobreakyet = false
										breakwidth[1] = background[1]
										breakwidth[2] = background[2]
										breakwidth[3] = background[3]
										breakwidth[4] = background[4]
										breakwidth[5] = background[5]
										breakwidth[6] = background[6]
										s = curp
										if breaktype > 0 {
											if curp != 0 {
												{
													t = mem[curp].hh.b1
													v = curp
													s = mem[(curp + 1)].hh.rh
													for t > 0 {
														{
															t = (t - 1)
															v = mem[v].hh.rh
															if v >= himemmin {
																{
																	f = mem[v].hh.b0
																	breakwidth[1] = (breakwidth[1] - fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[v].hh.b1)].qqqq.b0)].int)
																}
															} else {
																switch mem[v].hh.b0 {
																case 6:
																	{
																		f = mem[(v + 1)].hh.b0
																		breakwidth[1] = (breakwidth[1] - fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[(v+1)].hh.b1)].qqqq.b0)].int)
																	}
																case 0:
																	breakwidth[1] = (breakwidth[1] - mem[(v+1)].int)
																case 1:
																	breakwidth[1] = (breakwidth[1] - mem[(v+1)].int)
																case 2:
																	breakwidth[1] = (breakwidth[1] - mem[(v+1)].int)
																case 11:
																	breakwidth[1] = (breakwidth[1] - mem[(v+1)].int)
																default:
																	confusion(923)
																}
															}
														}
													}
													for s != 0 {
														{
															if s >= himemmin {
																{
																	f = mem[s].hh.b0
																	breakwidth[1] = (breakwidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[s].hh.b1)].qqqq.b0)].int)
																}
															} else {
																switch mem[s].hh.b0 {
																case 6:
																	{
																		f = mem[(s + 1)].hh.b0
																		breakwidth[1] = (breakwidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[(s+1)].hh.b1)].qqqq.b0)].int)
																	}
																case 0:
																	breakwidth[1] = (breakwidth[1] + mem[(s+1)].int)
																case 1:
																	breakwidth[1] = (breakwidth[1] + mem[(s+1)].int)
																case 2:
																	breakwidth[1] = (breakwidth[1] + mem[(s+1)].int)
																case 11:
																	breakwidth[1] = (breakwidth[1] + mem[(s+1)].int)
																default:
																	confusion(924)
																}
															}
															s = mem[s].hh.rh
														}
													}
													breakwidth[1] = (breakwidth[1] + discwidth)
													if mem[(curp+1)].hh.rh == 0 {
														s = mem[v].hh.rh
													}
												}
											}
										}
										for s != 0 {
											{
												if s >= himemmin {
													goto L30
												}
												switch mem[s].hh.b0 {
												case 10:
													{
														v = mem[(s + 1)].hh.lh
														breakwidth[1] = (breakwidth[1] - mem[(v+1)].int)
														breakwidth[(2 + mem[v].hh.b0)] = (breakwidth[(2+mem[v].hh.b0)] - mem[(v+2)].int)
														breakwidth[6] = (breakwidth[6] - mem[(v+3)].int)
													}
												case 12:
													// empty
												case 9:
													breakwidth[1] = (breakwidth[1] - mem[(s+1)].int)
												case 11:
													if mem[s].hh.b1 != 1 {
														goto L30
													} else {
														breakwidth[1] = (breakwidth[1] - mem[(s+1)].int)
													}
												default:
													goto L30
												}
												s = mem[s].hh.rh
											}
										}
									L30:
										// empty
									}
								}
								if mem[prevr].hh.b0 == 2 {
									{
										mem[(prevr + 1)].int = ((mem[(prevr+1)].int - curactivewidth[1]) + breakwidth[1])
										mem[(prevr + 2)].int = ((mem[(prevr+2)].int - curactivewidth[2]) + breakwidth[2])
										mem[(prevr + 3)].int = ((mem[(prevr+3)].int - curactivewidth[3]) + breakwidth[3])
										mem[(prevr + 4)].int = ((mem[(prevr+4)].int - curactivewidth[4]) + breakwidth[4])
										mem[(prevr + 5)].int = ((mem[(prevr+5)].int - curactivewidth[5]) + breakwidth[5])
										mem[(prevr + 6)].int = ((mem[(prevr+6)].int - curactivewidth[6]) + breakwidth[6])
									}
								} else {
									if prevr == 29993 {
										{
											activewidth[1] = breakwidth[1]
											activewidth[2] = breakwidth[2]
											activewidth[3] = breakwidth[3]
											activewidth[4] = breakwidth[4]
											activewidth[5] = breakwidth[5]
											activewidth[6] = breakwidth[6]
										}
									} else {
										{
											q = getnode(7)
											mem[q].hh.rh = r
											mem[q].hh.b0 = 2
											mem[q].hh.b1 = 0
											mem[(q + 1)].int = (breakwidth[1] - curactivewidth[1])
											mem[(q + 2)].int = (breakwidth[2] - curactivewidth[2])
											mem[(q + 3)].int = (breakwidth[3] - curactivewidth[3])
											mem[(q + 4)].int = (breakwidth[4] - curactivewidth[4])
											mem[(q + 5)].int = (breakwidth[5] - curactivewidth[5])
											mem[(q + 6)].int = (breakwidth[6] - curactivewidth[6])
											mem[prevr].hh.rh = q
											prevprevr = prevr
											prevr = q
										}
									}
								}
								if abs_(eqtb[5279].int) >= (1073741823 - minimumdemerits) {
									minimumdemerits = 1073741822
								} else {
									minimumdemerits = (minimumdemerits + abs_(eqtb[5279].int))
								}
								for fitclass := 0; fitclass <= 3; fitclass++ {
									{
										if minimaldemerits[fitclass] <= minimumdemerits {
											{
												q = getnode(2)
												mem[q].hh.rh = passive
												passive = q
												mem[(q + 1)].hh.rh = curp
												mem[(q + 1)].hh.lh = bestplace[fitclass]
												q = getnode(3)
												mem[(q + 1)].hh.rh = passive
												mem[(q + 1)].hh.lh = (bestplline[fitclass] + 1)
												mem[q].hh.b1 = fitclass
												mem[q].hh.b0 = breaktype
												mem[(q + 2)].int = minimaldemerits[fitclass]
												mem[q].hh.rh = r
												mem[prevr].hh.rh = q
												prevr = q
											}
										}
										minimaldemerits[fitclass] = 1073741823
									}
								}
								minimumdemerits = 1073741823
								if r != 29993 {
									{
										q = getnode(7)
										mem[q].hh.rh = r
										mem[q].hh.b0 = 2
										mem[q].hh.b1 = 0
										mem[(q + 1)].int = (curactivewidth[1] - breakwidth[1])
										mem[(q + 2)].int = (curactivewidth[2] - breakwidth[2])
										mem[(q + 3)].int = (curactivewidth[3] - breakwidth[3])
										mem[(q + 4)].int = (curactivewidth[4] - breakwidth[4])
										mem[(q + 5)].int = (curactivewidth[5] - breakwidth[5])
										mem[(q + 6)].int = (curactivewidth[6] - breakwidth[6])
										mem[prevr].hh.rh = q
										prevprevr = prevr
										prevr = q
									}
								}
							}
						}
						if r == 29993 {
							goto L10
						}
						if l > easyline {
							{
								linewidth = secondwidth
								oldl = 65534
							}
						} else {
							{
								oldl = l
								if l > lastspecialline {
									linewidth = secondwidth
								} else {
									if eqtb[3412].hh.rh == 0 {
										linewidth = firstwidth
									} else {
										linewidth = mem[(eqtb[3412].hh.rh + (2 * l))].int
									}
								}
							}
						}
					}
				}
			}
			{
				artificialdemerits = false
				shortfall = (linewidth - curactivewidth[1])
				if shortfall > 0 {
					if ((curactivewidth[3] != 0) || (curactivewidth[4] != 0)) || (curactivewidth[5] != 0) {
						{
							b = 0
							fitclass = 2
						}
					} else {
						{
							if shortfall > 7230584 {
								if curactivewidth[2] < 1663497 {
									{
										b = 10000
										fitclass = 0
										goto L31
									}
								}
							}
							b = badness(shortfall, curactivewidth[2])
							if b > 12 {
								if b > 99 {
									fitclass = 0
								} else {
									fitclass = 1
								}
							} else {
								fitclass = 2
							}
						L31:
							// empty
						}
					}
				} else {
					{
						if (-shortfall) > curactivewidth[6] {
							b = 10001
						} else {
							b = badness((-shortfall), curactivewidth[6])
						}
						if b > 12 {
							fitclass = 3
						} else {
							fitclass = 2
						}
					}
				}
				if (b > 10000) || (pi == (-10000)) {
					{
						if ((finalpass && (minimumdemerits == 1073741823)) && (mem[r].hh.rh == 29993)) && (prevr == 29993) {
							artificialdemerits = true
						} else {
							if b > threshold {
								goto L60
							}
						}
						noderstaysactive = false
					}
				} else {
					{
						prevr = r
						if b > threshold {
							goto L22
						}
						noderstaysactive = true
					}
				}
				if artificialdemerits {
					d = 0
				} else {
					{
						d = (eqtb[5265].int + b)
						if abs_(d) >= 10000 {
							d = 100000000
						} else {
							d = (d * d)
						}
						if pi != 0 {
							if pi > 0 {
								d = (d + (pi * pi))
							} else {
								if pi > (-10000) {
									d = (d - (pi * pi))
								}
							}
						}
						if (breaktype == 1) && (mem[r].hh.b0 == 1) {
							if curp != 0 {
								d = (d + eqtb[5277].int)
							} else {
								d = (d + eqtb[5278].int)
							}
						}
						if abs_((fitclass - mem[r].hh.b1)) > 1 {
							d = (d + eqtb[5279].int)
						}
					}
				}
				d = (d + mem[(r+2)].int)
				if d <= minimaldemerits[fitclass] {
					{
						minimaldemerits[fitclass] = d
						bestplace[fitclass] = mem[(r + 1)].hh.rh
						bestplline[fitclass] = l
						if d < minimumdemerits {
							minimumdemerits = d
						}
					}
				}
				if noderstaysactive {
					goto L22
				}
			L60:
				mem[prevr].hh.rh = mem[r].hh.rh
				freenode(r, 3)
				if prevr == 29993 {
					{
						r = mem[29993].hh.rh
						if mem[r].hh.b0 == 2 {
							{
								activewidth[1] = (activewidth[1] + mem[(r+1)].int)
								activewidth[2] = (activewidth[2] + mem[(r+2)].int)
								activewidth[3] = (activewidth[3] + mem[(r+3)].int)
								activewidth[4] = (activewidth[4] + mem[(r+4)].int)
								activewidth[5] = (activewidth[5] + mem[(r+5)].int)
								activewidth[6] = (activewidth[6] + mem[(r+6)].int)
								curactivewidth[1] = activewidth[1]
								curactivewidth[2] = activewidth[2]
								curactivewidth[3] = activewidth[3]
								curactivewidth[4] = activewidth[4]
								curactivewidth[5] = activewidth[5]
								curactivewidth[6] = activewidth[6]
								mem[29993].hh.rh = mem[r].hh.rh
								freenode(r, 7)
							}
						}
					}
				} else {
					if mem[prevr].hh.b0 == 2 {
						{
							r = mem[prevr].hh.rh
							if r == 29993 {
								{
									curactivewidth[1] = (curactivewidth[1] - mem[(prevr+1)].int)
									curactivewidth[2] = (curactivewidth[2] - mem[(prevr+2)].int)
									curactivewidth[3] = (curactivewidth[3] - mem[(prevr+3)].int)
									curactivewidth[4] = (curactivewidth[4] - mem[(prevr+4)].int)
									curactivewidth[5] = (curactivewidth[5] - mem[(prevr+5)].int)
									curactivewidth[6] = (curactivewidth[6] - mem[(prevr+6)].int)
									mem[prevprevr].hh.rh = 29993
									freenode(prevr, 7)
									prevr = prevprevr
								}
							} else {
								if mem[r].hh.b0 == 2 {
									{
										curactivewidth[1] = (curactivewidth[1] + mem[(r+1)].int)
										curactivewidth[2] = (curactivewidth[2] + mem[(r+2)].int)
										curactivewidth[3] = (curactivewidth[3] + mem[(r+3)].int)
										curactivewidth[4] = (curactivewidth[4] + mem[(r+4)].int)
										curactivewidth[5] = (curactivewidth[5] + mem[(r+5)].int)
										curactivewidth[6] = (curactivewidth[6] + mem[(r+6)].int)
										mem[(prevr + 1)].int = (mem[(prevr+1)].int + mem[(r+1)].int)
										mem[(prevr + 2)].int = (mem[(prevr+2)].int + mem[(r+2)].int)
										mem[(prevr + 3)].int = (mem[(prevr+3)].int + mem[(r+3)].int)
										mem[(prevr + 4)].int = (mem[(prevr+4)].int + mem[(r+4)].int)
										mem[(prevr + 5)].int = (mem[(prevr+5)].int + mem[(r+5)].int)
										mem[(prevr + 6)].int = (mem[(prevr+6)].int + mem[(r+6)].int)
										mem[prevr].hh.rh = mem[r].hh.rh
										freenode(r, 7)
									}
								}
							}
						}
					}
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: postlinebreak */
func postlinebreak(finalwidowpenalty int) {
	var (
		q             int
		r             int
		s             int
		discbreak     bool
		postdiscbreak bool
		curwidth      int
		curindent     int
		t             int
		pen           int
		curline       int
	)
	q = mem[(bestbet + 1)].hh.rh
	curp = 0
	for {
		r = q
		q = mem[(q + 1)].hh.lh
		mem[(r + 1)].hh.lh = curp
		curp = r
		if !(q == 0) {
			break
		}
	}
	curline = (curlist.pgfield + 1)
	for {
		q = mem[(curp + 1)].hh.rh
		discbreak = false
		postdiscbreak = false
		if q != 0 {
			if mem[q].hh.b0 == 10 {
				{
					deleteglueref(mem[(q + 1)].hh.lh)
					mem[(q + 1)].hh.lh = eqtb[2890].hh.rh
					mem[q].hh.b1 = 9
					mem[eqtb[2890].hh.rh].hh.rh = (mem[eqtb[2890].hh.rh].hh.rh + 1)
					goto L30
				}
			} else {
				{
					if mem[q].hh.b0 == 7 {
						{
							t = mem[q].hh.b1
							if t == 0 {
								r = mem[q].hh.rh
							} else {
								{
									r = q
									for t > 1 {
										{
											r = mem[r].hh.rh
											t = (t - 1)
										}
									}
									s = mem[r].hh.rh
									r = mem[s].hh.rh
									mem[s].hh.rh = 0
									flushnodelist(mem[q].hh.rh)
									mem[q].hh.b1 = 0
								}
							}
							if mem[(q+1)].hh.rh != 0 {
								{
									s = mem[(q + 1)].hh.rh
									for mem[s].hh.rh != 0 {
										s = mem[s].hh.rh
									}
									mem[s].hh.rh = r
									r = mem[(q + 1)].hh.rh
									mem[(q + 1)].hh.rh = 0
									postdiscbreak = true
								}
							}
							if mem[(q+1)].hh.lh != 0 {
								{
									s = mem[(q + 1)].hh.lh
									mem[q].hh.rh = s
									for mem[s].hh.rh != 0 {
										s = mem[s].hh.rh
									}
									mem[(q + 1)].hh.lh = 0
									q = s
								}
							}
							mem[q].hh.rh = r
							discbreak = true
						}
					} else {
						if (mem[q].hh.b0 == 9) || (mem[q].hh.b0 == 11) {
							mem[(q + 1)].int = 0
						}
					}
				}
			}
		} else {
			{
				q = 29997
				for mem[q].hh.rh != 0 {
					q = mem[q].hh.rh
				}
			}
		}
		r = newparamglue(8)
		mem[r].hh.rh = mem[q].hh.rh
		mem[q].hh.rh = r
		q = r
	L30:
		// empty
		r = mem[q].hh.rh
		mem[q].hh.rh = 0
		q = mem[29997].hh.rh
		mem[29997].hh.rh = r
		if eqtb[2889].hh.rh != 0 {
			{
				r = newparamglue(7)
				mem[r].hh.rh = q
				q = r
			}
		}
		if curline > lastspecialline {
			{
				curwidth = secondwidth
				curindent = secondindent
			}
		} else {
			if eqtb[3412].hh.rh == 0 {
				{
					curwidth = firstwidth
					curindent = firstindent
				}
			} else {
				{
					curwidth = mem[(eqtb[3412].hh.rh + (2 * curline))].int
					curindent = mem[((eqtb[3412].hh.rh + (2 * curline)) - 1)].int
				}
			}
		}
		adjusttail = 29995
		justbox = hpack(q, curwidth, 0)
		mem[(justbox + 4)].int = curindent
		appendtovlist(justbox)
		if 29995 != adjusttail {
			{
				mem[curlist.tailfield].hh.rh = mem[29995].hh.rh
				curlist.tailfield = adjusttail
			}
		}
		adjusttail = 0
		if (curline + 1) != bestline {
			{
				pen = eqtb[5276].int
				if curline == (curlist.pgfield + 1) {
					pen = (pen + eqtb[5268].int)
				}
				if (curline + 2) == bestline {
					pen = (pen + finalwidowpenalty)
				}
				if discbreak {
					pen = (pen + eqtb[5271].int)
				}
				if pen != 0 {
					{
						r = newpenalty(pen)
						mem[curlist.tailfield].hh.rh = r
						curlist.tailfield = r
					}
				}
			}
		}
		curline = (curline + 1)
		curp = mem[(curp + 1)].hh.lh
		if curp != 0 {
			if !postdiscbreak {
				{
					r = 29997
					for true {
						{
							q = mem[r].hh.rh
							if q == mem[(curp+1)].hh.rh {
								goto L31
							}
							if q >= himemmin {
								goto L31
							}
							if mem[q].hh.b0 < 9 {
								goto L31
							}
							if mem[q].hh.b0 == 11 {
								if mem[q].hh.b1 != 1 {
									goto L31
								}
							}
							r = q
						}
					}
				L31:
					if r != 29997 {
						{
							mem[r].hh.rh = 0
							flushnodelist(mem[29997].hh.rh)
							mem[29997].hh.rh = q
						}
					}
				}
			}
		}
		if !(curp == 0) {
			break
		}
	}
	if (curline != bestline) || (mem[29997].hh.rh != 0) {
		confusion(939)
	}
	curlist.pgfield = (bestline - 1)
}

/* function: reconstitute */
func reconstitute(j int, n int, bchar int, hchar int) int {
	var (
		p        int
		t        int
		q        *fourquarters_t
		currh    int
		testchar int
		w        int
		k        int
	)
	hyphenpassed = 0
	t = 29996
	w = 0
	mem[29996].hh.rh = 0
	curl = (hu[j] + 0)
	curq = t
	if j == 0 {
		{
			ligaturepresent = initlig
			p = initlist
			if ligaturepresent {
				lfthit = initlft
			}
			for p > 0 {
				{
					{
						mem[t].hh.rh = getavail
						t = mem[t].hh.rh
						mem[t].hh.b0 = hf
						mem[t].hh.b1 = mem[p].hh.b1
					}
					p = mem[p].hh.rh
				}
			}
		}
	} else {
		if curl < 256 {
			{
				mem[t].hh.rh = getavail
				t = mem[t].hh.rh
				mem[t].hh.b0 = hf
				mem[t].hh.b1 = curl
			}
		}
	}
	ligstack = 0
	{
		if j < n {
			curr = (hu[(j+1)] + 0)
		} else {
			curr = bchar
		}
		if (hyf[j] & 1) != 0 {
			currh = hchar
		} else {
			currh = 256
		}
	}
L22:
	if curl == 256 {
		{
			k = bcharlabel[hf]
			if k == 0 {
				goto L30
			} else {
				q = fontinfo[k].qqqq
			}
		}
	} else {
		{
			q = fontinfo[(charbase[hf] + curl)].qqqq
			if ((q.b2 - 0) % 4) != 1 {
				goto L30
			}
			k = (ligkernbase[hf] + q.b3)
			q = fontinfo[k].qqqq
			if q.b0 > 128 {
				{
					k = ((((ligkernbase[hf] + (256 * q.b2)) + q.b3) + 32768) - (256 * 128))
					q = fontinfo[k].qqqq
				}
			}
		}
	}
	if currh < 256 {
		testchar = currh
	} else {
		testchar = curr
	}
	for true {
		{
			if q.b1 == testchar {
				if q.b0 <= 128 {
					if currh < 256 {
						{
							hyphenpassed = j
							hchar = 256
							currh = 256
							goto L22
						}
					} else {
						{
							if hchar < 256 {
								if (hyf[j] & 1) != 0 {
									{
										hyphenpassed = j
										hchar = 256
									}
								}
							}
							if q.b2 < 128 {
								{
									if curl == 256 {
										lfthit = true
									}
									if j == n {
										if ligstack == 0 {
											rthit = true
										}
									}
									{
										if interrupt != 0 {
											pauseforinstructions()
										}
									}
									switch q.b2 {
									case 1:
										{
											curl = q.b3
											ligaturepresent = true
										}
									case 5:
										{
											curl = q.b3
											ligaturepresent = true
										}
									case 2:
										{
											curr = q.b3
											if ligstack > 0 {
												mem[ligstack].hh.b1 = curr
											} else {
												{
													ligstack = newligitem(curr)
													if j == n {
														bchar = 256
													} else {
														{
															p = getavail
															mem[(ligstack + 1)].hh.rh = p
															mem[p].hh.b1 = (hu[(j+1)] + 0)
															mem[p].hh.b0 = hf
														}
													}
												}
											}
										}
									case 6:
										{
											curr = q.b3
											if ligstack > 0 {
												mem[ligstack].hh.b1 = curr
											} else {
												{
													ligstack = newligitem(curr)
													if j == n {
														bchar = 256
													} else {
														{
															p = getavail
															mem[(ligstack + 1)].hh.rh = p
															mem[p].hh.b1 = (hu[(j+1)] + 0)
															mem[p].hh.b0 = hf
														}
													}
												}
											}
										}
									case 3:
										{
											curr = q.b3
											p = ligstack
											ligstack = newligitem(curr)
											mem[ligstack].hh.rh = p
										}
									case 7:
										{
											if ligaturepresent {
												{
													p = newligature(hf, curl, mem[curq].hh.rh)
													if lfthit {
														{
															mem[p].hh.b1 = 2
															lfthit = false
														}
													}
													if false {
														if ligstack == 0 {
															{
																mem[p].hh.b1 = (mem[p].hh.b1 + 1)
																rthit = false
															}
														}
													}
													mem[curq].hh.rh = p
													t = p
													ligaturepresent = false
												}
											}
											curq = t
											curl = q.b3
											ligaturepresent = true
										}
									case 11:
										{
											if ligaturepresent {
												{
													p = newligature(hf, curl, mem[curq].hh.rh)
													if lfthit {
														{
															mem[p].hh.b1 = 2
															lfthit = false
														}
													}
													if false {
														if ligstack == 0 {
															{
																mem[p].hh.b1 = (mem[p].hh.b1 + 1)
																rthit = false
															}
														}
													}
													mem[curq].hh.rh = p
													t = p
													ligaturepresent = false
												}
											}
											curq = t
											curl = q.b3
											ligaturepresent = true
										}
									default:
										{
											curl = q.b3
											ligaturepresent = true
											if ligstack > 0 {
												{
													if mem[(ligstack+1)].hh.rh > 0 {
														{
															mem[t].hh.rh = mem[(ligstack + 1)].hh.rh
															t = mem[t].hh.rh
															j = (j + 1)
														}
													}
													p = ligstack
													ligstack = mem[p].hh.rh
													freenode(p, 2)
													if ligstack == 0 {
														{
															if j < n {
																curr = (hu[(j+1)] + 0)
															} else {
																curr = bchar
															}
															if (hyf[j] & 1) != 0 {
																currh = hchar
															} else {
																currh = 256
															}
														}
													} else {
														curr = mem[ligstack].hh.b1
													}
												}
											} else {
												if j == n {
													goto L30
												} else {
													{
														{
															mem[t].hh.rh = getavail
															t = mem[t].hh.rh
															mem[t].hh.b0 = hf
															mem[t].hh.b1 = curr
														}
														j = (j + 1)
														{
															if j < n {
																curr = (hu[(j+1)] + 0)
															} else {
																curr = bchar
															}
															if (hyf[j] & 1) != 0 {
																currh = hchar
															} else {
																currh = 256
															}
														}
													}
												}
											}
										}
									}
									if q.b2 > 4 {
										if q.b2 != 7 {
											goto L30
										}
									}
									goto L22
								}
							}
							w = fontinfo[((kernbase[hf] + (256 * q.b2)) + q.b3)].int
							goto L30
						}
					}
				}
			}
			if q.b0 >= 128 {
				if currh == 256 {
					goto L30
				} else {
					{
						currh = 256
						goto L22
					}
				}
			}
			k = ((k + q.b0) + 1)
			q = fontinfo[k].qqqq
		}
	}
L30:
	// empty
	if ligaturepresent {
		{
			p = newligature(hf, curl, mem[curq].hh.rh)
			if lfthit {
				{
					mem[p].hh.b1 = 2
					lfthit = false
				}
			}
			if rthit {
				if ligstack == 0 {
					{
						mem[p].hh.b1 = (mem[p].hh.b1 + 1)
						rthit = false
					}
				}
			}
			mem[curq].hh.rh = p
			t = p
			ligaturepresent = false
		}
	}
	if w != 0 {
		{
			mem[t].hh.rh = newkern(w)
			t = mem[t].hh.rh
			w = 0
		}
	}
	if ligstack > 0 {
		{
			curq = t
			curl = mem[ligstack].hh.b1
			ligaturepresent = true
			{
				if mem[(ligstack+1)].hh.rh > 0 {
					{
						mem[t].hh.rh = mem[(ligstack + 1)].hh.rh
						t = mem[t].hh.rh
						j = (j + 1)
					}
				}
				p = ligstack
				ligstack = mem[p].hh.rh
				freenode(p, 2)
				if ligstack == 0 {
					{
						if j < n {
							curr = (hu[(j+1)] + 0)
						} else {
							curr = bchar
						}
						if (hyf[j] & 1) != 0 {
							currh = hchar
						} else {
							currh = 256
						}
					}
				} else {
					curr = mem[ligstack].hh.b1
				}
			}
			goto L22
		}
	}
	reconstitute = j
}

/* procedure: hyphenate */
func hyphenate() {
	var (
		i         int
		j         int
		l         int
		q         int
		r         int
		s         int
		bchar     int
		majortail int
		minortail int
		c         byte
		cloc      int
		rcount    int
		hyfnode   int
		z         *triepointer_t
		v         int
		h         int
		k         int
		u         int
	)
	for j := 0; j <= hn; j++ {
		hyf[j] = 0
	}
	h = hc[1]
	hn = (hn + 1)
	hc[hn] = curlang
	for j := 2; j <= hn; j++ {
		h = (((h + h) + hc[j]) % 307)
	}
	for true {
		{
			k = hyphword[h]
			if k == 0 {
				goto L45
			}
			if (strstart[(k+1)] - strstart[k]) < hn {
				goto L45
			}
			if (strstart[(k+1)] - strstart[k]) == hn {
				{
					j = 1
					u = strstart[k]
					for {
						if strpool[u] < hc[j] {
							goto L45
						}
						if strpool[u] > hc[j] {
							goto L30
						}
						j = (j + 1)
						u = (u + 1)
						if !(j > hn) {
							break
						}
					}
					s = hyphlist[h]
					for s != 0 {
						{
							hyf[mem[s].hh.lh] = 1
							s = mem[s].hh.rh
						}
					}
					hn = (hn - 1)
					goto L40
				}
			}
		L30:
			// empty
			if h > 0 {
				h = (h - 1)
			} else {
				h = 307
			}
		}
	}
L45:
	hn = (hn - 1)
	if trie[(curlang+1)].b1 != (curlang + 0) {
		goto L10
	}
	hc[0] = 0
	hc[(hn + 1)] = 0
	hc[(hn + 2)] = 256
	for j := 0; j <= ((hn - rhyf) + 1); j++ {
		{
			z = (trie[(curlang+1)].rh + hc[j])
			l = j
			for hc[l] == (trie[z].b1 - 0) {
				{
					if trie[z].b0 != 0 {
						{
							v = trie[z].b0
							for {
								v = (v + opstart[curlang])
								i = (l - hyfdistance[v])
								if hyfnum[v] > hyf[i] {
									hyf[i] = hyfnum[v]
								}
								v = hyfnext[v]
								if !(v == 0) {
									break
								}
							}
						}
					}
					l = (l + 1)
					z = (trie[z].rh + hc[l])
				}
			}
		}
	}
L40:
	for j := 0; j <= (lhyf - 1); j++ {
		hyf[j] = 0
	}
	for j := 0; j <= (rhyf - 1); j++ {
		hyf[(hn - j)] = 0
	}
	for j := lhyf; j <= (hn - rhyf); j++ {
		if (hyf[j] & 1) != 0 {
			goto L41
		}
	}
	goto L10
L41:
	// empty
	q = mem[hb].hh.rh
	mem[hb].hh.rh = 0
	r = mem[ha].hh.rh
	mem[ha].hh.rh = 0
	bchar = hyfbchar
	if ha >= himemmin {
		if mem[ha].hh.b0 != hf {
			goto L42
		} else {
			{
				initlist = ha
				initlig = false
				hu[0] = (mem[ha].hh.b1 - 0)
			}
		}
	} else {
		if mem[ha].hh.b0 == 6 {
			if mem[(ha+1)].hh.b0 != hf {
				goto L42
			} else {
				{
					initlist = mem[(ha + 1)].hh.rh
					initlig = true
					initlft = (mem[ha].hh.b1 > 1)
					hu[0] = (mem[(ha+1)].hh.b1 - 0)
					if initlist == 0 {
						if initlft {
							{
								hu[0] = 256
								initlig = false
							}
						}
					}
					freenode(ha, 2)
				}
			}
		} else {
			{
				if !(r >= himemmin) {
					if mem[r].hh.b0 == 6 {
						if mem[r].hh.b1 > 1 {
							goto L42
						}
					}
				}
				j = 1
				s = ha
				initlist = 0
				goto L50
			}
		}
	}
	s = curp
	for mem[s].hh.rh != ha {
		s = mem[s].hh.rh
	}
	j = 0
	goto L50
L42:
	s = ha
	j = 0
	hu[0] = 256
	initlig = false
	initlist = 0
L50:
	flushnodelist(r)
	for {
		l = j
		j = (reconstitute(j, hn, bchar, (hyfchar+0)) + 1)
		if hyphenpassed == 0 {
			{
				mem[s].hh.rh = mem[29996].hh.rh
				for mem[s].hh.rh > 0 {
					s = mem[s].hh.rh
				}
				if (hyf[(j-1)] & 1) != 0 {
					{
						l = j
						hyphenpassed = (j - 1)
						mem[29996].hh.rh = 0
					}
				}
			}
		}
		if hyphenpassed > 0 {
			for {
				r = getnode(2)
				mem[r].hh.rh = mem[29996].hh.rh
				mem[r].hh.b0 = 7
				majortail = r
				rcount = 0
				for mem[majortail].hh.rh > 0 {
					{
						majortail = mem[majortail].hh.rh
						rcount = (rcount + 1)
					}
				}
				i = hyphenpassed
				hyf[i] = 0
				minortail = 0
				mem[(r + 1)].hh.lh = 0
				hyfnode = newcharacter(hf, hyfchar)
				if hyfnode != 0 {
					{
						i = (i + 1)
						c = hu[i]
						hu[i] = hyfchar
						{
							mem[hyfnode].hh.rh = avail
							avail = hyfnode
						}
					}
				}
				for l <= i {
					{
						l = (reconstitute(l, i, fontbchar[hf], 256) + 1)
						if mem[29996].hh.rh > 0 {
							{
								if minortail == 0 {
									mem[(r + 1)].hh.lh = mem[29996].hh.rh
								} else {
									mem[minortail].hh.rh = mem[29996].hh.rh
								}
								minortail = mem[29996].hh.rh
								for mem[minortail].hh.rh > 0 {
									minortail = mem[minortail].hh.rh
								}
							}
						}
					}
				}
				if hyfnode != 0 {
					{
						hu[i] = c
						l = i
						i = (i - 1)
					}
				}
				minortail = 0
				mem[(r + 1)].hh.rh = 0
				cloc = 0
				if bcharlabel[hf] != 0 {
					{
						l = (l - 1)
						c = hu[l]
						cloc = l
						hu[l] = 256
					}
				}
				for l < j {
					{
						for {
							l = (reconstitute(l, hn, bchar, 256) + 1)
							if cloc > 0 {
								{
									hu[cloc] = c
									cloc = 0
								}
							}
							if mem[29996].hh.rh > 0 {
								{
									if minortail == 0 {
										mem[(r + 1)].hh.rh = mem[29996].hh.rh
									} else {
										mem[minortail].hh.rh = mem[29996].hh.rh
									}
									minortail = mem[29996].hh.rh
									for mem[minortail].hh.rh > 0 {
										minortail = mem[minortail].hh.rh
									}
								}
							}
							if !(l >= j) {
								break
							}
						}
						for l > j {
							{
								j = (reconstitute(j, hn, bchar, 256) + 1)
								mem[majortail].hh.rh = mem[29996].hh.rh
								for mem[majortail].hh.rh > 0 {
									{
										majortail = mem[majortail].hh.rh
										rcount = (rcount + 1)
									}
								}
							}
						}
					}
				}
				if rcount > 127 {
					{
						mem[s].hh.rh = mem[r].hh.rh
						mem[r].hh.rh = 0
						flushnodelist(r)
					}
				} else {
					{
						mem[s].hh.rh = r
						mem[r].hh.b1 = rcount
					}
				}
				s = majortail
				hyphenpassed = (j - 1)
				mem[29996].hh.rh = 0
				if !(!((hyf[(j-1)] & 1) != 0)) {
					break
				}
			}
		}
		if !(j > hn) {
			break
		}
	}
	mem[s].hh.rh = q
	flushlist(initlist)
L10:
	// empty
}

/* function: newtrieop */
func newtrieop(d int, n int, v int) int {
	var (
		h int
		u int
		l int
	)
	h = ((abs_((((n + (313 * d)) + (361 * v)) + (1009 * curlang))) % (trieopsize + trieopsize)) - trieopsize)
	for true {
		{
			l = trieophash[h]
			if l == 0 {
				{
					if trieopptr == trieopsize {
						overflow(949, trieopsize)
					}
					u = trieused[curlang]
					if u == 255 {
						overflow(950, 255)
					}
					trieopptr = (trieopptr + 1)
					u = (u + 1)
					trieused[curlang] = u
					hyfdistance[trieopptr] = d
					hyfnum[trieopptr] = n
					hyfnext[trieopptr] = v
					trieoplang[trieopptr] = curlang
					trieophash[h] = trieopptr
					trieopval[trieopptr] = u
					newtrieop = u
					goto L10
				}
			}
			if (((hyfdistance[l] == d) && (hyfnum[l] == n)) && (hyfnext[l] == v)) && (trieoplang[l] == curlang) {
				{
					newtrieop = trieopval[l]
					goto L10
				}
			}
			if h > (-trieopsize) {
				h = (h - 1)
			} else {
				h = trieopsize
			}
		}
	}
L10:
	// empty
}

/* function: trienode */
func trienode(p *triepointer_t) *triepointer_t {
	var (
		h *triepointer_t
		q *triepointer_t
	)
	h = (abs_((((triec[p] + (1009 * trieo[p])) + (2718 * triel[p])) + (3142 * trier[p]))) % triesize)
	for true {
		{
			q = triehash[h]
			if q == 0 {
				{
					triehash[h] = p
					trienode = p
					goto L10
				}
			}
			if (((triec[q] == triec[p]) && (trieo[q] == trieo[p])) && (triel[q] == triel[p])) && (trier[q] == trier[p]) {
				{
					trienode = q
					goto L10
				}
			}
			if h > 0 {
				h = (h - 1)
			} else {
				h = triesize
			}
		}
	}
L10:
	// empty
}

/* function: compresstrie */
func compresstrie(p *triepointer_t) *triepointer_t {
	if p == 0 {
		compresstrie = 0
	} else {
		{
			triel[p] = compresstrie(triel[p])
			trier[p] = compresstrie(trier[p])
			compresstrie = trienode(p)
		}
	}
}

/* procedure: firstfit */
func firstfit(p *triepointer_t) {
	var (
		h  *triepointer_t
		z  *triepointer_t
		q  *triepointer_t
		c  byte
		l  *triepointer_t
		r  *triepointer_t
		ll int
	)
	c = triec[p]
	z = triemin[c]
	for true {
		{
			h = (z - c)
			if triemax < (h + 256) {
				{
					if triesize <= (h + 256) {
						overflow(951, triesize)
					}
					for {
						triemax = (triemax + 1)
						trietaken[triemax] = false
						trie[triemax].rh = (triemax + 1)
						trie[triemax].lh = (triemax - 1)
						if !(triemax == (h + 256)) {
							break
						}
					}
				}
			}
			if trietaken[h] {
				goto L45
			}
			q = trier[p]
			for q > 0 {
				{
					if trie[(h+triec[q])].rh == 0 {
						goto L45
					}
					q = trier[q]
				}
			}
			goto L40
		L45:
			z = trie[z].rh
		}
	}
L40:
	trietaken[h] = true
	triehash[p] = h
	q = p
	for {
		z = (h + triec[q])
		l = trie[z].lh
		r = trie[z].rh
		trie[r].lh = l
		trie[l].rh = r
		trie[z].rh = 0
		if l < 256 {
			{
				if z < 256 {
					ll = z
				} else {
					ll = 256
				}
				for {
					triemin[l] = r
					l = (l + 1)
					if !(l == ll) {
						break
					}
				}
			}
		}
		q = trier[q]
		if !(q == 0) {
			break
		}
	}
}

/* procedure: triepack */
func triepack(p *triepointer_t) {
	var (
		q *triepointer_t
	)
	for {
		q = triel[p]
		if (q > 0) && (triehash[q] == 0) {
			{
				firstfit(q)
				triepack(q)
			}
		}
		p = trier[p]
		if !(p == 0) {
			break
		}
	}
}

/* procedure: triefix */
func triefix(p *triepointer_t) {
	var (
		q *triepointer_t
		c byte
		z *triepointer_t
	)
	z = triehash[p]
	for {
		q = triel[p]
		c = triec[p]
		trie[(z + c)].rh = triehash[q]
		trie[(z + c)].b1 = (c + 0)
		trie[(z + c)].b0 = trieo[p]
		if q > 0 {
			triefix(q)
		}
		p = trier[p]
		if !(p == 0) {
			break
		}
	}
}

/* procedure: newpatterns */
func newpatterns() {
	var (
		k           int
		l           int
		digitsensed bool
		v           int
		p           *triepointer_t
		q           *triepointer_t
		firstchild  bool
		c           byte
	)
	if trienotready {
		{
			if eqtb[5313].int <= 0 {
				curlang = 0
			} else {
				if eqtb[5313].int > 255 {
					curlang = 0
				} else {
					curlang = eqtb[5313].int
				}
			}
			scanleftbrace()
			k = 0
			hyf[0] = 0
			digitsensed = false
			for true {
				{
					getxtoken()
					switch curcmd {
					case 11:
						if (digitsensed || (curchr < 48)) || (curchr > 57) {
							{
								if curchr == 46 {
									curchr = 0
								} else {
									{
										curchr = eqtb[(4239 + curchr)].hh.rh
										if curchr == 0 {
											{
												{
													if interaction == 3 {
														// empty
													}
													printnl(262)
													print_(957)
												}
												{
													helpptr = 1
													helpline[0] = 956
												}
												error_()
											}
										}
									}
								}
								if k < 63 {
									{
										k = (k + 1)
										hc[k] = curchr
										hyf[k] = 0
										digitsensed = false
									}
								}
							}
						} else {
							if k < 63 {
								{
									hyf[k] = (curchr - 48)
									digitsensed = true
								}
							}
						}
					case 12:
						if (digitsensed || (curchr < 48)) || (curchr > 57) {
							{
								if curchr == 46 {
									curchr = 0
								} else {
									{
										curchr = eqtb[(4239 + curchr)].hh.rh
										if curchr == 0 {
											{
												{
													if interaction == 3 {
														// empty
													}
													printnl(262)
													print_(957)
												}
												{
													helpptr = 1
													helpline[0] = 956
												}
												error_()
											}
										}
									}
								}
								if k < 63 {
									{
										k = (k + 1)
										hc[k] = curchr
										hyf[k] = 0
										digitsensed = false
									}
								}
							}
						} else {
							if k < 63 {
								{
									hyf[k] = (curchr - 48)
									digitsensed = true
								}
							}
						}
					case 10:
						{
							if k > 0 {
								{
									if hc[1] == 0 {
										hyf[0] = 0
									}
									if hc[k] == 0 {
										hyf[k] = 0
									}
									l = k
									v = 0
									for true {
										{
											if hyf[l] != 0 {
												v = newtrieop((k - l), hyf[l], v)
											}
											if l > 0 {
												l = (l - 1)
											} else {
												goto L31
											}
										}
									}
								L31:
									// empty
									q = 0
									hc[0] = curlang
									for l <= k {
										{
											c = hc[l]
											l = (l + 1)
											p = triel[q]
											firstchild = true
											for (p > 0) && (c > triec[p]) {
												{
													q = p
													p = trier[q]
													firstchild = false
												}
											}
											if (p == 0) || (c < triec[p]) {
												{
													if trieptr == triesize {
														overflow(951, triesize)
													}
													trieptr = (trieptr + 1)
													trier[trieptr] = p
													p = trieptr
													triel[p] = 0
													if firstchild {
														triel[q] = p
													} else {
														trier[q] = p
													}
													triec[p] = c
													trieo[p] = 0
												}
											}
											q = p
										}
									}
									if trieo[q] != 0 {
										{
											{
												if interaction == 3 {
													// empty
												}
												printnl(262)
												print_(958)
											}
											{
												helpptr = 1
												helpline[0] = 956
											}
											error_()
										}
									}
									trieo[q] = v
								}
							}
							if curcmd == 2 {
								goto L30
							}
							k = 0
							hyf[0] = 0
							digitsensed = false
						}
					case 2:
						{
							if k > 0 {
								{
									if hc[1] == 0 {
										hyf[0] = 0
									}
									if hc[k] == 0 {
										hyf[k] = 0
									}
									l = k
									v = 0
									for true {
										{
											if hyf[l] != 0 {
												v = newtrieop((k - l), hyf[l], v)
											}
											if l > 0 {
												l = (l - 1)
											} else {
												goto L31
											}
										}
									}
								L31:
									// empty
									q = 0
									hc[0] = curlang
									for l <= k {
										{
											c = hc[l]
											l = (l + 1)
											p = triel[q]
											firstchild = true
											for (p > 0) && (c > triec[p]) {
												{
													q = p
													p = trier[q]
													firstchild = false
												}
											}
											if (p == 0) || (c < triec[p]) {
												{
													if trieptr == triesize {
														overflow(951, triesize)
													}
													trieptr = (trieptr + 1)
													trier[trieptr] = p
													p = trieptr
													triel[p] = 0
													if firstchild {
														triel[q] = p
													} else {
														trier[q] = p
													}
													triec[p] = c
													trieo[p] = 0
												}
											}
											q = p
										}
									}
									if trieo[q] != 0 {
										{
											{
												if interaction == 3 {
													// empty
												}
												printnl(262)
												print_(958)
											}
											{
												helpptr = 1
												helpline[0] = 956
											}
											error_()
										}
									}
									trieo[q] = v
								}
							}
							if curcmd == 2 {
								goto L30
							}
							k = 0
							hyf[0] = 0
							digitsensed = false
						}
					default:
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(955)
							}
							printesc(953)
							{
								helpptr = 1
								helpline[0] = 956
							}
							error_()
						}
					}
				}
			}
		L30:
			// empty
		}
	} else {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(952)
			}
			printesc(953)
			{
				helpptr = 1
				helpline[0] = 954
			}
			error_()
			mem[29988].hh.rh = scantoks(false, false)
			flushlist(defref)
		}
	}
}

/* procedure: inittrie */
func inittrie() {
	var (
		p *triepointer_t
		j int
		k int
		t int
		r *triepointer_t
		s *triepointer_t
		h *twohalves_t
	)
	opstart[0] = (-0)
	for j := 1; j <= 255; j++ {
		opstart[j] = ((opstart[(j-1)] + trieused[(j-1)]) - 0)
	}
	for j := 1; j <= trieopptr; j++ {
		trieophash[j] = (opstart[trieoplang[j]] + trieopval[j])
	}
	for j := 1; j <= trieopptr; j++ {
		for trieophash[j] > j {
			{
				k = trieophash[j]
				t = hyfdistance[k]
				hyfdistance[k] = hyfdistance[j]
				hyfdistance[j] = t
				t = hyfnum[k]
				hyfnum[k] = hyfnum[j]
				hyfnum[j] = t
				t = hyfnext[k]
				hyfnext[k] = hyfnext[j]
				hyfnext[j] = t
				trieophash[j] = trieophash[k]
				trieophash[k] = k
			}
		}
	}
	for p := 0; p <= triesize; p++ {
		triehash[p] = 0
	}
	triel[0] = compresstrie(triel[0])
	for p := 0; p <= trieptr; p++ {
		triehash[p] = 0
	}
	for p := 0; p <= 255; p++ {
		triemin[p] = (p + 1)
	}
	trie[0].rh = 1
	triemax = 0
	if triel[0] != 0 {
		{
			firstfit(triel[0])
			triepack(triel[0])
		}
	}
	h.rh = 0
	h.b0 = 0
	h.b1 = 0
	if triel[0] == 0 {
		{
			for r := 0; r <= 256; r++ {
				trie[r] = h
			}
			triemax = 256
		}
	} else {
		{
			triefix(triel[0])
			r = 0
			for {
				s = trie[r].rh
				trie[r] = h
				r = s
				if !(r > triemax) {
					break
				}
			}
		}
	}
	trie[0].b1 = 63
	trienotready = false
}

/* procedure: linebreak */
func linebreak(finalwidowpenalty int) {
	var (
		autobreaking bool
		prevp        int
		q            int
		r            int
		s            int
		prevs        int
		f            int
		j            int
		c            int
	)
	packbeginline = curlist.mlfield
	mem[29997].hh.rh = mem[curlist.headfield].hh.rh
	if curlist.tailfield >= himemmin {
		{
			mem[curlist.tailfield].hh.rh = newpenalty(10000)
			curlist.tailfield = mem[curlist.tailfield].hh.rh
		}
	} else {
		if mem[curlist.tailfield].hh.b0 != 10 {
			{
				mem[curlist.tailfield].hh.rh = newpenalty(10000)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
		} else {
			{
				mem[curlist.tailfield].hh.b0 = 12
				deleteglueref(mem[(curlist.tailfield + 1)].hh.lh)
				flushnodelist(mem[(curlist.tailfield + 1)].hh.rh)
				mem[(curlist.tailfield + 1)].int = 10000
			}
		}
	}
	mem[curlist.tailfield].hh.rh = newparamglue(14)
	initcurlang = (curlist.pgfield % 65536)
	initlhyf = (curlist.pgfield / 4194304)
	initrhyf = ((curlist.pgfield / 65536) % 64)
	popnest()
	noshrinkerroryet = true
	if (mem[eqtb[2889].hh.rh].hh.b1 != 0) && (mem[(eqtb[2889].hh.rh+3)].int != 0) {
		{
			eqtb[2889].hh.rh = finiteshrink(eqtb[2889].hh.rh)
		}
	}
	if (mem[eqtb[2890].hh.rh].hh.b1 != 0) && (mem[(eqtb[2890].hh.rh+3)].int != 0) {
		{
			eqtb[2890].hh.rh = finiteshrink(eqtb[2890].hh.rh)
		}
	}
	q = eqtb[2889].hh.rh
	r = eqtb[2890].hh.rh
	background[1] = (mem[(q+1)].int + mem[(r+1)].int)
	background[2] = 0
	background[3] = 0
	background[4] = 0
	background[5] = 0
	background[(2 + mem[q].hh.b0)] = mem[(q + 2)].int
	background[(2 + mem[r].hh.b0)] = (background[(2+mem[r].hh.b0)] + mem[(r+2)].int)
	background[6] = (mem[(q+3)].int + mem[(r+3)].int)
	minimumdemerits = 1073741823
	minimaldemerits[3] = 1073741823
	minimaldemerits[2] = 1073741823
	minimaldemerits[1] = 1073741823
	minimaldemerits[0] = 1073741823
	if eqtb[3412].hh.rh == 0 {
		if eqtb[5847].int == 0 {
			{
				lastspecialline = 0
				secondwidth = eqtb[5833].int
				secondindent = 0
			}
		} else {
			{
				lastspecialline = abs_(eqtb[5304].int)
				if eqtb[5304].int < 0 {
					{
						firstwidth = (eqtb[5833].int - abs_(eqtb[5847].int))
						if eqtb[5847].int >= 0 {
							firstindent = eqtb[5847].int
						} else {
							firstindent = 0
						}
						secondwidth = eqtb[5833].int
						secondindent = 0
					}
				} else {
					{
						firstwidth = eqtb[5833].int
						firstindent = 0
						secondwidth = (eqtb[5833].int - abs_(eqtb[5847].int))
						if eqtb[5847].int >= 0 {
							secondindent = eqtb[5847].int
						} else {
							secondindent = 0
						}
					}
				}
			}
		}
	} else {
		{
			lastspecialline = (mem[eqtb[3412].hh.rh].hh.lh - 1)
			secondwidth = mem[(eqtb[3412].hh.rh + (2 * (lastspecialline + 1)))].int
			secondindent = mem[((eqtb[3412].hh.rh + (2 * lastspecialline)) + 1)].int
		}
	}
	if eqtb[5282].int == 0 {
		easyline = lastspecialline
	} else {
		easyline = 65535
	}
	threshold = eqtb[5263].int
	if threshold >= 0 {
		{
			secondpass = false
			finalpass = false
		}
	} else {
		{
			threshold = eqtb[5264].int
			secondpass = true
			finalpass = (eqtb[5850].int <= 0)
		}
	}
	for true {
		{
			if threshold > 10000 {
				threshold = 10000
			}
			if secondpass {
				{
					if trienotready {
						inittrie()
					}
					curlang = initcurlang
					lhyf = initlhyf
					rhyf = initrhyf
				}
			}
			q = getnode(3)
			mem[q].hh.b0 = 0
			mem[q].hh.b1 = 2
			mem[q].hh.rh = 29993
			mem[(q + 1)].hh.rh = 0
			mem[(q + 1)].hh.lh = (curlist.pgfield + 1)
			mem[(q + 2)].int = 0
			mem[29993].hh.rh = q
			activewidth[1] = background[1]
			activewidth[2] = background[2]
			activewidth[3] = background[3]
			activewidth[4] = background[4]
			activewidth[5] = background[5]
			activewidth[6] = background[6]
			passive = 0
			printednode = 29997
			passnumber = 0
			fontinshortdisplay = 0
			curp = mem[29997].hh.rh
			autobreaking = true
			prevp = curp
			for (curp != 0) && (mem[29993].hh.rh != 29993) {
				{
					if curp >= himemmin {
						{
							prevp = curp
							for {
								f = mem[curp].hh.b0
								activewidth[1] = (activewidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[curp].hh.b1)].qqqq.b0)].int)
								curp = mem[curp].hh.rh
								if !(!(curp >= himemmin)) {
									break
								}
							}
						}
					}
					switch mem[curp].hh.b0 {
					case 0:
						activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
					case 1:
						activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
					case 2:
						activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
					case 8:
						if mem[curp].hh.b1 == 4 {
							{
								curlang = mem[(curp + 1)].hh.rh
								lhyf = mem[(curp + 1)].hh.b0
								rhyf = mem[(curp + 1)].hh.b1
							}
						}
					case 10:
						{
							if autobreaking {
								{
									if prevp >= himemmin {
										trybreak(0, 0)
									} else {
										if mem[prevp].hh.b0 < 9 {
											trybreak(0, 0)
										} else {
											if (mem[prevp].hh.b0 == 11) && (mem[prevp].hh.b1 != 1) {
												trybreak(0, 0)
											}
										}
									}
								}
							}
							if (mem[mem[(curp+1)].hh.lh].hh.b1 != 0) && (mem[(mem[(curp+1)].hh.lh+3)].int != 0) {
								{
									mem[(curp + 1)].hh.lh = finiteshrink(mem[(curp + 1)].hh.lh)
								}
							}
							q = mem[(curp + 1)].hh.lh
							activewidth[1] = (activewidth[1] + mem[(q+1)].int)
							activewidth[(2 + mem[q].hh.b0)] = (activewidth[(2+mem[q].hh.b0)] + mem[(q+2)].int)
							activewidth[6] = (activewidth[6] + mem[(q+3)].int)
							if secondpass && autobreaking {
								{
									prevs = curp
									s = mem[prevs].hh.rh
									if s != 0 {
										{
											for true {
												{
													if s >= himemmin {
														{
															c = (mem[s].hh.b1 - 0)
															hf = mem[s].hh.b0
														}
													} else {
														if mem[s].hh.b0 == 6 {
															if mem[(s+1)].hh.rh == 0 {
																goto L22
															} else {
																{
																	q = mem[(s + 1)].hh.rh
																	c = (mem[q].hh.b1 - 0)
																	hf = mem[q].hh.b0
																}
															}
														} else {
															if (mem[s].hh.b0 == 11) && (mem[s].hh.b1 == 0) {
																goto L22
															} else {
																if mem[s].hh.b0 == 8 {
																	{
																		if mem[s].hh.b1 == 4 {
																			{
																				curlang = mem[(s + 1)].hh.rh
																				lhyf = mem[(s + 1)].hh.b0
																				rhyf = mem[(s + 1)].hh.b1
																			}
																		}
																		goto L22
																	}
																} else {
																	goto L31
																}
															}
														}
													}
													if eqtb[(4239+c)].hh.rh != 0 {
														if (eqtb[(4239+c)].hh.rh == c) || (eqtb[5301].int > 0) {
															goto L32
														} else {
															goto L31
														}
													}
												L22:
													prevs = s
													s = mem[prevs].hh.rh
												}
											}
										L32:
											hyfchar = hyphenchar[hf]
											if hyfchar < 0 {
												goto L31
											}
											if hyfchar > 255 {
												goto L31
											}
											ha = prevs
											if (lhyf + rhyf) > 63 {
												goto L31
											}
											hn = 0
											for true {
												{
													if s >= himemmin {
														{
															if mem[s].hh.b0 != hf {
																goto L33
															}
															hyfbchar = mem[s].hh.b1
															c = (hyfbchar - 0)
															if eqtb[(4239+c)].hh.rh == 0 {
																goto L33
															}
															if hn == 63 {
																goto L33
															}
															hb = s
															hn = (hn + 1)
															hu[hn] = c
															hc[hn] = eqtb[(4239 + c)].hh.rh
															hyfbchar = 256
														}
													} else {
														if mem[s].hh.b0 == 6 {
															{
																if mem[(s+1)].hh.b0 != hf {
																	goto L33
																}
																j = hn
																q = mem[(s + 1)].hh.rh
																if q > 0 {
																	hyfbchar = mem[q].hh.b1
																}
																for q > 0 {
																	{
																		c = (mem[q].hh.b1 - 0)
																		if eqtb[(4239+c)].hh.rh == 0 {
																			goto L33
																		}
																		if j == 63 {
																			goto L33
																		}
																		j = (j + 1)
																		hu[j] = c
																		hc[j] = eqtb[(4239 + c)].hh.rh
																		q = mem[q].hh.rh
																	}
																}
																hb = s
																hn = j
																if (mem[s].hh.b1 & 1) != 0 {
																	hyfbchar = fontbchar[hf]
																} else {
																	hyfbchar = 256
																}
															}
														} else {
															if (mem[s].hh.b0 == 11) && (mem[s].hh.b1 == 0) {
																{
																	hb = s
																	hyfbchar = fontbchar[hf]
																}
															} else {
																goto L33
															}
														}
													}
													s = mem[s].hh.rh
												}
											}
										L33:
											// empty
											if hn < (lhyf + rhyf) {
												goto L31
											}
											for true {
												{
													if !(s >= himemmin) {
														switch mem[s].hh.b0 {
														case 6:
															// empty
														case 11:
															if mem[s].hh.b1 != 0 {
																goto L34
															}
														case 8:
															goto L34
														case 10:
															goto L34
														case 12:
															goto L34
														case 3:
															goto L34
														case 5:
															goto L34
														case 4:
															goto L34
														default:
															goto L31
														}
													}
													s = mem[s].hh.rh
												}
											}
										L34:
											// empty
											hyphenate()
										}
									}
								L31:
									// empty
								}
							}
						}
					case 11:
						if mem[curp].hh.b1 == 1 {
							{
								if (!(mem[curp].hh.rh >= himemmin)) && autobreaking {
									if mem[mem[curp].hh.rh].hh.b0 == 10 {
										trybreak(0, 0)
									}
								}
								activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
							}
						} else {
							activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
						}
					case 6:
						{
							f = mem[(curp + 1)].hh.b0
							activewidth[1] = (activewidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[(curp+1)].hh.b1)].qqqq.b0)].int)
						}
					case 7:
						{
							s = mem[(curp + 1)].hh.lh
							discwidth = 0
							if s == 0 {
								trybreak(eqtb[5267].int, 1)
							} else {
								{
									for {
										if s >= himemmin {
											{
												f = mem[s].hh.b0
												discwidth = (discwidth + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[s].hh.b1)].qqqq.b0)].int)
											}
										} else {
											switch mem[s].hh.b0 {
											case 6:
												{
													f = mem[(s + 1)].hh.b0
													discwidth = (discwidth + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[(s+1)].hh.b1)].qqqq.b0)].int)
												}
											case 0:
												discwidth = (discwidth + mem[(s+1)].int)
											case 1:
												discwidth = (discwidth + mem[(s+1)].int)
											case 2:
												discwidth = (discwidth + mem[(s+1)].int)
											case 11:
												discwidth = (discwidth + mem[(s+1)].int)
											default:
												confusion(937)
											}
										}
										s = mem[s].hh.rh
										if !(s == 0) {
											break
										}
									}
									activewidth[1] = (activewidth[1] + discwidth)
									trybreak(eqtb[5266].int, 1)
									activewidth[1] = (activewidth[1] - discwidth)
								}
							}
							r = mem[curp].hh.b1
							s = mem[curp].hh.rh
							for r > 0 {
								{
									if s >= himemmin {
										{
											f = mem[s].hh.b0
											activewidth[1] = (activewidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[s].hh.b1)].qqqq.b0)].int)
										}
									} else {
										switch mem[s].hh.b0 {
										case 6:
											{
												f = mem[(s + 1)].hh.b0
												activewidth[1] = (activewidth[1] + fontinfo[(widthbase[f]+fontinfo[(charbase[f]+mem[(s+1)].hh.b1)].qqqq.b0)].int)
											}
										case 0:
											activewidth[1] = (activewidth[1] + mem[(s+1)].int)
										case 1:
											activewidth[1] = (activewidth[1] + mem[(s+1)].int)
										case 2:
											activewidth[1] = (activewidth[1] + mem[(s+1)].int)
										case 11:
											activewidth[1] = (activewidth[1] + mem[(s+1)].int)
										default:
											confusion(938)
										}
									}
									r = (r - 1)
									s = mem[s].hh.rh
								}
							}
							prevp = curp
							curp = s
							goto L35
						}
					case 9:
						{
							autobreaking = (mem[curp].hh.b1 == 1)
							{
								if (!(mem[curp].hh.rh >= himemmin)) && autobreaking {
									if mem[mem[curp].hh.rh].hh.b0 == 10 {
										trybreak(0, 0)
									}
								}
								activewidth[1] = (activewidth[1] + mem[(curp+1)].int)
							}
						}
					case 12:
						trybreak(mem[(curp+1)].int, 0)
					case 4:
						// empty
					case 3:
						// empty
					case 5:
						// empty
					default:
						confusion(936)
					}
					prevp = curp
					curp = mem[curp].hh.rh
				L35:
					// empty
				}
			}
			if curp == 0 {
				{
					trybreak((-10000), 1)
					if mem[29993].hh.rh != 29993 {
						{
							r = mem[29993].hh.rh
							fewestdemerits = 1073741823
							for {
								if mem[r].hh.b0 != 2 {
									if mem[(r+2)].int < fewestdemerits {
										{
											fewestdemerits = mem[(r + 2)].int
											bestbet = r
										}
									}
								}
								r = mem[r].hh.rh
								if !(r == 29993) {
									break
								}
							}
							bestline = mem[(bestbet + 1)].hh.lh
							if eqtb[5282].int == 0 {
								goto L30
							}
							{
								r = mem[29993].hh.rh
								actuallooseness = 0
								for {
									if mem[r].hh.b0 != 2 {
										{
											linediff = (mem[(r+1)].hh.lh - bestline)
											if ((linediff < actuallooseness) && (eqtb[5282].int <= linediff)) || ((linediff > actuallooseness) && (eqtb[5282].int >= linediff)) {
												{
													bestbet = r
													actuallooseness = linediff
													fewestdemerits = mem[(r + 2)].int
												}
											} else {
												if (linediff == actuallooseness) && (mem[(r+2)].int < fewestdemerits) {
													{
														bestbet = r
														fewestdemerits = mem[(r + 2)].int
													}
												}
											}
										}
									}
									r = mem[r].hh.rh
									if !(r == 29993) {
										break
									}
								}
								bestline = mem[(bestbet + 1)].hh.lh
							}
							if (actuallooseness == eqtb[5282].int) || finalpass {
								goto L30
							}
						}
					}
				}
			}
			q = mem[29993].hh.rh
			for q != 29993 {
				{
					curp = mem[q].hh.rh
					if mem[q].hh.b0 == 2 {
						freenode(q, 7)
					} else {
						freenode(q, 3)
					}
					q = curp
				}
			}
			q = passive
			for q != 0 {
				{
					curp = mem[q].hh.rh
					freenode(q, 2)
					q = curp
				}
			}
			if !secondpass {
				{
					threshold = eqtb[5264].int
					secondpass = true
					finalpass = (eqtb[5850].int <= 0)
				}
			} else {
				{
					background[2] = (background[2] + eqtb[5850].int)
					finalpass = true
				}
			}
		}
	}
L30:
	// empty
	postlinebreak(finalwidowpenalty)
	q = mem[29993].hh.rh
	for q != 29993 {
		{
			curp = mem[q].hh.rh
			if mem[q].hh.b0 == 2 {
				freenode(q, 7)
			} else {
				freenode(q, 3)
			}
			q = curp
		}
	}
	q = passive
	for q != 0 {
		{
			curp = mem[q].hh.rh
			freenode(q, 2)
			q = curp
		}
	}
	packbeginline = 0
}

/* procedure: newhyphexceptions */
func newhyphexceptions() {
	var (
		n int
		j int
		h int
		k int
		p int
		q int
		s int
		t int
		u int
		v int
	)
	scanleftbrace()
	if eqtb[5313].int <= 0 {
		curlang = 0
	} else {
		if eqtb[5313].int > 255 {
			curlang = 0
		} else {
			curlang = eqtb[5313].int
		}
	}
	n = 0
	p = 0
	for true {
		{
			getxtoken()
		L21:
			switch curcmd {
			case 11:
				if curchr == 45 {
					{
						if n < 63 {
							{
								q = getavail
								mem[q].hh.rh = p
								mem[q].hh.lh = n
								p = q
							}
						}
					}
				} else {
					{
						if eqtb[(4239+curchr)].hh.rh == 0 {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(945)
								}
								{
									helpptr = 2
									helpline[1] = 946
									helpline[0] = 947
								}
								error_()
							}
						} else {
							if n < 63 {
								{
									n = (n + 1)
									hc[n] = eqtb[(4239 + curchr)].hh.rh
								}
							}
						}
					}
				}
			case 12:
				if curchr == 45 {
					{
						if n < 63 {
							{
								q = getavail
								mem[q].hh.rh = p
								mem[q].hh.lh = n
								p = q
							}
						}
					}
				} else {
					{
						if eqtb[(4239+curchr)].hh.rh == 0 {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(945)
								}
								{
									helpptr = 2
									helpline[1] = 946
									helpline[0] = 947
								}
								error_()
							}
						} else {
							if n < 63 {
								{
									n = (n + 1)
									hc[n] = eqtb[(4239 + curchr)].hh.rh
								}
							}
						}
					}
				}
			case 68:
				if curchr == 45 {
					{
						if n < 63 {
							{
								q = getavail
								mem[q].hh.rh = p
								mem[q].hh.lh = n
								p = q
							}
						}
					}
				} else {
					{
						if eqtb[(4239+curchr)].hh.rh == 0 {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(945)
								}
								{
									helpptr = 2
									helpline[1] = 946
									helpline[0] = 947
								}
								error_()
							}
						} else {
							if n < 63 {
								{
									n = (n + 1)
									hc[n] = eqtb[(4239 + curchr)].hh.rh
								}
							}
						}
					}
				}
			case 16:
				{
					scancharnum()
					curchr = curval
					curcmd = 68
					goto L21
				}
			case 10:
				{
					if n > 1 {
						{
							n = (n + 1)
							hc[n] = curlang
							{
								if (poolptr + n) > poolsize {
									overflow(257, (poolsize - initpoolptr))
								}
							}
							h = 0
							for j := 1; j <= n; j++ {
								{
									h = (((h + h) + hc[j]) % 307)
									{
										strpool[poolptr] = hc[j]
										poolptr = (poolptr + 1)
									}
								}
							}
							s = makestring
							if hyphcount == 307 {
								overflow(948, 307)
							}
							hyphcount = (hyphcount + 1)
							for hyphword[h] != 0 {
								{
									k = hyphword[h]
									if (strstart[(k+1)] - strstart[k]) < (strstart[(s+1)] - strstart[s]) {
										goto L40
									}
									if (strstart[(k+1)] - strstart[k]) > (strstart[(s+1)] - strstart[s]) {
										goto L45
									}
									u = strstart[k]
									v = strstart[s]
									for {
										if strpool[u] < strpool[v] {
											goto L40
										}
										if strpool[u] > strpool[v] {
											goto L45
										}
										u = (u + 1)
										v = (v + 1)
										if !(u == strstart[(k+1)]) {
											break
										}
									}
								L40:
									q = hyphlist[h]
									hyphlist[h] = p
									p = q
									t = hyphword[h]
									hyphword[h] = s
									s = t
								L45:
									// empty
									if h > 0 {
										h = (h - 1)
									} else {
										h = 307
									}
								}
							}
							hyphword[h] = s
							hyphlist[h] = p
						}
					}
					if curcmd == 2 {
						goto L10
					}
					n = 0
					p = 0
				}
			case 2:
				{
					if n > 1 {
						{
							n = (n + 1)
							hc[n] = curlang
							{
								if (poolptr + n) > poolsize {
									overflow(257, (poolsize - initpoolptr))
								}
							}
							h = 0
							for j := 1; j <= n; j++ {
								{
									h = (((h + h) + hc[j]) % 307)
									{
										strpool[poolptr] = hc[j]
										poolptr = (poolptr + 1)
									}
								}
							}
							s = makestring
							if hyphcount == 307 {
								overflow(948, 307)
							}
							hyphcount = (hyphcount + 1)
							for hyphword[h] != 0 {
								{
									k = hyphword[h]
									if (strstart[(k+1)] - strstart[k]) < (strstart[(s+1)] - strstart[s]) {
										goto L40
									}
									if (strstart[(k+1)] - strstart[k]) > (strstart[(s+1)] - strstart[s]) {
										goto L45
									}
									u = strstart[k]
									v = strstart[s]
									for {
										if strpool[u] < strpool[v] {
											goto L40
										}
										if strpool[u] > strpool[v] {
											goto L45
										}
										u = (u + 1)
										v = (v + 1)
										if !(u == strstart[(k+1)]) {
											break
										}
									}
								L40:
									q = hyphlist[h]
									hyphlist[h] = p
									p = q
									t = hyphword[h]
									hyphword[h] = s
									s = t
								L45:
									// empty
									if h > 0 {
										h = (h - 1)
									} else {
										h = 307
									}
								}
							}
							hyphword[h] = s
							hyphlist[h] = p
						}
					}
					if curcmd == 2 {
						goto L10
					}
					n = 0
					p = 0
				}
			default:
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(680)
					}
					printesc(941)
					print_(942)
					{
						helpptr = 2
						helpline[1] = 943
						helpline[0] = 944
					}
					error_()
				}
			}
		}
	}
L10:
	// empty
}

/* function: prunepagetop */
func prunepagetop(p int) int {
	var (
		prevp int
		q     int
	)
	prevp = 29997
	mem[29997].hh.rh = p
	for p != 0 {
		switch mem[p].hh.b0 {
		case 0:
			{
				q = newskipparam(10)
				mem[prevp].hh.rh = q
				mem[q].hh.rh = p
				if mem[(tempptr+1)].int > mem[(p+3)].int {
					mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
				} else {
					mem[(tempptr + 1)].int = 0
				}
				p = 0
			}
		case 1:
			{
				q = newskipparam(10)
				mem[prevp].hh.rh = q
				mem[q].hh.rh = p
				if mem[(tempptr+1)].int > mem[(p+3)].int {
					mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
				} else {
					mem[(tempptr + 1)].int = 0
				}
				p = 0
			}
		case 2:
			{
				q = newskipparam(10)
				mem[prevp].hh.rh = q
				mem[q].hh.rh = p
				if mem[(tempptr+1)].int > mem[(p+3)].int {
					mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
				} else {
					mem[(tempptr + 1)].int = 0
				}
				p = 0
			}
		case 8:
			{
				prevp = p
				p = mem[prevp].hh.rh
			}
		case 4:
			{
				prevp = p
				p = mem[prevp].hh.rh
			}
		case 3:
			{
				prevp = p
				p = mem[prevp].hh.rh
			}
		case 10:
			{
				q = p
				p = mem[q].hh.rh
				mem[q].hh.rh = 0
				mem[prevp].hh.rh = p
				flushnodelist(q)
			}
		case 11:
			{
				q = p
				p = mem[q].hh.rh
				mem[q].hh.rh = 0
				mem[prevp].hh.rh = p
				flushnodelist(q)
			}
		case 12:
			{
				q = p
				p = mem[q].hh.rh
				mem[q].hh.rh = 0
				mem[prevp].hh.rh = p
				flushnodelist(q)
			}
		default:
			confusion(959)
		}
	}
	prunepagetop = mem[29997].hh.rh
}

/* function: vertbreak */
func vertbreak(p int, h int, d int) int {
	var (
		prevp     int
		q         int
		r         int
		pi        int
		b         int
		leastcost int
		bestplace int
		prevdp    int
		t         int
	)
	prevp = p
	leastcost = 1073741823
	activewidth[1] = 0
	activewidth[2] = 0
	activewidth[3] = 0
	activewidth[4] = 0
	activewidth[5] = 0
	activewidth[6] = 0
	prevdp = 0
	for true {
		{
			if p == 0 {
				pi = (-10000)
			} else {
				switch mem[p].hh.b0 {
				case 0:
					{
						activewidth[1] = ((activewidth[1] + prevdp) + mem[(p+3)].int)
						prevdp = mem[(p + 2)].int
						goto L45
					}
				case 1:
					{
						activewidth[1] = ((activewidth[1] + prevdp) + mem[(p+3)].int)
						prevdp = mem[(p + 2)].int
						goto L45
					}
				case 2:
					{
						activewidth[1] = ((activewidth[1] + prevdp) + mem[(p+3)].int)
						prevdp = mem[(p + 2)].int
						goto L45
					}
				case 8:
					goto L45
				case 10:
					if mem[prevp].hh.b0 < 9 {
						pi = 0
					} else {
						goto L90
					}
				case 11:
					{
						if mem[p].hh.rh == 0 {
							t = 12
						} else {
							t = mem[mem[p].hh.rh].hh.b0
						}
						if t == 10 {
							pi = 0
						} else {
							goto L90
						}
					}
				case 12:
					pi = mem[(p + 1)].int
				case 4:
					goto L45
				case 3:
					goto L45
				default:
					confusion(960)
				}
			}
			if pi < 10000 {
				{
					if activewidth[1] < h {
						if ((activewidth[3] != 0) || (activewidth[4] != 0)) || (activewidth[5] != 0) {
							b = 0
						} else {
							b = badness((h - activewidth[1]), activewidth[2])
						}
					} else {
						if (activewidth[1] - h) > activewidth[6] {
							b = 1073741823
						} else {
							b = badness((activewidth[1] - h), activewidth[6])
						}
					}
					if b < 1073741823 {
						if pi <= (-10000) {
							b = pi
						} else {
							if b < 10000 {
								b = (b + pi)
							} else {
								b = 100000
							}
						}
					}
					if b <= leastcost {
						{
							bestplace = p
							leastcost = b
							bestheightplusdepth = (activewidth[1] + prevdp)
						}
					}
					if (b == 1073741823) || (pi <= (-10000)) {
						goto L30
					}
				}
			}
			if (mem[p].hh.b0 < 10) || (mem[p].hh.b0 > 11) {
				goto L45
			}
		L90:
			if mem[p].hh.b0 == 11 {
				q = p
			} else {
				{
					q = mem[(p + 1)].hh.lh
					activewidth[(2 + mem[q].hh.b0)] = (activewidth[(2+mem[q].hh.b0)] + mem[(q+2)].int)
					activewidth[6] = (activewidth[6] + mem[(q+3)].int)
					if (mem[q].hh.b1 != 0) && (mem[(q+3)].int != 0) {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(961)
							}
							{
								helpptr = 4
								helpline[3] = 962
								helpline[2] = 963
								helpline[1] = 964
								helpline[0] = 922
							}
							error_()
							r = newspec(q)
							mem[r].hh.b1 = 0
							deleteglueref(q)
							mem[(p + 1)].hh.lh = r
							q = r
						}
					}
				}
			}
			activewidth[1] = ((activewidth[1] + prevdp) + mem[(q+1)].int)
			prevdp = 0
		L45:
			if prevdp > d {
				{
					activewidth[1] = ((activewidth[1] + prevdp) - d)
					prevdp = d
				}
			}
			prevp = p
			p = mem[prevp].hh.rh
		}
	}
L30:
	vertbreak = bestplace
}

/* function: vsplit */
func vsplit(n byte, h int) int {
	var (
		v int
		p int
		q int
	)
	v = eqtb[(3678 + n)].hh.rh
	if curmark[3] != 0 {
		{
			deletetokenref(curmark[3])
			curmark[3] = 0
			deletetokenref(curmark[4])
			curmark[4] = 0
		}
	}
	if v == 0 {
		{
			vsplit = 0
			goto L10
		}
	}
	if mem[v].hh.b0 != 1 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(338)
			}
			printesc(965)
			print_(966)
			printesc(967)
			{
				helpptr = 2
				helpline[1] = 968
				helpline[0] = 969
			}
			error_()
			vsplit = 0
			goto L10
		}
	}
	q = vertbreak(mem[(v+5)].hh.rh, h, eqtb[5836].int)
	p = mem[(v + 5)].hh.rh
	if p == q {
		mem[(v + 5)].hh.rh = 0
	} else {
		for true {
			{
				if mem[p].hh.b0 == 4 {
					if curmark[3] == 0 {
						{
							curmark[3] = mem[(p + 1)].int
							curmark[4] = curmark[3]
							mem[curmark[3]].hh.lh = (mem[curmark[3]].hh.lh + 2)
						}
					} else {
						{
							deletetokenref(curmark[4])
							curmark[4] = mem[(p + 1)].int
							mem[curmark[4]].hh.lh = (mem[curmark[4]].hh.lh + 1)
						}
					}
				}
				if mem[p].hh.rh == q {
					{
						mem[p].hh.rh = 0
						goto L30
					}
				}
				p = mem[p].hh.rh
			}
		}
	}
L30:
	// empty
	q = prunepagetop(q)
	p = mem[(v + 5)].hh.rh
	freenode(v, 7)
	if q == 0 {
		eqtb[(3678 + n)].hh.rh = 0
	} else {
		eqtb[(3678 + n)].hh.rh = vpackage(q, 0, 1, 1073741823)
	}
	vsplit = vpackage(p, h, 0, eqtb[5836].int)
L10:
	// empty
}

/* procedure: printtotals */
func printtotals() {
	printscaled(pagesofar[1])
	if pagesofar[2] != 0 {
		{
			print_(312)
			printscaled(pagesofar[2])
			print_(338)
		}
	}
	if pagesofar[3] != 0 {
		{
			print_(312)
			printscaled(pagesofar[3])
			print_(311)
		}
	}
	if pagesofar[4] != 0 {
		{
			print_(312)
			printscaled(pagesofar[4])
			print_(978)
		}
	}
	if pagesofar[5] != 0 {
		{
			print_(312)
			printscaled(pagesofar[5])
			print_(979)
		}
	}
	if pagesofar[6] != 0 {
		{
			print_(313)
			printscaled(pagesofar[6])
		}
	}
}

/* procedure: freezepagespecs */
func freezepagespecs(s int) {
	pagecontents = s
	pagesofar[0] = eqtb[5834].int
	pagemaxdepth = eqtb[5835].int
	pagesofar[7] = 0
	pagesofar[1] = 0
	pagesofar[2] = 0
	pagesofar[3] = 0
	pagesofar[4] = 0
	pagesofar[5] = 0
	pagesofar[6] = 0
	leastpagecost = 1073741823
}

/* procedure: boxerror */
func boxerror(n byte) {
	error_()
	begindiagnostic()
	printnl(836)
	showbox(eqtb[(3678 + n)].hh.rh)
	enddiagnostic(true)
	flushnodelist(eqtb[(3678 + n)].hh.rh)
	eqtb[(3678 + n)].hh.rh = 0
}

/* procedure: ensurevbox */
func ensurevbox(n byte) {
	var (
		p int
	)
	p = eqtb[(3678 + n)].hh.rh
	if p != 0 {
		if mem[p].hh.b0 == 0 {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(989)
				}
				{
					helpptr = 3
					helpline[2] = 990
					helpline[1] = 991
					helpline[0] = 992
				}
				boxerror(n)
			}
		}
	}
}

/* procedure: fireup */
func fireup(c int) {
	var (
		p                int
		q                int
		r                int
		s                int
		prevp            int
		n                int
		wait             bool
		savevbadness     int
		savevfuzz        int
		savesplittopskip int
	)
	if mem[bestpagebreak].hh.b0 == 12 {
		{
			geqworddefine(5302, mem[(bestpagebreak+1)].int)
			mem[(bestpagebreak + 1)].int = 10000
		}
	} else {
		geqworddefine(5302, 10000)
	}
	if curmark[2] != 0 {
		{
			if curmark[0] != 0 {
				deletetokenref(curmark[0])
			}
			curmark[0] = curmark[2]
			mem[curmark[0]].hh.lh = (mem[curmark[0]].hh.lh + 1)
			deletetokenref(curmark[1])
			curmark[1] = 0
		}
	}
	if c == bestpagebreak {
		bestpagebreak = 0
	}
	if eqtb[3933].hh.rh != 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(338)
			}
			printesc(409)
			print_(1003)
			{
				helpptr = 2
				helpline[1] = 1004
				helpline[0] = 992
			}
			boxerror(255)
		}
	}
	insertpenalties = 0
	savesplittopskip = eqtb[2892].hh.rh
	if eqtb[5316].int <= 0 {
		{
			r = mem[30000].hh.rh
			for r != 30000 {
				{
					if mem[(r+2)].hh.lh != 0 {
						{
							n = (mem[r].hh.b1 - 0)
							ensurevbox(n)
							if eqtb[(3678+n)].hh.rh == 0 {
								eqtb[(3678 + n)].hh.rh = newnullbox
							}
							p = (eqtb[(3678+n)].hh.rh + 5)
							for mem[p].hh.rh != 0 {
								p = mem[p].hh.rh
							}
							mem[(r + 2)].hh.rh = p
						}
					}
					r = mem[r].hh.rh
				}
			}
		}
	}
	q = 29996
	mem[q].hh.rh = 0
	prevp = 29998
	p = mem[prevp].hh.rh
	for p != bestpagebreak {
		{
			if mem[p].hh.b0 == 3 {
				{
					if eqtb[5316].int <= 0 {
						{
							r = mem[30000].hh.rh
							for mem[r].hh.b1 != mem[p].hh.b1 {
								r = mem[r].hh.rh
							}
							if mem[(r+2)].hh.lh == 0 {
								wait = true
							} else {
								{
									wait = false
									s = mem[(r + 2)].hh.rh
									mem[s].hh.rh = mem[(p + 4)].hh.lh
									if mem[(r+2)].hh.lh == p {
										{
											if mem[r].hh.b0 == 1 {
												if (mem[(r+1)].hh.lh == p) && (mem[(r+1)].hh.rh != 0) {
													{
														for mem[s].hh.rh != mem[(r+1)].hh.rh {
															s = mem[s].hh.rh
														}
														mem[s].hh.rh = 0
														eqtb[2892].hh.rh = mem[(p + 4)].hh.rh
														mem[(p + 4)].hh.lh = prunepagetop(mem[(r + 1)].hh.rh)
														if mem[(p+4)].hh.lh != 0 {
															{
																tempptr = vpackage(mem[(p+4)].hh.lh, 0, 1, 1073741823)
																mem[(p + 3)].int = (mem[(tempptr+3)].int + mem[(tempptr+2)].int)
																freenode(tempptr, 7)
																wait = true
															}
														}
													}
												}
											}
											mem[(r + 2)].hh.lh = 0
											n = (mem[r].hh.b1 - 0)
											tempptr = mem[(eqtb[(3678+n)].hh.rh + 5)].hh.rh
											freenode(eqtb[(3678+n)].hh.rh, 7)
											eqtb[(3678 + n)].hh.rh = vpackage(tempptr, 0, 1, 1073741823)
										}
									} else {
										{
											for mem[s].hh.rh != 0 {
												s = mem[s].hh.rh
											}
											mem[(r + 2)].hh.rh = s
										}
									}
								}
							}
							mem[prevp].hh.rh = mem[p].hh.rh
							mem[p].hh.rh = 0
							if wait {
								{
									mem[q].hh.rh = p
									q = p
									insertpenalties = (insertpenalties + 1)
								}
							} else {
								{
									deleteglueref(mem[(p + 4)].hh.rh)
									freenode(p, 5)
								}
							}
							p = prevp
						}
					}
				}
			} else {
				if mem[p].hh.b0 == 4 {
					{
						if curmark[1] == 0 {
							{
								curmark[1] = mem[(p + 1)].int
								mem[curmark[1]].hh.lh = (mem[curmark[1]].hh.lh + 1)
							}
						}
						if curmark[2] != 0 {
							deletetokenref(curmark[2])
						}
						curmark[2] = mem[(p + 1)].int
						mem[curmark[2]].hh.lh = (mem[curmark[2]].hh.lh + 1)
					}
				}
			}
			prevp = p
			p = mem[prevp].hh.rh
		}
	}
	eqtb[2892].hh.rh = savesplittopskip
	if p != 0 {
		{
			if mem[29999].hh.rh == 0 {
				if nestptr == 0 {
					curlist.tailfield = pagetail
				} else {
					nest[0].tailfield = pagetail
				}
			}
			mem[pagetail].hh.rh = mem[29999].hh.rh
			mem[29999].hh.rh = p
			mem[prevp].hh.rh = 0
		}
	}
	savevbadness = eqtb[5290].int
	eqtb[5290].int = 10000
	savevfuzz = eqtb[5839].int
	eqtb[5839].int = 1073741823
	eqtb[3933].hh.rh = vpackage(mem[29998].hh.rh, bestsize, 0, pagemaxdepth)
	eqtb[5290].int = savevbadness
	eqtb[5839].int = savevfuzz
	if lastglue != 65535 {
		deleteglueref(lastglue)
	}
	pagecontents = 0
	pagetail = 29998
	mem[29998].hh.rh = 0
	lastglue = 65535
	lastpenalty = 0
	lastkern = 0
	pagesofar[7] = 0
	pagemaxdepth = 0
	if q != 29996 {
		{
			mem[29998].hh.rh = mem[29996].hh.rh
			pagetail = q
		}
	}
	r = mem[30000].hh.rh
	for r != 30000 {
		{
			q = mem[r].hh.rh
			freenode(r, 4)
			r = q
		}
	}
	mem[30000].hh.rh = 30000
	if (curmark[0] != 0) && (curmark[1] == 0) {
		{
			curmark[1] = curmark[0]
			mem[curmark[0]].hh.lh = (mem[curmark[0]].hh.lh + 1)
		}
	}
	if eqtb[3413].hh.rh != 0 {
		if deadcycles >= eqtb[5303].int {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(1005)
				}
				printint(deadcycles)
				print_(1006)
				{
					helpptr = 3
					helpline[2] = 1007
					helpline[1] = 1008
					helpline[0] = 1009
				}
				error_()
			}
		} else {
			{
				outputactive = true
				deadcycles = (deadcycles + 1)
				pushnest()
				curlist.modefield = (-1)
				curlist.auxfield.int = (-65536000)
				curlist.mlfield = (-line)
				begintokenlist(eqtb[3413].hh.rh, 6)
				newsavelevel(8)
				normalparagraph()
				scanleftbrace()
				goto L10
			}
		}
	}
	{
		if mem[29998].hh.rh != 0 {
			{
				if mem[29999].hh.rh == 0 {
					if nestptr == 0 {
						curlist.tailfield = pagetail
					} else {
						nest[0].tailfield = pagetail
					}
				} else {
					mem[pagetail].hh.rh = mem[29999].hh.rh
				}
				mem[29999].hh.rh = mem[29998].hh.rh
				mem[29998].hh.rh = 0
				pagetail = 29998
			}
		}
		shipout(eqtb[3933].hh.rh)
		eqtb[3933].hh.rh = 0
	}
L10:
	// empty
}

/* procedure: buildpage */
func buildpage() {
	var (
		p     int
		q     int
		r     int
		b     int
		c     int
		pi    int
		n     int
		delta int
		h     int
		w     int
	)
	if (mem[29999].hh.rh == 0) || outputactive {
		goto L10
	}
	for {
	L22:
		p = mem[29999].hh.rh
		if lastglue != 65535 {
			deleteglueref(lastglue)
		}
		lastpenalty = 0
		lastkern = 0
		if mem[p].hh.b0 == 10 {
			{
				lastglue = mem[(p + 1)].hh.lh
				mem[lastglue].hh.rh = (mem[lastglue].hh.rh + 1)
			}
		} else {
			{
				lastglue = 65535
				if mem[p].hh.b0 == 12 {
					lastpenalty = mem[(p + 1)].int
				} else {
					if mem[p].hh.b0 == 11 {
						lastkern = mem[(p + 1)].int
					}
				}
			}
		}
		switch mem[p].hh.b0 {
		case 0:
			if pagecontents < 2 {
				{
					if pagecontents == 0 {
						freezepagespecs(2)
					} else {
						pagecontents = 2
					}
					q = newskipparam(9)
					if mem[(tempptr+1)].int > mem[(p+3)].int {
						mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
					} else {
						mem[(tempptr + 1)].int = 0
					}
					mem[q].hh.rh = p
					mem[29999].hh.rh = q
					goto L22
				}
			} else {
				{
					pagesofar[1] = ((pagesofar[1] + pagesofar[7]) + mem[(p+3)].int)
					pagesofar[7] = mem[(p + 2)].int
					goto L80
				}
			}
		case 1:
			if pagecontents < 2 {
				{
					if pagecontents == 0 {
						freezepagespecs(2)
					} else {
						pagecontents = 2
					}
					q = newskipparam(9)
					if mem[(tempptr+1)].int > mem[(p+3)].int {
						mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
					} else {
						mem[(tempptr + 1)].int = 0
					}
					mem[q].hh.rh = p
					mem[29999].hh.rh = q
					goto L22
				}
			} else {
				{
					pagesofar[1] = ((pagesofar[1] + pagesofar[7]) + mem[(p+3)].int)
					pagesofar[7] = mem[(p + 2)].int
					goto L80
				}
			}
		case 2:
			if pagecontents < 2 {
				{
					if pagecontents == 0 {
						freezepagespecs(2)
					} else {
						pagecontents = 2
					}
					q = newskipparam(9)
					if mem[(tempptr+1)].int > mem[(p+3)].int {
						mem[(tempptr + 1)].int = (mem[(tempptr+1)].int - mem[(p+3)].int)
					} else {
						mem[(tempptr + 1)].int = 0
					}
					mem[q].hh.rh = p
					mem[29999].hh.rh = q
					goto L22
				}
			} else {
				{
					pagesofar[1] = ((pagesofar[1] + pagesofar[7]) + mem[(p+3)].int)
					pagesofar[7] = mem[(p + 2)].int
					goto L80
				}
			}
		case 8:
			goto L80
		case 10:
			if pagecontents < 2 {
				goto L31
			} else {
				if mem[pagetail].hh.b0 < 9 {
					pi = 0
				} else {
					goto L90
				}
			}
		case 11:
			if pagecontents < 2 {
				goto L31
			} else {
				if mem[p].hh.rh == 0 {
					goto L10
				} else {
					if mem[mem[p].hh.rh].hh.b0 == 10 {
						pi = 0
					} else {
						goto L90
					}
				}
			}
		case 12:
			if pagecontents < 2 {
				goto L31
			} else {
				pi = mem[(p + 1)].int
			}
		case 4:
			goto L80
		case 3:
			{
				if pagecontents == 0 {
					freezepagespecs(1)
				}
				n = mem[p].hh.b1
				r = 30000
				for n >= mem[mem[r].hh.rh].hh.b1 {
					r = mem[r].hh.rh
				}
				n = (n - 0)
				if mem[r].hh.b1 != (n + 0) {
					{
						q = getnode(4)
						mem[q].hh.rh = mem[r].hh.rh
						mem[r].hh.rh = q
						r = q
						mem[r].hh.b1 = (n + 0)
						mem[r].hh.b0 = 0
						ensurevbox(n)
						if eqtb[(3678+n)].hh.rh == 0 {
							mem[(r + 3)].int = 0
						} else {
							mem[(r + 3)].int = (mem[(eqtb[(3678+n)].hh.rh+3)].int + mem[(eqtb[(3678+n)].hh.rh+2)].int)
						}
						mem[(r + 2)].hh.lh = 0
						q = eqtb[(2900 + n)].hh.rh
						if eqtb[(5318+n)].int == 1000 {
							h = mem[(r + 3)].int
						} else {
							h = (xovern(mem[(r+3)].int, 1000) * eqtb[(5318+n)].int)
						}
						pagesofar[0] = ((pagesofar[0] - h) - mem[(q+1)].int)
						pagesofar[(2 + mem[q].hh.b0)] = (pagesofar[(2+mem[q].hh.b0)] + mem[(q+2)].int)
						pagesofar[6] = (pagesofar[6] + mem[(q+3)].int)
						if (mem[q].hh.b1 != 0) && (mem[(q+3)].int != 0) {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(998)
								}
								printesc(395)
								printint(n)
								{
									helpptr = 3
									helpline[2] = 999
									helpline[1] = 1000
									helpline[0] = 922
								}
								error_()
							}
						}
					}
				}
				if mem[r].hh.b0 == 1 {
					insertpenalties = (insertpenalties + mem[(p+1)].int)
				} else {
					{
						mem[(r + 2)].hh.rh = p
						delta = (((pagesofar[0] - pagesofar[1]) - pagesofar[7]) + pagesofar[6])
						if eqtb[(5318+n)].int == 1000 {
							h = mem[(p + 3)].int
						} else {
							h = (xovern(mem[(p+3)].int, 1000) * eqtb[(5318+n)].int)
						}
						if ((h <= 0) || (h <= delta)) && ((mem[(p+3)].int + mem[(r+3)].int) <= eqtb[(5851+n)].int) {
							{
								pagesofar[0] = (pagesofar[0] - h)
								mem[(r + 3)].int = (mem[(r+3)].int + mem[(p+3)].int)
							}
						} else {
							{
								if eqtb[(5318+n)].int <= 0 {
									w = 1073741823
								} else {
									{
										w = ((pagesofar[0] - pagesofar[1]) - pagesofar[7])
										if eqtb[(5318+n)].int != 1000 {
											w = (xovern(w, eqtb[(5318+n)].int) * 1000)
										}
									}
								}
								if w > (eqtb[(5851+n)].int - mem[(r+3)].int) {
									w = (eqtb[(5851+n)].int - mem[(r+3)].int)
								}
								q = vertbreak(mem[(p+4)].hh.lh, w, mem[(p+2)].int)
								mem[(r + 3)].int = (mem[(r+3)].int + bestheightplusdepth)
								if eqtb[(5318+n)].int != 1000 {
									bestheightplusdepth = (xovern(bestheightplusdepth, 1000) * eqtb[(5318+n)].int)
								}
								pagesofar[0] = (pagesofar[0] - bestheightplusdepth)
								mem[r].hh.b0 = 1
								mem[(r + 1)].hh.rh = q
								mem[(r + 1)].hh.lh = p
								if q == 0 {
									insertpenalties = (insertpenalties - 10000)
								} else {
									if mem[q].hh.b0 == 12 {
										insertpenalties = (insertpenalties + mem[(q+1)].int)
									}
								}
							}
						}
					}
				}
				goto L80
			}
		default:
			confusion(993)
		}
		if pi < 10000 {
			{
				if pagesofar[1] < pagesofar[0] {
					if ((pagesofar[3] != 0) || (pagesofar[4] != 0)) || (pagesofar[5] != 0) {
						b = 0
					} else {
						b = badness((pagesofar[0] - pagesofar[1]), pagesofar[2])
					}
				} else {
					if (pagesofar[1] - pagesofar[0]) > pagesofar[6] {
						b = 1073741823
					} else {
						b = badness((pagesofar[1] - pagesofar[0]), pagesofar[6])
					}
				}
				if b < 1073741823 {
					if pi <= (-10000) {
						c = pi
					} else {
						if b < 10000 {
							c = ((b + pi) + insertpenalties)
						} else {
							c = 100000
						}
					}
				} else {
					c = b
				}
				if insertpenalties >= 10000 {
					c = 1073741823
				}
				if c <= leastpagecost {
					{
						bestpagebreak = p
						bestsize = pagesofar[0]
						leastpagecost = c
						r = mem[30000].hh.rh
						for r != 30000 {
							{
								mem[(r + 2)].hh.lh = mem[(r + 2)].hh.rh
								r = mem[r].hh.rh
							}
						}
					}
				}
				if (c == 1073741823) || (pi <= (-10000)) {
					{
						fireup(p)
						if outputactive {
							goto L10
						}
						goto L30
					}
				}
			}
		}
		if (mem[p].hh.b0 < 10) || (mem[p].hh.b0 > 11) {
			goto L80
		}
	L90:
		if mem[p].hh.b0 == 11 {
			q = p
		} else {
			{
				q = mem[(p + 1)].hh.lh
				pagesofar[(2 + mem[q].hh.b0)] = (pagesofar[(2+mem[q].hh.b0)] + mem[(q+2)].int)
				pagesofar[6] = (pagesofar[6] + mem[(q+3)].int)
				if (mem[q].hh.b1 != 0) && (mem[(q+3)].int != 0) {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(994)
						}
						{
							helpptr = 4
							helpline[3] = 995
							helpline[2] = 963
							helpline[1] = 964
							helpline[0] = 922
						}
						error_()
						r = newspec(q)
						mem[r].hh.b1 = 0
						deleteglueref(q)
						mem[(p + 1)].hh.lh = r
						q = r
					}
				}
			}
		}
		pagesofar[1] = ((pagesofar[1] + pagesofar[7]) + mem[(q+1)].int)
		pagesofar[7] = 0
	L80:
		if pagesofar[7] > pagemaxdepth {
			{
				pagesofar[1] = ((pagesofar[1] + pagesofar[7]) - pagemaxdepth)
				pagesofar[7] = pagemaxdepth
			}
		}
		mem[pagetail].hh.rh = p
		pagetail = p
		mem[29999].hh.rh = mem[p].hh.rh
		mem[p].hh.rh = 0
		goto L30
	L31:
		mem[29999].hh.rh = mem[p].hh.rh
		mem[p].hh.rh = 0
		flushnodelist(p)
	L30:
		// empty
		if !(mem[29999].hh.rh == 0) {
			break
		}
	}
	if nestptr == 0 {
		curlist.tailfield = 29999
	} else {
		nest[0].tailfield = 29999
	}
L10:
	// empty
}

/* procedure: appspace */
func appspace() {
	var (
		q int
	)
	if (curlist.auxfield.hh.lh >= 2000) && (eqtb[2895].hh.rh != 0) {
		q = newparamglue(13)
	} else {
		{
			if eqtb[2894].hh.rh != 0 {
				mainp = eqtb[2894].hh.rh
			} else {
				{
					mainp = fontglue[eqtb[3934].hh.rh]
					if mainp == 0 {
						{
							mainp = newspec(0)
							maink = (parambase[eqtb[3934].hh.rh] + 2)
							mem[(mainp + 1)].int = fontinfo[maink].int
							mem[(mainp + 2)].int = fontinfo[(maink + 1)].int
							mem[(mainp + 3)].int = fontinfo[(maink + 2)].int
							fontglue[eqtb[3934].hh.rh] = mainp
						}
					}
				}
			}
			mainp = newspec(mainp)
			if curlist.auxfield.hh.lh >= 2000 {
				mem[(mainp + 1)].int = (mem[(mainp+1)].int + fontinfo[(7+parambase[eqtb[3934].hh.rh])].int)
			}
			mem[(mainp + 2)].int = xnoverd(mem[(mainp+2)].int, curlist.auxfield.hh.lh, 1000)
			mem[(mainp + 3)].int = xnoverd(mem[(mainp+3)].int, 1000, curlist.auxfield.hh.lh)
			q = newglue(mainp)
			mem[mainp].hh.rh = 0
		}
	}
	mem[curlist.tailfield].hh.rh = q
	curlist.tailfield = q
}

/* procedure: insertdollarsign */
func insertdollarsign() {
	backinput()
	curtok = 804
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1017)
	}
	{
		helpptr = 2
		helpline[1] = 1018
		helpline[0] = 1019
	}
	inserror()
}

/* procedure: youcant */
func youcant() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(685)
	}
	printcmdchr(curcmd, curchr)
	print_(1020)
	printmode(curlist.modefield)
}

/* procedure: reportillegalcase */
func reportillegalcase() {
	youcant()
	{
		helpptr = 4
		helpline[3] = 1021
		helpline[2] = 1022
		helpline[1] = 1023
		helpline[0] = 1024
	}
	error_()
}

/* function: privileged */
func privileged() bool {
	if curlist.modefield > 0 {
		privileged = true
	} else {
		{
			reportillegalcase()
			privileged = false
		}
	}
}

/* function: itsallover */
func itsallover() bool {
	if privileged {
		{
			if ((29998 == pagetail) && (curlist.headfield == curlist.tailfield)) && (deadcycles == 0) {
				{
					itsallover = true
					goto L10
				}
			}
			backinput()
			{
				mem[curlist.tailfield].hh.rh = newnullbox
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[(curlist.tailfield + 1)].int = eqtb[5833].int
			{
				mem[curlist.tailfield].hh.rh = newglue(8)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			{
				mem[curlist.tailfield].hh.rh = newpenalty((-1073741824))
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			buildpage()
		}
	}
	itsallover = false
L10:
	// empty
}

/* procedure: appendglue */
func appendglue() {
	var (
		s int
	)
	s = curchr
	switch s {
	case 0:
		curval = 4
	case 1:
		curval = 8
	case 2:
		curval = 12
	case 3:
		curval = 16
	case 4:
		scanglue(2)
	case 5:
		scanglue(3)
	}
	{
		mem[curlist.tailfield].hh.rh = newglue(curval)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	if s >= 4 {
		{
			mem[curval].hh.rh = (mem[curval].hh.rh - 1)
			if s > 4 {
				mem[curlist.tailfield].hh.b1 = 99
			}
		}
	}
}

/* procedure: appendkern */
func appendkern() {
	var (
		s int
	)
	s = curchr
	scandimen((s == 99), false, false)
	{
		mem[curlist.tailfield].hh.rh = newkern(curval)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	mem[curlist.tailfield].hh.b1 = s
}

/* procedure: offsave */
func offsave() {
	var (
		p int
	)
	if curgroup == 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(777)
			}
			printcmdchr(curcmd, curchr)
			{
				helpptr = 1
				helpline[0] = 1043
			}
			error_()
		}
	} else {
		{
			backinput()
			p = getavail
			mem[29997].hh.rh = p
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(625)
			}
			switch curgroup {
			case 14:
				{
					mem[p].hh.lh = 6711
					printesc(516)
				}
			case 15:
				{
					mem[p].hh.lh = 804
					printchar(36)
				}
			case 16:
				{
					mem[p].hh.lh = 6712
					mem[p].hh.rh = getavail
					p = mem[p].hh.rh
					mem[p].hh.lh = 3118
					printesc(1042)
				}
			default:
				{
					mem[p].hh.lh = 637
					printchar(125)
				}
			}
			print_(626)
			begintokenlist(mem[29997].hh.rh, 4)
			{
				helpptr = 5
				helpline[4] = 1037
				helpline[3] = 1038
				helpline[2] = 1039
				helpline[1] = 1040
				helpline[0] = 1041
			}
			error_()
		}
	}
}

/* procedure: extrarightbrace */
func extrarightbrace() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1048)
	}
	switch curgroup {
	case 14:
		printesc(516)
	case 15:
		printchar(36)
	case 16:
		printesc(877)
	}
	{
		helpptr = 5
		helpline[4] = 1049
		helpline[3] = 1050
		helpline[2] = 1051
		helpline[1] = 1052
		helpline[0] = 1053
	}
	error_()
	alignstate = (alignstate + 1)
}

/* procedure: normalparagraph */
func normalparagraph() {
	if eqtb[5282].int != 0 {
		eqworddefine(5282, 0)
	}
	if eqtb[5847].int != 0 {
		eqworddefine(5847, 0)
	}
	if eqtb[5304].int != 1 {
		eqworddefine(5304, 1)
	}
	if eqtb[3412].hh.rh != 0 {
		eqdefine(3412, 118, 0)
	}
}

/* procedure: boxend */
func boxend(boxcontext int) {
	var (
		p int
	)
	if boxcontext < 1073741824 {
		{
			if curbox != 0 {
				{
					mem[(curbox + 4)].int = boxcontext
					if abs_(curlist.modefield) == 1 {
						{
							appendtovlist(curbox)
							if adjusttail != 0 {
								{
									if 29995 != adjusttail {
										{
											mem[curlist.tailfield].hh.rh = mem[29995].hh.rh
											curlist.tailfield = adjusttail
										}
									}
									adjusttail = 0
								}
							}
							if curlist.modefield > 0 {
								buildpage()
							}
						}
					} else {
						{
							if abs_(curlist.modefield) == 102 {
								curlist.auxfield.hh.lh = 1000
							} else {
								{
									p = newnoad
									mem[(p + 1)].hh.rh = 2
									mem[(p + 1)].hh.lh = curbox
									curbox = p
								}
							}
							mem[curlist.tailfield].hh.rh = curbox
							curlist.tailfield = curbox
						}
					}
				}
			}
		}
	} else {
		if boxcontext < 1073742336 {
			if boxcontext < 1073742080 {
				eqdefine(((-1073738146) + boxcontext), 119, curbox)
			} else {
				geqdefine(((-1073738402) + boxcontext), 119, curbox)
			}
		} else {
			if curbox != 0 {
				if boxcontext > 1073742336 {
					{
						for {
							getxtoken()
							if !((curcmd != 10) && (curcmd != 0)) {
								break
							}
						}
						if ((curcmd == 26) && (abs_(curlist.modefield) != 1)) || ((curcmd == 27) && (abs_(curlist.modefield) == 1)) {
							{
								appendglue()
								mem[curlist.tailfield].hh.b1 = (boxcontext - 1073742237)
								mem[(curlist.tailfield + 1)].hh.rh = curbox
							}
						} else {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(1066)
								}
								{
									helpptr = 3
									helpline[2] = 1067
									helpline[1] = 1068
									helpline[0] = 1069
								}
								backerror()
								flushnodelist(curbox)
							}
						}
					}
				} else {
					shipout(curbox)
				}
			}
		}
	}
}

/* procedure: beginbox */
func beginbox(boxcontext int) {
	var (
		p int
		q int
		m int
		k int
		n byte
	)
	switch curchr {
	case 0:
		{
			scaneightbitint()
			curbox = eqtb[(3678 + curval)].hh.rh
			eqtb[(3678 + curval)].hh.rh = 0
		}
	case 1:
		{
			scaneightbitint()
			curbox = copynodelist(eqtb[(3678 + curval)].hh.rh)
		}
	case 2:
		{
			curbox = 0
			if abs_(curlist.modefield) == 203 {
				{
					youcant()
					{
						helpptr = 1
						helpline[0] = 1070
					}
					error_()
				}
			} else {
				if (curlist.modefield == 1) && (curlist.headfield == curlist.tailfield) {
					{
						youcant()
						{
							helpptr = 2
							helpline[1] = 1071
							helpline[0] = 1072
						}
						error_()
					}
				} else {
					{
						if !(curlist.tailfield >= himemmin) {
							if (mem[curlist.tailfield].hh.b0 == 0) || (mem[curlist.tailfield].hh.b0 == 1) {
								{
									q = curlist.headfield
									for {
										p = q
										if !(q >= himemmin) {
											if mem[q].hh.b0 == 7 {
												{
													for m := 1; m <= mem[q].hh.b1; m++ {
														p = mem[p].hh.rh
													}
													if p == curlist.tailfield {
														goto L30
													}
												}
											}
										}
										q = mem[p].hh.rh
										if !(q == curlist.tailfield) {
											break
										}
									}
									curbox = curlist.tailfield
									mem[(curbox + 4)].int = 0
									curlist.tailfield = p
									mem[p].hh.rh = 0
								L30:
									// empty
								}
							}
						}
					}
				}
			}
		}
	case 3:
		{
			scaneightbitint()
			n = curval
			if !scankeyword(842) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1073)
					}
					{
						helpptr = 2
						helpline[1] = 1074
						helpline[0] = 1075
					}
					error_()
				}
			}
			scandimen(false, false, false)
			curbox = vsplit(n, curval)
		}
	default:
		{
			k = (curchr - 4)
			savestack[(saveptr + 0)].int = boxcontext
			if k == 102 {
				if (boxcontext < 1073741824) && (abs_(curlist.modefield) == 1) {
					scanspec(3, true)
				} else {
					scanspec(2, true)
				}
			} else {
				{
					if k == 1 {
						scanspec(4, true)
					} else {
						{
							scanspec(5, true)
							k = 1
						}
					}
					normalparagraph()
				}
			}
			pushnest()
			curlist.modefield = (-k)
			if k == 1 {
				{
					curlist.auxfield.int = (-65536000)
					if eqtb[3418].hh.rh != 0 {
						begintokenlist(eqtb[3418].hh.rh, 11)
					}
				}
			} else {
				{
					curlist.auxfield.hh.lh = 1000
					if eqtb[3417].hh.rh != 0 {
						begintokenlist(eqtb[3417].hh.rh, 10)
					}
				}
			}
			goto L10
		}
	}
	boxend(boxcontext)
L10:
	// empty
}

/* procedure: scanbox */
func scanbox(boxcontext int) {
	for {
		getxtoken()
		if !((curcmd != 10) && (curcmd != 0)) {
			break
		}
	}
	if curcmd == 20 {
		beginbox(boxcontext)
	} else {
		if (boxcontext >= 1073742337) && ((curcmd == 36) || (curcmd == 35)) {
			{
				curbox = scanrulespec
				boxend(boxcontext)
			}
		} else {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(1076)
				}
				{
					helpptr = 3
					helpline[2] = 1077
					helpline[1] = 1078
					helpline[0] = 1079
				}
				backerror()
			}
		}
	}
}

/* procedure: package */
func package_(c int) {
	var (
		h int
		p int
		d int
	)
	d = eqtb[5837].int
	unsave()
	saveptr = (saveptr - 3)
	if curlist.modefield == (-102) {
		curbox = hpack(mem[curlist.headfield].hh.rh, savestack[(saveptr+2)].int, savestack[(saveptr+1)].int)
	} else {
		{
			curbox = vpackage(mem[curlist.headfield].hh.rh, savestack[(saveptr+2)].int, savestack[(saveptr+1)].int, d)
			if c == 4 {
				{
					h = 0
					p = mem[(curbox + 5)].hh.rh
					if p != 0 {
						if mem[p].hh.b0 <= 2 {
							h = mem[(p + 3)].int
						}
					}
					mem[(curbox + 2)].int = ((mem[(curbox+2)].int - h) + mem[(curbox+3)].int)
					mem[(curbox + 3)].int = h
				}
			}
		}
	}
	popnest()
	boxend(savestack[(saveptr + 0)].int)
}

/* function: normmin */
func normmin(h int) int {
	if h <= 0 {
		normmin = 1
	} else {
		if h >= 63 {
			normmin = 63
		} else {
			normmin = h
		}
	}
}

/* procedure: newgraf */
func newgraf(indented bool) {
	curlist.pgfield = 0
	if (curlist.modefield == 1) || (curlist.headfield != curlist.tailfield) {
		{
			mem[curlist.tailfield].hh.rh = newparamglue(2)
			curlist.tailfield = mem[curlist.tailfield].hh.rh
		}
	}
	pushnest()
	curlist.modefield = 102
	curlist.auxfield.hh.lh = 1000
	if eqtb[5313].int <= 0 {
		curlang = 0
	} else {
		if eqtb[5313].int > 255 {
			curlang = 0
		} else {
			curlang = eqtb[5313].int
		}
	}
	curlist.auxfield.hh.rh = curlang
	curlist.pgfield = ((((normmin(eqtb[5314].int) * 64) + normmin(eqtb[5315].int)) * 65536) + curlang)
	if indented {
		{
			curlist.tailfield = newnullbox
			mem[curlist.headfield].hh.rh = curlist.tailfield
			mem[(curlist.tailfield + 1)].int = eqtb[5830].int
		}
	}
	if eqtb[3414].hh.rh != 0 {
		begintokenlist(eqtb[3414].hh.rh, 7)
	}
	if nestptr == 1 {
		buildpage()
	}
}

/* procedure: indentinhmode */
func indentinhmode() {
	var (
		p int
		q int
	)
	if curchr > 0 {
		{
			p = newnullbox
			mem[(p + 1)].int = eqtb[5830].int
			if abs_(curlist.modefield) == 102 {
				curlist.auxfield.hh.lh = 1000
			} else {
				{
					q = newnoad
					mem[(q + 1)].hh.rh = 2
					mem[(q + 1)].hh.lh = p
					p = q
				}
			}
			{
				mem[curlist.tailfield].hh.rh = p
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
		}
	}
}

/* procedure: headforvmode */
func headforvmode() {
	if curlist.modefield < 0 {
		if curcmd != 36 {
			offsave()
		} else {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(685)
				}
				printesc(521)
				print_(1082)
				{
					helpptr = 2
					helpline[1] = 1083
					helpline[0] = 1084
				}
				error_()
			}
		}
	} else {
		{
			backinput()
			curtok = partoken
			backinput()
			curinput.indexfield = 4
		}
	}
}

/* procedure: endgraf */
func endgraf() {
	if curlist.modefield == 102 {
		{
			if curlist.headfield == curlist.tailfield {
				popnest()
			} else {
				linebreak(eqtb[5269].int)
			}
			normalparagraph()
			errorcount = 0
		}
	}
}

/* procedure: begininsertoradjust */
func begininsertoradjust() {
	if curcmd == 38 {
		curval = 255
	} else {
		{
			scaneightbitint()
			if curval == 255 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1085)
					}
					printesc(330)
					printint(255)
					{
						helpptr = 1
						helpline[0] = 1086
					}
					error_()
					curval = 0
				}
			}
		}
	}
	savestack[(saveptr + 0)].int = curval
	saveptr = (saveptr + 1)
	newsavelevel(11)
	scanleftbrace()
	normalparagraph()
	pushnest()
	curlist.modefield = (-1)
	curlist.auxfield.int = (-65536000)
}

/* procedure: makemark */
func makemark() {
	var (
		p int
	)
	p = scantoks(false, true)
	p = getnode(2)
	mem[p].hh.b0 = 4
	mem[p].hh.b1 = 0
	mem[(p + 1)].int = defref
	mem[curlist.tailfield].hh.rh = p
	curlist.tailfield = p
}

/* procedure: appendpenalty */
func appendpenalty() {
	scanint()
	{
		mem[curlist.tailfield].hh.rh = newpenalty(curval)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	if curlist.modefield == 1 {
		buildpage()
	}
}

/* procedure: deletelast */
func deletelast() {
	var (
		p int
		q int
		m int
	)
	if (curlist.modefield == 1) && (curlist.tailfield == curlist.headfield) {
		{
			if (curchr != 10) || (lastglue != 65535) {
				{
					youcant()
					{
						helpptr = 2
						helpline[1] = 1071
						helpline[0] = 1087
					}
					if curchr == 11 {
						helpline[0] = 1088
					} else {
						if curchr != 10 {
							helpline[0] = 1089
						}
					}
					error_()
				}
			}
		}
	} else {
		{
			if !(curlist.tailfield >= himemmin) {
				if mem[curlist.tailfield].hh.b0 == curchr {
					{
						q = curlist.headfield
						for {
							p = q
							if !(q >= himemmin) {
								if mem[q].hh.b0 == 7 {
									{
										for m := 1; m <= mem[q].hh.b1; m++ {
											p = mem[p].hh.rh
										}
										if p == curlist.tailfield {
											goto L10
										}
									}
								}
							}
							q = mem[p].hh.rh
							if !(q == curlist.tailfield) {
								break
							}
						}
						mem[p].hh.rh = 0
						flushnodelist(curlist.tailfield)
						curlist.tailfield = p
					}
				}
			}
		}
	}
L10:
	// empty
}

/* procedure: unpackage */
func unpackage() {
	var (
		p int
		c int
	)
	c = curchr
	scaneightbitint()
	p = eqtb[(3678 + curval)].hh.rh
	if p == 0 {
		goto L10
	}
	if ((abs_(curlist.modefield) == 203) || ((abs_(curlist.modefield) == 1) && (mem[p].hh.b0 != 1))) || ((abs_(curlist.modefield) == 102) && (mem[p].hh.b0 != 0)) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1097)
			}
			{
				helpptr = 3
				helpline[2] = 1098
				helpline[1] = 1099
				helpline[0] = 1100
			}
			error_()
			goto L10
		}
	}
	if c == 1 {
		mem[curlist.tailfield].hh.rh = copynodelist(mem[(p + 5)].hh.rh)
	} else {
		{
			mem[curlist.tailfield].hh.rh = mem[(p + 5)].hh.rh
			eqtb[(3678 + curval)].hh.rh = 0
			freenode(p, 7)
		}
	}
	for mem[curlist.tailfield].hh.rh != 0 {
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
L10:
	// empty
}

/* procedure: appenditaliccorrection */
func appenditaliccorrection() {
	var (
		p int
		f int
	)
	if curlist.tailfield != curlist.headfield {
		{
			if curlist.tailfield >= himemmin {
				p = curlist.tailfield
			} else {
				if mem[curlist.tailfield].hh.b0 == 6 {
					p = (curlist.tailfield + 1)
				} else {
					goto L10
				}
			}
			f = mem[p].hh.b0
			{
				mem[curlist.tailfield].hh.rh = newkern(fontinfo[(italicbase[f] + ((fontinfo[(charbase[f]+mem[p].hh.b1)].qqqq.b2 - 0) / 4))].int)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.b1 = 1
		}
	}
L10:
	// empty
}

/* procedure: appenddiscretionary */
func appenddiscretionary() {
	var (
		c int
	)
	{
		mem[curlist.tailfield].hh.rh = newdisc
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	if curchr == 1 {
		{
			c = hyphenchar[eqtb[3934].hh.rh]
			if c >= 0 {
				if c < 256 {
					mem[(curlist.tailfield + 1)].hh.lh = newcharacter(eqtb[3934].hh.rh, c)
				}
			}
		}
	} else {
		{
			saveptr = (saveptr + 1)
			savestack[(saveptr - 1)].int = 0
			newsavelevel(10)
			scanleftbrace()
			pushnest()
			curlist.modefield = (-102)
			curlist.auxfield.hh.lh = 1000
		}
	}
}

/* procedure: builddiscretionary */
func builddiscretionary() {
	var (
		p int
		q int
		n int
	)
	unsave()
	q = curlist.headfield
	p = mem[q].hh.rh
	n = 0
	for p != 0 {
		{
			if !(p >= himemmin) {
				if mem[p].hh.b0 > 2 {
					if mem[p].hh.b0 != 11 {
						if mem[p].hh.b0 != 6 {
							{
								{
									if interaction == 3 {
										// empty
									}
									printnl(262)
									print_(1107)
								}
								{
									helpptr = 1
									helpline[0] = 1108
								}
								error_()
								begindiagnostic()
								printnl(1109)
								showbox(p)
								enddiagnostic(true)
								flushnodelist(p)
								mem[q].hh.rh = 0
								goto L30
							}
						}
					}
				}
			}
			q = p
			p = mem[q].hh.rh
			n = (n + 1)
		}
	}
L30:
	// empty
	p = mem[curlist.headfield].hh.rh
	popnest()
	switch savestack[(saveptr - 1)].int {
	case 0:
		mem[(curlist.tailfield + 1)].hh.lh = p
	case 1:
		mem[(curlist.tailfield + 1)].hh.rh = p
	case 2:
		{
			if (n > 0) && (abs_(curlist.modefield) == 203) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1101)
					}
					printesc(349)
					{
						helpptr = 2
						helpline[1] = 1102
						helpline[0] = 1103
					}
					flushnodelist(p)
					n = 0
					error_()
				}
			} else {
				mem[curlist.tailfield].hh.rh = p
			}
			if n <= 255 {
				mem[curlist.tailfield].hh.b1 = n
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1104)
					}
					{
						helpptr = 2
						helpline[1] = 1105
						helpline[0] = 1106
					}
					error_()
				}
			}
			if n > 0 {
				curlist.tailfield = q
			}
			saveptr = (saveptr - 1)
			goto L10
		}
	}
	savestack[(saveptr - 1)].int = (savestack[(saveptr-1)].int + 1)
	newsavelevel(10)
	scanleftbrace()
	pushnest()
	curlist.modefield = (-102)
	curlist.auxfield.hh.lh = 1000
L10:
	// empty
}

/* procedure: makeaccent */
func makeaccent() {
	var (
		s     float64
		t     float64
		p     int
		q     int
		r     int
		f     int
		a     int
		h     int
		x     int
		w     int
		delta int
		i     *fourquarters_t
	)
	scancharnum()
	f = eqtb[3934].hh.rh
	p = newcharacter(f, curval)
	if p != 0 {
		{
			x = fontinfo[(5 + parambase[f])].int
			s = (fontinfo[(1+parambase[f])].int / 65536)
			a = fontinfo[(widthbase[f] + fontinfo[(charbase[f]+mem[p].hh.b1)].qqqq.b0)].int
			doassignments()
			q = 0
			f = eqtb[3934].hh.rh
			if ((curcmd == 11) || (curcmd == 12)) || (curcmd == 68) {
				q = newcharacter(f, curchr)
			} else {
				if curcmd == 16 {
					{
						scancharnum()
						q = newcharacter(f, curval)
					}
				} else {
					backinput()
				}
			}
			if q != 0 {
				{
					t = (fontinfo[(1+parambase[f])].int / 65536)
					i = fontinfo[(charbase[f] + mem[q].hh.b1)].qqqq
					w = fontinfo[(widthbase[f] + i.b0)].int
					h = fontinfo[(heightbase[f] + ((i.b1 - 0) / 16))].int
					if h != x {
						{
							p = hpack(p, 0, 1)
							mem[(p + 4)].int = (x - h)
						}
					}
					delta = round_(((((w - a) / 2) + (h * t)) - (x * s)))
					r = newkern(delta)
					mem[r].hh.b1 = 2
					mem[curlist.tailfield].hh.rh = r
					mem[r].hh.rh = p
					curlist.tailfield = newkern(((-a) - delta))
					mem[curlist.tailfield].hh.b1 = 2
					mem[p].hh.rh = curlist.tailfield
					p = q
				}
			}
			mem[curlist.tailfield].hh.rh = p
			curlist.tailfield = p
			curlist.auxfield.hh.lh = 1000
		}
	}
}

/* procedure: alignerror */
func alignerror() {
	if abs_(alignstate) > 2 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1114)
			}
			printcmdchr(curcmd, curchr)
			if curtok == 1062 {
				{
					{
						helpptr = 6
						helpline[5] = 1115
						helpline[4] = 1116
						helpline[3] = 1117
						helpline[2] = 1118
						helpline[1] = 1119
						helpline[0] = 1120
					}
				}
			} else {
				{
					{
						helpptr = 5
						helpline[4] = 1115
						helpline[3] = 1121
						helpline[2] = 1118
						helpline[1] = 1119
						helpline[0] = 1120
					}
				}
			}
			error_()
		}
	} else {
		{
			backinput()
			if alignstate < 0 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(657)
					}
					alignstate = (alignstate + 1)
					curtok = 379
				}
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1110)
					}
					alignstate = (alignstate - 1)
					curtok = 637
				}
			}
			{
				helpptr = 3
				helpline[2] = 1111
				helpline[1] = 1112
				helpline[0] = 1113
			}
			inserror()
		}
	}
}

/* procedure: noalignerror */
func noalignerror() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1114)
	}
	printesc(527)
	{
		helpptr = 2
		helpline[1] = 1122
		helpline[0] = 1123
	}
	error_()
}

/* procedure: omiterror */
func omiterror() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1114)
	}
	printesc(530)
	{
		helpptr = 2
		helpline[1] = 1124
		helpline[0] = 1123
	}
	error_()
}

/* procedure: doendv */
func doendv() {
	baseptr = inputptr
	inputstack[baseptr] = curinput
	for ((inputstack[baseptr].indexfield != 2) && (inputstack[baseptr].locfield == 0)) && (inputstack[baseptr].statefield == 0) {
		baseptr = (baseptr - 1)
	}
	if ((inputstack[baseptr].indexfield != 2) || (inputstack[baseptr].locfield != 0)) || (inputstack[baseptr].statefield != 0) {
		fatalerror(595)
	}
	if curgroup == 6 {
		{
			endgraf()
			if fincol {
				finrow()
			}
		}
	} else {
		offsave()
	}
}

/* procedure: cserror */
func cserror() {
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(777)
	}
	printesc(505)
	{
		helpptr = 1
		helpline[0] = 1126
	}
	error_()
}

/* procedure: pushmath */
func pushmath(c int) {
	pushnest()
	curlist.modefield = (-203)
	curlist.auxfield.int = 0
	newsavelevel(c)
}

/* procedure: initmath */
func initmath() {
	var (
		w int
		l int
		s int
		p int
		q int
		f int
		n int
		v int
		d int
	)
	gettoken()
	if (curcmd == 3) && (curlist.modefield > 0) {
		{
			if curlist.headfield == curlist.tailfield {
				{
					popnest()
					w = (-1073741823)
				}
			} else {
				{
					linebreak(eqtb[5270].int)
					v = (mem[(justbox+4)].int + (2 * fontinfo[(6+parambase[eqtb[3934].hh.rh])].int))
					w = (-1073741823)
					p = mem[(justbox + 5)].hh.rh
					for p != 0 {
						{
						L21:
							if p >= himemmin {
								{
									f = mem[p].hh.b0
									d = fontinfo[(widthbase[f] + fontinfo[(charbase[f]+mem[p].hh.b1)].qqqq.b0)].int
									goto L40
								}
							}
							switch mem[p].hh.b0 {
							case 0:
								{
									d = mem[(p + 1)].int
									goto L40
								}
							case 1:
								{
									d = mem[(p + 1)].int
									goto L40
								}
							case 2:
								{
									d = mem[(p + 1)].int
									goto L40
								}
							case 6:
								{
									mem[29988] = mem[(p + 1)]
									mem[29988].hh.rh = mem[p].hh.rh
									p = 29988
									goto L21
								}
							case 11:
								d = mem[(p + 1)].int
							case 9:
								d = mem[(p + 1)].int
							case 10:
								{
									q = mem[(p + 1)].hh.lh
									d = mem[(q + 1)].int
									if mem[(justbox+5)].hh.b0 == 1 {
										{
											if (mem[(justbox+5)].hh.b1 == mem[q].hh.b0) && (mem[(q+2)].int != 0) {
												v = 1073741823
											}
										}
									} else {
										if mem[(justbox+5)].hh.b0 == 2 {
											{
												if (mem[(justbox+5)].hh.b1 == mem[q].hh.b1) && (mem[(q+3)].int != 0) {
													v = 1073741823
												}
											}
										}
									}
									if mem[p].hh.b1 >= 100 {
										goto L40
									}
								}
							case 8:
								d = 0
							default:
								d = 0
							}
							if v < 1073741823 {
								v = (v + d)
							}
							goto L45
						L40:
							if v < 1073741823 {
								{
									v = (v + d)
									w = v
								}
							} else {
								{
									w = 1073741823
									goto L30
								}
							}
						L45:
							p = mem[p].hh.rh
						}
					}
				L30:
					// empty
				}
			}
			if eqtb[3412].hh.rh == 0 {
				if (eqtb[5847].int != 0) && (((eqtb[5304].int >= 0) && ((curlist.pgfield + 2) > eqtb[5304].int)) || ((curlist.pgfield + 1) < (-eqtb[5304].int))) {
					{
						l = (eqtb[5833].int - abs_(eqtb[5847].int))
						if eqtb[5847].int > 0 {
							s = eqtb[5847].int
						} else {
							s = 0
						}
					}
				} else {
					{
						l = eqtb[5833].int
						s = 0
					}
				}
			} else {
				{
					n = mem[eqtb[3412].hh.rh].hh.lh
					if (curlist.pgfield + 2) >= n {
						p = (eqtb[3412].hh.rh + (2 * n))
					} else {
						p = (eqtb[3412].hh.rh + (2 * (curlist.pgfield + 2)))
					}
					s = mem[(p - 1)].int
					l = mem[p].int
				}
			}
			pushmath(15)
			curlist.modefield = 203
			eqworddefine(5307, (-1))
			eqworddefine(5843, w)
			eqworddefine(5844, l)
			eqworddefine(5845, s)
			if eqtb[3416].hh.rh != 0 {
				begintokenlist(eqtb[3416].hh.rh, 9)
			}
			if nestptr == 1 {
				buildpage()
			}
		}
	} else {
		{
			backinput()
			{
				pushmath(15)
				eqworddefine(5307, (-1))
				if eqtb[3415].hh.rh != 0 {
					begintokenlist(eqtb[3415].hh.rh, 8)
				}
			}
		}
	}
}

/* procedure: starteqno */
func starteqno() {
	savestack[(saveptr + 0)].int = curchr
	saveptr = (saveptr + 1)
	{
		pushmath(15)
		eqworddefine(5307, (-1))
		if eqtb[3415].hh.rh != 0 {
			begintokenlist(eqtb[3415].hh.rh, 8)
		}
	}
}

/* procedure: scanmath */
func scanmath(p int) {
	var (
		c int
	)
L20:
	for {
		getxtoken()
		if !((curcmd != 10) && (curcmd != 0)) {
			break
		}
	}
L21:
	switch curcmd {
	case 11:
		{
			c = (eqtb[(5007+curchr)].hh.rh - 0)
			if c == 32768 {
				{
					{
						curcs = (curchr + 1)
						curcmd = eqtb[curcs].hh.b0
						curchr = eqtb[curcs].hh.rh
						xtoken()
						backinput()
					}
					goto L20
				}
			}
		}
	case 12:
		{
			c = (eqtb[(5007+curchr)].hh.rh - 0)
			if c == 32768 {
				{
					{
						curcs = (curchr + 1)
						curcmd = eqtb[curcs].hh.b0
						curchr = eqtb[curcs].hh.rh
						xtoken()
						backinput()
					}
					goto L20
				}
			}
		}
	case 68:
		{
			c = (eqtb[(5007+curchr)].hh.rh - 0)
			if c == 32768 {
				{
					{
						curcs = (curchr + 1)
						curcmd = eqtb[curcs].hh.b0
						curchr = eqtb[curcs].hh.rh
						xtoken()
						backinput()
					}
					goto L20
				}
			}
		}
	case 16:
		{
			scancharnum()
			curchr = curval
			curcmd = 68
			goto L21
		}
	case 17:
		{
			scanfifteenbitint()
			c = curval
		}
	case 69:
		c = curchr
	case 15:
		{
			scantwentysevenbitint()
			c = (curval / 4096)
		}
	default:
		{
			backinput()
			scanleftbrace()
			savestack[(saveptr + 0)].int = p
			saveptr = (saveptr + 1)
			pushmath(9)
			goto L10
		}
	}
	mem[p].hh.rh = 1
	mem[p].hh.b1 = ((c % 256) + 0)
	if (c >= 28672) && ((eqtb[5307].int >= 0) && (eqtb[5307].int < 16)) {
		mem[p].hh.b0 = eqtb[5307].int
	} else {
		mem[p].hh.b0 = ((c / 256) % 16)
	}
L10:
	// empty
}

/* procedure: setmathchar */
func setmathchar(c int) {
	var (
		p int
	)
	if c >= 32768 {
		{
			curcs = (curchr + 1)
			curcmd = eqtb[curcs].hh.b0
			curchr = eqtb[curcs].hh.rh
			xtoken()
			backinput()
		}
	} else {
		{
			p = newnoad
			mem[(p + 1)].hh.rh = 1
			mem[(p + 1)].hh.b1 = ((c % 256) + 0)
			mem[(p + 1)].hh.b0 = ((c / 256) % 16)
			if c >= 28672 {
				{
					if (eqtb[5307].int >= 0) && (eqtb[5307].int < 16) {
						mem[(p + 1)].hh.b0 = eqtb[5307].int
					}
					mem[p].hh.b0 = 16
				}
			} else {
				mem[p].hh.b0 = (16 + (c / 4096))
			}
			mem[curlist.tailfield].hh.rh = p
			curlist.tailfield = p
		}
	}
}

/* procedure: mathlimitswitch */
func mathlimitswitch() {
	if curlist.headfield != curlist.tailfield {
		if mem[curlist.tailfield].hh.b0 == 17 {
			{
				mem[curlist.tailfield].hh.b1 = curchr
				goto L10
			}
		}
	}
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1130)
	}
	{
		helpptr = 1
		helpline[0] = 1131
	}
	error_()
L10:
	// empty
}

/* procedure: scandelimiter */
func scandelimiter(p int, r bool) {
	if r {
		scantwentysevenbitint()
	} else {
		{
			for {
				getxtoken()
				if !((curcmd != 10) && (curcmd != 0)) {
					break
				}
			}
			switch curcmd {
			case 11:
				curval = eqtb[(5574 + curchr)].int
			case 12:
				curval = eqtb[(5574 + curchr)].int
			case 15:
				scantwentysevenbitint()
			default:
				curval = (-1)
			}
		}
	}
	if curval < 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1132)
			}
			{
				helpptr = 6
				helpline[5] = 1133
				helpline[4] = 1134
				helpline[3] = 1135
				helpline[2] = 1136
				helpline[1] = 1137
				helpline[0] = 1138
			}
			backerror()
			curval = 0
		}
	}
	mem[p].qqqq.b0 = ((curval / 1048576) % 16)
	mem[p].qqqq.b1 = (((curval / 4096) % 256) + 0)
	mem[p].qqqq.b2 = ((curval / 256) % 16)
	mem[p].qqqq.b3 = ((curval % 256) + 0)
}

/* procedure: mathradical */
func mathradical() {
	{
		mem[curlist.tailfield].hh.rh = getnode(5)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	mem[curlist.tailfield].hh.b0 = 24
	mem[curlist.tailfield].hh.b1 = 0
	mem[(curlist.tailfield + 1)].hh = emptyfield
	mem[(curlist.tailfield + 3)].hh = emptyfield
	mem[(curlist.tailfield + 2)].hh = emptyfield
	scandelimiter((curlist.tailfield + 4), true)
	scanmath((curlist.tailfield + 1))
}

/* procedure: mathac */
func mathac() {
	if curcmd == 45 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1139)
			}
			printesc(523)
			print_(1140)
			{
				helpptr = 2
				helpline[1] = 1141
				helpline[0] = 1142
			}
			error_()
		}
	}
	{
		mem[curlist.tailfield].hh.rh = getnode(5)
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	mem[curlist.tailfield].hh.b0 = 28
	mem[curlist.tailfield].hh.b1 = 0
	mem[(curlist.tailfield + 1)].hh = emptyfield
	mem[(curlist.tailfield + 3)].hh = emptyfield
	mem[(curlist.tailfield + 2)].hh = emptyfield
	mem[(curlist.tailfield + 4)].hh.rh = 1
	scanfifteenbitint()
	mem[(curlist.tailfield + 4)].hh.b1 = ((curval % 256) + 0)
	if (curval >= 28672) && ((eqtb[5307].int >= 0) && (eqtb[5307].int < 16)) {
		mem[(curlist.tailfield + 4)].hh.b0 = eqtb[5307].int
	} else {
		mem[(curlist.tailfield + 4)].hh.b0 = ((curval / 256) % 16)
	}
	scanmath((curlist.tailfield + 1))
}

/* procedure: appendchoices */
func appendchoices() {
	{
		mem[curlist.tailfield].hh.rh = newchoice
		curlist.tailfield = mem[curlist.tailfield].hh.rh
	}
	saveptr = (saveptr + 1)
	savestack[(saveptr - 1)].int = 0
	pushmath(13)
	scanleftbrace()
}

/* function: finmlist */
func finmlist(p int) int {
	var (
		q int
	)
	if curlist.auxfield.int != 0 {
		{
			mem[(curlist.auxfield.int + 3)].hh.rh = 3
			mem[(curlist.auxfield.int + 3)].hh.lh = mem[curlist.headfield].hh.rh
			if p == 0 {
				q = curlist.auxfield.int
			} else {
				{
					q = mem[(curlist.auxfield.int + 2)].hh.lh
					if mem[q].hh.b0 != 30 {
						confusion(877)
					}
					mem[(curlist.auxfield.int + 2)].hh.lh = mem[q].hh.rh
					mem[q].hh.rh = curlist.auxfield.int
					mem[curlist.auxfield.int].hh.rh = p
				}
			}
		}
	} else {
		{
			mem[curlist.tailfield].hh.rh = p
			q = mem[curlist.headfield].hh.rh
		}
	}
	popnest()
	finmlist = q
}

/* procedure: buildchoices */
func buildchoices() {
	var (
		p int
	)
	unsave()
	p = finmlist(0)
	switch savestack[(saveptr - 1)].int {
	case 0:
		mem[(curlist.tailfield + 1)].hh.lh = p
	case 1:
		mem[(curlist.tailfield + 1)].hh.rh = p
	case 2:
		mem[(curlist.tailfield + 2)].hh.lh = p
	case 3:
		{
			mem[(curlist.tailfield + 2)].hh.rh = p
			saveptr = (saveptr - 1)
			goto L10
		}
	}
	savestack[(saveptr - 1)].int = (savestack[(saveptr-1)].int + 1)
	pushmath(13)
	scanleftbrace()
L10:
	// empty
}

/* procedure: subsup */
func subsup() {
	var (
		t int
		p int
	)
	t = 0
	p = 0
	if curlist.tailfield != curlist.headfield {
		if (mem[curlist.tailfield].hh.b0 >= 16) && (mem[curlist.tailfield].hh.b0 < 30) {
			{
				p = (((curlist.tailfield + 2) + curcmd) - 7)
				t = mem[p].hh.rh
			}
		}
	}
	if (p == 0) || (t != 0) {
		{
			{
				mem[curlist.tailfield].hh.rh = newnoad
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			p = (((curlist.tailfield + 2) + curcmd) - 7)
			if t != 0 {
				{
					if curcmd == 7 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(1143)
							}
							{
								helpptr = 1
								helpline[0] = 1144
							}
						}
					} else {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(1145)
							}
							{
								helpptr = 1
								helpline[0] = 1146
							}
						}
					}
					error_()
				}
			}
		}
	}
	scanmath(p)
}

/* procedure: mathfraction */
func mathfraction() {
	var (
		c int
	)
	c = curchr
	if curlist.auxfield.int != 0 {
		{
			if c >= 3 {
				{
					scandelimiter(29988, false)
					scandelimiter(29988, false)
				}
			}
			if (c % 3) == 0 {
				scandimen(false, false, false)
			}
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1153)
			}
			{
				helpptr = 3
				helpline[2] = 1154
				helpline[1] = 1155
				helpline[0] = 1156
			}
			error_()
		}
	} else {
		{
			curlist.auxfield.int = getnode(6)
			mem[curlist.auxfield.int].hh.b0 = 25
			mem[curlist.auxfield.int].hh.b1 = 0
			mem[(curlist.auxfield.int + 2)].hh.rh = 3
			mem[(curlist.auxfield.int + 2)].hh.lh = mem[curlist.headfield].hh.rh
			mem[(curlist.auxfield.int + 3)].hh = emptyfield
			mem[(curlist.auxfield.int + 4)].qqqq = nulldelimiter
			mem[(curlist.auxfield.int + 5)].qqqq = nulldelimiter
			mem[curlist.headfield].hh.rh = 0
			curlist.tailfield = curlist.headfield
			if c >= 3 {
				{
					scandelimiter((curlist.auxfield.int + 4), false)
					scandelimiter((curlist.auxfield.int + 5), false)
				}
			}
			switch c % 3 {
			case 0:
				{
					scandimen(false, false, false)
					mem[(curlist.auxfield.int + 1)].int = curval
				}
			case 1:
				mem[(curlist.auxfield.int + 1)].int = 1073741824
			case 2:
				mem[(curlist.auxfield.int + 1)].int = 0
			}
		}
	}
}

/* procedure: mathleftright */
func mathleftright() {
	var (
		t int
		p int
	)
	t = curchr
	if (t == 31) && (curgroup != 16) {
		{
			if curgroup == 15 {
				{
					scandelimiter(29988, false)
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(777)
					}
					printesc(877)
					{
						helpptr = 1
						helpline[0] = 1157
					}
					error_()
				}
			} else {
				offsave()
			}
		}
	} else {
		{
			p = newnoad
			mem[p].hh.b0 = t
			scandelimiter((p + 1), false)
			if t == 30 {
				{
					pushmath(16)
					mem[curlist.headfield].hh.rh = p
					curlist.tailfield = p
				}
			} else {
				{
					p = finmlist(p)
					unsave()
					{
						mem[curlist.tailfield].hh.rh = newnoad
						curlist.tailfield = mem[curlist.tailfield].hh.rh
					}
					mem[curlist.tailfield].hh.b0 = 23
					mem[(curlist.tailfield + 1)].hh.rh = 3
					mem[(curlist.tailfield + 1)].hh.lh = p
				}
			}
		}
	}
}

/* procedure: aftermath */
func aftermath() {
	var (
		l      bool
		danger bool
		m      int
		p      int
		a      int
		b      int
		w      int
		z      int
		e      int
		q      int
		d      int
		s      int
		g1     int
		g2     int
		r      int
		t      int
	)
	danger = false
	if ((fontparams[eqtb[3937].hh.rh] < 22) || (fontparams[eqtb[3953].hh.rh] < 22)) || (fontparams[eqtb[3969].hh.rh] < 22) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1158)
			}
			{
				helpptr = 3
				helpline[2] = 1159
				helpline[1] = 1160
				helpline[0] = 1161
			}
			error_()
			flushmath()
			danger = true
		}
	} else {
		if ((fontparams[eqtb[3938].hh.rh] < 13) || (fontparams[eqtb[3954].hh.rh] < 13)) || (fontparams[eqtb[3970].hh.rh] < 13) {
			{
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(1162)
				}
				{
					helpptr = 3
					helpline[2] = 1163
					helpline[1] = 1164
					helpline[0] = 1165
				}
				error_()
				flushmath()
				danger = true
			}
		}
	}
	m = curlist.modefield
	l = false
	p = finmlist(0)
	if curlist.modefield == (-m) {
		{
			{
				getxtoken()
				if curcmd != 3 {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(1166)
						}
						{
							helpptr = 2
							helpline[1] = 1167
							helpline[0] = 1168
						}
						backerror()
					}
				}
			}
			curmlist = p
			curstyle = 2
			mlistpenalties = false
			mlisttohlist()
			a = hpack(mem[29997].hh.rh, 0, 1)
			unsave()
			saveptr = (saveptr - 1)
			if savestack[(saveptr+0)].int == 1 {
				l = true
			}
			danger = false
			if ((fontparams[eqtb[3937].hh.rh] < 22) || (fontparams[eqtb[3953].hh.rh] < 22)) || (fontparams[eqtb[3969].hh.rh] < 22) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1158)
					}
					{
						helpptr = 3
						helpline[2] = 1159
						helpline[1] = 1160
						helpline[0] = 1161
					}
					error_()
					flushmath()
					danger = true
				}
			} else {
				if ((fontparams[eqtb[3938].hh.rh] < 13) || (fontparams[eqtb[3954].hh.rh] < 13)) || (fontparams[eqtb[3970].hh.rh] < 13) {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(1162)
						}
						{
							helpptr = 3
							helpline[2] = 1163
							helpline[1] = 1164
							helpline[0] = 1165
						}
						error_()
						flushmath()
						danger = true
					}
				}
			}
			m = curlist.modefield
			p = finmlist(0)
		}
	} else {
		a = 0
	}
	if m < 0 {
		{
			{
				mem[curlist.tailfield].hh.rh = newmath(eqtb[5831].int, 0)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			curmlist = p
			curstyle = 2
			mlistpenalties = (curlist.modefield > 0)
			mlisttohlist()
			mem[curlist.tailfield].hh.rh = mem[29997].hh.rh
			for mem[curlist.tailfield].hh.rh != 0 {
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			{
				mem[curlist.tailfield].hh.rh = newmath(eqtb[5831].int, 1)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			curlist.auxfield.hh.lh = 1000
			unsave()
		}
	} else {
		{
			if a == 0 {
				{
					getxtoken()
					if curcmd != 3 {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(1166)
							}
							{
								helpptr = 2
								helpline[1] = 1167
								helpline[0] = 1168
							}
							backerror()
						}
					}
				}
			}
			curmlist = p
			curstyle = 0
			mlistpenalties = false
			mlisttohlist()
			p = mem[29997].hh.rh
			adjusttail = 29995
			b = hpack(p, 0, 1)
			p = mem[(b + 5)].hh.rh
			t = adjusttail
			adjusttail = 0
			w = mem[(b + 1)].int
			z = eqtb[5844].int
			s = eqtb[5845].int
			if (a == 0) || danger {
				{
					e = 0
					q = 0
				}
			} else {
				{
					e = mem[(a + 1)].int
					q = (e + fontinfo[(6+parambase[eqtb[3937].hh.rh])].int)
				}
			}
			if (w + q) > z {
				{
					if (e != 0) && ((((((w - totalshrink[0]) + q) <= z) || (totalshrink[1] != 0)) || (totalshrink[2] != 0)) || (totalshrink[3] != 0)) {
						{
							freenode(b, 7)
							b = hpack(p, (z - q), 0)
						}
					} else {
						{
							e = 0
							if w > z {
								{
									freenode(b, 7)
									b = hpack(p, z, 0)
								}
							}
						}
					}
					w = mem[(b + 1)].int
				}
			}
			d = half((z - w))
			if (e > 0) && (d < (2 * e)) {
				{
					d = half(((z - w) - e))
					if p != 0 {
						if !(p >= himemmin) {
							if mem[p].hh.b0 == 10 {
								d = 0
							}
						}
					}
				}
			}
			{
				mem[curlist.tailfield].hh.rh = newpenalty(eqtb[5274].int)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			if ((d + s) <= eqtb[5843].int) || l {
				{
					g1 = 3
					g2 = 4
				}
			} else {
				{
					g1 = 5
					g2 = 6
				}
			}
			if l && (e == 0) {
				{
					mem[(a + 4)].int = s
					appendtovlist(a)
					{
						mem[curlist.tailfield].hh.rh = newpenalty(10000)
						curlist.tailfield = mem[curlist.tailfield].hh.rh
					}
				}
			} else {
				{
					mem[curlist.tailfield].hh.rh = newparamglue(g1)
					curlist.tailfield = mem[curlist.tailfield].hh.rh
				}
			}
			if e != 0 {
				{
					r = newkern((((z - w) - e) - d))
					if l {
						{
							mem[a].hh.rh = r
							mem[r].hh.rh = b
							b = a
							d = 0
						}
					} else {
						{
							mem[b].hh.rh = r
							mem[r].hh.rh = a
						}
					}
					b = hpack(b, 0, 1)
				}
			}
			mem[(b + 4)].int = (s + d)
			appendtovlist(b)
			if ((a != 0) && (e == 0)) && (!l) {
				{
					{
						mem[curlist.tailfield].hh.rh = newpenalty(10000)
						curlist.tailfield = mem[curlist.tailfield].hh.rh
					}
					mem[(a + 4)].int = ((s + z) - mem[(a+1)].int)
					appendtovlist(a)
					g2 = 0
				}
			}
			if t != 29995 {
				{
					mem[curlist.tailfield].hh.rh = mem[29995].hh.rh
					curlist.tailfield = t
				}
			}
			{
				mem[curlist.tailfield].hh.rh = newpenalty(eqtb[5275].int)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			if g2 > 0 {
				{
					mem[curlist.tailfield].hh.rh = newparamglue(g2)
					curlist.tailfield = mem[curlist.tailfield].hh.rh
				}
			}
			resumeafterdisplay()
		}
	}
}

/* procedure: resumeafterdisplay */
func resumeafterdisplay() {
	if curgroup != 15 {
		confusion(1169)
	}
	unsave()
	curlist.pgfield = (curlist.pgfield + 3)
	pushnest()
	curlist.modefield = 102
	curlist.auxfield.hh.lh = 1000
	if eqtb[5313].int <= 0 {
		curlang = 0
	} else {
		if eqtb[5313].int > 255 {
			curlang = 0
		} else {
			curlang = eqtb[5313].int
		}
	}
	curlist.auxfield.hh.rh = curlang
	curlist.pgfield = ((((normmin(eqtb[5314].int) * 64) + normmin(eqtb[5315].int)) * 65536) + curlang)
	{
		getxtoken()
		if curcmd != 10 {
			backinput()
		}
	}
	if nestptr == 1 {
		buildpage()
	}
}

/* procedure: getrtoken */
func getrtoken() {
L20:
	for {
		gettoken()
		if !(curtok != 2592) {
			break
		}
	}
	if (curcs == 0) || (curcs > 2614) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1184)
			}
			{
				helpptr = 5
				helpline[4] = 1185
				helpline[3] = 1186
				helpline[2] = 1187
				helpline[1] = 1188
				helpline[0] = 1189
			}
			if curcs == 0 {
				backinput()
			}
			curtok = 6709
			inserror()
			goto L20
		}
	}
}

/* procedure: trapzeroglue */
func trapzeroglue() {
	if ((mem[(curval+1)].int == 0) && (mem[(curval+2)].int == 0)) && (mem[(curval+3)].int == 0) {
		{
			mem[0].hh.rh = (mem[0].hh.rh + 1)
			deleteglueref(curval)
			curval = 0
		}
	}
}

/* procedure: doregistercommand */
func doregistercommand(a int) {
	var (
		l int
		q int
		r int
		s int
		p int
	)
	q = curcmd
	{
		if q != 89 {
			{
				getxtoken()
				if (curcmd >= 73) && (curcmd <= 76) {
					{
						l = curchr
						p = (curcmd - 73)
						goto L40
					}
				}
				if curcmd != 89 {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(685)
						}
						printcmdchr(curcmd, curchr)
						print_(686)
						printcmdchr(q, 0)
						{
							helpptr = 1
							helpline[0] = 1210
						}
						error_()
						goto L10
					}
				}
			}
		}
		p = curchr
		scaneightbitint()
		switch p {
		case 0:
			l = (curval + 5318)
		case 1:
			l = (curval + 5851)
		case 2:
			l = (curval + 2900)
		case 3:
			l = (curval + 3156)
		}
	}
L40:
	// empty
	if q == 89 {
		scanoptionalequals()
	} else {
		if scankeyword(1206) {
			// empty
		}
	}
	aritherror = false
	if q < 91 {
		if p < 2 {
			{
				if p == 0 {
					scanint()
				} else {
					scandimen(false, false, false)
				}
				if q == 90 {
					curval = (curval + eqtb[l].int)
				}
			}
		} else {
			{
				scanglue(p)
				if q == 90 {
					{
						q = newspec(curval)
						r = eqtb[l].hh.rh
						deleteglueref(curval)
						mem[(q + 1)].int = (mem[(q+1)].int + mem[(r+1)].int)
						if mem[(q+2)].int == 0 {
							mem[q].hh.b0 = 0
						}
						if mem[q].hh.b0 == mem[r].hh.b0 {
							mem[(q + 2)].int = (mem[(q+2)].int + mem[(r+2)].int)
						} else {
							if (mem[q].hh.b0 < mem[r].hh.b0) && (mem[(r+2)].int != 0) {
								{
									mem[(q + 2)].int = mem[(r + 2)].int
									mem[q].hh.b0 = mem[r].hh.b0
								}
							}
						}
						if mem[(q+3)].int == 0 {
							mem[q].hh.b1 = 0
						}
						if mem[q].hh.b1 == mem[r].hh.b1 {
							mem[(q + 3)].int = (mem[(q+3)].int + mem[(r+3)].int)
						} else {
							if (mem[q].hh.b1 < mem[r].hh.b1) && (mem[(r+3)].int != 0) {
								{
									mem[(q + 3)].int = mem[(r + 3)].int
									mem[q].hh.b1 = mem[r].hh.b1
								}
							}
						}
						curval = q
					}
				}
			}
		}
	} else {
		{
			scanint()
			if p < 2 {
				if q == 91 {
					if p == 0 {
						curval = multandadd(eqtb[l].int, curval, 0, 2147483647)
					} else {
						curval = multandadd(eqtb[l].int, curval, 0, 1073741823)
					}
				} else {
					curval = xovern(eqtb[l].int, curval)
				}
			} else {
				{
					s = eqtb[l].hh.rh
					r = newspec(s)
					if q == 91 {
						{
							mem[(r + 1)].int = multandadd(mem[(s+1)].int, curval, 0, 1073741823)
							mem[(r + 2)].int = multandadd(mem[(s+2)].int, curval, 0, 1073741823)
							mem[(r + 3)].int = multandadd(mem[(s+3)].int, curval, 0, 1073741823)
						}
					} else {
						{
							mem[(r + 1)].int = xovern(mem[(s+1)].int, curval)
							mem[(r + 2)].int = xovern(mem[(s+2)].int, curval)
							mem[(r + 3)].int = xovern(mem[(s+3)].int, curval)
						}
					}
					curval = r
				}
			}
		}
	}
	if aritherror {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1207)
			}
			{
				helpptr = 2
				helpline[1] = 1208
				helpline[0] = 1209
			}
			if p >= 2 {
				deleteglueref(curval)
			}
			error_()
			goto L10
		}
	}
	if p < 2 {
		if a >= 4 {
			geqworddefine(l, curval)
		} else {
			eqworddefine(l, curval)
		}
	} else {
		{
			trapzeroglue()
			if a >= 4 {
				geqdefine(l, 117, curval)
			} else {
				eqdefine(l, 117, curval)
			}
		}
	}
L10:
	// empty
}

/* procedure: alteraux */
func alteraux() {
	var (
		c int
	)
	if curchr != abs_(curlist.modefield) {
		reportillegalcase()
	} else {
		{
			c = curchr
			scanoptionalequals()
			if c == 1 {
				{
					scandimen(false, false, false)
					curlist.auxfield.int = curval
				}
			} else {
				{
					scanint()
					if (curval <= 0) || (curval > 32767) {
						{
							{
								if interaction == 3 {
									// empty
								}
								printnl(262)
								print_(1213)
							}
							{
								helpptr = 1
								helpline[0] = 1214
							}
							interror(curval)
						}
					} else {
						curlist.auxfield.hh.lh = curval
					}
				}
			}
		}
	}
}

/* procedure: alterprevgraf */
func alterprevgraf() {
	var (
		p int
	)
	nest[nestptr] = curlist
	p = nestptr
	for abs_(nest[p].modefield) != 1 {
		p = (p - 1)
	}
	scanoptionalequals()
	scanint()
	if curval < 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(955)
			}
			printesc(532)
			{
				helpptr = 1
				helpline[0] = 1215
			}
			interror(curval)
		}
	} else {
		{
			nest[p].pgfield = curval
			curlist = nest[nestptr]
		}
	}
}

/* procedure: alterpagesofar */
func alterpagesofar() {
	var (
		c int
	)
	c = curchr
	scanoptionalequals()
	scandimen(false, false, false)
	pagesofar[c] = curval
}

/* procedure: alterinteger */
func alterinteger() {
	var (
		c int
	)
	c = curchr
	scanoptionalequals()
	scanint()
	if c == 0 {
		deadcycles = curval
	} else {
		insertpenalties = curval
	}
}

/* procedure: alterboxdimen */
func alterboxdimen() {
	var (
		c int
		b byte
	)
	c = curchr
	scaneightbitint()
	b = curval
	scanoptionalequals()
	scandimen(false, false, false)
	if eqtb[(3678+b)].hh.rh != 0 {
		mem[(eqtb[(3678+b)].hh.rh + c)].int = curval
	}
}

/* procedure: newfont */
func newfont(a int) {
	var (
		u               int
		s               int
		f               int
		t               int
		oldsetting      int
		flushablestring int
	)
	if jobname == 0 {
		openlogfile()
	}
	getrtoken()
	u = curcs
	if u >= 514 {
		t = hash[u].rh
	} else {
		if u >= 257 {
			if u == 513 {
				t = 1219
			} else {
				t = (u - 257)
			}
		} else {
			{
				oldsetting = selector
				selector = 21
				print_(1219)
				print_((u - 1))
				selector = oldsetting
				{
					if (poolptr + 1) > poolsize {
						overflow(257, (poolsize - initpoolptr))
					}
				}
				t = makestring
			}
		}
	}
	if a >= 4 {
		geqdefine(u, 87, 0)
	} else {
		eqdefine(u, 87, 0)
	}
	scanoptionalequals()
	scanfilename()
	nameinprogress = true
	if scankeyword(1220) {
		{
			scandimen(false, false, false)
			s = curval
			if (s <= 0) || (s >= 134217728) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1222)
					}
					printscaled(s)
					print_(1223)
					{
						helpptr = 2
						helpline[1] = 1224
						helpline[0] = 1225
					}
					error_()
					s = (10 * 65536)
				}
			}
		}
	} else {
		if scankeyword(1221) {
			{
				scanint()
				s = (-curval)
				if (curval <= 0) || (curval > 32768) {
					{
						{
							if interaction == 3 {
								// empty
							}
							printnl(262)
							print_(552)
						}
						{
							helpptr = 1
							helpline[0] = 553
						}
						interror(curval)
						s = (-1000)
					}
				}
			}
		} else {
			s = (-1000)
		}
	}
	nameinprogress = false
	flushablestring = (strptr - 1)
	for f := 1; f <= fontptr; f++ {
		if streqstr(fontname[f], curname) && streqstr(fontarea[f], curarea) {
			{
				if curname == flushablestring {
					{
						{
							strptr = (strptr - 1)
							poolptr = strstart[strptr]
						}
						curname = fontname[f]
					}
				}
				if s > 0 {
					{
						if s == fontsize[f] {
							goto L50
						}
					}
				} else {
					if fontsize[f] == xnoverd(fontdsize[f], (-s), 1000) {
						goto L50
					}
				}
			}
		}
	}
	f = readfontinfo(u, curname, curarea, s)
L50:
	eqtb[u].hh.rh = f
	eqtb[(2624 + f)] = eqtb[u]
	hash[(2624 + f)].rh = t
}

/* procedure: newinteraction */
func newinteraction() {
	println_()
	interaction = curchr
	if interaction == 0 {
		selector = 16
	} else {
		selector = 17
	}
	if logopened {
		selector = (selector + 2)
	}
}

/* procedure: prefixedcommand */
func prefixedcommand() {
	var (
		a int
		f int
		j int
		k int
		p int
		q int
		n int
		e bool
	)
	a = 0
	for curcmd == 93 {
		{
			if !(((a / curchr) & 1) != 0) {
				a = (a + curchr)
			}
			for {
				getxtoken()
				if !((curcmd != 10) && (curcmd != 0)) {
					break
				}
			}
			if curcmd <= 70 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1179)
					}
					printcmdchr(curcmd, curchr)
					printchar(39)
					{
						helpptr = 1
						helpline[0] = 1180
					}
					backerror()
					goto L10
				}
			}
		}
	}
	if (curcmd != 97) && ((a % 4) != 0) {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(685)
			}
			printesc(1171)
			print_(1181)
			printesc(1172)
			print_(1182)
			printcmdchr(curcmd, curchr)
			printchar(39)
			{
				helpptr = 1
				helpline[0] = 1183
			}
			error_()
		}
	}
	if eqtb[5306].int != 0 {
		if eqtb[5306].int < 0 {
			{
				if a >= 4 {
					a = (a - 4)
				}
			}
		} else {
			{
				if !(a >= 4) {
					a = (a + 4)
				}
			}
		}
	}
	switch curcmd {
	case 87:
		if a >= 4 {
			geqdefine(3934, 120, curchr)
		} else {
			eqdefine(3934, 120, curchr)
		}
	case 97:
		{
			if (((curchr & 1) != 0) && (!(a >= 4))) && (eqtb[5306].int >= 0) {
				a = (a + 4)
			}
			e = (curchr >= 2)
			getrtoken()
			p = curcs
			q = scantoks(true, e)
			if a >= 4 {
				geqdefine(p, (111 + (a % 4)), defref)
			} else {
				eqdefine(p, (111 + (a % 4)), defref)
			}
		}
	case 94:
		{
			n = curchr
			getrtoken()
			p = curcs
			if n == 0 {
				{
					for {
						gettoken()
						if !(curcmd != 10) {
							break
						}
					}
					if curtok == 3133 {
						{
							gettoken()
							if curcmd == 10 {
								gettoken()
							}
						}
					}
				}
			} else {
				{
					gettoken()
					q = curtok
					gettoken()
					backinput()
					curtok = q
					backinput()
				}
			}
			if curcmd >= 111 {
				mem[curchr].hh.lh = (mem[curchr].hh.lh + 1)
			}
			if a >= 4 {
				geqdefine(p, curcmd, curchr)
			} else {
				eqdefine(p, curcmd, curchr)
			}
		}
	case 95:
		{
			n = curchr
			getrtoken()
			p = curcs
			if a >= 4 {
				geqdefine(p, 0, 256)
			} else {
				eqdefine(p, 0, 256)
			}
			scanoptionalequals()
			switch n {
			case 0:
				{
					scancharnum()
					if a >= 4 {
						geqdefine(p, 68, curval)
					} else {
						eqdefine(p, 68, curval)
					}
				}
			case 1:
				{
					scanfifteenbitint()
					if a >= 4 {
						geqdefine(p, 69, curval)
					} else {
						eqdefine(p, 69, curval)
					}
				}
			default:
				{
					scaneightbitint()
					switch n {
					case 2:
						if a >= 4 {
							geqdefine(p, 73, (5318 + curval))
						} else {
							eqdefine(p, 73, (5318 + curval))
						}
					case 3:
						if a >= 4 {
							geqdefine(p, 74, (5851 + curval))
						} else {
							eqdefine(p, 74, (5851 + curval))
						}
					case 4:
						if a >= 4 {
							geqdefine(p, 75, (2900 + curval))
						} else {
							eqdefine(p, 75, (2900 + curval))
						}
					case 5:
						if a >= 4 {
							geqdefine(p, 76, (3156 + curval))
						} else {
							eqdefine(p, 76, (3156 + curval))
						}
					case 6:
						if a >= 4 {
							geqdefine(p, 72, (3422 + curval))
						} else {
							eqdefine(p, 72, (3422 + curval))
						}
					}
				}
			}
		}
	case 96:
		{
			scanint()
			n = curval
			if !scankeyword(842) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1073)
					}
					{
						helpptr = 2
						helpline[1] = 1200
						helpline[0] = 1201
					}
					error_()
				}
			}
			getrtoken()
			p = curcs
			readtoks(n, p)
			if a >= 4 {
				geqdefine(p, 111, curval)
			} else {
				eqdefine(p, 111, curval)
			}
		}
	case 71:
		{
			q = curcs
			if curcmd == 71 {
				{
					scaneightbitint()
					p = (3422 + curval)
				}
			} else {
				p = curchr
			}
			scanoptionalequals()
			for {
				getxtoken()
				if !((curcmd != 10) && (curcmd != 0)) {
					break
				}
			}
			if curcmd != 1 {
				{
					if curcmd == 71 {
						{
							scaneightbitint()
							curcmd = 72
							curchr = (3422 + curval)
						}
					}
					if curcmd == 72 {
						{
							q = eqtb[curchr].hh.rh
							if q == 0 {
								if a >= 4 {
									geqdefine(p, 101, 0)
								} else {
									eqdefine(p, 101, 0)
								}
							} else {
								{
									mem[q].hh.lh = (mem[q].hh.lh + 1)
									if a >= 4 {
										geqdefine(p, 111, q)
									} else {
										eqdefine(p, 111, q)
									}
								}
							}
							goto L30
						}
					}
				}
			}
			backinput()
			curcs = q
			q = scantoks(false, false)
			if mem[defref].hh.rh == 0 {
				{
					if a >= 4 {
						geqdefine(p, 101, 0)
					} else {
						eqdefine(p, 101, 0)
					}
					{
						mem[defref].hh.rh = avail
						avail = defref
					}
				}
			} else {
				{
					if p == 3413 {
						{
							mem[q].hh.rh = getavail
							q = mem[q].hh.rh
							mem[q].hh.lh = 637
							q = getavail
							mem[q].hh.lh = 379
							mem[q].hh.rh = mem[defref].hh.rh
							mem[defref].hh.rh = q
						}
					}
					if a >= 4 {
						geqdefine(p, 111, defref)
					} else {
						eqdefine(p, 111, defref)
					}
				}
			}
		}
	case 72:
		{
			q = curcs
			if curcmd == 71 {
				{
					scaneightbitint()
					p = (3422 + curval)
				}
			} else {
				p = curchr
			}
			scanoptionalequals()
			for {
				getxtoken()
				if !((curcmd != 10) && (curcmd != 0)) {
					break
				}
			}
			if curcmd != 1 {
				{
					if curcmd == 71 {
						{
							scaneightbitint()
							curcmd = 72
							curchr = (3422 + curval)
						}
					}
					if curcmd == 72 {
						{
							q = eqtb[curchr].hh.rh
							if q == 0 {
								if a >= 4 {
									geqdefine(p, 101, 0)
								} else {
									eqdefine(p, 101, 0)
								}
							} else {
								{
									mem[q].hh.lh = (mem[q].hh.lh + 1)
									if a >= 4 {
										geqdefine(p, 111, q)
									} else {
										eqdefine(p, 111, q)
									}
								}
							}
							goto L30
						}
					}
				}
			}
			backinput()
			curcs = q
			q = scantoks(false, false)
			if mem[defref].hh.rh == 0 {
				{
					if a >= 4 {
						geqdefine(p, 101, 0)
					} else {
						eqdefine(p, 101, 0)
					}
					{
						mem[defref].hh.rh = avail
						avail = defref
					}
				}
			} else {
				{
					if p == 3413 {
						{
							mem[q].hh.rh = getavail
							q = mem[q].hh.rh
							mem[q].hh.lh = 637
							q = getavail
							mem[q].hh.lh = 379
							mem[q].hh.rh = mem[defref].hh.rh
							mem[defref].hh.rh = q
						}
					}
					if a >= 4 {
						geqdefine(p, 111, defref)
					} else {
						eqdefine(p, 111, defref)
					}
				}
			}
		}
	case 73:
		{
			p = curchr
			scanoptionalequals()
			scanint()
			if a >= 4 {
				geqworddefine(p, curval)
			} else {
				eqworddefine(p, curval)
			}
		}
	case 74:
		{
			p = curchr
			scanoptionalequals()
			scandimen(false, false, false)
			if a >= 4 {
				geqworddefine(p, curval)
			} else {
				eqworddefine(p, curval)
			}
		}
	case 75:
		{
			p = curchr
			n = curcmd
			scanoptionalequals()
			if n == 76 {
				scanglue(3)
			} else {
				scanglue(2)
			}
			trapzeroglue()
			if a >= 4 {
				geqdefine(p, 117, curval)
			} else {
				eqdefine(p, 117, curval)
			}
		}
	case 76:
		{
			p = curchr
			n = curcmd
			scanoptionalequals()
			if n == 76 {
				scanglue(3)
			} else {
				scanglue(2)
			}
			trapzeroglue()
			if a >= 4 {
				geqdefine(p, 117, curval)
			} else {
				eqdefine(p, 117, curval)
			}
		}
	case 85:
		{
			if curchr == 3983 {
				n = 15
			} else {
				if curchr == 5007 {
					n = 32768
				} else {
					if curchr == 4751 {
						n = 32767
					} else {
						if curchr == 5574 {
							n = 16777215
						} else {
							n = 255
						}
					}
				}
			}
			p = curchr
			scancharnum()
			p = (p + curval)
			scanoptionalequals()
			scanint()
			if ((curval < 0) && (p < 5574)) || (curval > n) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1202)
					}
					printint(curval)
					if p < 5574 {
						print_(1203)
					} else {
						print_(1204)
					}
					printint(n)
					{
						helpptr = 1
						helpline[0] = 1205
					}
					error_()
					curval = 0
				}
			}
			if p < 5007 {
				if a >= 4 {
					geqdefine(p, 120, curval)
				} else {
					eqdefine(p, 120, curval)
				}
			} else {
				if p < 5574 {
					if a >= 4 {
						geqdefine(p, 120, (curval + 0))
					} else {
						eqdefine(p, 120, (curval + 0))
					}
				} else {
					if a >= 4 {
						geqworddefine(p, curval)
					} else {
						eqworddefine(p, curval)
					}
				}
			}
		}
	case 86:
		{
			p = curchr
			scanfourbitint()
			p = (p + curval)
			scanoptionalequals()
			scanfontident()
			if a >= 4 {
				geqdefine(p, 120, curval)
			} else {
				eqdefine(p, 120, curval)
			}
		}
	case 89:
		doregistercommand(a)
	case 90:
		doregistercommand(a)
	case 91:
		doregistercommand(a)
	case 92:
		doregistercommand(a)
	case 98:
		{
			scaneightbitint()
			if a >= 4 {
				n = (256 + curval)
			} else {
				n = curval
			}
			scanoptionalequals()
			if setboxallowed {
				scanbox((1073741824 + n))
			} else {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(680)
					}
					printesc(536)
					{
						helpptr = 2
						helpline[1] = 1211
						helpline[0] = 1212
					}
					error_()
				}
			}
		}
	case 79:
		alteraux()
	case 80:
		alterprevgraf()
	case 81:
		alterpagesofar()
	case 82:
		alterinteger()
	case 83:
		alterboxdimen()
	case 84:
		{
			scanoptionalequals()
			scanint()
			n = curval
			if n <= 0 {
				p = 0
			} else {
				{
					p = getnode(((2 * n) + 1))
					mem[p].hh.lh = n
					for j := 1; j <= n; j++ {
						{
							scandimen(false, false, false)
							mem[((p + (2 * j)) - 1)].int = curval
							scandimen(false, false, false)
							mem[(p + (2 * j))].int = curval
						}
					}
				}
			}
			if a >= 4 {
				geqdefine(3412, 118, p)
			} else {
				eqdefine(3412, 118, p)
			}
		}
	case 99:
		if curchr == 1 {
			{
				newpatterns()
				goto L30
				{
					if interaction == 3 {
						// empty
					}
					printnl(262)
					print_(1216)
				}
				helpptr = 0
				error_()
				for {
					gettoken()
					if !(curcmd == 2) {
						break
					}
				}
				goto L10
			}
		} else {
			{
				newhyphexceptions()
				goto L30
			}
		}
	case 77:
		{
			findfontdimen(true)
			k = curval
			scanoptionalequals()
			scandimen(false, false, false)
			fontinfo[k].int = curval
		}
	case 78:
		{
			n = curchr
			scanfontident()
			f = curval
			scanoptionalequals()
			scanint()
			if n == 0 {
				hyphenchar[f] = curval
			} else {
				skewchar[f] = curval
			}
		}
	case 88:
		newfont(a)
	case 100:
		newinteraction()
	default:
		confusion(1178)
	}
L30:
	if aftertoken != 0 {
		{
			curtok = aftertoken
			backinput()
			aftertoken = 0
		}
	}
L10:
	// empty
}

/* procedure: doassignments */
func doassignments() {
	for true {
		{
			for {
				getxtoken()
				if !((curcmd != 10) && (curcmd != 0)) {
					break
				}
			}
			if curcmd <= 70 {
				goto L10
			}
			setboxallowed = false
			prefixedcommand()
			setboxallowed = true
		}
	}
L10:
	// empty
}

/* procedure: openorclosein */
func openorclosein() {
	var (
		c int
		n int
	)
	c = curchr
	scanfourbitint()
	n = curval
	if readopen[n] != 2 {
		{
			aclose(readfile[n])
			readopen[n] = 2
		}
	}
	if c != 0 {
		{
			scanoptionalequals()
			scanfilename()
			if curext == 338 {
				curext = 791
			}
			packfilename(curname, curarea, curext)
			if aopenin(readfile[n]) {
				readopen[n] = 1
			}
		}
	}
}

/* procedure: issuemessage */
func issuemessage() {
	var (
		oldsetting int
		c          int
		s          int
	)
	c = curchr
	mem[29988].hh.rh = scantoks(false, true)
	oldsetting = selector
	selector = 21
	tokenshow(defref)
	selector = oldsetting
	flushlist(defref)
	{
		if (poolptr + 1) > poolsize {
			overflow(257, (poolsize - initpoolptr))
		}
	}
	s = makestring
	if c == 0 {
		{
			if (termoffset + (strstart[(s+1)] - strstart[s])) > (maxprintline - 2) {
				println_()
			} else {
				if (termoffset > 0) || (fileoffset > 0) {
					printchar(32)
				}
			}
			slowprint(s)
			break_(termout)
		}
	} else {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(338)
			}
			slowprint(s)
			if eqtb[3421].hh.rh != 0 {
				useerrhelp = true
			} else {
				if longhelpseen {
					{
						helpptr = 1
						helpline[0] = 1232
					}
				} else {
					{
						if interaction < 3 {
							longhelpseen = true
						}
						{
							helpptr = 4
							helpline[3] = 1233
							helpline[2] = 1234
							helpline[1] = 1235
							helpline[0] = 1236
						}
					}
				}
			}
			error_()
			useerrhelp = false
		}
	}
	{
		strptr = (strptr - 1)
		poolptr = strstart[strptr]
	}
}

/* procedure: shiftcase */
func shiftcase() {
	var (
		b int
		p int
		t int
		c byte
	)
	b = curchr
	p = scantoks(false, false)
	p = mem[defref].hh.rh
	for p != 0 {
		{
			t = mem[p].hh.lh
			if t < 4352 {
				{
					c = (t % 256)
					if eqtb[(b+c)].hh.rh != 0 {
						mem[p].hh.lh = ((t - c) + eqtb[(b+c)].hh.rh)
					}
				}
			}
			p = mem[p].hh.rh
		}
	}
	begintokenlist(mem[defref].hh.rh, 3)
	{
		mem[defref].hh.rh = avail
		avail = defref
	}
}

/* procedure: showwhatever */
func showwhatever() {
	var (
		p int
	)
	switch curchr {
	case 3:
		{
			begindiagnostic()
			showactivities()
		}
	case 1:
		{
			scaneightbitint()
			begindiagnostic()
			printnl(1254)
			printint(curval)
			printchar(61)
			if eqtb[(3678+curval)].hh.rh == 0 {
				print_(410)
			} else {
				showbox(eqtb[(3678 + curval)].hh.rh)
			}
		}
	case 0:
		{
			gettoken()
			if interaction == 3 {
				// empty
			}
			printnl(1248)
			if curcs != 0 {
				{
					sprintcs(curcs)
					printchar(61)
				}
			}
			printmeaning()
			goto L50
		}
	default:
		{
			p = thetoks
			if interaction == 3 {
				// empty
			}
			printnl(1248)
			tokenshow(29997)
			flushlist(mem[29997].hh.rh)
			goto L50
		}
	}
	enddiagnostic(true)
	{
		if interaction == 3 {
			// empty
		}
		printnl(262)
		print_(1255)
	}
	if selector == 19 {
		if eqtb[5292].int <= 0 {
			{
				selector = 17
				print_(1256)
				selector = 19
			}
		}
	}
L50:
	if interaction < 3 {
		{
			helpptr = 0
			errorcount = (errorcount - 1)
		}
	} else {
		if eqtb[5292].int > 0 {
			{
				{
					helpptr = 3
					helpline[2] = 1243
					helpline[1] = 1244
					helpline[0] = 1245
				}
			}
		} else {
			{
				{
					helpptr = 5
					helpline[4] = 1243
					helpline[3] = 1244
					helpline[2] = 1245
					helpline[1] = 1246
					helpline[0] = 1247
				}
			}
		}
	}
	error_()
}

/* procedure: storefmtfile */
func storefmtfile() {
	var (
		j int
		k int
		l int
		p int
		q int
		x int
		w *fourquarters_t
	)
	if saveptr != 0 {
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1258)
			}
			{
				helpptr = 1
				helpline[0] = 1259
			}
			{
				if interaction == 3 {
					interaction = 2
				}
				if logopened {
					error_()
				}
				history = 3
				jumpout()
			}
		}
	}
	selector = 21
	print_(1272)
	print_(jobname)
	printchar(32)
	printint(eqtb[5286].int)
	printchar(46)
	printint(eqtb[5285].int)
	printchar(46)
	printint(eqtb[5284].int)
	printchar(41)
	if interaction == 0 {
		selector = 18
	} else {
		selector = 19
	}
	{
		if (poolptr + 1) > poolsize {
			overflow(257, (poolsize - initpoolptr))
		}
	}
	formatident = makestring
	packjobname(786)
	for !wopenout(fmtfile) {
		promptfilename(1273, 786)
	}
	printnl(1274)
	slowprint(wmakenamestring(fmtfile))
	{
		strptr = (strptr - 1)
		poolptr = strstart[strptr]
	}
	printnl(338)
	slowprint(formatident)
	{
		*fmtfile.int = 504454778
		put_(fmtfile)
	}
	{
		*fmtfile.int = 0
		put_(fmtfile)
	}
	{
		*fmtfile.int = 30000
		put_(fmtfile)
	}
	{
		*fmtfile.int = 6106
		put_(fmtfile)
	}
	{
		*fmtfile.int = 1777
		put_(fmtfile)
	}
	{
		*fmtfile.int = 307
		put_(fmtfile)
	}
	{
		*fmtfile.int = poolptr
		put_(fmtfile)
	}
	{
		*fmtfile.int = strptr
		put_(fmtfile)
	}
	for k := 0; k <= strptr; k++ {
		{
			*fmtfile.int = strstart[k]
			put_(fmtfile)
		}
	}
	k = 0
	for (k + 4) < poolptr {
		{
			w.b0 = (strpool[k] + 0)
			w.b1 = (strpool[(k+1)] + 0)
			w.b2 = (strpool[(k+2)] + 0)
			w.b3 = (strpool[(k+3)] + 0)
			{
				*fmtfile.qqqq = w
				put_(fmtfile)
			}
			k = (k + 4)
		}
	}
	k = (poolptr - 4)
	w.b0 = (strpool[k] + 0)
	w.b1 = (strpool[(k+1)] + 0)
	w.b2 = (strpool[(k+2)] + 0)
	w.b3 = (strpool[(k+3)] + 0)
	{
		*fmtfile.qqqq = w
		put_(fmtfile)
	}
	println_()
	printint(strptr)
	print_(1260)
	printint(poolptr)
	sortavail()
	varused = 0
	{
		*fmtfile.int = lomemmax
		put_(fmtfile)
	}
	{
		*fmtfile.int = rover
		put_(fmtfile)
	}
	p = 0
	q = rover
	x = 0
	for {
		for k := p; k <= (q + 1); k++ {
			{
				*fmtfile = mem[k]
				put_(fmtfile)
			}
		}
		x = (((x + q) + 2) - p)
		varused = ((varused + q) - p)
		p = (q + mem[q].hh.lh)
		q = mem[(q + 1)].hh.rh
		if !(q == rover) {
			break
		}
	}
	varused = ((varused + lomemmax) - p)
	dynused = ((memend + 1) - himemmin)
	for k := p; k <= lomemmax; k++ {
		{
			*fmtfile = mem[k]
			put_(fmtfile)
		}
	}
	x = (((x + lomemmax) + 1) - p)
	{
		*fmtfile.int = himemmin
		put_(fmtfile)
	}
	{
		*fmtfile.int = avail
		put_(fmtfile)
	}
	for k := himemmin; k <= memend; k++ {
		{
			*fmtfile = mem[k]
			put_(fmtfile)
		}
	}
	x = (((x + memend) + 1) - himemmin)
	p = avail
	for p != 0 {
		{
			dynused = (dynused - 1)
			p = mem[p].hh.rh
		}
	}
	{
		*fmtfile.int = varused
		put_(fmtfile)
	}
	{
		*fmtfile.int = dynused
		put_(fmtfile)
	}
	println_()
	printint(x)
	print_(1261)
	printint(varused)
	printchar(38)
	printint(dynused)
	k = 1
	for {
		j = k
		for j < 5262 {
			{
				if ((eqtb[j].hh.rh == eqtb[(j+1)].hh.rh) && (eqtb[j].hh.b0 == eqtb[(j+1)].hh.b0)) && (eqtb[j].hh.b1 == eqtb[(j+1)].hh.b1) {
					goto L41
				}
				j = (j + 1)
			}
		}
		l = 5263
		goto L31
	L41:
		j = (j + 1)
		l = j
		for j < 5262 {
			{
				if ((eqtb[j].hh.rh != eqtb[(j+1)].hh.rh) || (eqtb[j].hh.b0 != eqtb[(j+1)].hh.b0)) || (eqtb[j].hh.b1 != eqtb[(j+1)].hh.b1) {
					goto L31
				}
				j = (j + 1)
			}
		}
	L31:
		{
			*fmtfile.int = (l - k)
			put_(fmtfile)
		}
		for k < l {
			{
				{
					*fmtfile = eqtb[k]
					put_(fmtfile)
				}
				k = (k + 1)
			}
		}
		k = (j + 1)
		{
			*fmtfile.int = (k - l)
			put_(fmtfile)
		}
		if !(k == 5263) {
			break
		}
	}
	for {
		j = k
		for j < 6106 {
			{
				if eqtb[j].int == eqtb[(j+1)].int {
					goto L42
				}
				j = (j + 1)
			}
		}
		l = 6107
		goto L32
	L42:
		j = (j + 1)
		l = j
		for j < 6106 {
			{
				if eqtb[j].int != eqtb[(j+1)].int {
					goto L32
				}
				j = (j + 1)
			}
		}
	L32:
		{
			*fmtfile.int = (l - k)
			put_(fmtfile)
		}
		for k < l {
			{
				{
					*fmtfile = eqtb[k]
					put_(fmtfile)
				}
				k = (k + 1)
			}
		}
		k = (j + 1)
		{
			*fmtfile.int = (k - l)
			put_(fmtfile)
		}
		if !(k > 6106) {
			break
		}
	}
	{
		*fmtfile.int = parloc
		put_(fmtfile)
	}
	{
		*fmtfile.int = writeloc
		put_(fmtfile)
	}
	{
		*fmtfile.int = hashused
		put_(fmtfile)
	}
	cscount = (2613 - hashused)
	for p := 514; p <= hashused; p++ {
		if hash[p].rh != 0 {
			{
				{
					*fmtfile.int = p
					put_(fmtfile)
				}
				{
					*fmtfile.hh = hash[p]
					put_(fmtfile)
				}
				cscount = (cscount + 1)
			}
		}
	}
	for p := (hashused + 1); p <= 2880; p++ {
		{
			*fmtfile.hh = hash[p]
			put_(fmtfile)
		}
	}
	{
		*fmtfile.int = cscount
		put_(fmtfile)
	}
	println_()
	printint(cscount)
	print_(1262)
	{
		*fmtfile.int = fmemptr
		put_(fmtfile)
	}
	for k := 0; k <= (fmemptr - 1); k++ {
		{
			*fmtfile = fontinfo[k]
			put_(fmtfile)
		}
	}
	{
		*fmtfile.int = fontptr
		put_(fmtfile)
	}
	for k := 0; k <= fontptr; k++ {
		{
			{
				*fmtfile.qqqq = fontcheck[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontsize[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontdsize[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontparams[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = hyphenchar[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = skewchar[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontname[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontarea[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontbc[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontec[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = charbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = widthbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = heightbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = depthbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = italicbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = ligkernbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = kernbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = extenbase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = parambase[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontglue[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = bcharlabel[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontbchar[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = fontfalsebchar[k]
				put_(fmtfile)
			}
			printnl(1265)
			printesc(hash[(2624 + k)].rh)
			printchar(61)
			printfilename(fontname[k], fontarea[k], 338)
			if fontsize[k] != fontdsize[k] {
				{
					print_(741)
					printscaled(fontsize[k])
					print_(397)
				}
			}
		}
	}
	println_()
	printint((fmemptr - 7))
	print_(1263)
	printint((fontptr - 0))
	print_(1264)
	if fontptr != 1 {
		printchar(115)
	}
	{
		*fmtfile.int = hyphcount
		put_(fmtfile)
	}
	for k := 0; k <= 307; k++ {
		if hyphword[k] != 0 {
			{
				{
					*fmtfile.int = k
					put_(fmtfile)
				}
				{
					*fmtfile.int = hyphword[k]
					put_(fmtfile)
				}
				{
					*fmtfile.int = hyphlist[k]
					put_(fmtfile)
				}
			}
		}
	}
	println_()
	printint(hyphcount)
	print_(1266)
	if hyphcount != 1 {
		printchar(115)
	}
	if trienotready {
		inittrie()
	}
	{
		*fmtfile.int = triemax
		put_(fmtfile)
	}
	for k := 0; k <= triemax; k++ {
		{
			*fmtfile.hh = trie[k]
			put_(fmtfile)
		}
	}
	{
		*fmtfile.int = trieopptr
		put_(fmtfile)
	}
	for k := 1; k <= trieopptr; k++ {
		{
			{
				*fmtfile.int = hyfdistance[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = hyfnum[k]
				put_(fmtfile)
			}
			{
				*fmtfile.int = hyfnext[k]
				put_(fmtfile)
			}
		}
	}
	printnl(1267)
	printint(triemax)
	print_(1268)
	printint(trieopptr)
	print_(1269)
	if trieopptr != 1 {
		printchar(115)
	}
	print_(1270)
	printint(trieopsize)
	for k := 255; k >= 0; k-- {
		if trieused[k] > 0 {
			{
				printnl(800)
				printint((trieused[k] - 0))
				print_(1271)
				printint(k)
				{
					*fmtfile.int = k
					put_(fmtfile)
				}
				{
					*fmtfile.int = (trieused[k] - 0)
					put_(fmtfile)
				}
			}
		}
	}
	{
		*fmtfile.int = interaction
		put_(fmtfile)
	}
	{
		*fmtfile.int = formatident
		put_(fmtfile)
	}
	{
		*fmtfile.int = 69069
		put_(fmtfile)
	}
	eqtb[5294].int = 0
	wclose(fmtfile)
}

/* procedure: newwhatsit */
func newwhatsit(s int, w int) {
	var (
		p int
	)
	p = getnode(w)
	mem[p].hh.b0 = 8
	mem[p].hh.b1 = s
	mem[curlist.tailfield].hh.rh = p
	curlist.tailfield = p
}

/* procedure: newwritewhatsit */
func newwritewhatsit(w int) {
	newwhatsit(curchr, w)
	if w != 2 {
		scanfourbitint()
	} else {
		{
			scanint()
			if curval < 0 {
				curval = 17
			} else {
				if curval > 15 {
					curval = 16
				}
			}
		}
	}
	mem[(curlist.tailfield + 1)].hh.lh = curval
}

/* procedure: doextension */
func doextension() {
	var (
		i int
		j int
		k int
		p int
		q int
		r int
	)
	switch curchr {
	case 0:
		{
			newwritewhatsit(3)
			scanoptionalequals()
			scanfilename()
			mem[(curlist.tailfield + 1)].hh.rh = curname
			mem[(curlist.tailfield + 2)].hh.lh = curarea
			mem[(curlist.tailfield + 2)].hh.rh = curext
		}
	case 1:
		{
			k = curcs
			newwritewhatsit(2)
			curcs = k
			p = scantoks(false, false)
			mem[(curlist.tailfield + 1)].hh.rh = defref
		}
	case 2:
		{
			newwritewhatsit(2)
			mem[(curlist.tailfield + 1)].hh.rh = 0
		}
	case 3:
		{
			newwhatsit(3, 2)
			mem[(curlist.tailfield + 1)].hh.lh = 0
			p = scantoks(false, true)
			mem[(curlist.tailfield + 1)].hh.rh = defref
		}
	case 4:
		{
			getxtoken()
			if (curcmd == 59) && (curchr <= 2) {
				{
					p = curlist.tailfield
					doextension()
					outwhat(curlist.tailfield)
					flushnodelist(curlist.tailfield)
					curlist.tailfield = p
					mem[p].hh.rh = 0
				}
			} else {
				backinput()
			}
		}
	case 5:
		if abs_(curlist.modefield) != 102 {
			reportillegalcase()
		} else {
			{
				newwhatsit(4, 2)
				scanint()
				if curval <= 0 {
					curlist.auxfield.hh.rh = 0
				} else {
					if curval > 255 {
						curlist.auxfield.hh.rh = 0
					} else {
						curlist.auxfield.hh.rh = curval
					}
				}
				mem[(curlist.tailfield + 1)].hh.rh = curlist.auxfield.hh.rh
				mem[(curlist.tailfield + 1)].hh.b0 = normmin(eqtb[5314].int)
				mem[(curlist.tailfield + 1)].hh.b1 = normmin(eqtb[5315].int)
			}
		}
	default:
		confusion(1291)
	}
}

/* procedure: fixlanguage */
func fixlanguage() {
	var (
		l byte
	)
	if eqtb[5313].int <= 0 {
		l = 0
	} else {
		if eqtb[5313].int > 255 {
			l = 0
		} else {
			l = eqtb[5313].int
		}
	}
	if l != curlist.auxfield.hh.rh {
		{
			newwhatsit(4, 2)
			mem[(curlist.tailfield + 1)].hh.rh = l
			curlist.auxfield.hh.rh = l
			mem[(curlist.tailfield + 1)].hh.b0 = normmin(eqtb[5314].int)
			mem[(curlist.tailfield + 1)].hh.b1 = normmin(eqtb[5315].int)
		}
	}
}

/* procedure: handlerightbrace */
func handlerightbrace() {
	var (
		p int
		q int
		d int
		f int
	)
	switch curgroup {
	case 1:
		unsave()
	case 0:
		{
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(1044)
			}
			{
				helpptr = 2
				helpline[1] = 1045
				helpline[0] = 1046
			}
			error_()
		}
	case 14:
		extrarightbrace()
	case 15:
		extrarightbrace()
	case 16:
		extrarightbrace()
	case 2:
		package_(0)
	case 3:
		{
			adjusttail = 29995
			package_(0)
		}
	case 4:
		{
			endgraf()
			package_(0)
		}
	case 5:
		{
			endgraf()
			package_(4)
		}
	case 11:
		{
			endgraf()
			q = eqtb[2892].hh.rh
			mem[q].hh.rh = (mem[q].hh.rh + 1)
			d = eqtb[5836].int
			f = eqtb[5305].int
			unsave()
			saveptr = (saveptr - 1)
			p = vpackage(mem[curlist.headfield].hh.rh, 0, 1, 1073741823)
			popnest()
			if savestack[(saveptr+0)].int < 255 {
				{
					{
						mem[curlist.tailfield].hh.rh = getnode(5)
						curlist.tailfield = mem[curlist.tailfield].hh.rh
					}
					mem[curlist.tailfield].hh.b0 = 3
					mem[curlist.tailfield].hh.b1 = (savestack[(saveptr+0)].int + 0)
					mem[(curlist.tailfield + 3)].int = (mem[(p+3)].int + mem[(p+2)].int)
					mem[(curlist.tailfield + 4)].hh.lh = mem[(p + 5)].hh.rh
					mem[(curlist.tailfield + 4)].hh.rh = q
					mem[(curlist.tailfield + 2)].int = d
					mem[(curlist.tailfield + 1)].int = f
				}
			} else {
				{
					{
						mem[curlist.tailfield].hh.rh = getnode(2)
						curlist.tailfield = mem[curlist.tailfield].hh.rh
					}
					mem[curlist.tailfield].hh.b0 = 5
					mem[curlist.tailfield].hh.b1 = 0
					mem[(curlist.tailfield + 1)].int = mem[(p + 5)].hh.rh
					deleteglueref(q)
				}
			}
			freenode(p, 7)
			if nestptr == 0 {
				buildpage()
			}
		}
	case 8:
		{
			if (curinput.locfield != 0) || ((curinput.indexfield != 6) && (curinput.indexfield != 3)) {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1010)
					}
					{
						helpptr = 2
						helpline[1] = 1011
						helpline[0] = 1012
					}
					error_()
					for {
						gettoken()
						if !(curinput.locfield == 0) {
							break
						}
					}
				}
			}
			endtokenlist()
			endgraf()
			unsave()
			outputactive = false
			insertpenalties = 0
			if eqtb[3933].hh.rh != 0 {
				{
					{
						if interaction == 3 {
							// empty
						}
						printnl(262)
						print_(1013)
					}
					printesc(409)
					printint(255)
					{
						helpptr = 3
						helpline[2] = 1014
						helpline[1] = 1015
						helpline[0] = 1016
					}
					boxerror(255)
				}
			}
			if curlist.tailfield != curlist.headfield {
				{
					mem[pagetail].hh.rh = mem[curlist.headfield].hh.rh
					pagetail = curlist.tailfield
				}
			}
			if mem[29998].hh.rh != 0 {
				{
					if mem[29999].hh.rh == 0 {
						nest[0].tailfield = pagetail
					}
					mem[pagetail].hh.rh = mem[29999].hh.rh
					mem[29999].hh.rh = mem[29998].hh.rh
					mem[29998].hh.rh = 0
					pagetail = 29998
				}
			}
			popnest()
			buildpage()
		}
	case 10:
		builddiscretionary()
	case 6:
		{
			backinput()
			curtok = 6710
			{
				if interaction == 3 {
					// empty
				}
				printnl(262)
				print_(625)
			}
			printesc(899)
			print_(626)
			{
				helpptr = 1
				helpline[0] = 1125
			}
			inserror()
		}
	case 7:
		{
			endgraf()
			unsave()
			alignpeek()
		}
	case 12:
		{
			endgraf()
			unsave()
			saveptr = (saveptr - 2)
			p = vpackage(mem[curlist.headfield].hh.rh, savestack[(saveptr+1)].int, savestack[(saveptr+0)].int, 1073741823)
			popnest()
			{
				mem[curlist.tailfield].hh.rh = newnoad
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.b0 = 29
			mem[(curlist.tailfield + 1)].hh.rh = 2
			mem[(curlist.tailfield + 1)].hh.lh = p
		}
	case 13:
		buildchoices()
	case 9:
		{
			unsave()
			saveptr = (saveptr - 1)
			mem[savestack[(saveptr+0)].int].hh.rh = 3
			p = finmlist(0)
			mem[savestack[(saveptr+0)].int].hh.lh = p
			if p != 0 {
				if mem[p].hh.rh == 0 {
					if mem[p].hh.b0 == 16 {
						{
							if mem[(p+3)].hh.rh == 0 {
								if mem[(p+2)].hh.rh == 0 {
									{
										mem[savestack[(saveptr+0)].int].hh = mem[(p + 1)].hh
										freenode(p, 4)
									}
								}
							}
						}
					} else {
						if mem[p].hh.b0 == 28 {
							if savestack[(saveptr+0)].int == (curlist.tailfield + 1) {
								if mem[curlist.tailfield].hh.b0 == 16 {
									{
										q = curlist.headfield
										for mem[q].hh.rh != curlist.tailfield {
											q = mem[q].hh.rh
										}
										mem[q].hh.rh = p
										freenode(curlist.tailfield, 4)
										curlist.tailfield = p
									}
								}
							}
						}
					}
				}
			}
		}
	default:
		confusion(1047)
	}
}

/* procedure: maincontrol */
func maincontrol() {
	var (
		t int
	)
	if eqtb[3419].hh.rh != 0 {
		begintokenlist(eqtb[3419].hh.rh, 12)
	}
L60:
	getxtoken()
L21:
	if interrupt != 0 {
		if OKtointerrupt {
			{
				backinput()
				{
					if interrupt != 0 {
						pauseforinstructions()
					}
				}
				goto L60
			}
		}
	}
	if eqtb[5299].int > 0 {
		showcurcmdchr()
	}
	switch abs_(curlist.modefield) + curcmd {
	case 113:
		goto L70
	case 114:
		goto L70
	case 170:
		goto L70
	case 118:
		{
			scancharnum()
			curchr = curval
			goto L70
		}
	case 167:
		{
			getxtoken()
			if (((curcmd == 11) || (curcmd == 12)) || (curcmd == 68)) || (curcmd == 16) {
				cancelboundary = true
			}
			goto L21
		}
	case 112:
		if curlist.auxfield.hh.lh == 1000 {
			goto L120
		} else {
			appspace()
		}
	case 166:
		goto L120
	case 267:
		goto L120
	case 1:
		// empty
	case 102:
		// empty
	case 203:
		// empty
	case 11:
		// empty
	case 213:
		// empty
	case 268:
		// empty
	case 40:
		{
			for {
				getxtoken()
				if !(curcmd != 10) {
					break
				}
			}
			goto L21
		}
	case 141:
		{
			for {
				getxtoken()
				if !(curcmd != 10) {
					break
				}
			}
			goto L21
		}
	case 242:
		{
			for {
				getxtoken()
				if !(curcmd != 10) {
					break
				}
			}
			goto L21
		}
	case 15:
		if itsallover {
			goto L10
		}
	case 23:
		reportillegalcase()
	case 123:
		reportillegalcase()
	case 224:
		reportillegalcase()
	case 71:
		reportillegalcase()
	case 172:
		reportillegalcase()
	case 273:
		reportillegalcase()
	case 39:
		reportillegalcase()
	case 45:
		reportillegalcase()
	case 49:
		reportillegalcase()
	case 150:
		reportillegalcase()
	case 7:
		reportillegalcase()
	case 108:
		reportillegalcase()
	case 209:
		reportillegalcase()
	case 8:
		insertdollarsign()
	case 109:
		insertdollarsign()
	case 9:
		insertdollarsign()
	case 110:
		insertdollarsign()
	case 18:
		insertdollarsign()
	case 119:
		insertdollarsign()
	case 70:
		insertdollarsign()
	case 171:
		insertdollarsign()
	case 51:
		insertdollarsign()
	case 152:
		insertdollarsign()
	case 16:
		insertdollarsign()
	case 117:
		insertdollarsign()
	case 50:
		insertdollarsign()
	case 151:
		insertdollarsign()
	case 53:
		insertdollarsign()
	case 154:
		insertdollarsign()
	case 67:
		insertdollarsign()
	case 168:
		insertdollarsign()
	case 54:
		insertdollarsign()
	case 155:
		insertdollarsign()
	case 55:
		insertdollarsign()
	case 156:
		insertdollarsign()
	case 57:
		insertdollarsign()
	case 158:
		insertdollarsign()
	case 56:
		insertdollarsign()
	case 157:
		insertdollarsign()
	case 31:
		insertdollarsign()
	case 132:
		insertdollarsign()
	case 52:
		insertdollarsign()
	case 153:
		insertdollarsign()
	case 29:
		insertdollarsign()
	case 130:
		insertdollarsign()
	case 47:
		insertdollarsign()
	case 148:
		insertdollarsign()
	case 212:
		insertdollarsign()
	case 216:
		insertdollarsign()
	case 217:
		insertdollarsign()
	case 230:
		insertdollarsign()
	case 227:
		insertdollarsign()
	case 236:
		insertdollarsign()
	case 239:
		insertdollarsign()
	case 37:
		{
			{
				mem[curlist.tailfield].hh.rh = scanrulespec
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			if abs_(curlist.modefield) == 1 {
				curlist.auxfield.int = (-65536000)
			} else {
				if abs_(curlist.modefield) == 102 {
					curlist.auxfield.hh.lh = 1000
				}
			}
		}
	case 137:
		{
			{
				mem[curlist.tailfield].hh.rh = scanrulespec
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			if abs_(curlist.modefield) == 1 {
				curlist.auxfield.int = (-65536000)
			} else {
				if abs_(curlist.modefield) == 102 {
					curlist.auxfield.hh.lh = 1000
				}
			}
		}
	case 238:
		{
			{
				mem[curlist.tailfield].hh.rh = scanrulespec
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			if abs_(curlist.modefield) == 1 {
				curlist.auxfield.int = (-65536000)
			} else {
				if abs_(curlist.modefield) == 102 {
					curlist.auxfield.hh.lh = 1000
				}
			}
		}
	case 28:
		appendglue()
	case 128:
		appendglue()
	case 229:
		appendglue()
	case 231:
		appendglue()
	case 30:
		appendkern()
	case 131:
		appendkern()
	case 232:
		appendkern()
	case 233:
		appendkern()
	case 2:
		newsavelevel(1)
	case 103:
		newsavelevel(1)
	case 62:
		newsavelevel(14)
	case 163:
		newsavelevel(14)
	case 264:
		newsavelevel(14)
	case 63:
		if curgroup == 14 {
			unsave()
		} else {
			offsave()
		}
	case 164:
		if curgroup == 14 {
			unsave()
		} else {
			offsave()
		}
	case 265:
		if curgroup == 14 {
			unsave()
		} else {
			offsave()
		}
	case 3:
		handlerightbrace()
	case 104:
		handlerightbrace()
	case 205:
		handlerightbrace()
	case 22:
		{
			t = curchr
			scandimen(false, false, false)
			if t == 0 {
				scanbox(curval)
			} else {
				scanbox((-curval))
			}
		}
	case 124:
		{
			t = curchr
			scandimen(false, false, false)
			if t == 0 {
				scanbox(curval)
			} else {
				scanbox((-curval))
			}
		}
	case 225:
		{
			t = curchr
			scandimen(false, false, false)
			if t == 0 {
				scanbox(curval)
			} else {
				scanbox((-curval))
			}
		}
	case 32:
		scanbox((1073742237 + curchr))
	case 133:
		scanbox((1073742237 + curchr))
	case 234:
		scanbox((1073742237 + curchr))
	case 21:
		beginbox(0)
	case 122:
		beginbox(0)
	case 223:
		beginbox(0)
	case 44:
		newgraf((curchr > 0))
	case 12:
		{
			backinput()
			newgraf(true)
		}
	case 13:
		{
			backinput()
			newgraf(true)
		}
	case 17:
		{
			backinput()
			newgraf(true)
		}
	case 69:
		{
			backinput()
			newgraf(true)
		}
	case 4:
		{
			backinput()
			newgraf(true)
		}
	case 24:
		{
			backinput()
			newgraf(true)
		}
	case 36:
		{
			backinput()
			newgraf(true)
		}
	case 46:
		{
			backinput()
			newgraf(true)
		}
	case 48:
		{
			backinput()
			newgraf(true)
		}
	case 27:
		{
			backinput()
			newgraf(true)
		}
	case 34:
		{
			backinput()
			newgraf(true)
		}
	case 65:
		{
			backinput()
			newgraf(true)
		}
	case 66:
		{
			backinput()
			newgraf(true)
		}
	case 145:
		indentinhmode()
	case 246:
		indentinhmode()
	case 14:
		{
			normalparagraph()
			if curlist.modefield > 0 {
				buildpage()
			}
		}
	case 115:
		{
			if alignstate < 0 {
				offsave()
			}
			endgraf()
			if curlist.modefield == 1 {
				buildpage()
			}
		}
	case 116:
		headforvmode()
	case 129:
		headforvmode()
	case 138:
		headforvmode()
	case 126:
		headforvmode()
	case 134:
		headforvmode()
	case 38:
		begininsertoradjust()
	case 139:
		begininsertoradjust()
	case 240:
		begininsertoradjust()
	case 140:
		begininsertoradjust()
	case 241:
		begininsertoradjust()
	case 19:
		makemark()
	case 120:
		makemark()
	case 221:
		makemark()
	case 43:
		appendpenalty()
	case 144:
		appendpenalty()
	case 245:
		appendpenalty()
	case 26:
		deletelast()
	case 127:
		deletelast()
	case 228:
		deletelast()
	case 25:
		unpackage()
	case 125:
		unpackage()
	case 226:
		unpackage()
	case 146:
		appenditaliccorrection()
	case 247:
		{
			mem[curlist.tailfield].hh.rh = newkern(0)
			curlist.tailfield = mem[curlist.tailfield].hh.rh
		}
	case 149:
		appenddiscretionary()
	case 250:
		appenddiscretionary()
	case 147:
		makeaccent()
	case 6:
		alignerror()
	case 107:
		alignerror()
	case 208:
		alignerror()
	case 5:
		alignerror()
	case 106:
		alignerror()
	case 207:
		alignerror()
	case 35:
		noalignerror()
	case 136:
		noalignerror()
	case 237:
		noalignerror()
	case 64:
		omiterror()
	case 165:
		omiterror()
	case 266:
		omiterror()
	case 33:
		initalign()
	case 135:
		initalign()
	case 235:
		if privileged {
			if curgroup == 15 {
				initalign()
			} else {
				offsave()
			}
		}
	case 10:
		doendv()
	case 111:
		doendv()
	case 68:
		cserror()
	case 169:
		cserror()
	case 270:
		cserror()
	case 105:
		initmath()
	case 251:
		if privileged {
			if curgroup == 15 {
				starteqno()
			} else {
				offsave()
			}
		}
	case 204:
		{
			{
				mem[curlist.tailfield].hh.rh = newnoad
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			backinput()
			scanmath((curlist.tailfield + 1))
		}
	case 214:
		setmathchar((eqtb[(5007+curchr)].hh.rh - 0))
	case 215:
		setmathchar((eqtb[(5007+curchr)].hh.rh - 0))
	case 271:
		setmathchar((eqtb[(5007+curchr)].hh.rh - 0))
	case 219:
		{
			scancharnum()
			curchr = curval
			setmathchar((eqtb[(5007+curchr)].hh.rh - 0))
		}
	case 220:
		{
			scanfifteenbitint()
			setmathchar(curval)
		}
	case 272:
		setmathchar(curchr)
	case 218:
		{
			scantwentysevenbitint()
			setmathchar((curval / 4096))
		}
	case 253:
		{
			{
				mem[curlist.tailfield].hh.rh = newnoad
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.b0 = curchr
			scanmath((curlist.tailfield + 1))
		}
	case 254:
		mathlimitswitch()
	case 269:
		mathradical()
	case 248:
		mathac()
	case 249:
		mathac()
	case 259:
		{
			scanspec(12, false)
			normalparagraph()
			pushnest()
			curlist.modefield = (-1)
			curlist.auxfield.int = (-65536000)
			if eqtb[3418].hh.rh != 0 {
				begintokenlist(eqtb[3418].hh.rh, 11)
			}
		}
	case 256:
		{
			mem[curlist.tailfield].hh.rh = newstyle(curchr)
			curlist.tailfield = mem[curlist.tailfield].hh.rh
		}
	case 258:
		{
			{
				mem[curlist.tailfield].hh.rh = newglue(0)
				curlist.tailfield = mem[curlist.tailfield].hh.rh
			}
			mem[curlist.tailfield].hh.b1 = 98
		}
	case 257:
		appendchoices()
	case 211:
		subsup()
	case 210:
		subsup()
	case 255:
		mathfraction()
	case 252:
		mathleftright()
	case 206:
		if curgroup == 15 {
			aftermath()
		} else {
			offsave()
		}
	case 72:
		prefixedcommand()
	case 173:
		prefixedcommand()
	case 274:
		prefixedcommand()
	case 73:
		prefixedcommand()
	case 174:
		prefixedcommand()
	case 275:
		prefixedcommand()
	case 74:
		prefixedcommand()
	case 175:
		prefixedcommand()
	case 276:
		prefixedcommand()
	case 75:
		prefixedcommand()
	case 176:
		prefixedcommand()
	case 277:
		prefixedcommand()
	case 76:
		prefixedcommand()
	case 177:
		prefixedcommand()
	case 278:
		prefixedcommand()
	case 77:
		prefixedcommand()
	case 178:
		prefixedcommand()
	case 279:
		prefixedcommand()
	case 78:
		prefixedcommand()
	case 179:
		prefixedcommand()
	case 280:
		prefixedcommand()
	case 79:
		prefixedcommand()
	case 180:
		prefixedcommand()
	case 281:
		prefixedcommand()
	case 80:
		prefixedcommand()
	case 181:
		prefixedcommand()
	case 282:
		prefixedcommand()
	case 81:
		prefixedcommand()
	case 182:
		prefixedcommand()
	case 283:
		prefixedcommand()
	case 82:
		prefixedcommand()
	case 183:
		prefixedcommand()
	case 284:
		prefixedcommand()
	case 83:
		prefixedcommand()
	case 184:
		prefixedcommand()
	case 285:
		prefixedcommand()
	case 84:
		prefixedcommand()
	case 185:
		prefixedcommand()
	case 286:
		prefixedcommand()
	case 85:
		prefixedcommand()
	case 186:
		prefixedcommand()
	case 287:
		prefixedcommand()
	case 86:
		prefixedcommand()
	case 187:
		prefixedcommand()
	case 288:
		prefixedcommand()
	case 87:
		prefixedcommand()
	case 188:
		prefixedcommand()
	case 289:
		prefixedcommand()
	case 88:
		prefixedcommand()
	case 189:
		prefixedcommand()
	case 290:
		prefixedcommand()
	case 89:
		prefixedcommand()
	case 190:
		prefixedcommand()
	case 291:
		prefixedcommand()
	case 90:
		prefixedcommand()
	case 191:
		prefixedcommand()
	case 292:
		prefixedcommand()
	case 91:
		prefixedcommand()
	case 192:
		prefixedcommand()
	case 293:
		prefixedcommand()
	case 92:
		prefixedcommand()
	case 193:
		prefixedcommand()
	case 294:
		prefixedcommand()
	case 93:
		prefixedcommand()
	case 194:
		prefixedcommand()
	case 295:
		prefixedcommand()
	case 94:
		prefixedcommand()
	case 195:
		prefixedcommand()
	case 296:
		prefixedcommand()
	case 95:
		prefixedcommand()
	case 196:
		prefixedcommand()
	case 297:
		prefixedcommand()
	case 96:
		prefixedcommand()
	case 197:
		prefixedcommand()
	case 298:
		prefixedcommand()
	case 97:
		prefixedcommand()
	case 198:
		prefixedcommand()
	case 299:
		prefixedcommand()
	case 98:
		prefixedcommand()
	case 199:
		prefixedcommand()
	case 300:
		prefixedcommand()
	case 99:
		prefixedcommand()
	case 200:
		prefixedcommand()
	case 301:
		prefixedcommand()
	case 100:
		prefixedcommand()
	case 201:
		prefixedcommand()
	case 302:
		prefixedcommand()
	case 101:
		prefixedcommand()
	case 202:
		prefixedcommand()
	case 303:
		prefixedcommand()
	case 41:
		{
			gettoken()
			aftertoken = curtok
		}
	case 142:
		{
			gettoken()
			aftertoken = curtok
		}
	case 243:
		{
			gettoken()
			aftertoken = curtok
		}
	case 42:
		{
			gettoken()
			saveforafter(curtok)
		}
	case 143:
		{
			gettoken()
			saveforafter(curtok)
		}
	case 244:
		{
			gettoken()
			saveforafter(curtok)
		}
	case 61:
		openorclosein()
	case 162:
		openorclosein()
	case 263:
		openorclosein()
	case 59:
		issuemessage()
	case 160:
		issuemessage()
	case 261:
		issuemessage()
	case 58:
		shiftcase()
	case 159:
		shiftcase()
	case 260:
		shiftcase()
	case 20:
		showwhatever()
	case 121:
		showwhatever()
	case 222:
		showwhatever()
	case 60:
		doextension()
	case 161:
		doextension()
	case 262:
		doextension()
	}
	goto L60
L70:
	mains = eqtb[(4751 + curchr)].hh.rh
	if mains == 1000 {
		curlist.auxfield.hh.lh = 1000
	} else {
		if mains < 1000 {
			{
				if mains > 0 {
					curlist.auxfield.hh.lh = mains
				}
			}
		} else {
			if curlist.auxfield.hh.lh < 1000 {
				curlist.auxfield.hh.lh = 1000
			} else {
				curlist.auxfield.hh.lh = mains
			}
		}
	}
	mainf = eqtb[3934].hh.rh
	bchar = fontbchar[mainf]
	falsebchar = fontfalsebchar[mainf]
	if curlist.modefield > 0 {
		if eqtb[5313].int != curlist.auxfield.hh.rh {
			fixlanguage()
		}
	}
	{
		ligstack = avail
		if ligstack == 0 {
			ligstack = getavail
		} else {
			{
				avail = mem[ligstack].hh.rh
				mem[ligstack].hh.rh = 0
			}
		}
	}
	mem[ligstack].hh.b0 = mainf
	curl = (curchr + 0)
	mem[ligstack].hh.b1 = curl
	curq = curlist.tailfield
	if cancelboundary {
		{
			cancelboundary = false
			maink = 0
		}
	} else {
		maink = bcharlabel[mainf]
	}
	if maink == 0 {
		goto L92
	}
	curr = curl
	curl = 256
	goto L111
L80:
	if curl < 256 {
		{
			if mem[curq].hh.rh > 0 {
				if mem[curlist.tailfield].hh.b1 == (hyphenchar[mainf] + 0) {
					insdisc = true
				}
			}
			if ligaturepresent {
				{
					mainp = newligature(mainf, curl, mem[curq].hh.rh)
					if lfthit {
						{
							mem[mainp].hh.b1 = 2
							lfthit = false
						}
					}
					if rthit {
						if ligstack == 0 {
							{
								mem[mainp].hh.b1 = (mem[mainp].hh.b1 + 1)
								rthit = false
							}
						}
					}
					mem[curq].hh.rh = mainp
					curlist.tailfield = mainp
					ligaturepresent = false
				}
			}
			if insdisc {
				{
					insdisc = false
					if curlist.modefield > 0 {
						{
							mem[curlist.tailfield].hh.rh = newdisc
							curlist.tailfield = mem[curlist.tailfield].hh.rh
						}
					}
				}
			}
		}
	}
L90:
	if ligstack == 0 {
		goto L21
	}
	curq = curlist.tailfield
	curl = mem[ligstack].hh.b1
L91:
	if !(ligstack >= himemmin) {
		goto L95
	}
L92:
	if (curchr < fontbc[mainf]) || (curchr > fontec[mainf]) {
		{
			charwarning(mainf, curchr)
			{
				mem[ligstack].hh.rh = avail
				avail = ligstack
			}
			goto L60
		}
	}
	maini = fontinfo[(charbase[mainf] + curl)].qqqq
	if !(maini.b0 > 0) {
		{
			charwarning(mainf, curchr)
			{
				mem[ligstack].hh.rh = avail
				avail = ligstack
			}
			goto L60
		}
	}
	mem[curlist.tailfield].hh.rh = ligstack
	curlist.tailfield = ligstack
L100:
	getnext()
	if curcmd == 11 {
		goto L101
	}
	if curcmd == 12 {
		goto L101
	}
	if curcmd == 68 {
		goto L101
	}
	xtoken()
	if curcmd == 11 {
		goto L101
	}
	if curcmd == 12 {
		goto L101
	}
	if curcmd == 68 {
		goto L101
	}
	if curcmd == 16 {
		{
			scancharnum()
			curchr = curval
			goto L101
		}
	}
	if curcmd == 65 {
		bchar = 256
	}
	curr = bchar
	ligstack = 0
	goto L110
L101:
	mains = eqtb[(4751 + curchr)].hh.rh
	if mains == 1000 {
		curlist.auxfield.hh.lh = 1000
	} else {
		if mains < 1000 {
			{
				if mains > 0 {
					curlist.auxfield.hh.lh = mains
				}
			}
		} else {
			if curlist.auxfield.hh.lh < 1000 {
				curlist.auxfield.hh.lh = 1000
			} else {
				curlist.auxfield.hh.lh = mains
			}
		}
	}
	{
		ligstack = avail
		if ligstack == 0 {
			ligstack = getavail
		} else {
			{
				avail = mem[ligstack].hh.rh
				mem[ligstack].hh.rh = 0
			}
		}
	}
	mem[ligstack].hh.b0 = mainf
	curr = (curchr + 0)
	mem[ligstack].hh.b1 = curr
	if curr == falsebchar {
		curr = 256
	}
L110:
	if ((maini.b2 - 0) % 4) != 1 {
		goto L80
	}
	if curr == 256 {
		goto L80
	}
	maink = (ligkernbase[mainf] + maini.b3)
	mainj = fontinfo[maink].qqqq
	if mainj.b0 <= 128 {
		goto L112
	}
	maink = ((((ligkernbase[mainf] + (256 * mainj.b2)) + mainj.b3) + 32768) - (256 * 128))
L111:
	mainj = fontinfo[maink].qqqq
L112:
	if mainj.b1 == curr {
		if mainj.b0 <= 128 {
			{
				if mainj.b2 >= 128 {
					{
						if curl < 256 {
							{
								if mem[curq].hh.rh > 0 {
									if mem[curlist.tailfield].hh.b1 == (hyphenchar[mainf] + 0) {
										insdisc = true
									}
								}
								if ligaturepresent {
									{
										mainp = newligature(mainf, curl, mem[curq].hh.rh)
										if lfthit {
											{
												mem[mainp].hh.b1 = 2
												lfthit = false
											}
										}
										if rthit {
											if ligstack == 0 {
												{
													mem[mainp].hh.b1 = (mem[mainp].hh.b1 + 1)
													rthit = false
												}
											}
										}
										mem[curq].hh.rh = mainp
										curlist.tailfield = mainp
										ligaturepresent = false
									}
								}
								if insdisc {
									{
										insdisc = false
										if curlist.modefield > 0 {
											{
												mem[curlist.tailfield].hh.rh = newdisc
												curlist.tailfield = mem[curlist.tailfield].hh.rh
											}
										}
									}
								}
							}
						}
						{
							mem[curlist.tailfield].hh.rh = newkern(fontinfo[((kernbase[mainf] + (256 * mainj.b2)) + mainj.b3)].int)
							curlist.tailfield = mem[curlist.tailfield].hh.rh
						}
						goto L90
					}
				}
				if curl == 256 {
					lfthit = true
				} else {
					if ligstack == 0 {
						rthit = true
					}
				}
				{
					if interrupt != 0 {
						pauseforinstructions()
					}
				}
				switch mainj.b2 {
				case 1:
					{
						curl = mainj.b3
						maini = fontinfo[(charbase[mainf] + curl)].qqqq
						ligaturepresent = true
					}
				case 5:
					{
						curl = mainj.b3
						maini = fontinfo[(charbase[mainf] + curl)].qqqq
						ligaturepresent = true
					}
				case 2:
					{
						curr = mainj.b3
						if ligstack == 0 {
							{
								ligstack = newligitem(curr)
								bchar = 256
							}
						} else {
							if ligstack >= himemmin {
								{
									mainp = ligstack
									ligstack = newligitem(curr)
									mem[(ligstack + 1)].hh.rh = mainp
								}
							} else {
								mem[ligstack].hh.b1 = curr
							}
						}
					}
				case 6:
					{
						curr = mainj.b3
						if ligstack == 0 {
							{
								ligstack = newligitem(curr)
								bchar = 256
							}
						} else {
							if ligstack >= himemmin {
								{
									mainp = ligstack
									ligstack = newligitem(curr)
									mem[(ligstack + 1)].hh.rh = mainp
								}
							} else {
								mem[ligstack].hh.b1 = curr
							}
						}
					}
				case 3:
					{
						curr = mainj.b3
						mainp = ligstack
						ligstack = newligitem(curr)
						mem[ligstack].hh.rh = mainp
					}
				case 7:
					{
						if curl < 256 {
							{
								if mem[curq].hh.rh > 0 {
									if mem[curlist.tailfield].hh.b1 == (hyphenchar[mainf] + 0) {
										insdisc = true
									}
								}
								if ligaturepresent {
									{
										mainp = newligature(mainf, curl, mem[curq].hh.rh)
										if lfthit {
											{
												mem[mainp].hh.b1 = 2
												lfthit = false
											}
										}
										if false {
											if ligstack == 0 {
												{
													mem[mainp].hh.b1 = (mem[mainp].hh.b1 + 1)
													rthit = false
												}
											}
										}
										mem[curq].hh.rh = mainp
										curlist.tailfield = mainp
										ligaturepresent = false
									}
								}
								if insdisc {
									{
										insdisc = false
										if curlist.modefield > 0 {
											{
												mem[curlist.tailfield].hh.rh = newdisc
												curlist.tailfield = mem[curlist.tailfield].hh.rh
											}
										}
									}
								}
							}
						}
						curq = curlist.tailfield
						curl = mainj.b3
						maini = fontinfo[(charbase[mainf] + curl)].qqqq
						ligaturepresent = true
					}
				case 11:
					{
						if curl < 256 {
							{
								if mem[curq].hh.rh > 0 {
									if mem[curlist.tailfield].hh.b1 == (hyphenchar[mainf] + 0) {
										insdisc = true
									}
								}
								if ligaturepresent {
									{
										mainp = newligature(mainf, curl, mem[curq].hh.rh)
										if lfthit {
											{
												mem[mainp].hh.b1 = 2
												lfthit = false
											}
										}
										if false {
											if ligstack == 0 {
												{
													mem[mainp].hh.b1 = (mem[mainp].hh.b1 + 1)
													rthit = false
												}
											}
										}
										mem[curq].hh.rh = mainp
										curlist.tailfield = mainp
										ligaturepresent = false
									}
								}
								if insdisc {
									{
										insdisc = false
										if curlist.modefield > 0 {
											{
												mem[curlist.tailfield].hh.rh = newdisc
												curlist.tailfield = mem[curlist.tailfield].hh.rh
											}
										}
									}
								}
							}
						}
						curq = curlist.tailfield
						curl = mainj.b3
						maini = fontinfo[(charbase[mainf] + curl)].qqqq
						ligaturepresent = true
					}
				default:
					{
						curl = mainj.b3
						ligaturepresent = true
						if ligstack == 0 {
							goto L80
						} else {
							goto L91
						}
					}
				}
				if mainj.b2 > 4 {
					if mainj.b2 != 7 {
						goto L80
					}
				}
				if curl < 256 {
					goto L110
				}
				maink = bcharlabel[mainf]
				goto L111
			}
		}
	}
	if mainj.b0 == 0 {
		maink = (maink + 1)
	} else {
		{
			if mainj.b0 >= 128 {
				goto L80
			}
			maink = ((maink + mainj.b0) + 1)
		}
	}
	goto L111
L95:
	mainp = mem[(ligstack + 1)].hh.rh
	if mainp > 0 {
		{
			mem[curlist.tailfield].hh.rh = mainp
			curlist.tailfield = mem[curlist.tailfield].hh.rh
		}
	}
	tempptr = ligstack
	ligstack = mem[tempptr].hh.rh
	freenode(tempptr, 2)
	maini = fontinfo[(charbase[mainf] + curl)].qqqq
	ligaturepresent = true
	if ligstack == 0 {
		if mainp > 0 {
			goto L100
		} else {
			curr = bchar
		}
	} else {
		curr = mem[ligstack].hh.b1
	}
	goto L110
L120:
	if eqtb[2894].hh.rh == 0 {
		{
			{
				mainp = fontglue[eqtb[3934].hh.rh]
				if mainp == 0 {
					{
						mainp = newspec(0)
						maink = (parambase[eqtb[3934].hh.rh] + 2)
						mem[(mainp + 1)].int = fontinfo[maink].int
						mem[(mainp + 2)].int = fontinfo[(maink + 1)].int
						mem[(mainp + 3)].int = fontinfo[(maink + 2)].int
						fontglue[eqtb[3934].hh.rh] = mainp
					}
				}
			}
			tempptr = newglue(mainp)
		}
	} else {
		tempptr = newparamglue(12)
	}
	mem[curlist.tailfield].hh.rh = tempptr
	curlist.tailfield = tempptr
	goto L60
L10:
	// empty
}

/* procedure: giveerrhelp */
func giveerrhelp() {
	tokenshow(eqtb[3421].hh.rh)
}

/* function: openfmtfile */
func openfmtfile() bool {
	var (
		j int
	)
	j = curinput.locfield
	if buffer[curinput.locfield] == 38 {
		{
			curinput.locfield = (curinput.locfield + 1)
			j = curinput.locfield
			buffer[last] = 32
			for buffer[j] != 32 {
				j = (j + 1)
			}
			packbufferedname(0, curinput.locfield, (j - 1))
			if wopenin(fmtfile) {
				goto L40
			}
			packbufferedname(11, curinput.locfield, (j - 1))
			if wopenin(fmtfile) {
				goto L40
			}
			writeln_(termout, "Sorry, I can't find that format;", " will try PLAIN.")
			break_(termout)
		}
	}
	packbufferedname(16, 1, 0)
	if !wopenin(fmtfile) {
		{
			writeln_(termout, "I can't find the PLAIN format file!")
			openfmtfile = false
			goto L10
		}
	}
L40:
	curinput.locfield = j
	openfmtfile = true
L10:
	// empty
}

/* function: loadfmtfile */
func loadfmtfile() bool {
	var (
		j int
		k int
		p int
		q int
		x int
		w *fourquarters_t
	)
	x = *fmtfile.int
	if x != 504454778 {
		goto L6666
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if x != 0 {
		goto L6666
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if x != 30000 {
		goto L6666
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if x != 6106 {
		goto L6666
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if x != 1777 {
		goto L6666
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if x != 307 {
		goto L6666
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 0 {
			goto L6666
		}
		if x > poolsize {
			{
				writeln_(termout, "---! Must increase the ", "string pool size")
				goto L6666
			}
		} else {
			poolptr = x
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 0 {
			goto L6666
		}
		if x > maxstrings {
			{
				writeln_(termout, "---! Must increase the ", "max strings")
				goto L6666
			}
		} else {
			strptr = x
		}
	}
	for k := 0; k <= strptr; k++ {
		{
			{
				get_(fmtfile)
				x = *fmtfile.int
			}
			if (x < 0) || (x > poolptr) {
				goto L6666
			} else {
				strstart[k] = x
			}
		}
	}
	k = 0
	for (k + 4) < poolptr {
		{
			{
				get_(fmtfile)
				w = *fmtfile.qqqq
			}
			strpool[k] = (w.b0 - 0)
			strpool[(k + 1)] = (w.b1 - 0)
			strpool[(k + 2)] = (w.b2 - 0)
			strpool[(k + 3)] = (w.b3 - 0)
			k = (k + 4)
		}
	}
	k = (poolptr - 4)
	{
		get_(fmtfile)
		w = *fmtfile.qqqq
	}
	strpool[k] = (w.b0 - 0)
	strpool[(k + 1)] = (w.b1 - 0)
	strpool[(k + 2)] = (w.b2 - 0)
	strpool[(k + 3)] = (w.b3 - 0)
	initstrptr = strptr
	initpoolptr = poolptr
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 1019) || (x > 29986) {
			goto L6666
		} else {
			lomemmax = x
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 20) || (x > lomemmax) {
			goto L6666
		} else {
			rover = x
		}
	}
	p = 0
	q = rover
	for {
		for k := p; k <= (q + 1); k++ {
			{
				get_(fmtfile)
				mem[k] = *fmtfile
			}
		}
		p = (q + mem[q].hh.lh)
		if (p > lomemmax) || ((q >= mem[(q+1)].hh.rh) && (mem[(q+1)].hh.rh != rover)) {
			goto L6666
		}
		q = mem[(q + 1)].hh.rh
		if !(q == rover) {
			break
		}
	}
	for k := p; k <= lomemmax; k++ {
		{
			get_(fmtfile)
			mem[k] = *fmtfile
		}
	}
	if memmin < (-2) {
		{
			p = mem[(rover + 1)].hh.lh
			q = (memmin + 1)
			mem[memmin].hh.rh = 0
			mem[memmin].hh.lh = 0
			mem[(p + 1)].hh.rh = q
			mem[(rover + 1)].hh.lh = q
			mem[(q + 1)].hh.rh = rover
			mem[(q + 1)].hh.lh = p
			mem[q].hh.rh = 65535
			mem[q].hh.lh = ((-0) - q)
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < (lomemmax + 1)) || (x > 29987) {
			goto L6666
		} else {
			himemmin = x
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 0) || (x > 30000) {
			goto L6666
		} else {
			avail = x
		}
	}
	memend = 30000
	for k := himemmin; k <= memend; k++ {
		{
			get_(fmtfile)
			mem[k] = *fmtfile
		}
	}
	{
		get_(fmtfile)
		varused = *fmtfile.int
	}
	{
		get_(fmtfile)
		dynused = *fmtfile.int
	}
	k = 1
	for {
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 1) || ((k + x) > 6107) {
			goto L6666
		}
		for j := k; j <= ((k + x) - 1); j++ {
			{
				get_(fmtfile)
				eqtb[j] = *fmtfile
			}
		}
		k = (k + x)
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 0) || ((k + x) > 6107) {
			goto L6666
		}
		for j := k; j <= ((k + x) - 1); j++ {
			eqtb[j] = eqtb[(k - 1)]
		}
		k = (k + x)
		if !(k > 6106) {
			break
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 514) || (x > 2614) {
			goto L6666
		} else {
			parloc = x
		}
	}
	partoken = (4095 + parloc)
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 514) || (x > 2614) {
			goto L6666
		} else {
			writeloc = x
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 514) || (x > 2614) {
			goto L6666
		} else {
			hashused = x
		}
	}
	p = 513
	for {
		{
			{
				get_(fmtfile)
				x = *fmtfile.int
			}
			if (x < (p + 1)) || (x > hashused) {
				goto L6666
			} else {
				p = x
			}
		}
		{
			get_(fmtfile)
			hash[p] = *fmtfile.hh
		}
		if !(p == hashused) {
			break
		}
	}
	for p := (hashused + 1); p <= 2880; p++ {
		{
			get_(fmtfile)
			hash[p] = *fmtfile.hh
		}
	}
	{
		get_(fmtfile)
		cscount = *fmtfile.int
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 7 {
			goto L6666
		}
		if x > fontmemsize {
			{
				writeln_(termout, "---! Must increase the ", "font mem size")
				goto L6666
			}
		} else {
			fmemptr = x
		}
	}
	for k := 0; k <= (fmemptr - 1); k++ {
		{
			get_(fmtfile)
			fontinfo[k] = *fmtfile
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 0 {
			goto L6666
		}
		if x > fontmax {
			{
				writeln_(termout, "---! Must increase the ", "font max")
				goto L6666
			}
		} else {
			fontptr = x
		}
	}
	for k := 0; k <= fontptr; k++ {
		{
			{
				get_(fmtfile)
				fontcheck[k] = *fmtfile.qqqq
			}
			{
				get_(fmtfile)
				fontsize[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				fontdsize[k] = *fmtfile.int
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 65535) {
					goto L6666
				} else {
					fontparams[k] = x
				}
			}
			{
				get_(fmtfile)
				hyphenchar[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				skewchar[k] = *fmtfile.int
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > strptr) {
					goto L6666
				} else {
					fontname[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > strptr) {
					goto L6666
				} else {
					fontarea[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 255) {
					goto L6666
				} else {
					fontbc[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 255) {
					goto L6666
				} else {
					fontec[k] = x
				}
			}
			{
				get_(fmtfile)
				charbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				widthbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				heightbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				depthbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				italicbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				ligkernbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				kernbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				extenbase[k] = *fmtfile.int
			}
			{
				get_(fmtfile)
				parambase[k] = *fmtfile.int
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > lomemmax) {
					goto L6666
				} else {
					fontglue[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > (fmemptr - 1)) {
					goto L6666
				} else {
					bcharlabel[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 256) {
					goto L6666
				} else {
					fontbchar[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 256) {
					goto L6666
				} else {
					fontfalsebchar[k] = x
				}
			}
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 0) || (x > 307) {
			goto L6666
		} else {
			hyphcount = x
		}
	}
	for k := 1; k <= hyphcount; k++ {
		{
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 307) {
					goto L6666
				} else {
					j = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > strptr) {
					goto L6666
				} else {
					hyphword[j] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 65535) {
					goto L6666
				} else {
					hyphlist[j] = x
				}
			}
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 0 {
			goto L6666
		}
		if x > triesize {
			{
				writeln_(termout, "---! Must increase the ", "trie size")
				goto L6666
			}
		} else {
			j = x
		}
	}
	triemax = j
	for k := 0; k <= j; k++ {
		{
			get_(fmtfile)
			trie[k] = *fmtfile.hh
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if x < 0 {
			goto L6666
		}
		if x > trieopsize {
			{
				writeln_(termout, "---! Must increase the ", "trie op size")
				goto L6666
			}
		} else {
			j = x
		}
	}
	trieopptr = j
	for k := 1; k <= j; k++ {
		{
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 63) {
					goto L6666
				} else {
					hyfdistance[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 63) {
					goto L6666
				} else {
					hyfnum[k] = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > 255) {
					goto L6666
				} else {
					hyfnext[k] = x
				}
			}
		}
	}
	for k := 0; k <= 255; k++ {
		trieused[k] = 0
	}
	k = 256
	for j > 0 {
		{
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 0) || (x > (k - 1)) {
					goto L6666
				} else {
					k = x
				}
			}
			{
				{
					get_(fmtfile)
					x = *fmtfile.int
				}
				if (x < 1) || (x > j) {
					goto L6666
				} else {
					x = x
				}
			}
			trieused[k] = (x + 0)
			j = (j - x)
			opstart[k] = (j - 0)
		}
	}
	trienotready = false
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 0) || (x > 3) {
			goto L6666
		} else {
			interaction = x
		}
	}
	{
		{
			get_(fmtfile)
			x = *fmtfile.int
		}
		if (x < 0) || (x > strptr) {
			goto L6666
		} else {
			formatident = x
		}
	}
	{
		get_(fmtfile)
		x = *fmtfile.int
	}
	if (x != 69069) || eof_(fmtfile) {
		goto L6666
	}
	loadfmtfile = true
	goto L10
L6666:
	// empty
	writeln_(termout, "(Fatal format file error; I'm stymied)")
	loadfmtfile = false
L10:
	// empty
}

/* procedure: closefilesandterminate */
func closefilesandterminate() {
	var (
		k int
	)
	for k := 0; k <= 15; k++ {
		if writeopen[k] {
			aclose(writefile[k])
		}
	}
	eqtb[5312].int = (-1)
	for curs > (-1) {
		{
			if curs > 0 {
				{
					dvibuf[dviptr] = 142
					dviptr = (dviptr + 1)
					if dviptr == dvilimit {
						dviswap()
					}
				}
			} else {
				{
					{
						dvibuf[dviptr] = 140
						dviptr = (dviptr + 1)
						if dviptr == dvilimit {
							dviswap()
						}
					}
					totalpages = (totalpages + 1)
				}
			}
			curs = (curs - 1)
		}
	}
	if totalpages == 0 {
		printnl(837)
	} else {
		{
			{
				dvibuf[dviptr] = 248
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			dvifour(lastbop)
			lastbop = ((dvioffset + dviptr) - 5)
			dvifour(25400000)
			dvifour(473628672)
			preparemag()
			dvifour(eqtb[5280].int)
			dvifour(maxv)
			dvifour(maxh)
			{
				dvibuf[dviptr] = (maxpush / 256)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			{
				dvibuf[dviptr] = (maxpush % 256)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			{
				dvibuf[dviptr] = ((totalpages / 256) % 256)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			{
				dvibuf[dviptr] = (totalpages % 256)
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			for fontptr > 0 {
				{
					if fontused[fontptr] {
						dvifontdef(fontptr)
					}
					fontptr = (fontptr - 1)
				}
			}
			{
				dvibuf[dviptr] = 249
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			dvifour(lastbop)
			{
				dvibuf[dviptr] = 2
				dviptr = (dviptr + 1)
				if dviptr == dvilimit {
					dviswap()
				}
			}
			k = (4 + ((dvibufsize - dviptr) % 4))
			for k > 0 {
				{
					{
						dvibuf[dviptr] = 223
						dviptr = (dviptr + 1)
						if dviptr == dvilimit {
							dviswap()
						}
					}
					k = (k - 1)
				}
			}
			if dvilimit == halfbuf {
				writedvi(halfbuf, (dvibufsize - 1))
			}
			if dviptr > 0 {
				writedvi(0, (dviptr - 1))
			}
			printnl(838)
			slowprint(outputfilename)
			print_(286)
			printint(totalpages)
			print_(839)
			if totalpages != 1 {
				printchar(115)
			}
			print_(840)
			printint((dvioffset + dviptr))
			print_(841)
			bclose(dvifile)
		}
	}
	if logopened {
		{
			writeln_(logfile)
			aclose(logfile)
			selector = (selector - 2)
			if selector == 17 {
				{
					printnl(1275)
					slowprint(logname)
					printchar(46)
				}
			}
		}
	}
}

/* procedure: finalcleanup */
func finalcleanup() {
	var (
		c int
	)
	c = curchr
	if c != 1 {
		eqtb[5312].int = (-1)
	}
	if jobname == 0 {
		openlogfile()
	}
	for inputptr > 0 {
		if curinput.statefield == 0 {
			endtokenlist()
		} else {
			endfilereading()
		}
	}
	for openparens > 0 {
		{
			print_(1276)
			openparens = (openparens - 1)
		}
	}
	if curlevel > 1 {
		{
			printnl(40)
			printesc(1277)
			print_(1278)
			printint((curlevel - 1))
			printchar(41)
		}
	}
	for condptr != 0 {
		{
			printnl(40)
			printesc(1277)
			print_(1279)
			printcmdchr(105, curif)
			if ifline != 0 {
				{
					print_(1280)
					printint(ifline)
				}
			}
			print_(1281)
			ifline = mem[(condptr + 1)].int
			curif = mem[condptr].hh.b1
			tempptr = condptr
			condptr = mem[condptr].hh.rh
			freenode(tempptr, 2)
		}
	}
	if history != 0 {
		if (history == 1) || (interaction < 3) {
			if selector == 19 {
				{
					selector = 17
					printnl(1282)
					selector = 19
				}
			}
		}
	}
	if c == 1 {
		{
			for c := 0; c <= 4; c++ {
				if curmark[c] != 0 {
					deletetokenref(curmark[c])
				}
			}
			if lastglue != 65535 {
				deleteglueref(lastglue)
			}
			storefmtfile()
			goto L10
			printnl(1283)
			goto L10
		}
	}
L10:
	// empty
}

/* procedure: initprim */
func initprim() {
	nonewcontrolsequence = false
	primitive(376, 75, 2882)
	primitive(377, 75, 2883)
	primitive(378, 75, 2884)
	primitive(379, 75, 2885)
	primitive(380, 75, 2886)
	primitive(381, 75, 2887)
	primitive(382, 75, 2888)
	primitive(383, 75, 2889)
	primitive(384, 75, 2890)
	primitive(385, 75, 2891)
	primitive(386, 75, 2892)
	primitive(387, 75, 2893)
	primitive(388, 75, 2894)
	primitive(389, 75, 2895)
	primitive(390, 75, 2896)
	primitive(391, 76, 2897)
	primitive(392, 76, 2898)
	primitive(393, 76, 2899)
	primitive(398, 72, 3413)
	primitive(399, 72, 3414)
	primitive(400, 72, 3415)
	primitive(401, 72, 3416)
	primitive(402, 72, 3417)
	primitive(403, 72, 3418)
	primitive(404, 72, 3419)
	primitive(405, 72, 3420)
	primitive(406, 72, 3421)
	primitive(420, 73, 5263)
	primitive(421, 73, 5264)
	primitive(422, 73, 5265)
	primitive(423, 73, 5266)
	primitive(424, 73, 5267)
	primitive(425, 73, 5268)
	primitive(426, 73, 5269)
	primitive(427, 73, 5270)
	primitive(428, 73, 5271)
	primitive(429, 73, 5272)
	primitive(430, 73, 5273)
	primitive(431, 73, 5274)
	primitive(432, 73, 5275)
	primitive(433, 73, 5276)
	primitive(434, 73, 5277)
	primitive(435, 73, 5278)
	primitive(436, 73, 5279)
	primitive(437, 73, 5280)
	primitive(438, 73, 5281)
	primitive(439, 73, 5282)
	primitive(440, 73, 5283)
	primitive(441, 73, 5284)
	primitive(442, 73, 5285)
	primitive(443, 73, 5286)
	primitive(444, 73, 5287)
	primitive(445, 73, 5288)
	primitive(446, 73, 5289)
	primitive(447, 73, 5290)
	primitive(448, 73, 5291)
	primitive(449, 73, 5292)
	primitive(450, 73, 5293)
	primitive(451, 73, 5294)
	primitive(452, 73, 5295)
	primitive(453, 73, 5296)
	primitive(454, 73, 5297)
	primitive(455, 73, 5298)
	primitive(456, 73, 5299)
	primitive(457, 73, 5300)
	primitive(458, 73, 5301)
	primitive(459, 73, 5302)
	primitive(460, 73, 5303)
	primitive(461, 73, 5304)
	primitive(462, 73, 5305)
	primitive(463, 73, 5306)
	primitive(464, 73, 5307)
	primitive(465, 73, 5308)
	primitive(466, 73, 5309)
	primitive(467, 73, 5310)
	primitive(468, 73, 5311)
	primitive(469, 73, 5312)
	primitive(470, 73, 5313)
	primitive(471, 73, 5314)
	primitive(472, 73, 5315)
	primitive(473, 73, 5316)
	primitive(474, 73, 5317)
	primitive(478, 74, 5830)
	primitive(479, 74, 5831)
	primitive(480, 74, 5832)
	primitive(481, 74, 5833)
	primitive(482, 74, 5834)
	primitive(483, 74, 5835)
	primitive(484, 74, 5836)
	primitive(485, 74, 5837)
	primitive(486, 74, 5838)
	primitive(487, 74, 5839)
	primitive(488, 74, 5840)
	primitive(489, 74, 5841)
	primitive(490, 74, 5842)
	primitive(491, 74, 5843)
	primitive(492, 74, 5844)
	primitive(493, 74, 5845)
	primitive(494, 74, 5846)
	primitive(495, 74, 5847)
	primitive(496, 74, 5848)
	primitive(497, 74, 5849)
	primitive(498, 74, 5850)
	primitive(32, 64, 0)
	primitive(47, 44, 0)
	primitive(508, 45, 0)
	primitive(509, 90, 0)
	primitive(510, 40, 0)
	primitive(511, 41, 0)
	primitive(512, 61, 0)
	primitive(513, 16, 0)
	primitive(504, 107, 0)
	primitive(514, 15, 0)
	primitive(515, 92, 0)
	primitive(505, 67, 0)
	primitive(516, 62, 0)
	hash[2616].rh = 516
	eqtb[2616] = eqtb[curval]
	primitive(517, 102, 0)
	primitive(518, 88, 0)
	primitive(519, 77, 0)
	primitive(520, 32, 0)
	primitive(521, 36, 0)
	primitive(522, 39, 0)
	primitive(330, 37, 0)
	primitive(351, 18, 0)
	primitive(523, 46, 0)
	primitive(524, 17, 0)
	primitive(525, 54, 0)
	primitive(526, 91, 0)
	primitive(527, 34, 0)
	primitive(528, 65, 0)
	primitive(529, 103, 0)
	primitive(335, 55, 0)
	primitive(530, 63, 0)
	primitive(408, 84, 0)
	primitive(531, 42, 0)
	primitive(532, 80, 0)
	primitive(533, 66, 0)
	primitive(534, 96, 0)
	primitive(535, 0, 256)
	hash[2621].rh = 535
	eqtb[2621] = eqtb[curval]
	primitive(536, 98, 0)
	primitive(537, 109, 0)
	primitive(407, 71, 0)
	primitive(352, 38, 0)
	primitive(538, 33, 0)
	primitive(539, 56, 0)
	primitive(540, 35, 0)
	primitive(597, 13, 256)
	parloc = curval
	partoken = (4095 + parloc)
	primitive(629, 104, 0)
	primitive(630, 104, 1)
	primitive(631, 110, 0)
	primitive(632, 110, 1)
	primitive(633, 110, 2)
	primitive(634, 110, 3)
	primitive(635, 110, 4)
	primitive(476, 89, 0)
	primitive(500, 89, 1)
	primitive(395, 89, 2)
	primitive(396, 89, 3)
	primitive(668, 79, 102)
	primitive(669, 79, 1)
	primitive(670, 82, 0)
	primitive(671, 82, 1)
	primitive(672, 83, 1)
	primitive(673, 83, 3)
	primitive(674, 83, 2)
	primitive(675, 70, 0)
	primitive(676, 70, 1)
	primitive(677, 70, 2)
	primitive(678, 70, 3)
	primitive(679, 70, 4)
	primitive(735, 108, 0)
	primitive(736, 108, 1)
	primitive(737, 108, 2)
	primitive(738, 108, 3)
	primitive(739, 108, 4)
	primitive(740, 108, 5)
	primitive(757, 105, 0)
	primitive(758, 105, 1)
	primitive(759, 105, 2)
	primitive(760, 105, 3)
	primitive(761, 105, 4)
	primitive(762, 105, 5)
	primitive(763, 105, 6)
	primitive(764, 105, 7)
	primitive(765, 105, 8)
	primitive(766, 105, 9)
	primitive(767, 105, 10)
	primitive(768, 105, 11)
	primitive(769, 105, 12)
	primitive(770, 105, 13)
	primitive(771, 105, 14)
	primitive(772, 105, 15)
	primitive(773, 105, 16)
	primitive(774, 106, 2)
	hash[2618].rh = 774
	eqtb[2618] = eqtb[curval]
	primitive(775, 106, 4)
	primitive(776, 106, 3)
	primitive(801, 87, 0)
	hash[2624].rh = 801
	eqtb[2624] = eqtb[curval]
	primitive(898, 4, 256)
	primitive(899, 5, 257)
	hash[2615].rh = 899
	eqtb[2615] = eqtb[curval]
	primitive(900, 5, 258)
	hash[2619].rh = 901
	hash[2620].rh = 901
	eqtb[2620].hh.b0 = 9
	eqtb[2620].hh.rh = 29989
	eqtb[2620].hh.b1 = 1
	eqtb[2619] = eqtb[2620]
	eqtb[2619].hh.b0 = 115
	primitive(970, 81, 0)
	primitive(971, 81, 1)
	primitive(972, 81, 2)
	primitive(973, 81, 3)
	primitive(974, 81, 4)
	primitive(975, 81, 5)
	primitive(976, 81, 6)
	primitive(977, 81, 7)
	primitive(1025, 14, 0)
	primitive(1026, 14, 1)
	primitive(1027, 26, 4)
	primitive(1028, 26, 0)
	primitive(1029, 26, 1)
	primitive(1030, 26, 2)
	primitive(1031, 26, 3)
	primitive(1032, 27, 4)
	primitive(1033, 27, 0)
	primitive(1034, 27, 1)
	primitive(1035, 27, 2)
	primitive(1036, 27, 3)
	primitive(336, 28, 5)
	primitive(340, 29, 1)
	primitive(342, 30, 99)
	primitive(1054, 21, 1)
	primitive(1055, 21, 0)
	primitive(1056, 22, 1)
	primitive(1057, 22, 0)
	primitive(409, 20, 0)
	primitive(1058, 20, 1)
	primitive(1059, 20, 2)
	primitive(965, 20, 3)
	primitive(1060, 20, 4)
	primitive(967, 20, 5)
	primitive(1061, 20, 106)
	primitive(1062, 31, 99)
	primitive(1063, 31, 100)
	primitive(1064, 31, 101)
	primitive(1065, 31, 102)
	primitive(1080, 43, 1)
	primitive(1081, 43, 0)
	primitive(1090, 25, 12)
	primitive(1091, 25, 11)
	primitive(1092, 25, 10)
	primitive(1093, 23, 0)
	primitive(1094, 23, 1)
	primitive(1095, 24, 0)
	primitive(1096, 24, 1)
	primitive(45, 47, 1)
	primitive(349, 47, 0)
	primitive(1127, 48, 0)
	primitive(1128, 48, 1)
	primitive(866, 50, 16)
	primitive(867, 50, 17)
	primitive(868, 50, 18)
	primitive(869, 50, 19)
	primitive(870, 50, 20)
	primitive(871, 50, 21)
	primitive(872, 50, 22)
	primitive(873, 50, 23)
	primitive(875, 50, 26)
	primitive(874, 50, 27)
	primitive(1129, 51, 0)
	primitive(878, 51, 1)
	primitive(879, 51, 2)
	primitive(861, 53, 0)
	primitive(862, 53, 2)
	primitive(863, 53, 4)
	primitive(864, 53, 6)
	primitive(1147, 52, 0)
	primitive(1148, 52, 1)
	primitive(1149, 52, 2)
	primitive(1150, 52, 3)
	primitive(1151, 52, 4)
	primitive(1152, 52, 5)
	primitive(876, 49, 30)
	primitive(877, 49, 31)
	hash[2617].rh = 877
	eqtb[2617] = eqtb[curval]
	primitive(1171, 93, 1)
	primitive(1172, 93, 2)
	primitive(1173, 93, 4)
	primitive(1174, 97, 0)
	primitive(1175, 97, 1)
	primitive(1176, 97, 2)
	primitive(1177, 97, 3)
	primitive(1191, 94, 0)
	primitive(1192, 94, 1)
	primitive(1193, 95, 0)
	primitive(1194, 95, 1)
	primitive(1195, 95, 2)
	primitive(1196, 95, 3)
	primitive(1197, 95, 4)
	primitive(1198, 95, 5)
	primitive(1199, 95, 6)
	primitive(415, 85, 3983)
	primitive(419, 85, 5007)
	primitive(416, 85, 4239)
	primitive(417, 85, 4495)
	primitive(418, 85, 4751)
	primitive(477, 85, 5574)
	primitive(412, 86, 3935)
	primitive(413, 86, 3951)
	primitive(414, 86, 3967)
	primitive(941, 99, 0)
	primitive(953, 99, 1)
	primitive(1217, 78, 0)
	primitive(1218, 78, 1)
	primitive(274, 100, 0)
	primitive(275, 100, 1)
	primitive(276, 100, 2)
	primitive(1227, 100, 3)
	primitive(1228, 60, 1)
	primitive(1229, 60, 0)
	primitive(1230, 58, 0)
	primitive(1231, 58, 1)
	primitive(1237, 57, 4239)
	primitive(1238, 57, 4495)
	primitive(1239, 19, 0)
	primitive(1240, 19, 1)
	primitive(1241, 19, 2)
	primitive(1242, 19, 3)
	primitive(1285, 59, 0)
	primitive(594, 59, 1)
	writeloc = curval
	primitive(1286, 59, 2)
	primitive(1287, 59, 3)
	primitive(1288, 59, 4)
	primitive(1289, 59, 5)
	nonewcontrolsequence = true
}

/* ── Main program ── */

func main() {
	history = 3
	rewrite_(termout, "TTY:", "/O")
	if readyalready == 314159 {
		goto L1
	}
	bad = 0
	if (halferrorline < 30) || (halferrorline > (errorline - 15)) {
		bad = 1
	}
	if maxprintline < 60 {
		bad = 2
	}
	if (dvibufsize % 8) != 0 {
		bad = 3
	}
	if 1100 > 30000 {
		bad = 4
	}
	if 1777 > 2100 {
		bad = 5
	}
	if maxinopen >= 128 {
		bad = 6
	}
	if 30000 < 267 {
		bad = 7
	}
	if (memmin != 0) || (memmax != 30000) {
		bad = 10
	}
	if (memmin > 0) || (memmax < 30000) {
		bad = 10
	}
	if (0 > 0) || (255 < 127) {
		bad = 11
	}
	if (0 > 0) || (65535 < 32767) {
		bad = 12
	}
	if (0 < 0) || (255 > 65535) {
		bad = 13
	}
	if ((memmin < 0) || (memmax >= 65535)) || (((-0) - memmin) > 65536) {
		bad = 14
	}
	if (0 < 0) || (fontmax > 255) {
		bad = 15
	}
	if fontmax > 256 {
		bad = 16
	}
	if (savesize > 65535) || (maxstrings > 65535) {
		bad = 17
	}
	if bufsize > 65535 {
		bad = 18
	}
	if 255 < 255 {
		bad = 19
	}
	if 6976 > 65535 {
		bad = 21
	}
	if 20 > filenamesize {
		bad = 31
	}
	if (2 * 65535) < (30000 - memmin) {
		bad = 41
	}
	if bad > 0 {
		{
			writeln_(termout, "Ouch---my internal constants have been clobbered!", "---case ", bad)
			goto L9999
		}
	}
	initialize()
	if !getstringsstarted {
		goto L9999
	}
	initprim()
	initstrptr = strptr
	initpoolptr = poolptr
	fixdateandtime()
	readyalready = 314159
L1:
	selector = 17
	tally = 0
	termoffset = 0
	fileoffset = 0
	write_(termout, "This is TeX, Version 3.141592653")
	if formatident == 0 {
		writeln_(termout, " (no format preloaded)")
	} else {
		{
			slowprint(formatident)
			println_()
		}
	}
	break_(termout)
	jobname = 0
	nameinprogress = false
	logopened = false
	outputfilename = 0
	{
		{
			inputptr = 0
			maxinstack = 0
			inopen = 0
			openparens = 0
			maxbufstack = 0
			paramptr = 0
			maxparamstack = 0
			first = bufsize
			for {
				buffer[first] = 0
				first = (first - 1)
				if !(first == 0) {
					break
				}
			}
			scannerstatus = 0
			warningindex = 0
			first = 1
			curinput.statefield = 33
			curinput.startfield = 1
			curinput.indexfield = 0
			line = 0
			curinput.namefield = 0
			forceeof = false
			alignstate = 1000000
			if !initterminal {
				goto L9999
			}
			curinput.limitfield = last
			first = (last + 1)
		}
		if (formatident == 0) || (buffer[curinput.locfield] == 38) {
			{
				if formatident != 0 {
					initialize()
				}
				if !openfmtfile {
					goto L9999
				}
				if !loadfmtfile {
					{
						wclose(fmtfile)
						goto L9999
					}
				}
				wclose(fmtfile)
				for (curinput.locfield < curinput.limitfield) && (buffer[curinput.locfield] == 32) {
					curinput.locfield = (curinput.locfield + 1)
				}
			}
		}
		if (eqtb[5311].int < 0) || (eqtb[5311].int > 255) {
			curinput.limitfield = (curinput.limitfield - 1)
		} else {
			buffer[curinput.limitfield] = eqtb[5311].int
		}
		fixdateandtime()
		magicoffset = (strstart[892] - (9 * 16))
		if interaction == 0 {
			selector = 16
		} else {
			selector = 17
		}
		if (curinput.locfield < curinput.limitfield) && (eqtb[(3983+buffer[curinput.locfield])].hh.rh != 0) {
			startinput()
		}
	}
	history = 0
	maincontrol()
	finalcleanup()
L9998:
	closefilesandterminate()
L9999:
	readyalready = 0
}

// End of TeX.go
