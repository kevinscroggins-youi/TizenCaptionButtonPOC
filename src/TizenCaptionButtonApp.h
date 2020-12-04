// © You i Labs Inc. 2000-2020. All rights reserved.

#ifndef _TIZEN_CAPTION_BUTTON_APP_
#define _TIZEN_CAPTION_BUTTON_APP_

#include <framework/YiApp.h>
#include <event/YiEventHandler.h>

class TizenCaptionButtonApp : public CYIApp, public CYIEventHandler
{
public:
    TizenCaptionButtonApp();
    virtual ~TizenCaptionButtonApp();

    virtual bool UserInit() override;
    virtual bool UserStart() override;
    virtual void UserUpdate() override;
    virtual bool HandleEvent(const std::shared_ptr<CYIEventDispatcher> &pDispatcher, CYIEvent *pEvent) override;
};

#endif // _TIZEN_CAPTION_BUTTON_APP_
