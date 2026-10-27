" GPPX directive-block indentation. Does not parse embedded target languages.
if exists('b:did_indent')
  finish
endif
let b:did_indent = 1
setlocal autoindent
setlocal indentexpr=GetGPPXIndent()
setlocal indentkeys=o,O,*<Return>,0#,0%,0@
let b:undo_indent = 'setlocal autoindent< indentexpr< indentkeys<'

if !exists('*GetGPPXIndent')
  function! GetGPPXIndent() abort
    " Recover the block stack from the start; ignore raw and comment contents.
    let stack = []
    let inraw = ''
    let incomment = 0
    let lnum = 1
    while lnum <= v:lnum
      let line = getline(lnum)
      if !empty(inraw)
        if line =~# '^\s*' . inraw . '\s*endraw\>'
          let inraw = ''
        else
          if lnum == v:lnum
            return -1
          endif
          let lnum += 1
          continue
        endif
      endif
      " Remove comments and quoted strings before recognizing directives.
      let clean = ''
      let i = 0
      let quote = ''
      while i < strlen(line)
        let pair = strpart(line, i, 2)
        let c = strpart(line, i, 1)
        if incomment
          if pair ==# '*/'
            let incomment = 0
            let i += 2
            continue
          endif
        elseif !empty(quote)
          if c ==# '\'
            let i += 2
            continue
          elseif c ==# quote
            let quote = ''
          endif
        elseif pair ==# '//'
          break
        elseif pair ==# '/*'
          let incomment = 1
          let i += 2
          continue
        elseif c ==# '"' || c ==# "'"
          let quote = c
          let clean .= 'x'
        else
          let clean .= c
        endif
        let i += 1
      endwhile
      let cmd = matchstr(clean, '^\s*[#%@]\s*\zs\h\w*')
      let closer = index(['endif','endfor','endrepeat','endmacro','endregion','endraw'], cmd) >= 0
      let middle = index(['else','elif','elifmatch'], cmd) >= 0
      if lnum == v:lnum
        return max([0, len(stack) - (closer || middle)]) * shiftwidth()
      endif
      if closer && !empty(stack)
        call remove(stack, -1)
      elseif index(['if','ifdef','ifndef','ifmatch','for','repeat','macro','region','raw'], cmd) >= 0
        call add(stack, cmd)
        if cmd ==# 'raw'
          let inraw = matchstr(clean, '[#%@]')
        endif
      endif
      let lnum += 1
    endwhile
    return 0
  endfunction
endif
