#ifndef _LED_EFFECT_H
#define _LED_EFFECT_H

#include "Strip.h"

namespace pix {

  class LedStripEffectInterface {
    public:
      virtual ~LedStripEffectInterface() = default;
      virtual void update() = 0;
      virtual void start() = 0;
      virtual void stop() = 0;
      virtual void setSpeed() = 0;
      virtual void setBrightness() = 0;
      virtual void setBaseColor() = 0;
      virtual bool isActive() = 0;
      virtual const StripHandler getStrip() { return strip; }
    protected:
      StripHandler strip;
  };

}


#endif
