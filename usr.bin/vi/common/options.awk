#	$OpenBSD: options.awk,v 1.3 2001/01/29 01:58:31 niklas Exp $

#	@(#)options.awk	10.1 (Berkeley) 6/8/95

BEGIN {
	printf("enum {\n");
	first = 1;
}
/^\/\* O_[0-9A-Z_]*/ {
	printf("\t%s%s,\n", $2, first ? " = 0" : "");
	first = 0;
	next;
}
END {
	printf("\tO_OPTIONCOUNT\n};\n");
}
