// © You i Labs Inc. 2000-2020. All rights reserved.

#ifndef _TIZEN_CAPTION_BUTTON_APP_
#define _TIZEN_CAPTION_BUTTON_APP_

#include <framework/YiApp.h>

class TizenCaptionButtonApp : public CYIApp
{
public:
    TizenCaptionButtonApp();
    virtual ~TizenCaptionButtonApp();

    virtual bool UserInit() override;
    virtual bool UserStart() override;
    virtual void UserUpdate() override;
};

#endif // _TIZEN_CAPTION_BUTTON_APP_
