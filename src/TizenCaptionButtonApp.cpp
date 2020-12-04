// © You i Labs Inc. 2000-2020. All rights reserved.

#include "TizenCaptionButtonApp.h"

#include <event/YiKeyEvent.h>

#define LOG_TAG "TizenCaptionButtonApp"

TizenCaptionButtonApp::TizenCaptionButtonApp() = default;

TizenCaptionButtonApp::~TizenCaptionButtonApp() = default;

bool TizenCaptionButtonApp::UserInit()
{
    CYIEventDispatcher::GetDefaultDispatcher()->RegisterEventHandler(this);

    return true;
}

bool TizenCaptionButtonApp::UserStart()
{
    return true;
}

void TizenCaptionButtonApp::UserUpdate()
{
}

bool TizenCaptionButtonApp::HandleEvent(const std::shared_ptr<CYIEventDispatcher> &pDispatcher, CYIEvent *pEvent)
{
    YI_UNUSED(pDispatcher);

    CYIEvent::Type type = pEvent->GetType();

    if (pEvent->IsKeyEvent())
    {
        CYIKeyEvent *pKeyEvent = dynamic_cast<CYIKeyEvent *>(pEvent);

        if (type == CYIEvent::Type::KeyDown)
        {
            switch (pKeyEvent->m_keyCode)
            {
                case CYIKeyEvent::KeyCode::Captions:
                    YI_LOGI(LOG_TAG, "Captions button pressed!");
                    break;

                default:
                    break;
            }
        }
    }

    return false;
}
