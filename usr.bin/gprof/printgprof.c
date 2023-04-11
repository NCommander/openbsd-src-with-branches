/*	$OpenBSD: printgprof.c,v 1.14 2015/12/06 23:22:51 guenther Exp $	*/
/*	$NetBSD: printgprof.c,v 1.5 1995/04/19 07:16:21 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>

#include "gprof.h"
#include "pathnames.h"

int namecmp(const void *, const void *);

void
printprof()
{
    nltype		*np;
    nltype		**sortednlp;
    int			index;

    actime = 0.0;
    printf( "\f\n" );
    flatprofheader();
	/*
	 *	Sort the symbol table in by time
	 */
    sortednlp = calloc( nname , sizeof(nltype *) );
    if ( sortednlp == (nltype **) 0 )
	warnx("[printprof] ran out of memory for time sorting");
    for ( index = 0 ; index < nname ; index += 1 ) {
	sortednlp[ index ] = &nl[ index ];
    }
    qsort( sortednlp , nname , sizeof(nltype *) , timecmp );
    for ( index = 0 ; index < nname ; index += 1 ) {
	np = sortednlp[ index ];
	flatprofline( np );
    }
    actime = 0.0;
    free( sortednlp );
}

int
timecmp(const void *v1, const void *v2)
{
    const nltype * const *npp1 = v1;
    const nltype * const *npp2 = v2;

    if ((*npp2) -> time < (*npp1) -> time)
	return -1;
    if ((*npp2) -> time > (*npp1) -> time)
	return 1 ;
    if ((*npp2) -> ncall < (*npp1) -> ncall)
	return -1;
    if ((*npp2) -> ncall > (*npp1) -> ncall)
	return 1;
    return( strcmp( (*npp1) -> name , (*npp2) -> name ) );
}

    /*
     *	header for flatprofline
     */
void
flatprofheader()
{
    
    if (bflag)
	printblurb( _PATH_FLAT_BLURB );
    printf("\ngranularity: each sample hit covers %ld byte(s)",
	    (long) scale * sizeof(UNIT));
    if (totime > 0.0)
	printf(" for %.2f%% of %.2f seconds\n\n" , 100.0/totime, totime / hz);
    else {
	printf( " no time accumulated\n\n" );
	    /*
	     *	this doesn't hurt sinc eall the numerators will be zero.
	     */
	totime = 1.0;
    }
    printf("%5.5s %10.10s %8.8s %8.8s %8.8s %8.8s  %-8.8s\n" ,
	"%  " , "cumulative" , "self  " , "" , "self  " , "total " , "" );
    printf("%5.5s %10.10s %8.8s %8.8s %8.8s %8.8s  %-8.8s\n" ,
	"time" , "seconds " , "seconds" , "calls" ,
	"ms/call" , "ms/call" , "name" );
}

void
flatprofline(nltype *np)
{

    if ( zflag == 0 && np -> ncall == 0 && np -> time == 0 ) {
	return;
    }
    actime += np -> time;
    printf( "%5.1f %10.2f %8.2f" ,
	100 * np -> time / totime , actime / hz , np -> time / hz );
    if ( np -> ncall != 0 ) {
	printf( " %8ld %8.2f %8.2f  " , np -> ncall ,
	    1000 * np -> time / hz / np -> ncall ,
	    1000 * ( np -> time + np -> childtime ) / hz / np -> ncall );
    } else {
	printf( " %8.8s %8.8s %8.8s  " , "" , "" , "" );
    }
    printname( np );
    printf( "\n" );
}

void
gprofheader()
{

    if ( bflag ) {
	printblurb( _PATH_CALLG_BLURB );
    }
    printf( "\ngranularity: each sample hit covers %ld byte(s)" ,
	    (long) scale * sizeof(UNIT) );
    if ( printtime > 0.0 ) {
	printf( " for %.2f%% of %.2f seconds\n\n" ,
		100.0/printtime , printtime / hz );
    } else {
	printf( " no time propagated\n\n" );
	    /*
	     *	this doesn't hurt, since all the numerators will be 0.0
	     */
	printtime = 1.0;
    }
    printf( "%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n" ,
	"" , "" , "" , "" , "called" , "total" , "parents");
    printf( "%-6.6s %5.5s %7.7s %11.11s %7.7s+%-7.7s %-8.8s\t%5.5s\n" ,
	"index" , "%time" , "self" , "descendents" ,
	"called" , "self" , "name" , "index" );
    printf( "%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n" ,
	"" , "" , "" , "" , "called" , "total" , "children");
    printf( "\n" );
}

void
gprofline(nltype *np)
{
    char	kirkbuffer[ BUFSIZ ];

    snprintf(kirkbuffer, sizeof kirkbuffer, "[%d]" , np -> index );
    printf( "%-6.6s %5.1f %7.2f %11.2f" , kirkbuffer ,
	    100 * ( np -> propself + np -> propchild ) / printtime ,
	    np -> propself / hz , np -> propchild / hz );
    if ( ( np -> ncall + np -> selfcalls ) != 0 ) {
	printf( " %7ld" , np -> npropcall );
	if ( np -> selfcalls != 0 ) {
	    printf( "+%-7ld " , np -> selfcalls );
	} else {
	    printf( " %7.7s " , "" );
	}
    } else {
	printf( " %7.7s %7.7s " , "" , "" );
    }
    printname( np );
    printf( "\n" );
}

void
printgprof(nltype **timesortnlp)
{
    int		index;
    nltype	*parentp;

	/*
	 *	Print out the structured profiling list
	 */
    gprofheader();
    for ( index = 0 ; index < nname + ncycle ; index ++ ) {
	parentp = timesortnlp[ index ];
	if ( zflag == 0 &&
	     parentp -> ncall == 0 &&
	     parentp -> selfcalls == 0 &&
	     parentp -> propself == 0 &&
	     parentp -> propchild == 0 ) {
	    continue;
	}
	if ( ! parentp -> printflag ) {
	    continue;
	}
	if ( parentp -> name == 0 && parentp -> cycleno != 0 ) {
		/*
		 *	cycle header
		 */
	    printcycle( parentp );
	    printmembers( parentp );
	} else {
	    printparents( parentp );
	    gprofline( parentp );
	    printchildren( parentp );
	}
	printf( "\n" );
	printf( "-----------------------------------------------\n" );
	printf( "\n" );
    }
    free( timesortnlp );
}

    /*
     *	sort by decreasing propagated time
     *	if times are equal, but one is a cycle header,
     *		say that's first (e.g. less, i.e. -1).
     *	if one's name doesn't have an underscore and the other does,
     *		say the one is first.
     *	all else being equal, sort by names.
     */
int
totalcmp(const void *v1, const void *v2)
{
    const nltype *np1 = *(const nltype **)v1;
    const nltype *np2 = *(const nltype **)v2;
    double t1, t2;
    int np1noname, np2noname, np1cyclehdr, np2cyclehdr;

    t1 = np1 -> propself + np1 -> propchild;
    t2 = np2 -> propself + np2 -> propchild;
    if ( t2 > t1 )
	    return 1;
    if ( t2 < t1 )
	    return -1;

    np1noname = ( np1 -> name == 0 );
    np2noname = ( np2 -> name == 0 );
    np1cyclehdr = ( np1noname && np1 -> cycleno != 0 );
    np2cyclehdr = ( np2noname && np2 -> cycleno != 0 );

    if ( np1cyclehdr && !np2cyclehdr )
	return -1;
    else if ( !np1cyclehdr && np2cyclehdr )
	return 1;

    if ( np1noname && !np2noname )
	return -1;
    else if ( !np1noname && np2noname )
	return 1;
    else if ( np1noname && np2noname )
	return 0;

    if ( *(np1 -> name) != '_' && *(np2 -> name) == '_' )
	return -1;
    if ( *(np1 -> name) == '_' && *(np2 -> name) != '_' )
	return 1;
    if ( np1 -> ncall > np2 -> ncall )
	return -1;
    if ( np1 -> ncall < np2 -> ncall ) 
	return 1;
    return strcmp( np1 -> name , np2 -> name );
}

void
printparents(nltype *childp)
{
    nltype	*parentp;
    arctype	*arcp;
    nltype	*cycleheadp;

    if ( childp -> cyclehead != 0 ) {
	cycleheadp = childp -> cyclehead;
    } else {
	cycleheadp = childp;
    }
    if ( childp -> parents == 0 ) {
	printf( "%6.6s %5.5s %7.7s %11.11s %7.7s %7.7s     <spontaneous>\n" ,
		"" , "" , "" , "" , "" , "" );
	return;
    }
    sortparents( childp );
    for ( arcp = childp -> parents ; arcp ; arcp = arcp -> arc_parentlist ) {
	parentp = arcp -> arc_parentp;
	if ( childp == parentp || ( arcp -> arc_flags & DEADARC ) ||
	     ( childp->cycleno != 0 && parentp->cycleno == childp->cycleno ) ) {
		/*
		 *	selfcall or call among siblings
		 */
	    printf( "%6.6s %5.5s %7.7s %11.11s %7ld %7.7s     " ,
		    "" , "" , "" , "" ,
		    arcp -> arc_count , "" );
	    printname( parentp );
	    printf( "\n" );
	} else {
		/*
		 *	regular parent of child
		 */
	    printf( "%6.6s %5.5s %7.2f %11.2f %7ld/%-7ld     " ,
		    "" , "" ,
		    arcp -> arc_time / hz , arcp -> arc_childtime / hz ,
		    arcp -> arc_count , cycleheadp -> npropcall );
	    printname( parentp );
	    printf( "\n" );
	}
    }
}

void
printchildren(nltype *parentp)
{
    nltype	*childp;
    arctype	*arcp;

    sortchildren( parentp );
    arcp = parentp -> children;
    for ( arcp = parentp -> children ; arcp ; arcp = arcp -> arc_childlist ) {
	childp = arcp -> arc_childp;
	if ( childp == parentp || ( arcp -> arc_flags & DEADARC ) ||
	    ( childp->cycleno != 0 && childp->cycleno == parentp->cycleno ) ) {
		/*
		 *	self call or call to sibling
		 */
	    printf( "%6.6s %5.5s %7.7s %11.11s %7ld %7.7s     " ,
		    "" , "" , "" , "" , arcp -> arc_count , "" );
	    printname( childp );
	    printf( "\n" );
	} else {
		/*
		 *	regular child of parent
		 */
	    printf( "%6.6s %5.5s %7.2f %11.2f %7ld/%-7ld     " ,
		    "" , "" ,
		    arcp -> arc_time / hz , arcp -> arc_childtime / hz ,
		    arcp -> arc_count , childp -> cyclehead -> npropcall );
	    printname( childp );
	    printf( "\n" );
	}
    }
}

void
printname(nltype *selfp)
{

    if ( selfp -> name != 0 ) {
	printf( "%s" , selfp -> name );
#	ifdef DEBUG
	    if ( debug & DFNDEBUG ) {
		printf( "{%d} " , selfp -> toporder );
	    }
	    if ( debug & PROPDEBUG ) {
		printf( "%5.2f%% " , selfp -> propfraction );
	    }
#	endif /* DEBUG */
    }
    if ( selfp -> cycleno != 0 ) {
	printf( " <cycle %d>" , selfp -> cycleno );
    }
    if ( selfp -> index != 0 ) {
	if ( selfp -> printflag ) {
	    printf( " [%d]" , selfp -> index );
	} else {
	    printf( " (%d)" , selfp -> index );
	}
    }
}

void
sortchildren(nltype *parentp)
{
    arctype	*arcp;
    arctype	*detachedp;
    arctype	sorted;
    arctype	*prevp;

	/*
	 *	unlink children from parent,
	 *	then insertion sort back on to sorted's children.
	 *	    *arcp	the arc you have detached and are inserting.
	 *	    *detachedp	the rest of the arcs to be sorted.
	 *	    sorted	arc list onto which you insertion sort.
	 *	    *prevp	arc before the arc you are comparing.
	 */
    sorted.arc_childlist = 0;
    for (arcp = parentp -> children; arcp; arcp = detachedp) {
	detachedp = arcp -> arc_childlist;
	    /*
	     *	consider *arcp as disconnected
	     *	insert it into sorted
	     */
	for (   prevp = &sorted ;
		prevp -> arc_childlist ;
		prevp = prevp -> arc_childlist ) {
	    if ( arccmp( arcp , prevp -> arc_childlist ) != LESSTHAN ) {
		break;
	    }
	}
	arcp -> arc_childlist = prevp -> arc_childlist;
	prevp -> arc_childlist = arcp;
    }
	/*
	 *	reattach sorted children to parent
	 */
    parentp -> children = sorted.arc_childlist;
}

void
sortparents(nltype *childp)
{
    arctype	*arcp;
    arctype	*detachedp;
    arctype	sorted;
    arctype	*prevp;

	/*
	 *	unlink parents from child,
	 *	then insertion sort back on to sorted's parents.
	 *	    *arcp	the arc you have detached and are inserting.
	 *	    *detachedp	the rest of the arcs to be sorted.
	 *	    sorted	arc list onto which you insertion sort.
	 *	    *prevp	arc before the arc you are comparing.
	 */
    sorted.arc_parentlist = 0;
    for (arcp = childp->parents; arcp; arcp = detachedp) {
	detachedp = arcp->arc_parentlist;
	    /*
	     *	consider *arcp as disconnected
	     *	insert it into sorted
	     */
	for (prevp = &sorted; prevp->arc_parentlist;
	     prevp = prevp->arc_parentlist)
	    if (arccmp(arcp , prevp->arc_parentlist) != GREATERTHAN)
		break;
	arcp->arc_parentlist = prevp->arc_parentlist;
	prevp->arc_parentlist = arcp;
    }
	/*
	 *	reattach sorted arcs to child
	 */
    childp -> parents = sorted.arc_parentlist;
}

    /*
     *	print a cycle header
     */
void
printcycle(nltype *cyclep)
{
    char	kirkbuffer[ BUFSIZ ];

    snprintf(kirkbuffer, sizeof kirkbuffer, "[%d]" , cyclep->index);
    printf("%-6.6s %5.1f %7.2f %11.2f %7ld", kirkbuffer,
	    100 * (cyclep->propself + cyclep->propchild) / printtime,
	    cyclep->propself / hz, cyclep->propchild / hz, cyclep->npropcall);
    if (cyclep -> selfcalls != 0)
	printf("+%-7ld" , cyclep->selfcalls);
    else
	printf(" %7.7s" , "");
    printf(" <cycle %d as a whole>\t[%d]\n" ,
	    cyclep->cycleno , cyclep->index );
}

    /*
     *	print the members of a cycle
     */
void
printmembers(nltype *cyclep)
{
    nltype	*memberp;

    sortmembers( cyclep );
    for ( memberp = cyclep -> cnext ; memberp ; memberp = memberp -> cnext ) {
	printf( "%6.6s %5.5s %7.2f %11.2f %7ld" , "", "",
	  memberp->propself / hz, memberp->propchild / hz, memberp->npropcall );
	if (memberp -> selfcalls != 0)
	    printf("+%-7ld" , memberp -> selfcalls);
	else
	    printf(" %7.7s", "");
	printf("     ");
	printname(memberp);
	printf("\n");
    }
}

    /*
     *	sort members of a cycle
     */
void
sortmembers(nltype *cyclep)
{
    nltype	*todo;
    nltype	*doing;
    nltype	*prev;

	/*
	 *	detach cycle members from cyclehead,
	 *	and insertion sort them back on.
	 */
    todo = cyclep -> cnext;
    cyclep -> cnext = 0;
    for (doing = todo; doing; doing = todo) {
	todo = doing -> cnext;
	for (prev = cyclep; prev -> cnext; prev = prev -> cnext)
	    if (membercmp(doing, prev->cnext ) == GREATERTHAN)
		break;
	doing -> cnext = prev -> cnext;
	prev -> cnext = doing;
    }
}

    /*
     *	major sort is on propself + propchild,
     *	next is sort on ncalls + selfcalls.
     */
int
membercmp(nltype *this , nltype *that)
{
    double	thistime = this -> propself + this -> propchild;
    double	thattime = that -> propself + that -> propchild;
    long	thiscalls = this -> ncall + this -> selfcalls;
    long	thatcalls = that -> ncall + that -> selfcalls;

    if ( thistime > thattime ) {
	return GREATERTHAN;
    }
    if ( thistime < thattime ) {
	return LESSTHAN;
    }
    if ( thiscalls > thatcalls ) {
	return GREATERTHAN;
    }
    if ( thiscalls < thatcalls ) {
	return LESSTHAN;
    }
    return EQUALTO;
}
    /*
     *	compare two arcs to/from the same child/parent.
     *	- if one arc is a self arc, it's least.
     *	- if one arc is within a cycle, it's less than.
     *	- if both arcs are within a cycle, compare arc counts.
     *	- if neither arc is within a cycle, compare with
     *		arc_time + arc_childtime as major key
     *		arc count as minor key
     */
int
arccmp(arctype *thisp, arctype *thatp)
{
    nltype	*thisparentp = thisp -> arc_parentp;
    nltype	*thischildp = thisp -> arc_childp;
    nltype	*thatparentp = thatp -> arc_parentp;
    nltype	*thatchildp = thatp -> arc_childp;
    double	thistime;
    double	thattime;

#   ifdef DEBUG
	if ( debug & TIMEDEBUG ) {
	    printf( "[arccmp] " );
	    printname( thisparentp );
	    printf( " calls " );
	    printname ( thischildp );
	    printf( " %f + %f %ld/%ld\n" ,
		    thisp -> arc_time , thisp -> arc_childtime ,
		    thisp -> arc_count , thischildp -> ncall );
	    printf( "[arccmp] " );
	    printname( thatparentp );
	    printf( " calls " );
	    printname( thatchildp );
	    printf( " %f + %f %ld/%ld\n" ,
		    thatp -> arc_time , thatp -> arc_childtime ,
		    thatp -> arc_count , thatchildp -> ncall );
	    printf( "\n" );
	}
#   endif /* DEBUG */
    if ( thisparentp == thischildp ) {
	    /* this is a self call */
	return LESSTHAN;
    }
    if ( thatparentp == thatchildp ) {
	    /* that is a self call */
	return GREATERTHAN;
    }
    if ( thisparentp -> cycleno != 0 && thischildp -> cycleno != 0 &&
	thisparentp -> cycleno == thischildp -> cycleno ) {
	    /* this is a call within a cycle */
	if ( thatparentp -> cycleno != 0 && thatchildp -> cycleno != 0 &&
	    thatparentp -> cycleno == thatchildp -> cycleno ) {
		/* that is a call within the cycle, too */
	    if ( thisp -> arc_count < thatp -> arc_count ) {
		return LESSTHAN;
	    }
	    if ( thisp -> arc_count > thatp -> arc_count ) {
		return GREATERTHAN;
	    }
	    return EQUALTO;
	} else {
		/* that isn't a call within the cycle */
	    return LESSTHAN;
	}
    } else {
	    /* this isn't a call within a cycle */
	if ( thatparentp -> cycleno != 0 && thatchildp -> cycleno != 0 &&
	    thatparentp -> cycleno == thatchildp -> cycleno ) {
		/* that is a call within a cycle */
	    return GREATERTHAN;
	} else {
		/* neither is a call within a cycle */
	    thistime = thisp -> arc_time + thisp -> arc_childtime;
	    thattime = thatp -> arc_time + thatp -> arc_childtime;
	    if ( thistime < thattime )
		return LESSTHAN;
	    if ( thistime > thattime )
		return GREATERTHAN;
	    if ( thisp -> arc_count < thatp -> arc_count )
		return LESSTHAN;
	    if ( thisp -> arc_count > thatp -> arc_count )
		return GREATERTHAN;
	    return EQUALTO;
	}
    }
}

void
printblurb(const char *blurbname)
{
    FILE	*blurbfile;
    int		input;

    blurbfile = fopen( blurbname , "r" );
    if ( blurbfile == NULL ) {
	warn("fopen: %s", blurbname );
	return;
    }
    while ( ( input = getc( blurbfile ) ) != EOF )
	putchar( input );

    fclose( blurbfile );
}

int
namecmp(const void *v1, const void *v2)
{
    const nltype * const *npp1 = v1;
    const nltype * const *npp2 = v2;

    return( strcmp( (*npp1) -> name , (*npp2) -> name ) );
}

void
printindex()
{
    nltype		**namesortnlp;
    nltype		*nlp;
    int			index, nnames, todo, i, j;
    char		peterbuffer[ BUFSIZ ];

	/*
	 *	Now, sort regular function name alphbetically
	 *	to create an index.
	 */
    namesortnlp = calloc( nname + ncycle , sizeof(nltype *) );
    if ( namesortnlp == (nltype **) 0 )
	warnx("ran out of memory for sorting");
    for ( index = 0 , nnames = 0 ; index < nname ; index++ ) {
	if ( zflag == 0 && nl[index].ncall == 0 && nl[index].time == 0 )
		continue;
	namesortnlp[nnames++] = &nl[index];
    }
    qsort( namesortnlp , nnames , sizeof(nltype *) , namecmp );
    for ( index = 1 , todo = nnames ; index <= ncycle ; index++ ) {
	namesortnlp[todo++] = &cyclenl[index];
    }
    printf( "\f\nIndex by function name\n\n" );
    index = ( todo + 2 ) / 3;
    for ( i = 0; i < index ; i++ ) {
	for ( j = i; j < todo ; j += index ) {
	    nlp = namesortnlp[ j ];
	    if ( nlp -> printflag ) {
		snprintf(peterbuffer, sizeof peterbuffer, "[%d]" , nlp -> index );
	    } else {
		snprintf(peterbuffer, sizeof peterbuffer, "(%d)" , nlp -> index );
	    }
	    if ( j < nnames ) {
		printf( "%6.6s %-19.19s" , peterbuffer , nlp -> name );
	    } else {
		printf( "%6.6s " , peterbuffer );
		snprintf(peterbuffer, sizeof peterbuffer, "<cycle %d>"
		    , nlp -> cycleno );
		printf( "%-19.19s" , peterbuffer );
	    }
	}
	printf( "\n" );
    }
    free( namesortnlp );
}
