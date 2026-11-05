" Runtime filetype detection, shared by Vim and Neovim.
augroup metaml_filetype
  autocmd!
  autocmd BufRead,BufNewFile *.mml,*.mmli,*.mlli,MAMMAL setfiletype metaml
augroup END
