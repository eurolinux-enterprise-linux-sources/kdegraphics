
/*
   Copyright (c) 2003-2007 Clarence Dang <dang@kde.org>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#define DEBUG_KP_EFFECT_REDUCE_COLORS 0


#include <kpEffectReduceColors.h>

#include <qbitmap.h>
#include <qimage.h>
#include <qpixmap.h>

#include <kdebug.h>

#include <kpPixmapFX.h>


static QImage::Format DepthToFormat (int depth)
{
    // These values (1, 8, 32) are QImage's supported depths.
    switch (depth)
    {
    case 1:
        // (can be MSB instead, I suppose)
        return QImage::Format_MonoLSB;
    
    case 8:
        return QImage::Format_Indexed8;

    // 24-bit is known as 32-bit with QImage.
    case 32:
        // TODO: If the src image has no mask, we should really return RGB32, to
        //       avoid introducing a mask.
        return QImage::Format_ARGB32;

    default:
        Q_ASSERT (!"unknown depth");
        return QImage::Format_Invalid;
    }
}

// public static
QImage kpEffectReduceColors::convertImageDepth (const QImage &image, int depth, bool dither)
{
#if DEBUG_KP_EFFECT_REDUCE_COLORS
    kDebug () << "kpeffectreducecolors.cpp:ConvertImageDepth() changing image (w=" << image.width ()
               << ",h=" << image.height ()
               << ") depth from " << image.depth ()
                << " to " << depth
                << " (dither=" << dither << ")"
                << endl;
#endif

    if (image.isNull ())
        return image;

    if (depth == image.depth ())
        return image;


#if DEBUG_KP_EFFECT_REDUCE_COLORS && 0
    for (int y = 0; y < image.height (); y++)
    {
        for (int x = 0; x < image.width (); x++)
        {
            fprintf (stderr, " %08X", image.pixel (x, y));
        }
        fprintf (stderr, "\n");
    }
#endif


    // Hack around Qt's braindead QImage::convertToFormat(QImage::Format_MonoLSB, ...)
    // (with dithering off) which produces pathetic results with an image that
    // only has 2 colors - sometimes it just gives a completely black
    // result (try yellow and white as input).  Instead, we simply preserve
    // the 2 colours.
    //
    // One use case is resaving a "color monochrome" image (<= 2 colors but
    // not necessarily black & white).
    if (depth == 1 && !dither)
    {
    #if DEBUG_KP_EFFECT_REDUCE_COLORS
        kDebug () << "\tinvoking convert-to-depth 1 hack";
    #endif
        QRgb color0 = 0, color1 = 0;
        bool color0Valid = false, color1Valid = false;

        bool moreThan2Colors = false;

        QImage monoImage (image.width (), image.height (), QImage::Format_MonoLSB);
        monoImage.setNumColors (2);
    #if DEBUG_KP_EFFECT_REDUCE_COLORS
        kDebug () << "\t\tinitialising output image w=" << monoImage.width ()
                   << ",h=" << monoImage.height ()
                   << ",d=" << monoImage.depth ()
                   << endl;
    #endif
        for (int y = 0; y < image.height (); y++)
        {
            for (int x = 0; x < image.width (); x++)
            {
                // (this can be transparent)
                QRgb imagePixel = image.pixel (x, y);

                if (color0Valid && imagePixel == color0)
                    monoImage.setPixel (x, y, 0);
                else if (color1Valid && imagePixel == color1)
                    monoImage.setPixel (x, y, 1);
                else if (!color0Valid)
                {
                    color0 = imagePixel;
                    color0Valid = true;
                    monoImage.setPixel (x, y, 0);
                #if DEBUG_KP_EFFECT_REDUCE_COLORS
                    kDebug () << "\t\t\tcolor0=" << (int *) color0
                               << " at x=" << x << ",y=" << y << endl;
                #endif
                }
                else if (!color1Valid)
                {
                    color1 = imagePixel;
                    color1Valid = true;
                    monoImage.setPixel (x, y, 1);
                #if DEBUG_KP_EFFECT_REDUCE_COLORS
                    kDebug () << "\t\t\tcolor1=" << (int *) color1
                               << " at x=" << x << ",y=" << y << endl;
                #endif
                }
                else
                {
                #if DEBUG_KP_EFFECT_REDUCE_COLORS
                    kDebug () << "\t\t\timagePixel=" << (int *) imagePixel
                               << " at x=" << x << ",y=" << y
                               << " moreThan2Colors - abort hack" << endl;
                #endif
                    moreThan2Colors = true;

                    // Dijkstra, this is clearer than double break'ing or
                    // a check in both loops
                    goto exit_loop;
                }
            }
        }
    exit_loop:

        if (!moreThan2Colors)
        {
            monoImage.setColor (0, color0Valid ? color0 : 0xFFFFFF);
            monoImage.setColor (1, color1Valid ? color1 : 0x000000);
            return monoImage;
        }
    }


    QImage retImage = image.convertToFormat (::DepthToFormat (depth),
        Qt::AutoColor |
        (dither ? Qt::DiffuseDither : Qt::ThresholdDither) |
        Qt::ThresholdAlphaDither |
        (dither ? Qt::PreferDither : Qt::AvoidDither));
#if DEBUG_KP_EFFECT_REDUCE_COLORS
    kDebug () << "\tformat: before=" << image.format ()
              << "after=" << retImage.format ();
#endif

#if DEBUG_KP_EFFECT_REDUCE_COLORS && 0
    kDebug () << "After colour reduction:";
    for (int y = 0; y < image.height (); y++)
    {
        for (int x = 0; x < image.width (); x++)
        {
            fprintf (stderr, " %08X", image.pixel (x, y));
        }
        fprintf (stderr, "\n");
    }
#endif

    return retImage;
}


// public static
void kpEffectReduceColors::applyEffect (QPixmap *destPixmapPtr, int depth, bool dither)
{
    if (!destPixmapPtr)
        return;

    // You can't "reduce" to 32-bit since it's the highest depth.
    if (depth != 1 && depth != 8)
        return;


    QImage image = kpPixmapFX::convertToQImage (*destPixmapPtr);


    image = kpEffectReduceColors::convertImageDepth (image, depth, dither);

    if (image.isNull ())
        return;


    QPixmap pixmap = kpPixmapFX::convertToPixmap (image, false/*no dither*/);


#if DEBUG_KP_EFFECT_REDUCE_COLORS
    kDebug () << "after image depth conversion: hasMask="
              << kpPixmapFX::hasMask (pixmap)
              << " src.hasMask="
              << kpPixmapFX::hasMask (*destPixmapPtr);
#endif
    

    // HACK: The above "image.convertToFormat()" erases the mask
    //       (at least for monochrome).
    //       qpixmap.html says "alpha masks on monochrome images are ignored."
    //
    //       Put the mask back.
    //
    //       Still required under Qt 4.3.1.
    if (kpPixmapFX::hasMask (*destPixmapPtr))
        pixmap.setMask (destPixmapPtr->mask ());

    *destPixmapPtr = pixmap;
}

// public static
QPixmap kpEffectReduceColors::applyEffect (const QPixmap &pm, int depth, bool dither)
{
    QPixmap ret = pm;
    applyEffect (&ret, depth, dither);
    return ret;
}


