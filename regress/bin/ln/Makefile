#	$OpenBSD$

regress: recursive links/source
	@sh ${.CURDIR}/test_ln.sh && touch $@

clean:
	rm -rf links regress

links:
	@mkdir -p ${.OBJDIR}/links

recursive: links
#	@ln -s ${.OBJDIR}/links links/links

links/source: links
	@echo "Hello, world!" > ${.OBJDIR}/links/source

.include <bsd.regress.mk>
