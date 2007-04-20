/**
 * @file  tiff_write_image.c
 * @brief Tests a call to MRItoImageView, ImageWrite, and TiffWriteImage
 *
 * This is meant to generate a TIFF from a volume. It can be used to
 * test if the output of TiffWriteImage in imageio.c produces valid
 * TIFFs. It calls MRItoImageView to make an Image, then ImageWrite
 * with a .tif filename to call TiffWriteImage.
 */
/*
 * Original Author: Kevin Teich
 * CVS Revision Info:
 *    $Author: kteich $
 *    $Date: 2007/04/20 21:12:50 $
 *    $Revision: 1.1 $
 *
 * Copyright (C) 2002-2007,
 * The General Hospital Corporation (Boston, MA). 
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 * Bug reports: analysis-bugs@nmr.mgh.harvard.edu
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "mri.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "version.h"
#include "mrimorph.h"
#include "mri_circulars.h"

char *Progname;

int main ( int argc, char** argv ) 
{

  int nargs;
  char mri_fname[STRLEN];
  char tiff_fname[STRLEN];
  MRI* mri;
  int slice;
  IMAGE* image;

  Progname = argv[0];

  nargs = handle_version_option (argc, argv, "$Id: tiff_write_image.c,v 1.1 2007/04/20 21:12:50 kteich Exp $", "$Name:  $");
  argc -= nargs ;
  if (1 == argc)
    exit (0);

  if (3 != argc) {
    printf ("usage: %s <in vol> <out tiff>\n", Progname);
    printf ("  <in vol> is a MRIread-able volume file, and <out tiff> is the\n"
	    "  name of a TIFF file to write.");
    printf ("\n");
    exit (0);
  }

  strncpy (mri_fname, argv[1], sizeof(mri_fname));
  strncpy (tiff_fname, argv[2], sizeof(tiff_fname));

  mri = MRIread (mri_fname);
  if (!mri)
    ErrorExit(ERROR_BADPARM, "%s: could not read source volume %s",
              Progname, mri_fname) ;

  slice = mri->width / 2;

  image = MRItoImageView (mri, NULL, slice, MRI_CORONAL, 0);
  if (!image)
    ErrorExit(Gerror, "MRItoImageView failed");

  ImageWrite (image, tiff_fname);
  ImageFree (&image);

  exit (0);
}
