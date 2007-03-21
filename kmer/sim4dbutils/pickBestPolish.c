#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bio.h"
#include "sim4.H"

//  Picks the best polish (or set of polishes that are all of the same
//  best quality) for each cDNA.
//
//  Validate mode will print out ALL input matches, in the following
//  format
//
//  estid gaid nummatches percentid (genFr genTo %) () ()
//
//  With a * somewhere to denote the best ones.  Separate ESTs with
//  a dashed line.


#define    EPS_X	1
#define    EPS_N_ESTS	10
#define    EPS_N_MRNA	15
#define    EPS_I	3


int EPS_N       = EPS_N_ESTS;
int doValidate  = 0;


static
void
printPolishValidate(FILE *O, sim4polish *p, int isBest) {
  int i;

  fprintf(O, u32bitFMTW(8)" "u32bitFMTW(8)" "u32bitFMTW(4)" "u32bitFMTW(4), p->estID, p->genID, p->percentIdentity, p->numMatches);

  for (i=0; i<p->numExons; i++)
    fprintf(O, " ("u32bitFMTW(6)"/"u32bitFMTW(6)" "u32bitFMTW(6)"/"u32bitFMTW(6)" "u32bitFMTW(3)")",
            p->exons[i].estFrom, p->exons[i].genFrom,
            p->exons[i].estTo,   p->exons[i].genTo,
            p->exons[i].percentIdentity);

  if (isBest)
    fprintf(O, " *");

  fprintf(O, "\n");
}


static
void
pickBestSlave(sim4polish **p, int pNum) {
  int           i = 0;
  int           identitym = 0, nmatchesm = 0;  //  Best score for the mList
  int           identityi = 0, nmatchesi = 0;  //  Best score the the iList
  int           numExons = 0, numExonsi = 0, numExonsm = 0;
  int		tmp_nmatches = 0;
  double        alpha;

  //  Difficult choice here....
  //
  if (pNum == 1) {
    if (doValidate == 0)
      s4p_printPolish(stdout, p[0], S4P_PRINTPOLISH_FULL);
    return;
  }

  if ((p[0]->estID % 1287) == 0) {
    fprintf(stderr, "Picking Best for estID="u32bitFMT" with %5d choices.\r", p[0]->estID, pNum);
    fflush(stderr);
  }

  //  Find the best percentIdentity and best numberOfMatches.  
  //
  //  identityi is the best percent identity of all the matches for this EST, and
  //  nmatchesi is the number of matches for the longest best identity match(es).
  //
  //  nmatchesm is the best numMatches of all the matches for this EST, and 
  //  identitym is the highest percent identity for the best numMatches match(es).

  for (i=0; i<pNum; i++) {

    if ((p[i]->percentIdentity > identityi) || 
        (p[i]->percentIdentity == identityi && p[i]->numMatches > nmatchesi)) {
      identityi = p[i]->percentIdentity;
      nmatchesi = p[i]->numMatches;
    }
   
    if ((p[i]->numMatches > nmatchesm) ||
        (p[i]->numMatches == nmatchesm && p[i]->percentIdentity > identitym)) {
      nmatchesm = p[i]->numMatches;
      identitym = p[i]->percentIdentity;
    }

  }


  //  Otherwise, if the best scores on both lists are the same, pick
  //  the matches with the largest number of exons
  //
  if ((identityi == identitym) &&
      (nmatchesi == nmatchesm)) {

    //  Find the largest number of exons, allowing some margin in numMatches
    //
    numExonsi = 0;
    for (i=0; i<pNum; i++) 
      if ((p[i]->percentIdentity == identityi) &&
          (p[i]->numMatches >= nmatchesi) &&
          (numExonsi < p[i]->numExons))
          numExonsi = p[i]->numExons;

    numExons = numExonsi;
    tmp_nmatches = nmatchesi;
    for (i=0; i<pNum; i++)
      if ((p[i]->percentIdentity == identityi) &&
          (p[i]->numMatches >= nmatchesi - EPS_N) &&
          (numExons < p[i]->numExons - EPS_X)) {
          tmp_nmatches = p[i]->numMatches;
          numExons = p[i]->numExons;
      }

    //  Scan the entire list, printing the best stuff.  We cannot just
    //  scan both the mList and iList, as those probably contain
    //  duplicates.

    if (doValidate) {
      if (tmp_nmatches == nmatchesi) 
        fprintf(stdout, "--------------------1 (Clear Winner)\n");
      else
        fprintf(stdout, "--------------------2 (Exon Clear Winner)\n");
      for (i=0; i<pNum; i++)
        printPolishValidate(stdout, p[i], ((p[i]->percentIdentity == identityi) &&
                                           (p[i]->numMatches == tmp_nmatches) &&
                                           (p[i]->numExons == numExons)));
    } else {
      for (i=0; i<pNum; i++)
        if ((p[i]->percentIdentity == identityi) &&
            (p[i]->numMatches == tmp_nmatches) &&
            (p[i]->numExons == numExons))
          s4p_printPolish(stdout, p[i], S4P_PRINTPOLISH_FULL);
    }

    return;
  }

  //  Start over.  Find the best two percentIdentities.  Break ties
  //  with numMatches.
  //
  //  i will be the best,
  //  m will be the second best
  //
  identityi = identitym = 0;
  nmatchesi = nmatchesm = 0;

  for (i=0; i<pNum; i++) {

    //  Pick the two matches with the highest (different) percent
    //  identities; for each, pick the highest number of matches.
    //
    //  First block:  Have we found a new best percent identity?
    //  If so, save it, and shift former best to second best.
    //
    //  Second and third blocks: make sure that we save the
    //  best numMatches for each.
    //
    if (p[i]->percentIdentity > identityi) {
      identitym = identityi;
      nmatchesm = nmatchesi;

      identityi = p[i]->percentIdentity;
      nmatchesi = p[i]->numMatches;
    } else if ((p[i]->percentIdentity == identityi) &&
               (p[i]->numMatches > nmatchesi)) {
      nmatchesi = p[i]->numMatches;
    } else if ((p[i]->percentIdentity < identityi) &&
               ((p[i]->percentIdentity > identitym) ||
                ((p[i]->percentIdentity == identitym) &&
                (p[i]->numMatches       > nmatchesm)))) {
      nmatchesm = p[i]->numMatches;
      identitym = p[i]->percentIdentity;
    }
  }

  //  Now, 'i' is the highest percent identity, 'm' is the second
  //  highest.  By definition, numMatches for 'i' is less than
  //  numMatches for 'm'.

  //  If the number of matches is different, output everything with the
  //  top score.
  //
  //  We are guaranteed that the identities are the same.  (I think)

  if (nmatchesi >= nmatchesm) {

    //  Find the match(es) with the largest number of exons
    
    numExonsi = 0;
    for (i=0; i<pNum; i++) 
      if ((p[i]->percentIdentity == identityi) &&
          (p[i]->numMatches >= nmatchesi) &&
          (numExonsi < p[i]->numExons)) 
          numExonsi = p[i]->numExons;


    numExons = numExonsi;
    tmp_nmatches = nmatchesi;
    for (i=0; i<pNum; i++)
      if ((p[i]->percentIdentity == identityi) &&
          (p[i]->numMatches >= nmatchesi - EPS_N) &&
          (numExons < p[i]->numExons - EPS_X)) {
        numExons = p[i]->numExons;
        tmp_nmatches = p[i]->numMatches;
      }

    if (doValidate) {
      if (tmp_nmatches == nmatchesi)
        fprintf(stdout, "--------------------3 (?)\n");
      else 
        fprintf(stdout, "--------------------4 (Exon ?)\n");
      for (i=0; i<pNum; i++)
        printPolishValidate(stdout, p[i], ((p[i]->percentIdentity == identityi) &&
                                           (p[i]->numMatches      == tmp_nmatches) &&
                                           (p[i]->numExons        == numExons)));
    } else {
      for (i=0; i<pNum; i++)
        if ((p[i]->percentIdentity == identityi) &&
            (p[i]->numMatches      == tmp_nmatches) &&
            (p[i]->numExons	 == numExons))
          s4p_printPolish(stdout, p[i], S4P_PRINTPOLISH_FULL);
    }

    return;
  }

  //  Otherwise, compute alpha

  alpha = ((nmatchesm - nmatchesi) / 
           ((nmatchesm / (double)identitym) -
            (nmatchesi / (double)identityi)))/100;

  //  If alpha below a magic threshold, pick the shorter match.
  //
  if (alpha < 0.8) {

    //  Find the match(es) with the largest number of exons

    numExons = tmp_nmatches = 0;
    for (i=0; i<pNum; i++)
      if ((p[i]->percentIdentity == identityi) &&
          (p[i]->numMatches >= nmatchesi) &&
          (numExons < p[i]->numExons))
           numExons = p[i]->numExons;

    if (doValidate) {
      fprintf(stdout, "--------------------5 (alpha < 0.8)\n");
      for (i=0; i<pNum; i++)
        printPolishValidate(stdout, p[i], ((p[i]->percentIdentity == identityi) &&
                                           (p[i]->numMatches      == nmatchesi) &&
                                           (p[i]->numExons	== numExons)));
    } else {
      for (i=0; i<pNum; i++)
        if ((p[i]->percentIdentity == identityi) &&
            (p[i]->numMatches      == nmatchesi) &&
            (p[i]->numExons 	 == numExons))
          s4p_printPolish(stdout, p[i], S4P_PRINTPOLISH_FULL);
    }

    return;
  }

  //  Otherwise, pick the longer one.

  //  XXX: We can still check:
  //    if an internal gap is in N's
  //    the number of exons
  //    etc, etc.



  //  See if the smaller one has an internal gap that corresponds to
  //  N's in the genome.  If so, assume that the exon mapped to the
  //  N's and pick the smaller.
  //
  //  Need code to process genome, finding N's larger than some threshold.
  //  Output as 'genID beg end'


  //  Find the largest number of exons for each of the contenders
   
  numExonsi = numExonsm = 0;
  for (i=0; i<pNum; i++) {
    if ((p[i]->percentIdentity == identitym) &&
        (p[i]->numMatches == nmatchesm) &&
        (numExonsm < p[i]->numExons))
         numExonsm = p[i]->numExons;
    else if ((p[i]->percentIdentity == identityi) &&
        (p[i]->numMatches == nmatchesi) &&
        (numExonsi < p[i]->numExons))
         numExonsi = p[i]->numExons;
  }

  if ((numExonsi > numExonsm + EPS_X) || (identityi > identitym + EPS_I)) {

    if (doValidate) {
      if (numExonsi > numExonsm + EPS_X)
        fprintf(stdout, "--------------------6 (Exon Plus alpha > 0.8)\n");
      else 
        fprintf(stdout, "--------------------7 (Pctid Plus alpha > 0.8)\n");

      for (i=0; i<pNum; i++)
        printPolishValidate(stdout, p[i], ((p[i]->percentIdentity == identityi) &&
                                           (p[i]->numMatches      == nmatchesi) &&
                                           (p[i]->numExons	      == numExonsi)));
    } else {
      for (i=0; i<pNum; i++)
        if ((p[i]->percentIdentity == identityi) &&
            (p[i]->numMatches      == nmatchesi) &&
            (p[i]->numExons	       == numExonsi))
          s4p_printPolish(stdout, p[i], S4P_PRINTPOLISH_FULL);
    }
  } else {
    numExons = numExonsm;
    tmp_nmatches = nmatchesm;
    for (i=0; i<pNum; i++)
      if ((p[i]->percentIdentity == identitym) &&
          (p[i]->numMatches >= nmatchesm - EPS_N) &&
          (numExons < p[i]->numExons - EPS_X)) {
          tmp_nmatches = p[i]->numMatches;
          numExons = p[i]->numExons;
      }
 
    if (doValidate) {
      if (numExons == numExonsm)
        fprintf(stdout, "--------------------8 (alpha > 0.8)\n");
      else 
        fprintf(stdout, "--------------------9 (Exon alpha > 0.8)\n");
      for (i=0; i<pNum; i++)
        printPolishValidate(stdout, p[i], ((p[i]->percentIdentity == identitym) &&
                                           (p[i]->numMatches      == tmp_nmatches) &&
                                           (p[i]->numExons        == numExons)));
    } else {
      for (i=0; i<pNum; i++)
        if ((p[i]->percentIdentity == identitym) &&
            (p[i]->numMatches      == tmp_nmatches) &&
            (p[i]->numExons        == numExons))
          s4p_printPolish(stdout, p[i], S4P_PRINTPOLISH_FULL);
    }
  }
}


//  Just a wrapper around the real best picker, so that we can easily
//  destroy polishes when we're done.
//
static
void
pickBest(sim4polish **p, int pNum) {
  int i;

  pickBestSlave(p, pNum);

  for (i=0; i<pNum; i++)
    s4p_destroyPolish(p[i]);
}


int
main(int argc, char **argv) {
  int          pNum   = 0;
  int          pAlloc = 8388608;
  sim4polish **p      = 0L;
  sim4polish  *q      = 0L;
  int          estID  = ~0;

  int arg = 1;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-n", 2) == 0) {
      pAlloc = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-mrna", 2) == 0) {
      EPS_N = EPS_N_MRNA;
    } else if (strncmp(argv[arg], "-ests", 2) == 0) {
      EPS_N = EPS_N_ESTS;
    } else if (strncmp(argv[arg], "-validate", 2) == 0) {
      doValidate = 1;
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[arg]);
    }
    arg++;
  }

  if (isatty(fileno(stdin))) {
    fprintf(stderr, "usage: %s [-mrna|-ests] [-validate] < file > file\n", argv[0]);

    if (isatty(fileno(stdin)))
      fprintf(stderr, "error: I cannot read polishes from the terminal!\n\n");

    exit(1);
  }

  //  Read polishes, picking the best when we see a change in the
  //  estID.

  p = (sim4polish **)malloc(sizeof(sim4polish *) * pAlloc);

  while ((q = s4p_readPolish(stdin)) != 0L) {
    if ((q->estID != estID) && (pNum > 0)) {
      pickBest(p, pNum);
      pNum  = 0;
    }

    //  Reallocate pointers?
    //
    if (pNum >= pAlloc) {
      p = (sim4polish **)realloc(p, sizeof(sim4polish *) * (pAlloc *= 2));
      if (p == 0L) {
        fprintf(stderr, "Out of memory: Couldn't allocate space for polish pointers.\n");
        exit(1);
      }
    }

    p[pNum++] = q;
    estID     = q->estID;
  }

  if (pNum > 0)
    pickBest(p, pNum);

  return(0);
}

