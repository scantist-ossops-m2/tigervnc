#ifndef VNCTRANSLATOR_H
#define VNCTRANSLATOR_H

#include "gettext.h"

#include <QTranslator>

class VNCTranslator : public QTranslator
{
public:
  QString translate(char const* context,
                    char const* sourceText,
                    char const* disambiguation = nullptr,
                    int         n              = -1) const override
  {
    Q_UNUSED(context)
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)
    return QString::fromUtf8(gettext(sourceText));
  }
};

#endif // VNCTRANSLATOR_H
