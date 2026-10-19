module Docstrings := G:CreateModule!{ name -> MetaPP::StdKern::Docstrings, alias -> :DOC  };
module IO := G:LoadModule!{ name -> MetaPP::StdKern::IO; assure -> `mservectl checklib -PP IO` };
module Parsec := G:LoadModule!{ name -> MetaPP::StdKern::Parsec; alias -> :P };


type Scraper := G:CreateStruct!{
         
}


