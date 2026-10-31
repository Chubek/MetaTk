\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{bookguide}[2026/10/31 Programmer guide]
\LoadClass[10pt,oneside,openany]{book}
\RequirePackage[paperwidth=6in,paperheight=9in,inner=.64in,outer=.60in,top=.67in,bottom=.65in,headheight=14pt,headsep=.20in,footskip=.29in]{geometry}
\RequirePackage[T1]{fontenc}
\RequirePackage{lmodern}
\RequirePackage{microtype}
\RequirePackage{xcolor}
\definecolor{accent}{HTML}{165B72}
\definecolor{codebg}{HTML}{F2F5F6}
\definecolor{ink}{HTML}{23333B}
\RequirePackage{listings}
\lstdefinelanguage{D}{morekeywords={import,module,void,int,long,uint,ulong,ushort,string,auto,assert,struct,class,mixin,new,return,ref,alias,enum,if,else,foreach,scope,exit,try,catch,throw,cast,bool,true,false,null,is},sensitive=true,morecomment=[l]{//},morecomment=[s]{/*}{*/},morestring=[b]"}
\lstset{basicstyle=\ttfamily\fontsize{8}{10}\selectfont,keywordstyle=\bfseries\color{accent},commentstyle=\itshape\color{gray},stringstyle=\color{ink},backgroundcolor=\color{codebg},frame=single,rulecolor=\color{codebg},framerule=.4pt,framesep=5pt,breaklines=true,breakatwhitespace=false,columns=fullflexible,keepspaces=true,showstringspaces=false,tabsize=2,captionpos=b,aboveskip=9pt,belowskip=8pt,xleftmargin=5pt,xrightmargin=5pt}
\RequirePackage{titlesec}
\titleformat{\chapter}[display]{\sffamily\bfseries\color{ink}}{\color{accent}\small CHAPTER \thechapter}{5pt}{\fontsize{22}{25}\selectfont}
\titlespacing*{\chapter}{0pt}{0pt}{14pt}
\titleformat{\section}{\sffamily\bfseries\color{accent}\normalsize}{}{0pt}{}
\titlespacing*{\section}{0pt}{10pt}{5pt}
\RequirePackage{fancyhdr}
\pagestyle{fancy}\fancyhf{}
\fancyhead[L]{\sffamily\scriptsize DSLtk and MetaML}
\fancyhead[R]{\sffamily\scriptsize Working Programmer's Guide}
\fancyfoot[C]{\sffamily\small\thepage}
\renewcommand{\headrulewidth}{.3pt}
\fancypagestyle{plain}{\fancyhf{}\fancyfoot[C]{\sffamily\small\thepage}\renewcommand{\headrulewidth}{0pt}}
\RequirePackage{hyperref}
\hypersetup{colorlinks=true,linkcolor=accent,urlcolor=accent,pdftitle={DSLtk and MetaML: A Full Guide for the Working Programmer},pdfauthor={Programmer's guide},bookmarksopen=false}
\setlength{\parindent}{0pt}
\setlength{\parskip}{6pt plus 1pt}
\setcounter{secnumdepth}{0}
\raggedbottom
\emergencystretch=1.5em
\newcommand{\workshop}[3]{{\sffamily\small\color{accent}WORKSHOP #1\hfill #3\par}\vspace{5pt}{\sffamily\bfseries\fontsize{17}{20}\selectfont #2\par}\vspace{7pt}}
\endinput
