/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
----------------------- CHANGE HISTORY --------------------------

       1/29/84	Allowed use on standard input		
       2/1/84	Added E, N, P commands			
       4/17/84	Added '=' command, 'stop' signal handling	
       4/20/84	Added line folding				
v2     4/27/84	Fixed '=' command to use BOTTOM_PLUS_ONE,
		instead of TOP, added 'p' & 'v' commands	
v3     5/3/84	Added -m and -t options, '-' command	
v4     5/3/84	Added LESS environment variable		
v5     5/3/84	New comments, fixed '-' command slightly	
v6     5/15/84	Added -Q, visual bell			
v7     5/24/84	Fixed jump_back(n) bug: n should count real
		lines, not folded lines.  Also allow number on G command.
v8     5/30/84	Re-do -q and -Q commands			
v9     9/25/84	Added "+<cmd>" argument			
v10    10/10/84	Fixed bug in -b<n> argument processing	
v11    10/18/84	Made error() ring bell if \n not entered.	
-----------------------------------------------------------------
v12    2/13/85	Reorganized signal handling and made portable to 4.2bsd.
v13    2/16/85	Reword error message for '-' command.	
v14    2/22/85	Added -bf and -bp variants of -b.		
v15    2/25/85	Miscellaneous changes.			
v16    3/13/85	Added -u flag for backspace processing.	
v17    4/13/85	Added j and k commands, changed -t default.			
v18    4/20/85	Rewrote signal handling code.		
v19    5/2/85	Got rid of "verbose" eq_message().		
		Made search() scroll in some cases.
v20    5/21/85	Fixed screen.c ioctls for System V.	
v21    5/23/85	Fixed some first_cmd bugs.			
v22    5/24/85	Added support for no RECOMP nor REGCMP.	
v23    5/25/85	Miscellanous changes and prettying up.	
		Posted to USENET.
-----------------------------------------------------------------
v24    6/3/85	Added ti,te terminal init & de-init.       
		(Thanks to Mike Kersenbrock)
v25    6/8/85	Added -U flag, standout mode underlining.	
v26    6/9/85	Added -M flag.				
		Use underline termcap (us) if it exists.
v27    6/15/85	Renamed some variables to make unique in	
		6 chars.  Minor fix to -m.
v28    6/28/85	Fixed right margin bug.			
v29    6/28/85	Incorporated M.Rose's changes to signal.c	
v30    6/29/85	Fixed stupid bug in argument processing.	
v31    7/15/85	Added -p flag, changed repaint algorithm.  
		Added kludge for magic cookie terminals.
v32    7/16/85	Added cat_file if output not a tty.	
v33    7/23/85	Added -e flag and EDITOR.			
v34    7/26/85	Added -s flag.				
v35    7/27/85	Rewrote option handling; added option.c.	
v36    7/29/85	Fixed -e flag to work if not last file.	
v37    8/10/85	Added -x flag.				
v38    8/19/85	Changed prompting; created prompt.c.	
v39    8/24/85	(Not -p) does not initially clear screen.	
v40    8/26/85	Added "skipping" indicator in forw().	
		Posted to USENET.
-----------------------------------------------------------------
v41    9/17/85	ONLY_RETURN, control char commands,	
		faster search, other minor fixes.
v42    9/25/85	Added ++ command line syntax;		
		ch_fsize for pipes.
v43    10/15/85	Added -h flag, changed prim.c algorithms.	
v44    10/16/85	Made END print in all cases of eof;	
		ignore SIGTTOU after receiv ing SIGTSTP.
v45    10/16/85	Never print backspaces unless -u.		
v46    10/24/85	Backwards scroll in jump_loc.		
v47    10/30/85	Fixed bug in edit(): *first_cmd==0		
v48    11/16/85	Use TIOCSETN instead of TIOCSETP.		
		Added marks (m and ' commands).
		Posted to USENET.
-----------------------------------------------------------------
v49    1/9/86	Fixed bug: signal didn't clear mcc.	
v50    1/15/86	Added ' (quote) to gomark.			
v51    1/16/86	Added + cmd, fixed problem if first_cmd
		fails, made g cmd sort of "work" on pipes
		ev en if bof is no longer buffered.		
v52    1/17/86	Made short files work better.		
v53    1/20/86	Added -P option.				
v54    1/20/86	Changed help to use HELPFILE.		
v55    1/23/86	Messages work better if not tty output.	
v56    1/24/86	Added -l option.				
v57    1/31/86	Fixed -l to get confirmation before
		ov erwriting an existing file.		
v58    8/28/86	Added filename globbing.			
v59    9/15/86	Fixed some bugs with very long filenames.	
v60    9/26/86	Incorporated changes from Leith (Casey)
		Leedom for boldface and -z option.		
v61    9/26/86	Got rid of annoying repaints after ! cmd.	
		Posted to USENET.
-----------------------------------------------------------------
v62    12/23/86	Added is_directory(); change -z default to
		-1 instead of 24; cat-and-exit if -e and
		file is less than a screenful.		
v63    1/8/87	Fixed bug in cat-and-exit if > 1 file.	
v64    1/12/87	Changed puts/putstr, putc/putchr,
		getc/getchr to av oid name conflict with
		stdio functions.				
v65    1/26/87	Allowed '-' command to change NUMBER
		v alued options (thanks to Gary Puckering)	
v66    2/13/87	Fixed bug: prepaint should use force=1.	
v67    2/24/87	Added !! and % expansion to ! command.	
v68    2/25/87	Added SIGWINCH and TIOCGWINSZ support;
		changed is_directory to bad_file.
		(thanks to J. Robert Ward)			
v69    2/25/87	Added SIGWIND and WIOCGETD (for Unix PC).	
v70    3/13/87	Changed help cmd from 'h' to 'H'; better
		error msgs in bad_file, errno_message.	
v71    5/11/87	Changed -p to -c, made triple -c/-C
		for clear-eol like more's -c.		
v72    6/26/87	Added -E, -L, use $SHELL in lsystem().	
		(thanks to Stev e Spearman)
v73    6/26/87	Allow Examine "#" for previous file.	
		Posted to USENET 8/25/87.
-----------------------------------------------------------------
v74    9/18/87	Fix conflict in EOF symbol with stdio.h,	
		Make os.c more portable to BSD.
v75    9/23/87	Fix problems in get_term (thanks to 	
		Paul Eggert); new backwards scrolling in
		jump_loc (thanks to Marion Hakanson).
v76    9/23/87	Added -i flag; allow single "!" to		
		inv oke a shell (thanks to Franco Barber).
v77    9/24/87	Added -n flag and line number support.	
v78    9/25/87	Fixed problem with prompts longer than	
		the screen width.
v79    9/29/87	Added the _ command.			
v80    10/6/87	Allow signal to break out of linenum scan.	
v81    10/6/87	Allow -b to be changed from within less.	
v82    10/7/87	Add cmd_decode to use a table for key	
		binding (thanks to Dav id Nason).
v83    10/9/87	Allow .less file for user-defined keys.	
v84    10/11/87	Fix -e/-E problems (thanks to Felix Lee).	
v85    10/15/87	Search now keeps track of line numbers.	
v86    10/20/87	Added -B option and autobuf; fixed		
		"pipe error" bug.
v87    3/1/88	Fix bug re BSD signals while reading file.	
v88    3/12/88	Use new format for -P option (thanks to	
		der Mouse), allow "+-c" without message,
		fix bug re BSD hangup.
v89    3/18/88	Turn off line numbers if linenum scan	
		is interrupted.
v90    3/30/88	Allow -P from within less.			
v91    3/30/88	Added tags file support (new -t option)	
		(thanks to Brian Campbell).
v92    4/4/88	Added -+option syntax.			
v93    4/11/88	Add support for slow input (thanks to	
		Joe Orost & apologies for taking almost
		3 years to get this in!)
v94    4/11/88	Redo reading/signal stuff.			
v95    4/20/88	Repaint screen better after signal.	
v96    4/21/88	Add /! and ?! commands.			
v97    5/17/88	Allow -l/-L from within less.		
		Eliminate some static arrays (use calloc).
		Posted to USENET.
-----------------------------------------------------------------
v98    10/14/88	Fix incorrect calloc call; uninitialized	
		var in exec_mca; core dump on unknown TERM.
		Make v cmd work if past last line of file.
		Fix some signal bugs.
v99    10/29/88	Allow space between -X and string,		
		when X is a string-valued option.
v100   1/5/89	Fix globbing bug when $SHELL not set;	
		allow spaces after -t command.
v101   1/6/89	Fix problem with long (truncated) lines	
		in tags file (thanks to Neil Dixon).
v102   1/6/89	Fix bug with E# when no prev file;	
		allow spaces after -l command.
v103   3/14/89	Add -N, -f and -? options.  Add z and w	
		commands.  Add %L for prompt strings.
v104   3/16/89	Added EDITPROTO.				
v105   3/20/89	Fix bug in find_linenum which cached	
		incorrectly on long lines.
v106   3/31/89	Added -k option and multiple lesskey      
		files.
v107   4/27/89	Add 8-bit char support and -g option.	
		Split option code into 3 files.
v108   5/5/89	Allocate position table dynamically       
		(thanks to Paul Eggert); change % command
		from "percent" to vi-style brace finder.
v109   5/10/89	Added ESC-% command, split prim.c.	
v110   5/24/89	Fixed bug in + option; fixed repaint bug	
		under Sun windows (thanks to Paul Eggert).
v111   5/25/89	Generalized # and % expansion; use 	
		calloc for some error messages.
v112   5/30/89	Get rid of ESC-%, add {}()[] commands.	
v113   5/31/89	Optimize lseeks (thanks to Paul Eggert).	
v114   7/25/89	Added ESC-/ and ESC-/! commands.		
v115   7/26/89	Added ESC-n command.			
v116   7/31/89	Added find_pos to optimize g command.	
v117   8/1/89	Change -f option to -r.			
v118   8/2/89	Save positions for all previous files,	
		not just the immediately previous one.
v119   8/7/89	Save marks across file boundaries.	
		Add file handle stuff.
v120   8/11/89	Add :ta command.				
v121   8/16/89	Add -f option.				
v122   8/30/89	Fix performance with many buffers.	
v123   8/31/89	Verbose prompts for string options.	
		Posted beta to USENET.
-----------------------------------------------------------------
v124   9/18/89	Reorganize search commands,		
		N = rev, ESC-n = span, add ESC-N.
v125   9/18/89	Fix tab bug (thanks to Alex Liu).		
		Fix EOF bug when both -w and -c.
v126   10/25/89	Add -j option.				
v127   10/27/89	Fix problems with blank lines before BOF.	
v128   10/27/89	Add %bj, etc. to prompt strings.		
v129   11/3/89	Add -+,-- commands; add set-option and	
		unset-option to lesskey.
v130   11/6/89	Generalize A_EXTRA to string, remove	
		set-option, unset-option from lesskey.
v131   11/7/89	Changed name of EDITPROTO to LESSEDIT.	
v132   11/8/89	Allow editing of command prefix.		
v133   11/16/89	Add -y option (thanks to Jeff Sullivan).	
v134   12/1/89	Glob filenames in the -l command.		
v135   12/5/89	Combined {}()[] commands into one, and	
		added ESC-^F and ESC-^B commands.
v136   1/20/90	Added -S, -R flags.  Added | command.	
		Added warning for binary files. (thanks
		to Richard Brittain and J. Sullivan).
v137   1/21/90	Rewrote horrible pappend code.		
		Added * notation for hi-bit chars.
v138   1/24/90	Fix magic cookie terminal handling.	
		Get rid of "cleanup" loop in ch_get.
v139   1/27/90	Added MSDOS support.  (many thanks	
		to Richard Brittain).
v140   2/7/90	Editing a new file adds it to the		
		command line list.
v141   2/8/90	Add edit_list for editing >1 file.	
v142   2/10/90	Add :x command.				
v143   2/11/90	Add * and @ modifies to search cmds.	
		Change ESC-/ cmd from /@* to / *.
v144   3/1/90	Messed around with ch_zero; 		
		no real change.
v145   3/2/90	Added -R and -v/-V for MSDOS;		
		renamed FILENAME to avoid conflict.
v146   3/5/90	Pull cmdbuf functions out of command.c	
v147   3/7/90	Implement ?@; fix multi-file edit bugs.	
v148   3/29/90	Fixed bug in :e<file> then :e#.		
v149   4/3/90	Change error,ierror,query to use PARG.	
v150   4/6/90	Add LESS_CHARSET, LESS_CHARDEF.		
v151   4/13/90	Remove -g option; clean up ispipe.	
v152   4/14/90	lsystem() closes input file, for		
		editors which require exclusive open.
v153   4/18/90	Fix bug if SHELL unset; 			
		fix bug in overstrike control char.
v154   4/25/90	Output to fd 2 via buffer.		
v155   4/30/90	Ignore -i if uppercase in pattern		
		(thanks to Michael Rendell.)
v156   5/3/90	Remove scroll limits in forw() & back();	
		causes problems with -c.
v157   5/4/90	Forward search starts at next real line	
		(not screen line) after jump target.
v158   6/14/90	Added F command.				
v159   7/29/90	Fix bug in exiting: output not flushed.	
v160   7/29/90	Clear screen before initial output w/ -c.	
v161   7/29/90	Add -T flag.				
v162   8/14/90	Fix bug with +F on command line.		
v163   8/21/90	Added LESSBINFMT variable.		
v164   9/5/90	Added -p, LINES, COLUMNS and		
		unset mark ' == BOF, for 1003.2 D5.
v165   9/6/90	At EOF with -c set, don't display empty	
		screen when try to page forward.
v166   9/6/90	Fix G when final line in file wraps.	
v167   9/11/90	Translate CR/LF -> LF for 1003.2.		
v168   9/13/90	Return to curr file if "tag not found".	
v169   12/12/90	G goes to EOF even if file has grown.	
v170   1/17/91	Add optimization for BSD _setjmp;		
		fix #include ioctl.h TERMIO problem.
		(thanks to Paul Eggert)
		Posted to USENET.
-----------------------------------------------------------------
v171   3/6/91	Fix -? bug in get_filename.		
v172   3/15/91	Fix G bug in empty file.			
		Fix bug with ?\n and -i and uppercase
		pattern at EOF!
		(thanks to Paul Eggert)
v173   3/17/91	Change N cmd to not permanently change	
		direction. (thanks to Brian Matthews)
v174   3/18/91	Fix bug with namelogfile not getting	
		cleared when change files.
v175   3/18/91	Fix bug with ++cmd on command line.	
		(thanks to Jim Meyering)
v176   4/2/91	Change | to not force current screen,	
		include marked line, start/end from
		top of screen.  Improve search speed.
		(thanks to Don Mears)
v177   4/2/91	Add LESSHELP variable.			
		Fix bug with F command with -e.
		Try /dev/tty for input before using fd 2.
		Patches posted to USENET  4/2/91.
-----------------------------------------------------------------
v178   4/8/91	Fixed bug in globbing logfile name.	
		(thanks to Jim Meyering)
v179   4/9/91	Allow negative -z for screen-relative.	
v180   4/9/91	Clear to eos rather than eol if "db";	
		don't use "sr" if "da".
		(thanks to Tor Lillqvist)
v181   4/18/91	Fixed bug with "negative" chars 80 - FF.	
		(thanks to Benny Sander Hofmann)
v182   5/16/91	Fixed bug with attribute at EOL.		
		(thanks to Brian Matthews)
v183   6/1/91	Rewrite linstall to do smart config.	
v184   7/11/91	Process \b in searches based on -u	
		rather than -i.
v185   7/11/91	-Pxxx sets short prompt; assume SIGWINCH	
		after a SIGSTOP. (thanks to Ken Laprade)
-----------------------------------------------------------------
v186   4/20/92	Port to MS-DOS (Microsoft C).		
v187   4/23/92	Added -D option & TAB_COMPLETE_FILENAME.	
v188   4/28/92	Added command line editing features.	
v189   12/8/92	Fix mem overrun in anscreen.c:init; 	
		fix edit_list to recover from bin file.
v190   2/13/93	Make TAB enter one filename at a time;	
		create ^L with old TAB functionality.
v191   3/10/93	Defer creating "flash" page for MS-DOS.	
v192   9/6/93	Add BACK-TAB.				
v193   9/17/93	Simplify binary_file handling.		
v194   1/4/94	Add rudiments of alt_filename handling.	
v195   1/11/94	Port back to Unix; support keypad.	
-----------------------------------------------------------------
v196   6/7/94	Fix bug with bad filename; fix IFILE	
		type problem. (thanks to David MacKenzie)
v197   6/7/94	Fix bug with .less tables inserted wrong.	
v198   6/23/94	Use autoconf installation technology.	
		(thanks to David MacKenzie)
v199   6/29/94	Fix MS-DOS build (thanks to Tim Wiegman).	
v200   7/25/94	Clean up copyright, minor fixes.		
	Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v201   7/27/94	Check for no memcpy; add casts to calloc;	
		look for regcmp in libgen.a.
		(thanks to Kaveh Ghazi).
v202   7/28/94	Fix bug in edit_next/edit_prev with 	
		non-existant files.
v203   8/2/94	Fix a variety of configuration bugs on	
		various systems. (thanks to Sakai
		Kiyotaka, Harald Koenig, Bjorn Brox,
		Teemu Rantanen, and Thorsten Lockert)
v204   8/3/94	Use strerror if available.		
		(thanks to J.T. Conklin)
v205   8/5/94	Fix bug in finding "me" termcap entry.	
		(thanks to Andreas Stolcke)
8/10/94 	v205+: Change BUFSIZ to LBUFSIZE to avoid name	
		conflict with stdio.h.
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v206   8/10/94	Use initial_scrpos for -t to avoid	
		displaying first page before init().
		(thanks to Dominique Petitpierre)
v207   8/12/94	Fix bug if stdout is not tty.		
v208   8/16/94	Fix bug in close_altfile if goto err1	
		in edit_ifile. (Thanks to M.J. Hewitt)
v209   8/16/94	Change scroll to wscroll to avoid 	
		conflict with library function.
v210   8/16/94	Fix bug with bold on 8 bit chars.		
		(thanks to Vitor Duarte)
v211   8/16/94	Don't quit on EOI in jump_loc / forw.	
v212   8/18/94	Use time_t if available.			
v213   8/20/94	Allow ospeed to be defined in termcap.h.	
v214   8/20/94	Added HILITE_SEARCH, -F, ESC-u cmd.	
		(thanks to Paul Lew and Bob Byrnes)
v215   8/23/94	Fix -i toggle behavior.			
v216   8/23/94	Process BS in all searches, not only -u.	
v217   8/24/94	Added -X flag.				
v218   8/24/94	Reimplement undo_search.			
v219   8/24/94	Find tags marked with line number		
		instead of pattern.
v220   8/24/94	Stay at same position after SIG_WINCH.	
v221   8/24/94	Fix bug in file percentage in big file.	
v222   8/25/94	Do better if can't reopen current file.	
v223   8/27/94	Support setlocale.			
		(thanks to Robert Joop)
v224   8/29/94	Revert v216: process BS in search		
		only if -u.
v225   9/6/94	Rewrite undo_search again: toggle.	
v226   9/15/94	Configuration fixes. 			
		(thanks to David MacKenzie)
v227   9/19/94	Fixed strerror config problem.		
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v228   9/21/94	Fix bug in signals: repeated calls to	
		get_editkeys overflowed st_edittable.
v229   9/21/94	Fix "Nothing to search" error if -a	
		and SRCH_PAST_EOF.
v230   9/21/94	Don't print extra error msg in search	
		after regerror().
v231   9/22/94	Fix hilite bug if search matches 0 chars.	
		(thanks to John Polstra)
v232   9/23/94	Deal with weird systems that have 	
		termios.h but not tcgetattr().
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v233   9/26/94	Use get_term() instead of pos_init() in	
		psignals to re-get lower_left termcap.
		(Thanks to John Malecki)
v234   9/26/94	Make MIDDLE closer to middle of screen.	
v235   9/27/94	Use local strchr if system doesn't have.	
v236   9/28/94	Don't use libucb; use libterm if 		
		libtermcap & libcurses doesn't work.
		(Fix for Solaris; thanks to Frank Kaefer)
v237   9/30/94	Use system isupper() etc if provided.	
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v238   10/6/94	Make binary non-blinking if LESSBINFMT	
		is set to a string without a *.
v239   10/7/94	Don't let delimit_word run back past	
		beginning of cmdbuf.
v240   10/10/94	Don't write into termcap buffer.		
		(Thanks to Benoit Speckel)
v241   10/13/94	New lesskey file format.			
		Don't expand filenames in search command.
v242   10/14/94	Allow lesskey specification of "literal".	
v243   10/14/94	Add #stop command to lesskey.		
v244   10/16/94	Add -f flag to lesskey.			
v245   10/25/94	Allow TAB_COMPLETE_FILENAME to be undefd.	
v246   10/27/94	Move help file to /usr/local/share.	
v247   10/27/94	Add -V option.				
v248   11/5/94	Add -V option to lesskey.			
v249   11/5/94	Remove -f flag from lesskey; default	
		input file is ~/.lesskey.in, not stdin.
v250   11/7/94	Lesskey input file "-" means stdin.	
v251   11/9/94	Convert cfgetospeed result to ospeed.	
		(Thanks to Andrew Chernov)
v252   11/16/94	Change default lesskey input file from 	
		.lesskey.in to .lesskey.
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v253   11/21/94	Fix bug when tags file has a backslash.	
v254   12/6/94	Fix -k option.				
v255   12/8/94	Add #define EXAMINE to disable :e etc.	
v256   12/10/94	Change highlighting: only highlite search	
		results (but now it is reliable).
v257   12/10/94	Add goto_line and repaint_highlight	
		to optimize highlight repaints.
v258   12/12/94	Fixup in hilite_line if BS_SPECIAL.	
v259   12/12/94	Convert to autoconf 2.0.			
v260   12/13/94	Add SECURE define.			
v261   12/14/94	Use system WERASE char as EC_W_BACKSPACE.	
v262   12/16/94	Add -g/-G flag and screen_hilite.		
v263   12/20/94	Reimplement/optimize -G flag behavior.	
v264   12/23/94	Allow EXTRA string after line-edit cmd	
		in lesskey file.
v265   12/24/94	Add LESSOPEN=|cmd syntax.			
v266   12/26/94	Add -I flag.				
v267   12/28/94	Formalize the four-byte header emitted	
		by a LESSOPEN pipe.
v268   12/28/94	Get rid of four-byte header.		
v269   1/2/95	Close alt file before open new one.	
		Avoids multiple popen().
v270   1/3/95	Use VISUAL; use S_ISDIR/S_ISREG; fix	
		config problem with Solaris POSIX regcomp.
v271   1/4/95	Don't quit on read error.			
v272   1/5/95	Get rid of -L.				
v273   1/6/95	Fix ch_ungetchar bug; don't call		
		LESSOPEN on a pipe.
v274   1/6/95	Ported to OS/2 (thanks to Kai Uwe Rommel)	
v275   1/18/95	Fix bug if toggle -G at EOF.		
v276   1/30/95	Fix OS/2 version.				
v277   1/31/95	Add "next" charset; don't display ^X 	
		for X > 128.
v278   2/14/95	Change default for -G.			
		Posted to prep.ai.mit.edu
-----------------------------------------------------------------
v279   2/22/95	Add GNU options --help, --version.	
		Minor config fixes.
v280   2/24/95	Clean up calls to glob(); don't set #	
		if we can't open the new file.
v281   2/24/95	Repeat search should turn on hilites.	
v282   3/2/95	Minor fixes.				
v283   3/2/95	Fix homefile; make OS2 look in $HOME.	
v284   3/2/95	Error if "v" on LESSOPENed file;		
		"%" figures out file size on pipe.
v285   3/7/95	Don't set # in lsystem; 			
		lesskey try $HOME first.
v286   3/7/95	Reformat change history (too much free time?).
v287   3/8/95	Fix hilite bug if overstrike multiple chars.
v288   3/8/95	Allow lesskey to override get_editkey keys.
v289   3/9/95	Fix adj_hilite bug when line gets processed by
		hilite_line more than once.
v290   3/9/95	Make configure automatically.  Fix Sequent problem
		with incompatible sigsetmask().

*/

char version[] = "290";
