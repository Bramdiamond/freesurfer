#if defined(USE_LOCAL_MINC)

#ifndef  MINC_BASIC_HEADER_FILE
#define  MINC_BASIC_HEADER_FILE

/* ----------------------------- MNI Header -----------------------------------
@NAME       : minc_basic.h
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Constants and macros for private use by MINC routines.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : August 28, 1992 (Peter Neelin)
@MODIFIED   : 
 * $Log: minc_basic.h,v $
 * Revision 6.2  2001/04/17 18:40:13  neelin
 * Modifications to work with NetCDF 3.x
 * In particular, changed NC_LONG to NC_INT (and corresponding longs to ints).
 * Changed NC_UNSPECIFIED to NC_NAT.
 * A few fixes to the configure script.
 *
 * Revision 6.1  1999/10/19 14:45:08  neelin
 * Fixed Log subsitutions for CVS
 *
 * Revision 6.0  1997/09/12 13:24:54  neelin
 * Release of minc version 0.6
 *
 * Revision 5.0  1997/08/21  13:25:53  neelin
 * Release of minc version 0.5
 *
 * Revision 4.0  1997/05/07  20:07:52  neelin
 * Release of minc version 0.4
 *
 * Revision 3.0  1995/05/15  19:33:12  neelin
 * Release of minc version 0.3
 *
 * Revision 2.0  1994/09/28  10:38:01  neelin
 * Release of minc version 0.2
 *
 * Revision 1.8  94/09/28  10:37:26  neelin
 * Pre-release
 * 
 * Revision 1.7  93/10/28  10:18:23  neelin
 * Added FILLVALUE_EPSILON for doing fillvalue checking in icv's.
 * 
 * Revision 1.6  93/08/11  12:06:37  neelin
 * Added RCS logging in source.
 * 
@COPYRIGHT  :
              Copyright 1993 Peter Neelin, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.
@RCSID      : $Header: /private-cvsroot/minc/libsrc/minc_basic.h,v 6.2 2001/04/17 18:40:13 neelin Exp $ MINC (MNI)
---------------------------------------------------------------------------- */

#include <math.h>

static long longMIN(long lhs, long rhs) {
    return (lhs < rhs) ? lhs : rhs;
}
static long longMAX(long lhs, long rhs) {
    return (lhs > rhs) ? lhs : rhs;
}

static double doubleMIN(double lhs, double rhs) {
    return (lhs < rhs) ? lhs : rhs;
}
static double doubleMAX(double lhs, double rhs) {
    return (lhs > rhs) ? lhs : rhs;
}

#define MI_PRIV_DEFSIGN   0
#define MI_PRIV_SIGNED    1
#define MI_PRIV_UNSIGNED  2

#define MI_PRIV_GET 10
#define MI_PRIV_PUT 11

/* Epsilon for detecting fillvalues */
#define FILLVALUE_EPSILON (10.0 * FLT_EPSILON)

#define MI_TO_DOUBLE(dvalue, type, sign, ptr) \
   switch (type) { \
   case NC_BYTE : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned char *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed char *) ptr); break; \
      } \
      break; \
   case NC_SHORT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned short *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed short *) ptr); break; \
      } \
      break; \
   case NC_INT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned int *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed int  *) ptr); break; \
      } \
      break; \
   case NC_FLOAT : \
      dvalue = (double) *((float *) ptr); \
      break; \
   case NC_DOUBLE : \
      dvalue = (double) *((double *) ptr); \
      break; \
   default: \
      fprintf(stderr,"%s:%d no default\n", __FILE__,__LINE__); exit(1); \
   } 

#define MI_FROM_DOUBLE(dvalue, type, sign, ptr) \
   switch (type) { \
   case NC_BYTE : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = doubleMAX(0, dvalue); \
         dvalue = doubleMIN(UCHAR_MAX, dvalue); \
         *((unsigned char *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = doubleMAX(SCHAR_MIN, dvalue); \
         dvalue = doubleMIN(SCHAR_MAX, dvalue); \
         *((signed char *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_SHORT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = doubleMAX(0, dvalue); \
         dvalue = doubleMIN(USHRT_MAX, dvalue); \
         *((unsigned short *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = doubleMAX(SHRT_MIN, dvalue); \
         dvalue = doubleMIN(SHRT_MAX, dvalue); \
         *((signed short *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_INT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = doubleMAX(0, dvalue); \
         dvalue = doubleMIN(UINT_MAX, dvalue); \
         *((unsigned int *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = doubleMAX(INT_MIN, dvalue); \
         dvalue = doubleMIN(INT_MAX, dvalue); \
         *((signed int *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_FLOAT : \
      dvalue = doubleMAX(-FLT_MAX,dvalue); \
      *((float *) ptr) = doubleMIN(FLT_MAX,dvalue); \
      break; \
   case NC_DOUBLE : \
      *((double *) ptr) = dvalue; \
      break; \
   default: \
      fprintf(stderr,"%s:%d no default\n", __FILE__,__LINE__); exit(1); \
   }




#if 0	// BEVIN - things not shown to be needed by freesurfer


/* --------- MINC specific constants ------------- */

/* Maximum buffer size for conversions. Should not be a power of 2 - this
   can cause poor performance on some systems (e.g. SGI) due to caching-
   related inefficiencies */
#define MI_MAX_VAR_BUFFER_SIZE 10000

/* Possible values for sign of a value */
//#define MI_PRIV_DEFSIGN   0
//#define MI_PRIV_SIGNED    1
//#define MI_PRIV_UNSIGNED  2

/* Operations for MI_varaccess */
//#define MI_PRIV_GET 10
//#define MI_PRIV_PUT 11

/* Suffix for dimension width variable names */
#define MI_WIDTH_SUFFIX "-width"

/* Epsilon for detecting fillvalues */
//#define FILLVALUE_EPSILON (10.0 * FLT_EPSILON)

/* NetCDF routine name variable (for error logging) */
extern char *cdf_routine_name ; /* defined in globdef.c */
#define MI_NC_ROUTINE_VAR cdf_routine_name

/* MINC routine name variable, call depth counter (for keeping track of
   minc routines calling minc routines) and variable for keeping track
   of callers ncopts. All of these are for error logging. They are
   defined in minc_globdef.c */
extern char *minc_routine_name;
extern int minc_call_depth;
extern int minc_callers_ncopts;
extern int minc_trash_var;        /* Just for getting rid of lint messages */
#define MI_ROUTINE_VAR minc_routine_name
#define MI_CALL_DEPTH minc_call_depth
#define MI_CALLERS_NCOPTS minc_callers_ncopts
#define MI_TRASH_VAR minc_trash_var

/* Macros for logging errors. All routines should start with MI_SAVE_ROUTINE
   and exit with MI_RETURN (which includes MI_RETURN_ERROR and 
   MI_CHK_ERROR). All the macros except MI_CHK_ERROR are single line
   commands. MI_CHK_ERROR is in a block and so should not be followed by
   a ';' */
#define MI_SAVE_ROUTINE_NAME(name) \
   (MI_TRASH_VAR = (((MI_CALL_DEPTH++)==0) ? \
       MI_save_routine_name(name) : MI_NOERROR))
#define MI_RETURN(value) \
   return( (((--MI_CALL_DEPTH)!=0) || MI_return()) ? (value) : (value))
#define MI_RETURN_ERROR(error) \
   return( (((--MI_CALL_DEPTH)!=0) || MI_return_error()) ? (error) : (error))
#define MI_LOG_PKG_ERROR2(p1,p2) MI_log_pkg_error2(p1, p2)
#define MI_LOG_PKG_ERROR3(p1,p2,p3) MI_log_pkg_error3(p1, p2, p3)
#define MI_LOG_SYS_ERROR1(p1) MI_log_sys_error1(p1)
#define MI_CHK_ERR(expr) {if ((expr)==MI_ERROR) MI_RETURN_ERROR(MI_ERROR);}

/* Macros for converting data types. These macros are compound statements, 
   so don't put a semi-colon after them. dvalue should be a double, type
   is an int NetCDF type, sign is one of MI_PRIV_UNSIGNED and 
   MI_PRIV_SIGNED and ptr is a void pointer */

#if 0	// moved outside this unused area
#define MI_TO_DOUBLE(dvalue, type, sign, ptr) \
   switch (type) { \
   case NC_BYTE : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned char *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed char *) ptr); break; \
      } \
      break; \
   case NC_SHORT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned short *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed short *) ptr); break; \
      } \
      break; \
   case NC_INT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = (double) *((unsigned int *) ptr); break; \
      case MI_PRIV_SIGNED : \
         dvalue = (double) *((signed int  *) ptr); break; \
      } \
      break; \
   case NC_FLOAT : \
      dvalue = (double) *((float *) ptr); \
      break; \
   case NC_DOUBLE : \
      dvalue = (double) *((double *) ptr); \
      break; \
   } 

#define MI_FROM_DOUBLE(dvalue, type, sign, ptr) \
   switch (type) { \
   case NC_BYTE : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = MAX(0, dvalue); \
         dvalue = MIN(UCHAR_MAX, dvalue); \
         *((unsigned char *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = MAX(SCHAR_MIN, dvalue); \
         dvalue = MIN(SCHAR_MAX, dvalue); \
         *((signed char *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_SHORT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = MAX(0, dvalue); \
         dvalue = MIN(USHRT_MAX, dvalue); \
         *((unsigned short *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = MAX(SHRT_MIN, dvalue); \
         dvalue = MIN(SHRT_MAX, dvalue); \
         *((signed short *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_INT : \
      switch (sign) { \
      case MI_PRIV_UNSIGNED : \
         dvalue = MAX(0, dvalue); \
         dvalue = MIN(UINT_MAX, dvalue); \
         *((unsigned int *) ptr) = round(dvalue); \
         break; \
      case MI_PRIV_SIGNED : \
         dvalue = MAX(INT_MIN, dvalue); \
         dvalue = MIN(INT_MAX, dvalue); \
         *((signed int *) ptr) = round(dvalue); \
         break; \
      } \
      break; \
   case NC_FLOAT : \
      dvalue = MAX(-FLT_MAX,dvalue); \
      *((float *) ptr) = MIN(FLT_MAX,dvalue); \
      break; \
   case NC_DOUBLE : \
      *((double *) ptr) = dvalue; \
      break; \
   }
#endif

#endif

#endif // 0
#endif
