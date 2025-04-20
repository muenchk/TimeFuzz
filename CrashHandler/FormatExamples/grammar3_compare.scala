Grammar(
	'start := 'sstart,
	'sstart := 'option | 'sstart ~ 'SEQ_option,
	'option := 'SEQ_option,
	'SEQ_option := "[]" | "[ 'RIGHT' ]" | "[ 'Z' ]" | "[ 'RIGHT',  'Z'  ]",
)