/* $Id$ */

/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library.
 *
 * Directory Read Support Routines.
 */
#include "tiffiop.h"

#define IGNORE 0 /* tag placeholder used below */

#ifdef HAVE_IEEEFP
#define TIFFCvtIEEEFloatToNative(tif, n, fp)
#define TIFFCvtIEEEDoubleToNative(tif, n, dp)
#else
extern void TIFFCvtIEEEFloatToNative(TIFF *, uint32, float *);
extern void TIFFCvtIEEEDoubleToNative(TIFF *, uint32, double *);
#endif

static int EstimateStripByteCounts(TIFF *, TIFFDirEntry *, uint16);
static void MissingRequired(TIFF *, const char *);
static int CheckDirCount(TIFF *, TIFFDirEntry *, uint32);
static tsize_t TIFFFetchData(TIFF *, TIFFDirEntry *, char *);
static tsize_t TIFFFetchString(TIFF *, TIFFDirEntry *, char *);
static float TIFFFetchRational(TIFF *, TIFFDirEntry *);
static int TIFFFetchNormalTag(TIFF *, TIFFDirEntry *);
static int TIFFFetchPerSampleShorts(TIFF *, TIFFDirEntry *, uint16 *);
static int TIFFFetchPerSampleLongs(TIFF *, TIFFDirEntry *, uint32 *);
static int TIFFFetchPerSampleAnys(TIFF *, TIFFDirEntry *, double *);
static int TIFFFetchShortArray(TIFF *, TIFFDirEntry *, uint16 *);
static int TIFFFetchStripThing(TIFF *, TIFFDirEntry *, long, uint32 **);
static int TIFFFetchRefBlackWhite(TIFF *, TIFFDirEntry *);
static float TIFFFetchFloat(TIFF *, TIFFDirEntry *);
static int TIFFFetchFloatArray(TIFF *, TIFFDirEntry *, float *);
static int TIFFFetchDoubleArray(TIFF *, TIFFDirEntry *, double *);
static int TIFFFetchAnyArray(TIFF *, TIFFDirEntry *, double *);
static int TIFFFetchShortPair(TIFF *, TIFFDirEntry *);
static void ChopUpSingleUncompressedStrip(TIFF *);

/*
 * Read the next TIFF directory from a file
 * and convert it to the internal format.
 * We read directories sequentially.
 */
int TIFFReadDirectory(TIFF *tif) {
  static const char module[] = "TIFFReadDirectory";

  int n;
  TIFFDirectory *td;
  TIFFDirEntry *dp, *dir = NULL;
  uint16 iv;
  uint32 v;
  const TIFFFieldInfo *fip;
  size_t fix;
  uint16 dircount;
  toff_t nextdiroff;
  int diroutoforderwarning = 0;
  toff_t *new_dirlist;

  tif->tif_diroff = tif->tif_nextdiroff;
 /* jump:87 */  if (tif->tif_diroff == 0) { /* no more directories */
    return (0);
  }

  /*
   * XXX: Trick to prevent IFD looping. The one can create TIFF file
   * with looped directory pointers. We will maintain a list of already
   * seen directories and check every IFD offset against this list.
   */
 /* jump:98 */  for (n = 0; n < tif->tif_dirnumber; n++) {
 /* jump:97 */    if (tif->tif_dirlist[n] == tif->tif_diroff) {
      return (0);
    }
  }
  tif->tif_dirnumber++;
  new_dirlist = (toff_t *)_TIFFrealloc(tif->tif_dirlist,
                                       tif->tif_dirnumber * sizeof(toff_t));
 /* jump:106 */  if (!new_dirlist) {
    TIFFErrorExt(tif->tif_clientdata, module,
                 "%s: Failed to allocate space for IFD list", tif->tif_name);
    return (0);
  }
  tif->tif_dirlist = new_dirlist;
  tif->tif_dirlist[tif->tif_dirnumber - 1] = tif->tif_diroff;

  /*
   * Cleanup any previous compression state.
   */
  (*tif->tif_cleanup)(tif);
  tif->tif_curdir++;
  nextdiroff = 0;
 /* jump:144 */  if (!isMapped(tif)) {
 /* jump:121 */    if (!SeekOK(tif, tif->tif_diroff)) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Seek error accessing TIFF directory", tif->tif_name);
      return (0);
    }
 /* jump:126 */    if (!ReadOK(tif, &dircount, sizeof(uint16))) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory count", tif->tif_name);
      return (0);
    }
 /* jump:129 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabShort(&dircount);
    }
    dir = (TIFFDirEntry *)_TIFFCheckMalloc(tif, dircount, sizeof(TIFFDirEntry),
                                           "to read TIFF directory");
 /* jump:134 */    if (dir == NULL) {
      return (0);
    }
 /* jump:139 */    if (!ReadOK(tif, dir, dircount * sizeof(TIFFDirEntry))) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%.100s: Can not read TIFF directory", tif->tif_name);
      goto bad;
    }
    /*
     * Read offset to next directory for sequential scans.
     */
    (void)ReadOK(tif, &nextdiroff, sizeof(uint32));
  } else {
    toff_t off = tif->tif_diroff;

 /* jump:151 */    if (off + sizeof(uint16) > tif->tif_size) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory count", tif->tif_name);
      return (0);
    } else {
      _TIFFmemcpy(&dircount, tif->tif_base + off, sizeof(uint16));
    }
    off += sizeof(uint16);
 /* jump:157 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabShort(&dircount);
    }
    dir = (TIFFDirEntry *)_TIFFCheckMalloc(tif, dircount, sizeof(TIFFDirEntry),
                                           "to read TIFF directory");
 /* jump:162 */    if (dir == NULL) {
      return (0);
    }
 /* jump:167 */    if (off + dircount * sizeof(TIFFDirEntry) > tif->tif_size) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory", tif->tif_name);
      goto bad;
    } else {
      _TIFFmemcpy(dir, tif->tif_base + off, dircount * sizeof(TIFFDirEntry));
    }
    off += dircount * sizeof(TIFFDirEntry);
 /* jump:173 */    if (off + sizeof(uint32) <= tif->tif_size) {
      _TIFFmemcpy(&nextdiroff, tif->tif_base + off, sizeof(uint32));
    }
  }
 /* jump:177 */  if (tif->tif_flags & TIFF_SWAB) {
    TIFFSwabLong(&nextdiroff);
  }
  tif->tif_nextdiroff = nextdiroff;

  tif->tif_flags &= ~TIFF_BEENWRITING; /* reset before new dir */
  /*
   * Setup default value and then make a pass over
   * the fields to check type and tag information,
   * and to extract info required to size data
   * structures.  A second pass is made afterwards
   * to read in everthing not taken in the first pass.
   */
  td = &tif->tif_dir;
  /* free any old stuff and reinit */
  TIFFFreeDirectory(tif);
  TIFFDefaultDirectory(tif);
  /*
   * Electronic Arts writes gray-scale TIFF files
   * without a PlanarConfiguration directory entry.
   * Thus we setup a default value here, even though
   * the TIFF spec says there is no default value.
   */
  TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  /*
   * Sigh, we must make a separate pass through the
   * directory for the following reason:
   *
   * We must process the Compression tag in the first pass
   * in order to merge in codec-private tag definitions (otherwise
   * we may get complaints about unknown tags).  However, the
   * Compression tag may be dependent on the SamplesPerPixel
   * tag value because older TIFF specs permited Compression
   * to be written as a SamplesPerPixel-count tag entry.
   * Thus if we don't first figure out the correct SamplesPerPixel
   * tag value then we may end up ignoring the Compression tag
   * value because it has an incorrect count value (if the
   * true value of SamplesPerPixel is not 1).
   *
   * It sure would have been nice if Aldus had really thought
   * this stuff through carefully.
   */
 /* jump:229 */  for (dp = dir, n = dircount; n > 0; n--, dp++) {
 /* jump:222 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabArrayOfShort(&dp->tdir_tag, 2);
      TIFFSwabArrayOfLong(&dp->tdir_count, 2);
    }
 /* jump:228 */    if (dp->tdir_tag == TIFFTAG_SAMPLESPERPIXEL) {
 /* jump:226 */      if (!TIFFFetchNormalTag(tif, dp)) {
        goto bad;
      }
      dp->tdir_tag = IGNORE;
    }
  }
  /*
   * First real pass over the directory.
   */
  fix = 0;
 /* jump:364 */  for (dp = dir, n = dircount; n > 0; n--, dp++) {

 /* jump:238 */    if (fix >= tif->tif_nfields || dp->tdir_tag == IGNORE) {
      continue;
    }

    /*
     * Silicon Beach (at least) writes unordered
     * directory tags (violating the spec).  Handle
     * it here, but be obnoxious (maybe they'll fix it?).
     */
 /* jump:254 */    if (dp->tdir_tag < tif->tif_fieldinfo[fix]->field_tag) {
 /* jump:252 */      if (!diroutoforderwarning) {
        TIFFWarningExt(tif->tif_clientdata, module,
                       "%s: invalid TIFF directory; tags are not sorted in "
                       "ascending order",
                       tif->tif_name);
        diroutoforderwarning = 1;
      }
      fix = 0; /* O(n^2) */
    }
 /* jump:258 */    while (fix < tif->tif_nfields &&
           tif->tif_fieldinfo[fix]->field_tag < dp->tdir_tag) {
      fix++;
    }
 /* jump:275 */    if (fix >= tif->tif_nfields ||
        tif->tif_fieldinfo[fix]->field_tag != dp->tdir_tag) {

      TIFFWarningExt(tif->tif_clientdata, module,
                     "%s: unknown field with tag %d (0x%x) encountered",
                     tif->tif_name, dp->tdir_tag, dp->tdir_tag, dp->tdir_type);

      TIFFMergeFieldInfo(tif,
                         _TIFFCreateAnonFieldInfo(tif, dp->tdir_tag,
                                                  (TIFFDataType)dp->tdir_type),
                         1);
      fix = 0;
 /* jump:274 */      while (fix < tif->tif_nfields &&
             tif->tif_fieldinfo[fix]->field_tag < dp->tdir_tag) {
        fix++;
      }
    }
    /*
     * Null out old tags that we ignore.
     */
 /* jump:283 */    if (tif->tif_fieldinfo[fix]->field_bit == FIELD_IGNORE) {
    ignore:
      dp->tdir_tag = IGNORE;
      continue;
    }
    /*
     * Check data type.
     */
    fip = tif->tif_fieldinfo[fix];
 /* jump:301 */    while (dp->tdir_type != (unsigned short)fip->field_type &&
           fix < tif->tif_nfields) {
 /* jump:292 */      if (fip->field_type == TIFF_ANY) { /* wildcard */
        break;
      }
      fip = tif->tif_fieldinfo[++fix];
 /* jump:300 */      if (fix >= tif->tif_nfields || fip->field_tag != dp->tdir_tag) {
        TIFFWarningExt(tif->tif_clientdata, module,
                       "%s: wrong data type %d for \"%s\"; tag ignored",
                       tif->tif_name, dp->tdir_type,
                       tif->tif_fieldinfo[fix - 1]->field_name);
        goto ignore;
      }
    }
    /*
     * Check count if known in advance.
     */
 /* jump:313 */    if (fip->field_readcount != TIFF_VARIABLE &&
        fip->field_readcount != TIFF_VARIABLE2) {
      uint32 expected = (fip->field_readcount == TIFF_SPP)
                            ? (uint32)td->td_samplesperpixel
                            : (uint32)fip->field_readcount;
 /* jump:312 */      if (!CheckDirCount(tif, dp, expected)) {
        goto ignore;
      }
    }

    switch (dp->tdir_tag) {
    case TIFFTAG_COMPRESSION:
      /*
       * The 5.0 spec says the Compression tag has
       * one value, while earlier specs say it has
       * one value per sample.  Because of this, we
       * accept the tag if one value is supplied.
       */
 /* jump:330 */      if (dp->tdir_count == 1) {
        v = TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
 /* jump:327 */        if (!TIFFSetField(tif, dp->tdir_tag, (uint16)v)) {
          goto bad;
        }
        break;
        /* XXX: workaround for broken TIFFs */
 /* jump:335 */      } else if (dp->tdir_type == TIFF_LONG) {
 /* jump:334 */        if (!TIFFFetchPerSampleLongs(tif, dp, &v) ||
            !TIFFSetField(tif, dp->tdir_tag, (uint16)v)) {
          goto bad;
        }
      } else {
 /* jump:339 */        if (!TIFFFetchPerSampleShorts(tif, dp, &iv) ||
            !TIFFSetField(tif, dp->tdir_tag, iv)) {
          goto bad;
        }
      }
      dp->tdir_tag = IGNORE;
      break;
    case TIFFTAG_STRIPOFFSETS:
    case TIFFTAG_STRIPBYTECOUNTS:
    case TIFFTAG_TILEOFFSETS:
    case TIFFTAG_TILEBYTECOUNTS:
      TIFFSetFieldBit(tif, fip->field_bit);
      break;
    case TIFFTAG_IMAGEWIDTH:
    case TIFFTAG_IMAGELENGTH:
    case TIFFTAG_IMAGEDEPTH:
    case TIFFTAG_TILELENGTH:
    case TIFFTAG_TILEWIDTH:
    case TIFFTAG_TILEDEPTH:
    case TIFFTAG_PLANARCONFIG:
    case TIFFTAG_ROWSPERSTRIP:
    case TIFFTAG_EXTRASAMPLES:
 /* jump:360 */      if (!TIFFFetchNormalTag(tif, dp)) {
        goto bad;
      }
      dp->tdir_tag = IGNORE;
      break;
    }
  }

  /*
   * Allocate directory structure and setup defaults.
   */
 /* jump:372 */  if (!TIFFFieldSet(tif, FIELD_IMAGEDIMENSIONS)) {
    MissingRequired(tif, "ImageLength");
    goto bad;
  }
 /* jump:376 */  if (!TIFFFieldSet(tif, FIELD_PLANARCONFIG)) {
    MissingRequired(tif, "PlanarConfiguration");
    goto bad;
  }
  /*
   * Setup appropriate structures (by strip or by tile)
   */
 /* jump:386 */  if (!TIFFFieldSet(tif, FIELD_TILEDIMENSIONS)) {
    td->td_nstrips = TIFFNumberOfStrips(tif);
    td->td_tilewidth = td->td_imagewidth;
    td->td_tilelength = td->td_rowsperstrip;
    td->td_tiledepth = td->td_imagedepth;
    tif->tif_flags &= ~TIFF_ISTILED;
  } else {
    td->td_nstrips = TIFFNumberOfTiles(tif);
    tif->tif_flags |= TIFF_ISTILED;
  }
 /* jump:395 */  if (!td->td_nstrips) {
    TIFFErrorExt(tif->tif_clientdata, module,
                 "%s: cannot handle zero number of %s", tif->tif_name,
                 isTiled(tif) ? "tiles" : "strips");
    goto bad;
  }
  td->td_stripsperimage = td->td_nstrips;
 /* jump:399 */  if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
    td->td_stripsperimage /= td->td_samplesperpixel;
  }
 /* jump:403 */  if (!TIFFFieldSet(tif, FIELD_STRIPOFFSETS)) {
    MissingRequired(tif, isTiled(tif) ? "TileOffsets" : "StripOffsets");
    goto bad;
  }

  /*
   * Second pass: extract other information.
   */
 /* jump:536 */  for (dp = dir, n = dircount; n > 0; n--, dp++) {
 /* jump:411 */    if (dp->tdir_tag == IGNORE) {
      continue;
    }
    switch (dp->tdir_tag) {
    case TIFFTAG_MINSAMPLEVALUE:
    case TIFFTAG_MAXSAMPLEVALUE:
    case TIFFTAG_BITSPERSAMPLE:
    case TIFFTAG_DATATYPE:
    case TIFFTAG_SAMPLEFORMAT:
      /*
       * The 5.0 spec says the Compression tag has
       * one value, while earlier specs say it has
       * one value per sample.  Because of this, we
       * accept the tag if one value is supplied.
       *
       * The MinSampleValue, MaxSampleValue, BitsPerSample
       * DataType and SampleFormat tags are supposed to be
       * written as one value/sample, but some vendors
       * incorrectly write one value only -- so we accept
       * that as well (yech). Other vendors write correct
       * value for NumberOfSamples, but incorrect one for
       * BitsPerSample and friends, and we will read this
       * too.
       */
 /* jump:439 */      if (dp->tdir_count == 1) {
        v = TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
 /* jump:437 */        if (!TIFFSetField(tif, dp->tdir_tag, (uint16)v)) {
          goto bad;
        }
        /* XXX: workaround for broken TIFFs */
 /* jump:445 */      } else if (dp->tdir_tag == TIFFTAG_BITSPERSAMPLE &&
                 dp->tdir_type == TIFF_LONG) {
 /* jump:444 */        if (!TIFFFetchPerSampleLongs(tif, dp, &v) ||
            !TIFFSetField(tif, dp->tdir_tag, (uint16)v)) {
          goto bad;
        }
      } else {
 /* jump:449 */        if (!TIFFFetchPerSampleShorts(tif, dp, &iv) ||
            !TIFFSetField(tif, dp->tdir_tag, iv)) {
          goto bad;
        }
      }
      break;
    case TIFFTAG_SMINSAMPLEVALUE:
    case TIFFTAG_SMAXSAMPLEVALUE: {
      double dv = 0.0;
 /* jump:458 */      if (!TIFFFetchPerSampleAnys(tif, dp, &dv) ||
          !TIFFSetField(tif, dp->tdir_tag, dv)) {
        goto bad;
      }
    } break;
    case TIFFTAG_STRIPOFFSETS:
    case TIFFTAG_TILEOFFSETS:
 /* jump:464 */      if (!TIFFFetchStripThing(tif, dp, td->td_nstrips, &td->td_stripoffset)) {
        goto bad;
      }
      break;
    case TIFFTAG_STRIPBYTECOUNTS:
    case TIFFTAG_TILEBYTECOUNTS:
 /* jump:471 */      if (!TIFFFetchStripThing(tif, dp, td->td_nstrips,
                               &td->td_stripbytecount)) {
        goto bad;
      }
      break;
    case TIFFTAG_COLORMAP:
    case TIFFTAG_TRANSFERFUNCTION: {
      char *cp;
      /*
       * TransferFunction can have either 1x or 3x
       * data values; Colormap can have only 3x
       * items.
       */
      v = 1L << td->td_bitspersample;
 /* jump:486 */      if (dp->tdir_tag == TIFFTAG_COLORMAP || dp->tdir_count != v) {
 /* jump:485 */        if (!CheckDirCount(tif, dp, 3 * v)) {
          break;
        }
      }
      v *= sizeof(uint16);
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(uint16),
                                    "to read \"TransferFunction\" tag");
 /* jump:504 */      if (cp != NULL) {
 /* jump:502 */        if (TIFFFetchData(tif, dp, cp)) {
          /*
           * This deals with there being
           * only one array to apply to
           * all samples.
           */
          uint32 c = 1L << td->td_bitspersample;
 /* jump:500 */          if (dp->tdir_count == c) {
            v = 0L;
          }
          TIFFSetField(tif, dp->tdir_tag, cp, cp + v, cp + 2 * v);
        }
        _TIFFfree(cp);
      }
      break;
    }
    case TIFFTAG_PAGENUMBER:
    case TIFFTAG_HALFTONEHINTS:
    case TIFFTAG_YCBCRSUBSAMPLING:
    case TIFFTAG_DOTRANGE:
      (void)TIFFFetchShortPair(tif, dp);
      break;
    case TIFFTAG_REFERENCEBLACKWHITE:
      (void)TIFFFetchRefBlackWhite(tif, dp);
      break;
      /* BEGIN REV 4.0 COMPATIBILITY */
    case TIFFTAG_OSUBFILETYPE:
      v = 0L;
      switch (TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset)) {
      case OFILETYPE_REDUCEDIMAGE:
        v = FILETYPE_REDUCEDIMAGE;
        break;
      case OFILETYPE_PAGE:
        v = FILETYPE_PAGE;
        break;
      }
 /* jump:529 */      if (v) {
        TIFFSetField(tif, TIFFTAG_SUBFILETYPE, v);
      }
      break;
      /* END REV 4.0 COMPATIBILITY */
    default:
      (void)TIFFFetchNormalTag(tif, dp);
      break;
    }
  }
  /*
   * Verify Palette image has a Colormap.
   */
 /* jump:544 */  if (td->td_photometric == PHOTOMETRIC_PALETTE &&
      !TIFFFieldSet(tif, FIELD_COLORMAP)) {
    MissingRequired(tif, "Colormap");
    goto bad;
  }
  /*
   * Attempt to deal with a missing StripByteCounts tag.
   */
 /* jump:586 */  if (!TIFFFieldSet(tif, FIELD_STRIPBYTECOUNTS)) {
    /*
     * Some manufacturers violate the spec by not giving
     * the size of the strips.  In this case, assume there
     * is one uncompressed strip of data.
     */
 /* jump:559 */    if ((td->td_planarconfig == PLANARCONFIG_CONTIG && td->td_nstrips > 1) ||
        (td->td_planarconfig == PLANARCONFIG_SEPARATE &&
         td->td_nstrips != td->td_samplesperpixel)) {
      MissingRequired(tif, "StripByteCounts");
      goto bad;
    }
    TIFFWarningExt(tif->tif_clientdata, module,
                   "%s: TIFF directory is missing required "
                   "\"%s\" field, calculating from imagelength",
                   tif->tif_name,
                   _TIFFFieldWithTag(tif, TIFFTAG_STRIPBYTECOUNTS)->field_name);
 /* jump:567 */    if (EstimateStripByteCounts(tif, dir, dircount) < 0) {
      goto bad;
    }
/*
 * Assume we have wrong StripByteCount value (in case of single strip) in
 * following cases:
 *   - it is equal to zero along with StripOffset;
 *   - it is larger than file itself (in case of uncompressed image);
 *   - it is smaller than the size of the bytes per row multiplied on the
 *     number of rows.  The last case should not be checked in the case of
 *     writing new image, because we may do not know the exact strip size
 *     until the whole image will be written and directory dumped out.
 */
#define BYTECOUNTLOOKSBAD                                                      \
  ((td->td_stripbytecount[0] == 0 && td->td_stripoffset[0] != 0) ||            \
   (td->td_compression == COMPRESSION_NONE &&                                  \
    td->td_stripbytecount[0] >                                                 \
        TIFFGetFileSize(tif) - td->td_stripoffset[0]) ||                       \
   (tif->tif_mode == O_RDONLY && td->td_compression == COMPRESSION_NONE &&     \
    td->td_stripbytecount[0] < TIFFScanlineSize(tif) * td->td_imagelength))

 /* jump:602 */  } else if (td->td_nstrips == 1 && td->td_stripoffset[0] != 0 &&
             BYTECOUNTLOOKSBAD) {
    /*
     * XXX: Plexus (and others) sometimes give a value of zero for
     * a tag when they don't know what the correct value is!  Try
     * and handle the simple case of estimating the size of a one
     * strip image.
     */
    TIFFWarningExt(
        tif->tif_clientdata, module,
        "%s: Bogus \"%s\" field, ignoring and calculating from imagelength",
        tif->tif_name,
        _TIFFFieldWithTag(tif, TIFFTAG_STRIPBYTECOUNTS)->field_name);
 /* jump:601 */    if (EstimateStripByteCounts(tif, dir, dircount) < 0) {
      goto bad;
    }
 /* jump:618 */  } else if (td->td_planarconfig == PLANARCONFIG_CONTIG && td->td_nstrips > 2 &&
             td->td_compression == COMPRESSION_NONE &&
             td->td_stripbytecount[0] != td->td_stripbytecount[1]) {
    /*
     * XXX: Some vendors fill StripByteCount array with absolutely
     * wrong values (it can be equal to StripOffset array, for
     * example). Catch this case here.
     */
    TIFFWarningExt(
        tif->tif_clientdata, module,
        "%s: Wrong \"%s\" field, ignoring and calculating from imagelength",
        tif->tif_name,
        _TIFFFieldWithTag(tif, TIFFTAG_STRIPBYTECOUNTS)->field_name);
 /* jump:617 */    if (EstimateStripByteCounts(tif, dir, dircount) < 0) {
      goto bad;
    }
  }
 /* jump:622 */  if (dir) {
    _TIFFfree((char *)dir);
    dir = NULL;
  }
 /* jump:625 */  if (!TIFFFieldSet(tif, FIELD_MAXSAMPLEVALUE)) {
    td->td_maxsamplevalue = (uint16)((1L << td->td_bitspersample) - 1);
  }
  /*
   * Setup default compression scheme.
   */

  /*
   * XXX: We can optimize checking for the strip bounds using the sorted
   * bytecounts array. See also comments for TIFFAppendToStrip()
   * function in tif_write.c.
   */
 /* jump:645 */  if (td->td_nstrips > 1) {
    tstrip_t strip;

    td->td_stripbytecountsorted = 1;
 /* jump:644 */    for (strip = 1; strip < td->td_nstrips; strip++) {
 /* jump:643 */      if (td->td_stripoffset[strip - 1] > td->td_stripoffset[strip]) {
        td->td_stripbytecountsorted = 0;
        break;
      }
    }
  }

 /* jump:649 */  if (!TIFFFieldSet(tif, FIELD_COMPRESSION)) {
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
  }
  /*
   * Some manufacturers make life difficult by writing
   * large amounts of uncompressed data as a single strip.
   * This is contrary to the recommendations of the spec.
   * The following makes an attempt at breaking such images
   * into strips closer to the recommended 8k bytes.  A
   * side effect, however, is that the RowsPerStrip tag
   * value may be changed.
   */
 /* jump:662 */  if (td->td_nstrips == 1 && td->td_compression == COMPRESSION_NONE &&
      (tif->tif_flags & (TIFF_STRIPCHOP | TIFF_ISTILED)) == TIFF_STRIPCHOP) {
    ChopUpSingleUncompressedStrip(tif);
  }

  /*
   * Reinitialize i/o since we are starting on a new directory.
   */
  tif->tif_row = (uint32)-1;
  tif->tif_curstrip = (tstrip_t)-1;
  tif->tif_col = (uint32)-1;
  tif->tif_curtile = (ttile_t)-1;
  tif->tif_tilesize = (tsize_t)-1;

  tif->tif_scanlinesize = TIFFScanlineSize(tif);
 /* jump:678 */  if (!tif->tif_scanlinesize) {
    TIFFErrorExt(tif->tif_clientdata, module,
                 "%s: cannot handle zero scanline size", tif->tif_name);
    return (0);
  }

 /* jump:687 */  if (isTiled(tif)) {
    tif->tif_tilesize = TIFFTileSize(tif);
 /* jump:686 */    if (!tif->tif_tilesize) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: cannot handle zero tile size", tif->tif_name);
      return (0);
    }
  } else {
 /* jump:692 */    if (!TIFFStripSize(tif)) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: cannot handle zero strip size", tif->tif_name);
      return (0);
    }
  }
  return (1);
bad:
 /* jump:698 */  if (dir) {
    _TIFFfree(dir);
  }
  return (0);
}

/*
 * Read custom directory from the arbitarry offset.
 * The code is very similar to TIFFReadDirectory().
 */
int TIFFReadCustomDirectory(TIFF *tif, toff_t diroff,
                            const TIFFFieldInfo info[], size_t n) {
  static const char module[] = "TIFFReadCustomDirectory";

  TIFFDirectory *td = &tif->tif_dir;
  TIFFDirEntry *dp, *dir = NULL;
  const TIFFFieldInfo *fip;
  size_t fix;
  uint16 i, dircount;

  _TIFFSetupFieldInfo(tif, info, n);

  tif->tif_diroff = diroff;

 /* jump:744 */  if (!isMapped(tif)) {
 /* jump:725 */    if (!SeekOK(tif, diroff)) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Seek error accessing TIFF directory", tif->tif_name);
      return (0);
    }
 /* jump:730 */    if (!ReadOK(tif, &dircount, sizeof(uint16))) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory count", tif->tif_name);
      return (0);
    }
 /* jump:733 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabShort(&dircount);
    }
    dir = (TIFFDirEntry *)_TIFFCheckMalloc(tif, dircount, sizeof(TIFFDirEntry),
                                           "to read TIFF directory");
 /* jump:738 */    if (dir == NULL) {
      return (0);
    }
 /* jump:743 */    if (!ReadOK(tif, dir, dircount * sizeof(TIFFDirEntry))) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%.100s: Can not read TIFF directory", tif->tif_name);
      goto bad;
    }
  } else {
    toff_t off = diroff;

 /* jump:751 */    if (off + sizeof(uint16) > tif->tif_size) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory count", tif->tif_name);
      return (0);
    } else {
      _TIFFmemcpy(&dircount, tif->tif_base + off, sizeof(uint16));
    }
    off += sizeof(uint16);
 /* jump:757 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabShort(&dircount);
    }
    dir = (TIFFDirEntry *)_TIFFCheckMalloc(tif, dircount, sizeof(TIFFDirEntry),
                                           "to read TIFF directory");
 /* jump:762 */    if (dir == NULL) {
      return (0);
    }
 /* jump:767 */    if (off + dircount * sizeof(TIFFDirEntry) > tif->tif_size) {
      TIFFErrorExt(tif->tif_clientdata, module,
                   "%s: Can not read TIFF directory", tif->tif_name);
      goto bad;
    } else {
      _TIFFmemcpy(dir, tif->tif_base + off, dircount * sizeof(TIFFDirEntry));
    }
  }

  TIFFFreeDirectory(tif);

  fix = 0;
 /* jump:848 */  for (dp = dir, i = dircount; i > 0; i--, dp++) {
 /* jump:779 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabArrayOfShort(&dp->tdir_tag, 2);
      TIFFSwabArrayOfLong(&dp->tdir_count, 2);
    }

 /* jump:783 */    if (fix >= tif->tif_nfields || dp->tdir_tag == IGNORE) {
      continue;
    }

 /* jump:788 */    while (fix < tif->tif_nfields &&
           tif->tif_fieldinfo[fix]->field_tag < dp->tdir_tag) {
      fix++;
    }

 /* jump:807 */    if (fix >= tif->tif_nfields ||
        tif->tif_fieldinfo[fix]->field_tag != dp->tdir_tag) {

      TIFFWarningExt(tif->tif_clientdata, module,
                     "%s: unknown field with tag %d (0x%x) encountered",
                     tif->tif_name, dp->tdir_tag, dp->tdir_tag, dp->tdir_type);

      TIFFMergeFieldInfo(tif,
                         _TIFFCreateAnonFieldInfo(tif, dp->tdir_tag,
                                                  (TIFFDataType)dp->tdir_type),
                         1);

      fix = 0;
 /* jump:806 */      while (fix < tif->tif_nfields &&
             tif->tif_fieldinfo[fix]->field_tag < dp->tdir_tag) {
        fix++;
      }
    }
    /*
     * Null out old tags that we ignore.
     */
 /* jump:815 */    if (tif->tif_fieldinfo[fix]->field_bit == FIELD_IGNORE) {
    ignore:
      dp->tdir_tag = IGNORE;
      continue;
    }
    /*
     * Check data type.
     */
    fip = tif->tif_fieldinfo[fix];
 /* jump:833 */    while (dp->tdir_type != (unsigned short)fip->field_type &&
           fix < tif->tif_nfields) {
 /* jump:824 */      if (fip->field_type == TIFF_ANY) { /* wildcard */
        break;
      }
      fip = tif->tif_fieldinfo[++fix];
 /* jump:832 */      if (fix >= tif->tif_nfields || fip->field_tag != dp->tdir_tag) {
        TIFFWarningExt(tif->tif_clientdata, module,
                       "%s: wrong data type %d for \"%s\"; tag ignored",
                       tif->tif_name, dp->tdir_type,
                       tif->tif_fieldinfo[fix - 1]->field_name);
        goto ignore;
      }
    }
    /*
     * Check count if known in advance.
     */
 /* jump:845 */    if (fip->field_readcount != TIFF_VARIABLE &&
        fip->field_readcount != TIFF_VARIABLE2) {
      uint32 expected = (fip->field_readcount == TIFF_SPP)
                            ? (uint32)td->td_samplesperpixel
                            : (uint32)fip->field_readcount;
 /* jump:844 */      if (!CheckDirCount(tif, dp, expected)) {
        goto ignore;
      }
    }

    (void)TIFFFetchNormalTag(tif, dp);
  }

 /* jump:852 */  if (dir) {
    _TIFFfree(dir);
  }
  return 1;

bad:
 /* jump:858 */  if (dir) {
    _TIFFfree(dir);
  }
  return 0;
}

/*
 * EXIF is important special case of custom IFD, so we have a special
 * function to read it.
 */
int TIFFReadEXIFDirectory(TIFF *tif, toff_t diroff) {
  size_t exifFieldInfoCount;
  const TIFFFieldInfo *exifFieldInfo =
      _TIFFGetExifFieldInfo(&exifFieldInfoCount);
  return TIFFReadCustomDirectory(tif, diroff, exifFieldInfo,
                                 exifFieldInfoCount);
}

static int EstimateStripByteCounts(TIFF *tif, TIFFDirEntry *dir,
                                   uint16 dircount) {
  static const char module[] = "EstimateStripByteCounts";

  register TIFFDirEntry *dp;
  register TIFFDirectory *td = &tif->tif_dir;
  uint16 i;

 /* jump:884 */  if (td->td_stripbytecount) {
    _TIFFfree(td->td_stripbytecount);
  }
  td->td_stripbytecount = (uint32 *)_TIFFCheckMalloc(
      tif, td->td_nstrips, sizeof(uint32), "for \"StripByteCounts\" array");
 /* jump:926 */  if (td->td_compression != COMPRESSION_NONE) {
    uint32 space = (uint32)(sizeof(TIFFHeader) + sizeof(uint16) +
                            (dircount * sizeof(TIFFDirEntry)) + sizeof(uint32));
    toff_t filesize = TIFFGetFileSize(tif);
    uint16 n;

    /* calculate amount of space used by indirect values */
 /* jump:906 */    for (dp = dir, n = dircount; n > 0; n--, dp++) {
      uint32 cc = TIFFDataWidth((TIFFDataType)dp->tdir_type);
 /* jump:901 */      if (cc == 0) {
        TIFFErrorExt(tif->tif_clientdata, module,
                     "%s: Cannot determine size of unknown tag type %d",
                     tif->tif_name, dp->tdir_type);
        return -1;
      }
      cc = cc * dp->tdir_count;
 /* jump:905 */      if (cc > sizeof(uint32)) {
        space += cc;
      }
    }
    space = filesize - space;
 /* jump:910 */    if (td->td_planarconfig == PLANARCONFIG_SEPARATE) {
      space /= td->td_samplesperpixel;
    }
 /* jump:913 */    for (i = 0; i < td->td_nstrips; i++) {
      td->td_stripbytecount[i] = space;
    }
    /*
     * This gross hack handles the case were the offset to
     * the last strip is past the place where we think the strip
     * should begin.  Since a strip of data must be contiguous,
     * it's safe to assume that we've overestimated the amount
     * of data in the strip and trim this number back accordingly.
     */
    i--;
 /* jump:925 */    if (((toff_t)(td->td_stripoffset[i] + td->td_stripbytecount[i])) >
        filesize) {
      td->td_stripbytecount[i] = filesize - td->td_stripoffset[i];
    }
  } else {
    uint32 rowbytes = TIFFScanlineSize(tif);
    uint32 rowsperstrip = td->td_imagelength / td->td_stripsperimage;
 /* jump:931 */    for (i = 0; i < td->td_nstrips; i++) {
      td->td_stripbytecount[i] = rowbytes * rowsperstrip;
    }
  }
  TIFFSetFieldBit(tif, FIELD_STRIPBYTECOUNTS);
 /* jump:936 */  if (!TIFFFieldSet(tif, FIELD_ROWSPERSTRIP)) {
    td->td_rowsperstrip = td->td_imagelength;
  }
  return 1;
}

static void MissingRequired(TIFF *tif, const char *tagname) {
  static const char module[] = "MissingRequired";

  TIFFErrorExt(tif->tif_clientdata, module,
               "%s: TIFF directory is missing required \"%s\" field",
               tif->tif_name, tagname);
}

/*
 * Check the count field of a directory
 * entry against a known value.  The caller
 * is expected to skip/ignore the tag if
 * there is a mismatch.
 */
static int CheckDirCount(TIFF *tif, TIFFDirEntry *dir, uint32 count) {
 /* jump:962 */  if (count > dir->tdir_count) {
    TIFFWarningExt(
        tif->tif_clientdata, tif->tif_name,
        "incorrect count for field \"%s\" (%lu, expecting %lu); tag ignored",
        _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name, dir->tdir_count,
        count);
    return (0);
 /* jump:969 */  } else if (count < dir->tdir_count) {
    TIFFWarningExt(
        tif->tif_clientdata, tif->tif_name,
        "incorrect count for field \"%s\" (%lu, expecting %lu); tag trimmed",
        _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name, dir->tdir_count,
        count);
    return (1);
  }
  return (1);
}

/*
 * Fetch a contiguous directory item.
 */
static tsize_t TIFFFetchData(TIFF *tif, TIFFDirEntry *dir, char *cp) {
  int w = TIFFDataWidth((TIFFDataType)dir->tdir_type);
  tsize_t cc = dir->tdir_count * w;

  /* Check for overflow. */
 /* jump:983 */  if (!dir->tdir_count || !w || (tsize_t)dir->tdir_count / w != cc) {
    goto bad;
  }

 /* jump:992 */  if (!isMapped(tif)) {
 /* jump:988 */    if (!SeekOK(tif, dir->tdir_offset)) {
      goto bad;
    }
 /* jump:991 */    if (!ReadOK(tif, cp, cc)) {
      goto bad;
    }
  } else {
    tsize_t offset = dir->tdir_offset + cc;
    /* Check for overflow. */
 /* jump:998 */    if ((tsize_t)dir->tdir_offset != offset - cc ||
        offset > (tsize_t)tif->tif_size) {
      goto bad;
    }
    _TIFFmemcpy(cp, tif->tif_base + dir->tdir_offset, cc);
  }
 /* jump:1020 */  if (tif->tif_flags & TIFF_SWAB) {
    switch (dir->tdir_type) {
    case TIFF_SHORT:
    case TIFF_SSHORT:
      TIFFSwabArrayOfShort((uint16 *)cp, dir->tdir_count);
      break;
    case TIFF_LONG:
    case TIFF_SLONG:
    case TIFF_FLOAT:
      TIFFSwabArrayOfLong((uint32 *)cp, dir->tdir_count);
      break;
    case TIFF_RATIONAL:
    case TIFF_SRATIONAL:
      TIFFSwabArrayOfLong((uint32 *)cp, 2 * dir->tdir_count);
      break;
    case TIFF_DOUBLE:
      TIFFSwabArrayOfDouble((double *)cp, dir->tdir_count);
      break;
    }
  }
  return (cc);
bad:
  TIFFErrorExt(tif->tif_clientdata, tif->tif_name,
               "Error fetching data for field \"%s\"",
               _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name);
  return (tsize_t)0;
}

/*
 * Fetch an ASCII item from the file.
 */
static tsize_t TIFFFetchString(TIFF *tif, TIFFDirEntry *dir, char *cp) {
 /* jump:1040 */  if (dir->tdir_count <= 4) {
    uint32 l = dir->tdir_offset;
 /* jump:1037 */    if (tif->tif_flags & TIFF_SWAB) {
      TIFFSwabLong(&l);
    }
    _TIFFmemcpy(cp, &l, dir->tdir_count);
    return (1);
  }
  return (TIFFFetchData(tif, dir, cp));
}

/*
 * Convert numerator+denominator to float.
 */
static int cvtRational(TIFF *tif, TIFFDirEntry *dir, uint32 num, uint32 denom,
                       float *rv) {
 /* jump:1054 */  if (denom == 0) {
    TIFFErrorExt(tif->tif_clientdata, tif->tif_name,
                 "%s: Rational with zero denominator (num = %lu)",
                 _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name, num);
    return (0);
  } else {
 /* jump:1057 */    if (dir->tdir_type == TIFF_RATIONAL) {
      *rv = ((float)num / (float)denom);
    } else {
      *rv = ((float)(int32)num / (float)(int32)denom);
    }
    return (1);
  }
}

/*
 * Fetch a rational item from the file
 * at offset off and return the value
 * as a floating point number.
 */
static float TIFFFetchRational(TIFF *tif, TIFFDirEntry *dir) {
  uint32 l[2];
  float v;

  return (!TIFFFetchData(tif, dir, (char *)l) ||
                  !cvtRational(tif, dir, l[0], l[1], &v)
              ? 1.0f
              : v);
}

/*
 * Fetch a single floating point value
 * from the offset field and return it
 * as a native float.
 */
static float TIFFFetchFloat(TIFF *tif, TIFFDirEntry *dir) {
  float v;
  int32 l = TIFFExtractData(tif, dir->tdir_type, dir->tdir_offset);
  _TIFFmemcpy(&v, &l, sizeof(float));
  TIFFCvtIEEEFloatToNative(tif, 1, &v);
  return (v);
}

/*
 * Fetch an array of BYTE or SBYTE values.
 */
static int TIFFFetchByteArray(TIFF *tif, TIFFDirEntry *dir, uint8 *v) {
 /* jump:1150 */  if (dir->tdir_count <= 4) {
    /*
     * Extract data from offset field.
     */
 /* jump:1124 */    if (tif->tif_header.tiff_magic == TIFF_BIGENDIAN) {
 /* jump:1112 */      if (dir->tdir_type == TIFF_SBYTE) {
        switch (dir->tdir_count) {
        case 4:
          v[3] = dir->tdir_offset & 0xff;
        case 3:
          v[2] = (dir->tdir_offset >> 8) & 0xff;
        case 2:
          v[1] = (dir->tdir_offset >> 16) & 0xff;
        case 1:
          v[0] = dir->tdir_offset >> 24;
        }
      } else {
        switch (dir->tdir_count) {
        case 4:
          v[3] = dir->tdir_offset & 0xff;
        case 3:
          v[2] = (dir->tdir_offset >> 8) & 0xff;
        case 2:
          v[1] = (dir->tdir_offset >> 16) & 0xff;
        case 1:
          v[0] = dir->tdir_offset >> 24;
        }
      }
    } else {
 /* jump:1136 */      if (dir->tdir_type == TIFF_SBYTE) {
        switch (dir->tdir_count) {
        case 4:
          v[3] = dir->tdir_offset >> 24;
        case 3:
          v[2] = (dir->tdir_offset >> 16) & 0xff;
        case 2:
          v[1] = (dir->tdir_offset >> 8) & 0xff;
        case 1:
          v[0] = dir->tdir_offset & 0xff;
        }
      } else {
        switch (dir->tdir_count) {
        case 4:
          v[3] = dir->tdir_offset >> 24;
        case 3:
          v[2] = (dir->tdir_offset >> 16) & 0xff;
        case 2:
          v[1] = (dir->tdir_offset >> 8) & 0xff;
        case 1:
          v[0] = dir->tdir_offset & 0xff;
        }
      }
    }
    return (1);
  } else {
    return (TIFFFetchData(tif, dir, (char *)v) != 0); /* XXX */
  }
}

/*
 * Fetch an array of SHORT or SSHORT values.
 */
static int TIFFFetchShortArray(TIFF *tif, TIFFDirEntry *dir, uint16 *v) {
 /* jump:1176 */  if (dir->tdir_count <= 2) {
 /* jump:1167 */    if (tif->tif_header.tiff_magic == TIFF_BIGENDIAN) {
      switch (dir->tdir_count) {
      case 2:
        v[1] = (uint16)(dir->tdir_offset & 0xffff);
      case 1:
        v[0] = (uint16)(dir->tdir_offset >> 16);
      }
    } else {
      switch (dir->tdir_count) {
      case 2:
        v[1] = (uint16)(dir->tdir_offset >> 16);
      case 1:
        v[0] = (uint16)(dir->tdir_offset & 0xffff);
      }
    }
    return (1);
  } else {
    return (TIFFFetchData(tif, dir, (char *)v) != 0);
  }
}

/*
 * Fetch a pair of SHORT or BYTE values. Some tags may have either BYTE
 * or SHORT type and this function works with both ones.
 */
static int TIFFFetchShortPair(TIFF *tif, TIFFDirEntry *dir) {
  switch (dir->tdir_type) {
  case TIFF_BYTE:
  case TIFF_SBYTE: {
    uint8 v[4];
    return TIFFFetchByteArray(tif, dir, v) &&
           TIFFSetField(tif, dir->tdir_tag, v[0], v[1]);
  }
  case TIFF_SHORT:
  case TIFF_SSHORT: {
    uint16 v[2];
    return TIFFFetchShortArray(tif, dir, v) &&
           TIFFSetField(tif, dir->tdir_tag, v[0], v[1]);
  }
  default:
    return 0;
  }
}

/*
 * Fetch an array of LONG or SLONG values.
 */
static int TIFFFetchLongArray(TIFF *tif, TIFFDirEntry *dir, uint32 *v) {
 /* jump:1211 */  if (dir->tdir_count == 1) {
    v[0] = dir->tdir_offset;
    return (1);
  } else {
    return (TIFFFetchData(tif, dir, (char *)v) != 0);
  }
}

/*
 * Fetch an array of RATIONAL or SRATIONAL values.
 */
static int TIFFFetchRationalArray(TIFF *tif, TIFFDirEntry *dir, float *v) {
  int ok = 0;
  uint32 *l;

  l = (uint32 *)_TIFFCheckMalloc(tif, dir->tdir_count,
                                 TIFFDataWidth((TIFFDataType)dir->tdir_type),
                                 "to fetch array of rationals");
 /* jump:1237 */  if (l) {
 /* jump:1235 */    if (TIFFFetchData(tif, dir, (char *)l)) {
      uint32 i;
 /* jump:1234 */      for (i = 0; i < dir->tdir_count; i++) {
        ok = cvtRational(tif, dir, l[2 * i + 0], l[2 * i + 1], &v[i]);
 /* jump:1233 */        if (!ok) {
          break;
        }
      }
    }
    _TIFFfree((char *)l);
  }
  return (ok);
}

/*
 * Fetch an array of FLOAT values.
 */
static int TIFFFetchFloatArray(TIFF *tif, TIFFDirEntry *dir, float *v) {

 /* jump:1250 */  if (dir->tdir_count == 1) {
    v[0] = *(float *)&dir->tdir_offset;
    TIFFCvtIEEEFloatToNative(tif, dir->tdir_count, v);
    return (1);
 /* jump:1253 */  } else if (TIFFFetchData(tif, dir, (char *)v)) {
    TIFFCvtIEEEFloatToNative(tif, dir->tdir_count, v);
    return (1);
  } else {
    return (0);
  }
}

/*
 * Fetch an array of DOUBLE values.
 */
static int TIFFFetchDoubleArray(TIFF *tif, TIFFDirEntry *dir, double *v) {
 /* jump:1265 */  if (TIFFFetchData(tif, dir, (char *)v)) {
    TIFFCvtIEEEDoubleToNative(tif, dir->tdir_count, v);
    return (1);
  } else {
    return (0);
  }
}

/*
 * Fetch an array of ANY values.  The actual values are
 * returned as doubles which should be able hold all the
 * types.  Yes, there really should be an tany_t to avoid
 * this potential non-portability ...  Note in particular
 * that we assume that the double return value vector is
 * large enough to read in any fundamental type.  We use
 * that vector as a buffer to read in the base type vector
 * and then convert it in place to double (from end
 * to front of course).
 */
static int TIFFFetchAnyArray(TIFF *tif, TIFFDirEntry *dir, double *v) {
  int i;

  switch (dir->tdir_type) {
  case TIFF_BYTE:
  case TIFF_SBYTE:
 /* jump:1289 */    if (!TIFFFetchByteArray(tif, dir, (uint8 *)v)) {
      return (0);
    }
 /* jump:1295 */    if (dir->tdir_type == TIFF_BYTE) {
      uint8 *vp = (uint8 *)v;
 /* jump:1294 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    } else {
      int8 *vp = (int8 *)v;
 /* jump:1299 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    }
    break;
  case TIFF_SHORT:
  case TIFF_SSHORT:
 /* jump:1306 */    if (!TIFFFetchShortArray(tif, dir, (uint16 *)v)) {
      return (0);
    }
 /* jump:1312 */    if (dir->tdir_type == TIFF_SHORT) {
      uint16 *vp = (uint16 *)v;
 /* jump:1311 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    } else {
      int16 *vp = (int16 *)v;
 /* jump:1316 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    }
    break;
  case TIFF_LONG:
  case TIFF_SLONG:
 /* jump:1323 */    if (!TIFFFetchLongArray(tif, dir, (uint32 *)v)) {
      return (0);
    }
 /* jump:1329 */    if (dir->tdir_type == TIFF_LONG) {
      uint32 *vp = (uint32 *)v;
 /* jump:1328 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    } else {
      int32 *vp = (int32 *)v;
 /* jump:1333 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    }
    break;
  case TIFF_RATIONAL:
  case TIFF_SRATIONAL:
 /* jump:1340 */    if (!TIFFFetchRationalArray(tif, dir, (float *)v)) {
      return (0);
    }
    {
      float *vp = (float *)v;
 /* jump:1345 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    }
    break;
  case TIFF_FLOAT:
 /* jump:1351 */    if (!TIFFFetchFloatArray(tif, dir, (float *)v)) {
      return (0);
    }
    {
      float *vp = (float *)v;
 /* jump:1356 */      for (i = dir->tdir_count - 1; i >= 0; i--) {
        v[i] = vp[i];
      }
    }
    break;
  case TIFF_DOUBLE:
    return (TIFFFetchDoubleArray(tif, dir, (double *)v));
  default:
    /* TIFF_NOTYPE */
    /* TIFF_ASCII */
    /* TIFF_UNDEFINED */
    TIFFErrorExt(tif->tif_clientdata, tif->tif_name,
                 "cannot read TIFF_ANY type %d for field \"%s\"",
                 _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name);
    return (0);
  }
  return (1);
}

/*
 * Fetch a tag that is not handled by special case code.
 */
static int TIFFFetchNormalTag(TIFF *tif, TIFFDirEntry *dp) {
  static const char mesg[] = "to fetch tag value";
  int ok = 0;
  const TIFFFieldInfo *fip = _TIFFFieldWithTag(tif, dp->tdir_tag);

 /* jump:1433 */  if (dp->tdir_count > 1) { /* array of values */
    char *cp = NULL;

    switch (dp->tdir_type) {
    case TIFF_BYTE:
    case TIFF_SBYTE:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(uint8), mesg);
      ok = cp && TIFFFetchByteArray(tif, dp, (uint8 *)cp);
      break;
    case TIFF_SHORT:
    case TIFF_SSHORT:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(uint16), mesg);
      ok = cp && TIFFFetchShortArray(tif, dp, (uint16 *)cp);
      break;
    case TIFF_LONG:
    case TIFF_SLONG:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(uint32), mesg);
      ok = cp && TIFFFetchLongArray(tif, dp, (uint32 *)cp);
      break;
    case TIFF_RATIONAL:
    case TIFF_SRATIONAL:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(float), mesg);
      ok = cp && TIFFFetchRationalArray(tif, dp, (float *)cp);
      break;
    case TIFF_FLOAT:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(float), mesg);
      ok = cp && TIFFFetchFloatArray(tif, dp, (float *)cp);
      break;
    case TIFF_DOUBLE:
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count, sizeof(double), mesg);
      ok = cp && TIFFFetchDoubleArray(tif, dp, (double *)cp);
      break;
    case TIFF_ASCII:
    case TIFF_UNDEFINED: /* bit of a cheat... */
      /*
       * Some vendors write strings w/o the trailing
       * NULL byte, so always append one just in case.
       */
      cp = (char *)_TIFFCheckMalloc(tif, dp->tdir_count + 1, 1, mesg);
 /* jump:1422 */      if ((ok = (cp && TIFFFetchString(tif, dp, cp))) != 0) {
        cp[dp->tdir_count] = '\0'; /* XXX */
      }
      break;
    }
 /* jump:1429 */    if (ok) {
      ok = (fip->field_passcount
                ? TIFFSetField(tif, dp->tdir_tag, dp->tdir_count, cp)
                : TIFFSetField(tif, dp->tdir_tag, cp));
    }
 /* jump:1432 */    if (cp != NULL) {
      _TIFFfree(cp);
    }
 /* jump:1494 */  } else if (CheckDirCount(tif, dp, 1)) { /* singleton value */
    switch (dp->tdir_type) {
    case TIFF_BYTE:
    case TIFF_SBYTE:
    case TIFF_SHORT:
    case TIFF_SSHORT:
      /*
       * If the tag is also acceptable as a LONG or SLONG
       * then TIFFSetField will expect an uint32 parameter
       * passed to it (through varargs).  Thus, for machines
       * where sizeof (int) != sizeof (uint32) we must do
       * a careful check here.  It's hard to say if this
       * is worth optimizing.
       *
       * NB: We use TIFFFieldWithTag here knowing that
       *     it returns us the first entry in the table
       *     for the tag and that that entry is for the
       *     widest potential data type the tag may have.
       */
      {
        TIFFDataType type = fip->field_type;
 /* jump:1460 */        if (type != TIFF_LONG && type != TIFF_SLONG) {
          uint16 v =
              (uint16)TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
          ok = (fip->field_passcount ? TIFFSetField(tif, dp->tdir_tag, 1, &v)
                                     : TIFFSetField(tif, dp->tdir_tag, v));
          break;
        }
      }
      /* fall thru... */
    case TIFF_LONG:
    case TIFF_SLONG: {
      uint32 v32 = TIFFExtractData(tif, dp->tdir_type, dp->tdir_offset);
      ok = (fip->field_passcount ? TIFFSetField(tif, dp->tdir_tag, 1, &v32)
                                 : TIFFSetField(tif, dp->tdir_tag, v32));
    } break;
    case TIFF_RATIONAL:
    case TIFF_SRATIONAL:
    case TIFF_FLOAT: {
      float v = (dp->tdir_type == TIFF_FLOAT ? TIFFFetchFloat(tif, dp)
                                             : TIFFFetchRational(tif, dp));
      ok = (fip->field_passcount ? TIFFSetField(tif, dp->tdir_tag, 1, &v)
                                 : TIFFSetField(tif, dp->tdir_tag, v));
    } break;
    case TIFF_DOUBLE: {
      double v;
      ok = (TIFFFetchDoubleArray(tif, dp, &v) &&
            (fip->field_passcount ? TIFFSetField(tif, dp->tdir_tag, 1, &v)
                                  : TIFFSetField(tif, dp->tdir_tag, v)));
    } break;
    case TIFF_ASCII:
    case TIFF_UNDEFINED: /* bit of a cheat... */
    {
      char c[2];
 /* jump:1491 */      if ((ok = (TIFFFetchString(tif, dp, c) != 0)) != 0) {
        c[1] = '\0'; /* XXX paranoid */
        ok = (fip->field_passcount ? TIFFSetField(tif, dp->tdir_tag, 1, c)
                                   : TIFFSetField(tif, dp->tdir_tag, c));
      }
    } break;
    }
  }
  return (ok);
}

#define NITEMS(x) (sizeof(x) / sizeof(x[0]))
/*
 * Fetch samples/pixel short values for
 * the specified tag and verify that
 * all values are the same.
 */
static int TIFFFetchPerSampleShorts(TIFF *tif, TIFFDirEntry *dir, uint16 *pl) {
  uint16 samples = tif->tif_dir.td_samplesperpixel;
  int status = 0;

 /* jump:1539 */  if (CheckDirCount(tif, dir, (uint32)samples)) {
    uint16 buf[10];
    uint16 *v = buf;

 /* jump:1515 */    if (dir->tdir_count > NITEMS(buf)) {
      v = (uint16 *)_TIFFCheckMalloc(tif, dir->tdir_count, sizeof(uint16),
                                     "to fetch per-sample values");
    }
 /* jump:1534 */    if (v && TIFFFetchShortArray(tif, dir, v)) {
      uint16 i;
      int check_count = dir->tdir_count;
 /* jump:1521 */      if (samples < check_count) {
        check_count = samples;
      }

 /* jump:1531 */      for (i = 1; i < check_count; i++) {
 /* jump:1530 */        if (v[i] != v[0]) {
          TIFFErrorExt(
              tif->tif_clientdata, tif->tif_name,
              "Cannot handle different per-sample values for field \"%s\"",
              _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name);
          goto bad;
        }
      }
      *pl = v[0];
      status = 1;
    }
  bad:
 /* jump:1538 */    if (v && v != buf) {
      _TIFFfree(v);
    }
  }
  return (status);
}

/*
 * Fetch samples/pixel long values for
 * the specified tag and verify that
 * all values are the same.
 */
static int TIFFFetchPerSampleLongs(TIFF *tif, TIFFDirEntry *dir, uint32 *pl) {
  uint16 samples = tif->tif_dir.td_samplesperpixel;
  int status = 0;

 /* jump:1583 */  if (CheckDirCount(tif, dir, (uint32)samples)) {
    uint32 buf[10];
    uint32 *v = buf;

 /* jump:1559 */    if (dir->tdir_count > NITEMS(buf)) {
      v = (uint32 *)_TIFFCheckMalloc(tif, dir->tdir_count, sizeof(uint32),
                                     "to fetch per-sample values");
    }
 /* jump:1578 */    if (v && TIFFFetchLongArray(tif, dir, v)) {
      uint16 i;
      int check_count = dir->tdir_count;

 /* jump:1566 */      if (samples < check_count) {
        check_count = samples;
      }
 /* jump:1575 */      for (i = 1; i < check_count; i++) {
 /* jump:1574 */        if (v[i] != v[0]) {
          TIFFErrorExt(
              tif->tif_clientdata, tif->tif_name,
              "Cannot handle different per-sample values for field \"%s\"",
              _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name);
          goto bad;
        }
      }
      *pl = v[0];
      status = 1;
    }
  bad:
 /* jump:1582 */    if (v && v != buf) {
      _TIFFfree(v);
    }
  }
  return (status);
}

/*
 * Fetch samples/pixel ANY values for the specified tag and verify that all
 * values are the same.
 */
static int TIFFFetchPerSampleAnys(TIFF *tif, TIFFDirEntry *dir, double *pl) {
  uint16 samples = tif->tif_dir.td_samplesperpixel;
  int status = 0;

 /* jump:1626 */  if (CheckDirCount(tif, dir, (uint32)samples)) {
    double buf[10];
    double *v = buf;

 /* jump:1602 */    if (dir->tdir_count > NITEMS(buf)) {
      v = (double *)_TIFFCheckMalloc(tif, dir->tdir_count, sizeof(double),
                                     "to fetch per-sample values");
    }
 /* jump:1621 */    if (v && TIFFFetchAnyArray(tif, dir, v)) {
      uint16 i;
      int check_count = dir->tdir_count;
 /* jump:1608 */      if (samples < check_count) {
        check_count = samples;
      }

 /* jump:1618 */      for (i = 1; i < check_count; i++) {
 /* jump:1617 */        if (v[i] != v[0]) {
          TIFFErrorExt(
              tif->tif_clientdata, tif->tif_name,
              "Cannot handle different per-sample values for field \"%s\"",
              _TIFFFieldWithTag(tif, dir->tdir_tag)->field_name);
          goto bad;
        }
      }
      *pl = v[0];
      status = 1;
    }
  bad:
 /* jump:1625 */    if (v && v != buf) {
      _TIFFfree(v);
    }
  }
  return (status);
}
#undef NITEMS

/*
 * Fetch a set of offsets or lengths.
 * While this routine says "strips", in fact it's also used for tiles.
 */
static int TIFFFetchStripThing(TIFF *tif, TIFFDirEntry *dir, long nstrips,
                               uint32 **lpp) {
  register uint32 *lp;
  int status;

  CheckDirCount(tif, dir, (uint32)nstrips);

  /*
   * Allocate space for strip information.
   */
 /* jump:1649 */  if (*lpp == NULL &&
      (*lpp = (uint32 *)_TIFFCheckMalloc(tif, nstrips, sizeof(uint32),
                                         "for strip array")) == NULL) {
    return (0);
  }
  lp = *lpp;
  _TIFFmemset(lp, 0, sizeof(uint32) * nstrips);

 /* jump:1671 */  if (dir->tdir_type == (int)TIFF_SHORT) {
    /*
     * Handle uint16->uint32 expansion.
     */
    uint16 *dp = (uint16 *)_TIFFCheckMalloc(
        tif, dir->tdir_count, sizeof(uint16), "to fetch strip tag");
 /* jump:1661 */    if (dp == NULL) {
      return (0);
    }
 /* jump:1668 */    if ((status = TIFFFetchShortArray(tif, dir, dp)) != 0) {
      int i;

 /* jump:1667 */      for (i = 0; i < nstrips && i < (int)dir->tdir_count; i++) {
        lp[i] = dp[i];
      }
    }
    _TIFFfree((char *)dp);

 /* jump:1690 */  } else if (nstrips != (int)dir->tdir_count) {
    /* Special case to correct length */

    uint32 *dp = (uint32 *)_TIFFCheckMalloc(
        tif, dir->tdir_count, sizeof(uint32), "to fetch strip tag");
 /* jump:1678 */    if (dp == NULL) {
      return (0);
    }

    status = TIFFFetchLongArray(tif, dir, dp);
 /* jump:1687 */    if (status != 0) {
      int i;

 /* jump:1686 */      for (i = 0; i < nstrips && i < (int)dir->tdir_count; i++) {
        lp[i] = dp[i];
      }
    }

    _TIFFfree((char *)dp);
  } else {
    status = TIFFFetchLongArray(tif, dir, lp);
  }

  return (status);
}

/*
 * Fetch and set the RefBlackWhite tag.
 */
static int TIFFFetchRefBlackWhite(TIFF *tif, TIFFDirEntry *dir) {
  static const char mesg[] = "for \"ReferenceBlackWhite\" array";
  char *cp;
  int ok;

 /* jump:1707 */  if (dir->tdir_type == TIFF_RATIONAL) {
    return (TIFFFetchNormalTag(tif, dir));
  }
  /*
   * Handle LONG's for backward compatibility.
   */
  cp = (char *)_TIFFCheckMalloc(tif, dir->tdir_count, sizeof(uint32), mesg);
 /* jump:1723 */  if ((ok = (cp && TIFFFetchLongArray(tif, dir, (uint32 *)cp))) != 0) {
    float *fp =
        (float *)_TIFFCheckMalloc(tif, dir->tdir_count, sizeof(float), mesg);
 /* jump:1722 */    if ((ok = (fp != NULL)) != 0) {
      uint32 i;
 /* jump:1719 */      for (i = 0; i < dir->tdir_count; i++) {
        fp[i] = (float)((uint32 *)cp)[i];
      }
      ok = TIFFSetField(tif, dir->tdir_tag, fp);
      _TIFFfree((char *)fp);
    }
  }
 /* jump:1726 */  if (cp) {
    _TIFFfree(cp);
  }
  return (ok);
}

/*
 * Replace a single strip (tile) of uncompressed data by
 * multiple strips (tiles), each approximately 8Kbytes.
 * This is useful for dealing with large images or
 * for dealing with machines with a limited amount
 * memory.
 */
static void ChopUpSingleUncompressedStrip(TIFF *tif) {
  register TIFFDirectory *td = &tif->tif_dir;
  uint32 bytecount = td->td_stripbytecount[0];
  uint32 offset = td->td_stripoffset[0];
  tsize_t rowbytes = TIFFVTileSize(tif, 1), stripbytes;
  tstrip_t strip, nstrips, rowsperstrip;
  uint32 *newcounts;
  uint32 *newoffsets;

  /*
   * Make the rows hold at least one scanline, but fill specified amount
   * of data if possible.
   */
#ifndef STRIP_SIZE_DEFAULT
#define STRIP_SIZE_DEFAULT 8192
#endif
 /* jump:1756 */  if (rowbytes > STRIP_SIZE_DEFAULT) {
    stripbytes = rowbytes;
    rowsperstrip = 1;
 /* jump:1759 */  } else if (rowbytes > 0) {
    rowsperstrip = STRIP_SIZE_DEFAULT / rowbytes;
    stripbytes = rowbytes * rowsperstrip;
  } else {
    return;
  }
#undef STRIP_SIZE_DEFAULT

  /*
   * never increase the number of strips in an image
   */
 /* jump:1769 */  if (rowsperstrip >= td->td_rowsperstrip) {
    return;
  }
  nstrips = (tstrip_t)TIFFhowmany(bytecount, stripbytes);
 /* jump:1773 */  if (nstrips == 0) { /* something is wonky, do nothing. */
    return;
  }

  newcounts = (uint32 *)_TIFFCheckMalloc(
      tif, nstrips, sizeof(uint32), "for chopped \"StripByteCounts\" array");
  newoffsets = (uint32 *)_TIFFCheckMalloc(tif, nstrips, sizeof(uint32),
                                          "for chopped \"StripOffsets\" array");
 /* jump:1791 */  if (newcounts == NULL || newoffsets == NULL) {
    /*
     * Unable to allocate new strip information, give
     * up and use the original one strip information.
     */
 /* jump:1786 */    if (newcounts != NULL) {
      _TIFFfree(newcounts);
    }
 /* jump:1789 */    if (newoffsets != NULL) {
      _TIFFfree(newoffsets);
    }
    return;
  }
  /*
   * Fill the strip information arrays with new bytecounts and offsets
   * that reflect the broken-up format.
   */
 /* jump:1804 */  for (strip = 0; strip < nstrips; strip++) {
 /* jump:1799 */    if (stripbytes > (tsize_t)bytecount) {
      stripbytes = bytecount;
    }
    newcounts[strip] = stripbytes;
    newoffsets[strip] = offset;
    offset += stripbytes;
    bytecount -= stripbytes;
  }
  /*
   * Replace old single strip info with multi-strip info.
   */
  td->td_stripsperimage = td->td_nstrips = nstrips;
  TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsperstrip);

  _TIFFfree(td->td_stripbytecount);
  _TIFFfree(td->td_stripoffset);
  td->td_stripbytecount = newcounts;
  td->td_stripoffset = newoffsets;
  td->td_stripbytecountsorted = 1;
}

/* vim: set ts=8 sts=8 sw=8 noet: */
