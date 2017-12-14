#	$OpenBSD: ex.awk,v 1.2 2001/01/29 01:58:40 niklas Exp $

#	@(#)ex.awk	10.1 (Berkeley) 6/8/95

BEGIN {
	printf("enum {");
	first = 1;
}
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("%s\n\t%s%s", first ? "" : ",", $2, first ? " = 0" : "");
	first = 0;
	next;
}
END {
	printf("\n};\n");
}
