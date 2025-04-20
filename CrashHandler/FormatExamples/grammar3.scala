Grammar(
	'start := 'sstart,
	'sstart := 'SEQ_option | 'sstart ~ 'SEQ_option,
	'SEQ_option := "[]" | "[ 'RIGHT' ]" | "[ 'Z' ]" | "[ 'RIGHT',  'Z'  ]",
)