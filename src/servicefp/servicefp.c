/*
** Copyright (C) 2009 Redpill Linpro, AS.
** Copyright (C) 2009 Edward Fjellskål <edward.fjellskaal@redpill-linpro.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* $Id$ */

/* servicefp 
 * 
 * Purpose:
 *
 * This file holds essential functions for the service fingerprinting
 *
 * Arguments:
 *   
 * *NONE
 *
 * Effect:
 *
 * HOLDS all the stuff that needs to be initialized.
 *
 * Comments:
 *
 * Old school...
 */

/* ----------------------------------------------------------
 * FUNCTION     : strlcpy
 * DESCRIPTION  : Replacement for strncpy.  This function is
 *              : native in *BSD.  This function was taken
 *              : from Secure Programming Cookbook by
 *              : O'Reilly.
 * INPUT        : 0 - Destination String
 *              : 1 - Source String
 *              : 2 - Size
 * RETURN       : Length of String Created
 *  ---------------------------------------------------------- */
#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t size) {
  char       *dstptr = dst;
  size_t     tocopy  = size;
  const char *srcptr = src;

  if (tocopy && --tocopy) {
    do {
      if (!(*dstptr++ = *srcptr++)) break;
    } while (--tocopy);
  }
  if (!tocopy) {
    if (size) *dstptr = 0;
    while (*srcptr++);
  }

  return (srcptr - src - 1);
}
#endif

/* ----------------------------------------------------------
 * FUNCTION     : strlcat
 * DESCRIPTION  : Replacement for strcat.  This function is
 *              : native in *BSD.  This function was taken
 *              : from Secure Programming Cookbook by
 *              : O'Reilly.
 * INPUT        : 0 - Destination String
 *              : 1 - Source String
 *              : 2 - Size
 * RETURN       : Length of String Created
 * ---------------------------------------------------------- */
#ifndef HAVE_STRLCAT
size_t
strlcat(char *dst, const char *src, size_t len) {
  char       *dstptr = dst;
  size_t     dstlen, tocopy = len;
  const char *srcptr = src;

  while (tocopy-- && *dstptr) dstptr++;
  dstlen = dstptr - dst;
  if (!(tocopy = len - dstlen)) return (dstlen + strlen(src));
  while (*srcptr) {
    if (tocopy != 1) {
      *dstptr++ = *srcptr;
      tocopy--;
    }
    srcptr++;
  }
  *dstptr = 0;

  return (dstlen + (srcptr - src));
}
#endif

/* ----------------------------------------------------------
 * FUNCTION     : init_identification
 * DESCRIPTION  : This function will read the signature file
 *              : into the signature data structure.
 * INPUT        : 0 - Data Structure
 * RETURN       : -1 - Error
 *              : 0 - Normal Return
 * ---------------------------------------------------------- */
int load_servicefp_file(int storage, char *sigfile) {

   FILE *fp;
   bstring filename;
   bstring filedata;
   struct bstrList *lines;
   int i;

   /* Check for a PADS_SIGNATURE_LIST file within the current directory.  */
   if ((fp = fopen(TCP_SIGNATURE_LIST, "r")) != NULL) {
      filename = bformat("./%s", sigfile);
      fclose(fp);
   } else {
      filename = bformat(sigfile);
   }

   /* Open Signature File */
   if ((fp = fopen((char *)bdata(filename), "r")) == NULL) {
      printf("Unable to open signature file - %s", bdata(filename));
      return 1;
   }

   /* Read file into 'filedata' and process it accordingly. */
   filedata = bread ((bNread) fread, fp);
   if ((lines = bsplit(filedata, '\n')) != NULL) {
      for (i = 0; i < lines->qty; i++) {
         parse_raw_signature(lines->entry[i], i + 1, storage);
      }
   }

   /* Clean Up */
   bdestroy(filename);
   bdestroy(filedata);
   bstrListDestroy(lines);
   fclose(fp);

   return 0;
}

/* ----------------------------------------------------------
 * FUNCTION     : parse_raw_signature
 * DESCRIPTION  : This function will take a line from the
 *              : signature file and parse it into it's data
 *              : structure.
 * INPUT        : 0 - Raw Signature (bstring)
 *              : 1 - The line number this signature is on.
 * RETURN       : 0 - Success
 *              : -1 - Error
 * ---------------------------------------------------------- */
int parse_raw_signature (bstring line, int lineno, int storage) {
   struct bstrList *raw_sig = NULL;
   struct bstrList *title = NULL;
   signature *sig, *head;
   sig = head = NULL;
   bstring pcre_string = NULL;
   const char *err = NULL;     /* PCRE */
   int erroffset;              /* PCRE */
   int ret = 0;
   int i;

   /* Check to see if this line has something to read. */
   if (line->data[0] == '\0' || line->data[0] == '#')
      return -1;

   /* Split Line */
   //if ((raw_sig = bsplitstr(line, bformat("||") )) == NULL)
   if ((raw_sig = bsplit(line, ',')) == NULL)
      return -1;

   /* Reconstruct the PCRE string.  This is needed in case there are PCRE
    * strings containing commas within them. */
   if (raw_sig->qty < 3) {
      ret = -1;
   } else if (raw_sig->qty > 3) {
      pcre_string = bstrcpy(raw_sig->entry[2]);
      for (i = 3; i < raw_sig->qty; i++) {
         //bstring tmp = bfromcstr("||");
         bstring tmp = bfromcstr(",");
         if ((bconcat(pcre_string, tmp)) == BSTR_ERR)
            ret = -1;
         if ((bconcat(pcre_string, raw_sig->entry[i])) == BSTR_ERR)
            ret = -1;
         bdestroy(tmp);
      }
   } else {
      pcre_string = bstrcpy(raw_sig->entry[2]);
   }

   /* Split Title */
   if (raw_sig->entry[1] != NULL && ret != -1)
      title = bsplit(raw_sig->entry[1], '/');
      if (title == NULL) {
         bdestroy(pcre_string);
         return -1;
      }
   if (title->qty < 3)
      ret = -1;

   /* Create signature data structure for this record. */
   if (ret != -1) {
      sig = (signature*)calloc(1,sizeof(signature));
      if (raw_sig->entry[0] != NULL)
         sig->service = bstrcpy(raw_sig->entry[0]);
      if (title->entry[1] != NULL)
         sig->title.app = bstrcpy(title->entry[1]);
      if (title->entry[2] != NULL)
         sig->title.ver = bstrcpy(title->entry[2]);
      if (title->entry[3] != NULL)
         sig->title.misc = bstrcpy(title->entry[3]);

      /* PCRE */
      if (pcre_string != NULL) {
         if ((sig->regex = pcre_compile ((char *)bdata(pcre_string), 0, &err, &erroffset, NULL)) == NULL) {
            printf("Unable to compile signature:  %s at line %d (%s)",
            err, lineno, bdata(line));
            ret = -1;
         }
      }
      if (ret != -1) {
         sig->study = pcre_study (sig->regex, 0, &err);
         if (err != NULL)
            printf("Unable to study signature:  %s", err);
      }

      /* Add signature to 'signature_list' data structure. */
      if (ret != -1) {
         //add_signature (sig);
         if (storage == 1) {
            extern signature *sig_serv_tcp;
            head = sig_serv_tcp;
            sig->next  = head;
            sig_serv_tcp = sig;
         } else
         if (storage == 2) {
            extern signature *sig_serv_udp;
            head = sig_serv_udp;
            sig->next  = head;
            sig_serv_udp = sig;
         } else
         if (storage == 3) {
            extern signature *sig_client_tcp;
            head = sig_client_tcp;
            sig->next  = head;
            sig_client_tcp = sig;
         } else
         if (storage == 4) {
            extern signature *sig_client_udp;
            head = sig_client_udp;
            sig->next  = head;
            sig_client_udp = sig;
         }
        /* printf("SIG ADDED:%s to %d\n",(char *)bdata(sig->service),storage); */
      }
   }

   /* Garbage Collection */
   if (raw_sig != NULL)
      bstrListDestroy(raw_sig);
   if (title != NULL)
      bstrListDestroy(title);
   if (pcre_string != NULL)
      bdestroy(pcre_string);

   return ret;
}

/* ----------------------------------------------------------
 * FUNCTION     : get_app_name
 * DESCRIPTION  : This function will take the results of a
 *              : pcre match and compile the application name
 *              : based off of the signature.
 * INPUT        : 0 - Signature Pointer
 *              : 1 - payload
 *              : 2 - ovector
 *              : 3 - rc (return from pcre_exec)
 * RETURN       : processed app name
 * ---------------------------------------------------------- */
bstring
get_app_name (signature *sig,
            const char *payload,
            int *ovector,
            int rc)
{
    char sub[100];
    char app[5000];
    char expr[100];
    bstring retval;
    int i = 0;
    int n = 0;
    int x = 0;
    int z = 0;

    /* Create Application string using the values in signature[i].title.  */
    if (sig->title.app != NULL) {
        strlcpy(app, (char *)bdata(sig->title.app), MAX_APP);
    }
    if (sig->title.ver != NULL) {
        if (sig->title.ver->slen > 0) {
            strcat(app, " ");
            strlcat(app, (char *)bdata(sig->title.ver), MAX_VER);
        }
    }
    if (sig->title.misc != NULL) {
        if (sig->title.misc->slen > 0) {
            strcat(app, " (");
            strlcat(app, (char *)bdata(sig->title.misc), MAX_MISC);
            strcat(app, ")");
        }
    }

    /* Replace $1, $2, etc. with the appropriate substring.  */
    while (app[i] != '\0' && z < (sizeof(sub) - 1)) {
        /* Check to see if the string contains a $? mark variable. */
        if (app[i] == '$') {
            /* Yes it does, replace it with the appropriate match string. */
            i++;
            n = atoi(&app[i]);

            pcre_copy_substring(payload, ovector, rc, n, expr, sizeof(expr));
            x = 0;
            while (expr[x] != '\0' && z < (sizeof(sub) - 1)) {
                sub[z] = expr[x];
                z++;
                x++;
            }
            for (x = 0; x < sizeof(expr); x++)
                expr[x] = '\0';
            i++;
        } else {
            /* No it doesn't, copy to new string. */
            sub[z] = app[i];
            i++;
            z++;
        }
    }
    sub[z] = '\0';

    retval = bfromcstr(sub);
    return retval;

}
