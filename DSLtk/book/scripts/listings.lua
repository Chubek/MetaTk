-- Render fenced source blocks as portable listings without external highlighters.
local serial = 0
function Code(code)
  -- Plain identifiers, paths and commands use the book's breakable helper.
  -- Let Pandoc escape punctuation-heavy C++ expressions itself.
  if code.text:match("^[%w_%.%/%- :]+$") then
    return pandoc.RawInline("latex", "\\code{" .. code.text .. "}")
  end
end
function CodeBlock(block)
  serial = serial + 1
  local language = block.classes[1] or "text"
  local styles = {cpp="dslcpp", c="dslcpp", d="dsld", lisp="dslsexpr", dot="dsldot"}
  local style = styles[language] or "dslcmd"
  local caption = (language == "cpp" or language == "d") and "Source or application fragment" or "Notation, algorithm, or output"
  if block.text:find("int main",1,true) then caption = "Application example (see chapter's snapshot notes)" end
  return pandoc.RawBlock("latex", "\\begin{lstlisting}[style=" .. style .. ",caption={" .. caption .. "}]\n" .. block.text .. "\n\\end{lstlisting}")
end
