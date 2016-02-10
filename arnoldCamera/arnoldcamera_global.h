#ifndef ARNOLDCAMERA_GLOBAL_H
#define ARNOLDCAMERA_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(ARNOLDCAMERA_LIBRARY)
#  define ARNOLDCAMERASHARED_EXPORT Q_DECL_EXPORT
#else
#  define ARNOLDCAMERASHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // ARNOLDCAMERA_GLOBAL_H
