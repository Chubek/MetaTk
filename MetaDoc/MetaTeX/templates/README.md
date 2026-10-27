# Document templates

Copy a template and edit its title, author and body. These contain executable
source for the currently supported native subset; no substitution tool is needed.

```sh
metatex -f plain -T pdf -o plain.pdf templates/plain.tex
metatex -f latex -T epub -o article.epub templates/article.ltx
metatex -f nekotex -T html -o notebook.html templates/notebook.nltx
```
