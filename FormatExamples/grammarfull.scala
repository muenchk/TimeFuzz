/*
 * This file contains the grammar for the example.
 * This is a scala-based dsl, also used within tribble (https://github.com/havrikov/tribble).
 * Please have a look at their documentation to understand the format.
 */

Grammar(
  'start := 'x2func | 'x3func | 'efunc | 'nfunc,
  'nfunc := "n " ~ 'pattern ~ " " ~ 'str,
  'x2func := "x2 " ~ 'pattern ~ " " ~ 'str ~ " " ~ 'from ~ " " ~ 'to,
  'x3func := "x3 " ~ 'pattern ~ " " ~ 'str ~ " " ~ 'from ~ " " ~ 'to ~ " " ~ 'mem,
  'efunc := "e " ~ 'pattern ~ " " ~ 'str ~ " " ~ 'errorno,
  'pattern := "\"" ~ 'subexpr ~ "\"",
  'str := "\"" ~ 'symbols ~ "\"",

  'subexpr := "" | 'symbolsregex ~ 'subexpr | 'chartypes ~ 'subexpr | 'quantifier ~ 'subexpr | 'anchor ~ 'subexpr | 'charclass ~ 'subexpr | 'extendedgroups ~ 'subexpr,
  'subexprnonempty := 'symbolsregex ~ 'subexpr | 'chartypes ~ 'subexpr | 'quantifier ~ 'subexpr | 'anchor ~ 'subexpr | 'charclass ~ 'subexpr | 'extendedgroups ~ 'subexpr,

  'symbols := 'symbol ~ 'symbols | 'symbol,
  'symbolsregex := 'symbol | 'symbol ~ 'separator ~ 'symbol,
  'symbol := 'utf8 | 'unicode | 'octal | 'specialchar | 'alphanum,
  'specialchar := "\\t" | "\\v" | "\\n" | "\\r" | "\\b" | "\\f" | "\\a" | "\\e",
  'octal := "\\n" ~ "[0-7]".regex ~ "[0-7]".regex,
  'utf8 := "\\x" ~ "[0-9a-f]".regex ~ "[0-9a-f]".regex,
  'unicode := "\\u" ~ "[0-9a-f]".regex ~ "[0-9a-f]".regex ~ "[0-9a-f]".regex ~ "[0-9a-f]".regex,
  'alphanum := "[0-9a-zA-z]".regex,

  'separator := "|",

  'chartypes := 'chartype | 'chartype ~ 'chartypes,
  'chartype := "." | "\\w" | "\\W" | "\\s" | "\\S" | "\\d" | "\\D" | "\\h" | "\\H" | "\\R" | "\\N" | "\\O" | "\\X",

  'quantifier := "?" | "*" | "+" | "{" ~ 'numshortpos ~ "}" | "{" ~ 'numshort ~ "," ~ 'numshortpos ~ "}" | "{" ~ 'numshort ~ ",}" | "{," ~ 'numshortpos ~ "}",

  'anchor := "^" | "$" | "\\b" | "\\B" | "\\A" | "\\Z" | "\\z" | "\\G" | "\\K" | "\\y" | "\\Y",

  'extendedgroups := "(?" ~ "[imxWDSPCIL]".regex.rep(0,3) ~ ")" ~ 'subexprnonempty | "(?" ~ "[imxWDSPCIL]".regex.rep(0,3) ~ ":" ~ 'subexprnonempty ~ ")" | "(?:" ~ 'subexprnonempty ~ ")" | "(" ~ 'subexprnonempty ~ ")" | "(?=" ~ 'subexprnonempty ~ ")" | "(?!" ~ 'subexprnonempty ~ ")" | "(?<=" ~ 'subexprnonempty ~ ")" | "(?<!" ~ 'subexprnonempty ~ ")" | "(?>" ~ 'subexprnonempty ~ ")",
  
  'charclass := "^" ~ 'charclass | "[" ~ 'charclass ~ "]" | 'charclass ~ "&&" ~ 'charclass | 'alphanum ~ "-" ~ 'alphanum | "[:alnum:]" | "[:alpha:]" | "[:ascii:]" | "[:blank:]" | "[:cntrl:]" | "[:digit:]" | "[:graph:]" | "[:lower:]" | "[:print:]" | "[:punct:]" | "[:space:]" | "[:upper:]" | "[:xdigit:]" | "[:word:]",

  'from := 'numshort,
  'to := 'numshort,
  'mem := 'numshort,
  'errorno := "1" | "0",
  'numshort := "0" | "[1-9]".regex ~ "[0-9]".regex.rep(0,2),
  'numshortpos := "[1-9]".regex ~ "[0-9]".regex.rep(0,2)
)