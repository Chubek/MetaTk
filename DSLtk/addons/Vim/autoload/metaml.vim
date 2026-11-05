" Commands are explicit, synchronous, and never save or execute on buffer events.
function! s:File() abort
  if empty(expand('%:p')) || &buftype !=# ''
    echoerr 'MetaML: this command requires a named file buffer'
    return ''
  endif
  if &modified || !filereadable(expand('%:p'))
    echoerr 'MetaML: save the buffer before running this command'
    return ''
  endif
  return expand('%:p')
endfunction

function! s:CLI() abort
  return [get(b:, 'metaml_command', get(g:, 'metaml_command', 'metaml'))]
        \ + get(g:, 'metaml_options', [])
        \ + ['--no-init', '--no-history', '--color', 'never']
endfunction

function! s:Invoke(argv) abort
  if !executable(a:argv[0])
    echoerr 'MetaML: executable not found: ' . a:argv[0]
    return {'status': 127, 'lines': []}
  endif
  " Escape each argument independently, including executable paths with spaces.
  let command = join(map(copy(a:argv), 'shellescape(v:val)'), ' ')
  let lines = systemlist(command . ' 2>&1')
  return {'status': v:shell_error, 'lines': lines}
endfunction

function! s:Output(result, title) abort
  botright new
  setlocal buftype=nofile bufhidden=wipe noswapfile nobuflisted
  call setline(1, [a:title . ' (exit ' . a:result.status . ')', ''] + a:result.lines)
  setlocal nomodifiable
endfunction

function! metaml#Check() abort
  let file = s:File()
  if empty(file) | return | endif
  if fnamemodify(file, ':t') ==# 'MAMMAL'
    call metaml#Project('check')
    return
  endif
  let result = s:Invoke(s:CLI() + ['--diagnostic-format', 'json', '--check', file])
  let items = []
  for line in result.lines
    try
      let d = json_decode(line)
      if type(d) != type({}) || !has_key(d, 'message')
        throw 'not a diagnostic'
      endif
      call add(items, {'filename': get(d, 'file', file), 'lnum': get(d, 'line', 1),
            \ 'col': get(d, 'column', 1), 'text': get(d, 'code', '') . ': ' . d.message,
            \ 'type': get(d, 'severity', 'error') ==# 'warning' ? 'W' : 'E'})
    catch
      if !empty(line)
        call add(items, {'text': line, 'type': 'E'})
      endif
    endtry
  endfor
  call setqflist([], 'r', {'title': 'MetaML check: ' . file, 'items': items})
  if !empty(items)
    cwindow
  elseif result.status == 0
    echo 'MetaML: check passed'
  else
    echoerr 'MetaML: check failed (exit ' . result.status . ')'
  endif
endfunction

function! metaml#Run(args) abort
  let file = s:File()
  if empty(file) | return | endif
  if fnamemodify(file, ':t') ==# 'MAMMAL'
    call s:Project('run', a:args)
  elseif index(['mmli', 'mlli'], fnamemodify(file, ':e')) >= 0
    echoerr 'MetaML: interfaces are not runnable; use :MetaMLCheck'
  else
    call s:Output(s:Invoke(s:CLI() + [file, '--'] + a:args), 'MetaML run')
  endif
endfunction

function! metaml#AST() abort
  let file = s:File()
  if !empty(file)
    call s:Output(s:Invoke(s:CLI() + ['--ast', file]), 'MetaML AST')
  endif
endfunction

function! metaml#ProjectCommands(lead, line, pos) abort
  return filter(['show', 'check', 'build', 'run', 'test', 'install'], 'stridx(v:val, a:lead) == 0')
endfunction

function! s:Project(command, args) abort
  let file = s:File()
  if empty(file) | return | endif
  if index(metaml#ProjectCommands('', '', 0), a:command) < 0
    echoerr 'MetaML: unsupported MAMMAL command: ' . a:command
    return
  endif
  let argv = [get(b:, 'metaml_mammal_command', get(g:, 'metaml_mammal_command', 'metaml-mammal'))]
        \ + get(g:, 'metaml_mammal_options', []) + ['-C', fnamemodify(file, ':h'), a:command]
  if !empty(a:args)
    let argv += ['--'] + a:args
  endif
  call s:Output(s:Invoke(argv), 'MAMMAL ' . a:command)
endfunction

function! metaml#Project(command) abort
  call s:Project(a:command, [])
endfunction
