/***************************************************************************
 *   Copyright (C) 2008 by Ely Levy <elylevy@cs.huji.ac.il>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "epubdocument.h"

using namespace Epub;

EpubDocument::EpubDocument(const QString &fileName) : QTextDocument() 
{
  mEpub = epub_open(qPrintable(fileName), 3);
}

bool EpubDocument::isValid() 
{
  return (mEpub?true:false); 
}

EpubDocument::~EpubDocument() {

  if (mEpub)
    epub_close(mEpub);

  epub_cleanup();  
}
  
struct epub *EpubDocument::getEpub() 
{
  return mEpub;
}
    
QVariant EpubDocument::loadResource(int type, const QUrl &name) 
{
  int size;
  char *data;
   
  // Get the data from the epub file
  size = epub_get_data(mEpub, name.toString().toUtf8(), &data);


  QVariant resource;

  if (size > 0) {
    switch(type) {
    case QTextDocument::ImageResource:
      resource.setValue(QImage::fromData((unsigned char *)data, size));
      break;
      
    default:
      resource.setValue(QString::fromUtf8(data));
      break;
    }
    
    free(data);
  }
 
  // add to cache
  addResource(type, name, resource); 
    
  return resource;
}
    

