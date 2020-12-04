// © You i Labs Inc. 2000-2020. All rights reserved.
#if defined(YI_TIZEN_NACL)

#    include "AppFactory.h"

#    include <event/YiActionEvent.h>
#    include <event/YiKeyEvent.h>
#    include <framework/YiFramework.h>
#    include <input/YiBackButtonHandler.h>
#    include <logging/YiLogger.h>
#    include <logging/YiLoggerConfiguration.h>
#    include <platform/YiAppLifeCycleBridgeLocator.h>
#    include <platform/YiInputBridgeLocator.h>
#    include <platform/YiWebBridgeLocator.h>
#    include <utility/YiRapidJSONUtility.h>
#    include <utility/YiTimer.h>
#    include <utility/YiUtilities.h>

#    include <ppapi/cpp/input_event.h>
#    include <ppapi/cpp/instance.h>
#    include <ppapi/cpp/rect.h>
#    include <ppapi/cpp/text_input_controller.h>
#    include <ppapi/cpp/var_dictionary.h>
#    include <ppapi/cpp/view.h>
#    include <ppapi_simple/ps_instance.h>
#    include <ppapi_simple/ps_main.h>

#    include <glm/vec2.hpp>

#    include <sys/mount.h>

#    define LOG_TAG "TizenNaClMainDefault"

static const char *TIZEN_APPLICATION_CLASS_NAME = "CYIApplication";

static const uint64_t HIDE_MOUSE_CURSOR_INTERVAL_MS = 5000; // Tizen hides the mouse cursor after 5 seconds of inactivity
static const uint32_t DEFAULT_SCREEN_DENSITY = 72;

static std::unique_ptr<CYIApp> s_pApp;
static CYITimer s_mouseActivityTimer;
static uint64_t s_timezoneChangedEventHandlerId = 0;
static uint64_t s_visibilityHandlerId = 0;

static CYIWebMessagingBridge::FutureResponse CallTizenApplicationFunction(yi::rapidjson::Document &&message, const CYIString &functionName, yi::rapidjson::Value &&functionArgumentsValue = std::move(yi::rapidjson::Value(yi::rapidjson::kArrayType)))
{
    return CYIWebBridgeLocator::GetWebMessagingBridge()->CallStaticFunctionWithArgs(std::move(message), TIZEN_APPLICATION_CLASS_NAME, functionName, std::move(functionArgumentsValue));
}

static uint64_t RegisterTizenApplicationEventHandler(const CYIString &eventName, CYIWebMessagingBridge::EventCallback &&eventCallback)
{
    yi::rapidjson::Document filterDocument(yi::rapidjson::kObjectType);
    yi::rapidjson::MemoryPoolAllocator<yi::rapidjson::CrtAllocator> &filterAllocator = filterDocument.GetAllocator();

    filterDocument.AddMember(yi::rapidjson::StringRef(CYIWebMessagingBridge::EVENT_CONTEXT_ATTRIBUTE_NAME), yi::rapidjson::StringRef(TIZEN_APPLICATION_CLASS_NAME), filterAllocator);

    yi::rapidjson::Value eventNameValue(eventName.GetData(), filterAllocator);
    filterDocument.AddMember(yi::rapidjson::StringRef(CYIWebMessagingBridge::EVENT_NAME_ATTRIBUTE_NAME), eventNameValue, filterAllocator);

    return CYIWebBridgeLocator::GetWebMessagingBridge()->RegisterEventHandler(std::move(filterDocument), std::move(eventCallback));
}

static void UnregisterTizenApplicationEventHandler(uint64_t &eventHandlerId)
{
    CYIWebBridgeLocator::GetWebMessagingBridge()->UnregisterEventHandler(eventHandlerId);
    eventHandlerId = 0;
}

CYIActionEvent::ButtonType PPButtonToYiButton(PP_InputEvent_MouseButton naclButton)
{
    switch (naclButton)
    {
        case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
            return CYIActionEvent::ButtonType::Left;
        case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
            return CYIActionEvent::ButtonType::Middle;
        case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
            return CYIActionEvent::ButtonType::Right;
        case PP_INPUTEVENT_MOUSEBUTTON_NONE:
        default:
            return CYIActionEvent::ButtonType::None;
    }
}

CYIActionEvent::ButtonType YiButtonFromPPEventModifier(uint32_t modifiers)
{
    if (modifiers & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN)
    {
        return CYIActionEvent::ButtonType::Left;
    }
    else if (modifiers & PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN)
    {
        return CYIActionEvent::ButtonType::Right;
    }
    else if (modifiers & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN)
    {
        return CYIActionEvent::ButtonType::Middle;
    }

    return CYIActionEvent::ButtonType::None;
}

void PPKeyToYiKey(const pp::KeyboardInputEvent &ppKeyEvent, CYIKeyEvent &rKeyEvent)
{
    uint32_t modifier = ppKeyEvent.GetModifiers();
    if (modifier & PP_INPUTEVENT_MODIFIER_SHIFTKEY)
    {
        rKeyEvent.m_shiftKey = true;
    }
    if (modifier & PP_INPUTEVENT_MODIFIER_CONTROLKEY)
    {
        rKeyEvent.m_controlKey = true;
    }
    if (modifier & PP_INPUTEVENT_MODIFIER_ALTKEY)
    {
        rKeyEvent.m_altKey = true;
    }
    if (modifier & PP_INPUTEVENT_MODIFIER_METAKEY)
    {
        rKeyEvent.m_metaKey = true;
    }
    if (modifier & PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT)
    {
        rKeyEvent.m_repeat = true;
    }

    bool handled = true;
    switch (ppKeyEvent.GetKeyCode())
    {
        case 8:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Backspace;
            break;
        case 9:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Tab;
            break;
        case 12:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Clear;
            break;
        case 13:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Enter;
            break;
        case 16:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Shift;
            break;
        case 17:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Control;
            break;
        case 18:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Alt;
            break;
        case 19:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Pause;
            break;
        case 20:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::CapsLock;
            break;
        case 27:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Escape;
            break;
        case 32:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Space;
            break;
        case 33:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::PageUp;
            break;
        case 34:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::PageDown;
            break;
        case 35:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::End;
            break;
        case 36:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Home;
            break;
        case 37:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::ArrowLeft;
            break;
        case 38:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::ArrowUp;
            break;
        case 39:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::ArrowRight;
            break;
        case 40:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::ArrowDown;
            break;
        case 41:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Select;
            break;
        case 43:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Execute;
            break;
        case 44:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::PrintScreen;
            break;
        case 45:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Insert;
            break;
        case 46:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Delete;
            break;
        case 91: // Windows Key / Left command / Chromebook Search key
        case 92: // right window key
        case 93: // Windows Menu / Right command
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Meta;
            break;
        case 106:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Multiply;
            break;
        case 107:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Add;
            break;
        case 109:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Subtract;
            break;
        case 111:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Divide;
            break;
        case 112:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 113:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 114:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F3;
            break;
        case 115:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F4;
            break;
        case 116:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F5;
            break;
        case 117:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F6;
            break;
        case 118:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F7;
            break;
        case 119:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F8;
            break;
        case 120:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F9;
            break;
        case 121:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 122:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 123:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 124:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 125:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 126:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 127:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 128:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 129:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 130:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F1;
            break;
        case 131:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 132:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 133:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 134:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 135:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::F2;
            break;
        case 144:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::NumLock;
            break;
        case 145:
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::ScrollLock;
            break;
        // TIZEN KEYS
        case 403: // ColorF0Red
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Red;
            break;
        case 404: // ColorF1Green
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Green;
            break;
        case 405: // ColorF2Yellow
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Yellow;
            break;
        case 406: // ColorF3Blue
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Blue;
            break;
        case 412: // MediaRewind
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaRewind;
            break;
        case 413: // MediaStop
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaStop;
            break;
        case 415: // MediaPlay
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaPlay;
            break;
        case 416: // MediaRecord
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaRecord;
            break;
        case 417: // MediaFastForward
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaFastForward;
            break;
        case 447: // VolumeUp
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::VolumeUp;
            break;
        case 448: // VolumeDown
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::VolumeDown;
            break;
        case 457: // Info
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Info;
            break;
        case 10009: // Return
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::SystemBack;
            break;
        case 10252: // MediaPlayPause
            rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::MediaPlayPause;
            break;
        case 10182: // Exit (not handled for now)
        default:
            handled = false;
    }

    rKeyEvent.m_keyLocation = CYIKeyEvent::CYIKeyEvent::Location::Mobile;

    if (handled)
    {
        return;
    }

    rKeyEvent.m_keyCode = CYIKeyEvent::KeyCode::Unidentified;
    rKeyEvent.m_keyValue = 0;
}

void MouseActivity()
{
    s_mouseActivityTimer.Start(HIDE_MOUSE_CURSOR_INTERVAL_MS);
    CYICursorInputBridge *pCursorInputBridge = CYIInputBridgeLocator::GetCursorInputBridge();
    if (pCursorInputBridge)
    {
        pCursorInputBridge->SetCursorState(CYICursorInputBridge::CursorState::On);
    }
}

glm::vec2 GetScreenDensity()
{
    static const CYIString FUNCTION_NAME("getScreenDensity");

    static const char *WIDTH_ATTRIBUTE_NAME = "width";
    static const char *HEIGHT_ATTRIBUTE_NAME = "height";

    CYIWebMessagingBridge::FutureResponse futureResponse = CallTizenApplicationFunction(yi::rapidjson::Document(), FUNCTION_NAME);

    bool valueAssigned = false;
    CYIWebMessagingBridge::Response response = std::move(futureResponse.Take(CYIWebMessagingBridge::DEFAULT_RESPONSE_TIMEOUT_MS, &valueAssigned));

    if (!valueAssigned)
    {
        YI_LOGE(LOG_TAG, "GetScreenDensity did not receive a response from the web messaging bridge!");
    }
    else if (response.HasError())
    {
        YI_LOGE(LOG_TAG, "%s", response.GetError()->GetStacktrace().GetData());
    }
    else
    {
        const yi::rapidjson::Value *pResult = response.GetResult();

        if (!pResult->IsObject())
        {
            YI_LOGE(LOG_TAG, "GetScreenDensity expected an object type for result, received %s. JSON string for result: '%s'.", CYIRapidJSONUtility::TypeToString(pResult->GetType()).GetData(), CYIRapidJSONUtility::CreateStringFromValue(*pResult).GetData());
        }
        else
        {
            if (!pResult->HasMember(WIDTH_ATTRIBUTE_NAME) || !(*pResult)[WIDTH_ATTRIBUTE_NAME].IsInt())
            {
                YI_LOGE(LOG_TAG, "GetScreenDensity encountered a missing or invalid integer value for result '%s'. JSON string for result: '%s'.", WIDTH_ATTRIBUTE_NAME, CYIRapidJSONUtility::CreateStringFromValue(*pResult).GetData());
            }
            else if (!pResult->HasMember(HEIGHT_ATTRIBUTE_NAME) || !(*pResult)[HEIGHT_ATTRIBUTE_NAME].IsInt())
            {
                YI_LOGE(LOG_TAG, "GetScreenDensity encountered a missing or invalid integer value for result '%s'. JSON string for result: '%s'.", HEIGHT_ATTRIBUTE_NAME, CYIRapidJSONUtility::CreateStringFromValue(*pResult).GetData());
            }
            else
            {
                return glm::vec2((*pResult)[WIDTH_ATTRIBUTE_NAME].GetInt(), (*pResult)[HEIGHT_ATTRIBUTE_NAME].GetInt());
            }
        }
    }

    return glm::vec2(DEFAULT_SCREEN_DENSITY, DEFAULT_SCREEN_DENSITY);
}

class TimezoneHandler : public CYISignalHandler
{
public:
    TimezoneHandler()
    {
        static const CYIString GET_TIMEZONE_FUNCTION_NAME("getTimezone");

        // Get the initial timezone.
        CYIWebMessagingBridge::FutureResponse futureResponse = CallTizenApplicationFunction(yi::rapidjson::Document(), GET_TIMEZONE_FUNCTION_NAME);

        bool valueAssigned = false;
        CYIWebMessagingBridge::Response response = std::move(futureResponse.Take(CYIWebMessagingBridge::DEFAULT_RESPONSE_TIMEOUT_MS, &valueAssigned));

        if (!valueAssigned)
        {
            YI_LOGE(LOG_TAG, "getTimezone did not receive a response from the web messaging bridge!");
        }
        else if (response.HasError())
        {
            YI_LOGE(LOG_TAG, "%s", response.GetError()->GetStacktrace().GetData());
        }
        else
        {
            const yi::rapidjson::Value *pResult = response.GetResult();

            if (!pResult->IsString())
            {
                YI_LOGE(LOG_TAG, "getTimezone expected a string type for result, received %s. JSON string for result: '%s'.", CYIRapidJSONUtility::TypeToString(pResult->GetType()).GetData(), CYIRapidJSONUtility::CreateStringFromValue(*pResult).GetData());
            }
            else
            {
                setenv("TZ", pResult->GetString(), 1);
            }
        }

        // Register timezone changed event handler.
        s_timezoneChangedEventHandlerId = RegisterTizenApplicationEventHandler("timezoneChanged", [](yi::rapidjson::Document &&event) {
            if (!event.HasMember(CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME) || !event[CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME].IsString())
            {
                YI_LOGE(LOG_TAG, "Invalid 'timezoneChanged' event data. JSON string for 'timezoneChanged' event: '%s'.", CYIRapidJSONUtility::CreateStringFromValue(event).GetData());
            }
            else
            {
                setenv("TZ", event[CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME].GetString(), 1);
            }
        });
    }

    virtual ~TimezoneHandler()
    {
        UnregisterTizenApplicationEventHandler(s_timezoneChangedEventHandlerId);
    }
};

class AppVisibilityHandler : public CYISignalHandler
{
public:
    AppVisibilityHandler()
    {
        s_visibilityHandlerId = RegisterTizenApplicationEventHandler("visibilityChanged", [](yi::rapidjson::Document &&event) {
            if (!event.HasMember(CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME) || !event[CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME].IsBool())
            {
                YI_LOGE(LOG_TAG, "Invalid 'visibilityChanged' event data. JSON string for 'visibilityChanged' event: '%s'.", CYIRapidJSONUtility::CreateStringFromValue(event).GetData());
            }
            else
            {
                CYIAppLifeCycleBridge *pAppLifeCycleBridge = CYIAppLifeCycleBridgeLocator::GetAppLifeCycleBridge();

                if (pAppLifeCycleBridge)
                {
                    if (event[CYIWebMessagingBridge::EVENT_DATA_ATTRIBUTE_NAME].GetBool())
                    {
                        pAppLifeCycleBridge->OnForegroundEntered();
                    }
                    else
                    {
                        pAppLifeCycleBridge->OnBackgroundEntered();
                    }
                }
            }
        });
    }

    virtual ~AppVisibilityHandler()
    {
        UnregisterTizenApplicationEventHandler(s_visibilityHandlerId);
    }
};

// When the mouse activity timer expires this class notifies the engine that the mouse is inactive and
// the cursor has likely been hidden by the OS.
class HideCursorHandler : public CYISignalHandler
{
public:
    void OnMouseInactive()
    {
        CYICursorInputBridge *pCursorInputBridge = CYIInputBridgeLocator::GetCursorInputBridge();
        if (pCursorInputBridge)
        {
            pCursorInputBridge->SetCursorState(CYICursorInputBridge::CursorState::Off);
        }
    }
};

void ProcessEvents()
{
    static int32_t s_mousePosX = 0;
    static int32_t s_mousePosY = 0;
    static const int32_t ZERO_WHEEL_DELTA = 0;
    static const uint8_t POINTER_ID = 0;

    PSEvent *pEvent;

    while ((pEvent = PSEventTryAcquire()) != NULL)
    {
        switch (pEvent->type)
        {
            /* From DidChangeView, contains a pp:View. */
            case PSE_INSTANCE_DIDCHANGEVIEW:
            {
                const pp::View currentView(pEvent->as_resource);
                const pp::Rect viewRect = currentView.GetRect();
                const int32_t width = viewRect.size().width();
                const int32_t height = viewRect.size().height();

                glm::vec2 DPI = GetScreenDensity();

                s_pApp->SetScreenProperties(width,
                                            height,
                                            DPI.x,
                                            DPI.y);
                s_pApp->SurfaceWasResized(width, height);
                break;
            }

            /* From HandleInputEvent, contains a pp::InputEvent. */
            case PSE_INSTANCE_HANDLEINPUT:
            {
                const pp::InputEvent inputEvent(pEvent->as_resource);
                switch (inputEvent.GetType())
                {
                    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
                    {
                        pp::MouseInputEvent mouseInputEvent(inputEvent);
                        s_pApp->HandleActionInputs(s_mousePosX, s_mousePosY, ZERO_WHEEL_DELTA, PPButtonToYiButton(mouseInputEvent.GetButton()), CYIEvent::Type::ActionDown, POINTER_ID);

                        MouseActivity();
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_MOUSEUP:
                    {
                        pp::MouseInputEvent mouseInputEvent(inputEvent);
                        s_pApp->HandleActionInputs(s_mousePosX, s_mousePosY, ZERO_WHEEL_DELTA, PPButtonToYiButton(mouseInputEvent.GetButton()), CYIEvent::Type::ActionUp, POINTER_ID);

                        MouseActivity();
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_WHEEL:
                    {
                        pp::WheelInputEvent wheelInputEvent(inputEvent);
                        s_pApp->HandleActionInputs(s_mousePosX, s_mousePosY, wheelInputEvent.GetDelta().y(), CYIActionEvent::ButtonType::None, CYIEvent::Type::ActionWheel, POINTER_ID);

                        MouseActivity();
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
                    {
                        pp::MouseInputEvent mouseInputEvent(inputEvent);
                        pp::Point mousePositon = mouseInputEvent.GetPosition();
                        s_mousePosX = mousePositon.x();
                        s_mousePosY = mousePositon.y();
                        s_pApp->HandleActionInputs(s_mousePosX, s_mousePosY, 0, YiButtonFromPPEventModifier(mouseInputEvent.GetModifiers()), CYIEvent::Type::ActionMove, POINTER_ID, true);

                        MouseActivity();
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_KEYDOWN:
                    {
                        pp::KeyboardInputEvent KeyboardInputEvent(inputEvent);

                        CYIKeyEvent keyEvent(CYIEvent::Type::KeyDown);
                        PPKeyToYiKey(KeyboardInputEvent, keyEvent);
                        // The back event is only handled on key up and is provided to CYIBackButtonHandler.
                        if (keyEvent.m_keyCode != CYIKeyEvent::KeyCode::SystemBack)
                        {
                            s_pApp->HandleKeyInputs(keyEvent);
                        }

                        CYICursorInputBridge *pCursorInputBridge = CYIInputBridgeLocator::GetCursorInputBridge();
                        if (pCursorInputBridge)
                        {
                            pCursorInputBridge->SetCursorState(CYICursorInputBridge::CursorState::Off);
                        }

                        break;
                    }
                    case PP_INPUTEVENT_TYPE_KEYUP:
                    {
                        pp::KeyboardInputEvent KeyboardInputEvent(inputEvent);

                        CYIKeyEvent keyEvent(CYIEvent::Type::KeyUp);
                        PPKeyToYiKey(KeyboardInputEvent, keyEvent);
                        if (keyEvent.m_keyCode == CYIKeyEvent::KeyCode::SystemBack)
                        {
                            CYIBackButtonHandler::NotifyBackButtonPressed();
                        }
                        else
                        {
                            s_pApp->HandleKeyInputs(keyEvent);
                        }
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_CHAR:
                    {
                        pp::KeyboardInputEvent KeyboardInputEvent(inputEvent);
                        CYIKeyboardInputBridge *pKeyboardInputBridge = CYIInputBridgeLocator::GetKeyboardInputBridge();
                        if (pKeyboardInputBridge)
                        {
                            CYIKeyboardInputBridge::Receiver *pReceiver = pKeyboardInputBridge->GetCurrentReceiver();

                            CYIString newChar(KeyboardInputEvent.GetCharacterText().AsString());

                            // Printable characters
                            if (pReceiver && newChar.At(0) > 31 && newChar.At(0) != 127)
                            {
                                pReceiver->OnTextEntered(newChar, 1);
                            }
                        }

                        CYICursorInputBridge *pCursorInputBridge = CYIInputBridgeLocator::GetCursorInputBridge();
                        if (pCursorInputBridge)
                        {
                            pCursorInputBridge->SetCursorState(CYICursorInputBridge::CursorState::Off);
                        }
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
                    {
                        CYICursorInputBridge *pCursorInputBridge = CYIInputBridgeLocator::GetCursorInputBridge();
                        if (pCursorInputBridge)
                        {
                            pCursorInputBridge->SetCursorState(CYICursorInputBridge::CursorState::Off);
                        }
                        break;
                    }
                    case PP_INPUTEVENT_TYPE_TOUCHSTART:
                    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
                    case PP_INPUTEVENT_TYPE_TOUCHEND:
                    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
                    case PP_INPUTEVENT_TYPE_MOUSEENTER:
                    default:
                        break;
                }
                break;
            }

            /* Handled via CYIWebMessagingBridge. */
            case PSE_INSTANCE_HANDLEMESSAGE:
                break;

            /* From DidChangeFocus, contains a PP_Bool with the current focus state. */
            case PSE_INSTANCE_DIDCHANGEFOCUS:
                break;

            /* When the 3D context is lost, no resource. */
            case PSE_GRAPHICS3D_GRAPHICS3DCONTEXTLOST:
                break;

            /* When the mouse lock is lost. */
            case PSE_MOUSELOCK_MOUSELOCKLOST:
                break;

            default:
                break;
        }

        PSEventRelease(pEvent);
    }
}

int main(int argc, char **argv)
{
    YI_UNUSED(argc);
    YI_UNUSED(argv);

    CYILogger::Initialize();

    PSEvent *pEvent;
    bool shouldStop = false;
    pp::Rect moduleRect;

    // Wait for the first PSE_INSTANCE_DIDCHANGEVIEW event to get the size of the NaCl module.
    PSEventSetFilter(PSE_INSTANCE_DIDCHANGEVIEW);
    while (!shouldStop)
    {
        while (!shouldStop && (pEvent = PSEventWaitAcquire()) != NULL)
        {
            if (pEvent->type == PSE_INSTANCE_DIDCHANGEVIEW)
            {
                const pp::View currentView(pEvent->as_resource);
                moduleRect = currentView.GetRect();
                shouldStop = true;
            }
            PSEventRelease(pEvent);
        }
    }

    pp::Instance currentInstance(PSGetInstanceId());
    pp::InstanceHandle currentInstanceHandle(PSGetInstanceId());

    pp::TextInputController text_input_controller(currentInstanceHandle);
    text_input_controller.SetTextInputType(PP_TEXTINPUT_TYPE_NONE);

    // Create You.i Engine's surface with the required width, height and depth.
    CYISurface::Config surfaceConfig;
    YI_MEMSET(&surfaceConfig, 0, sizeof(CYISurface::Config));
    surfaceConfig.width = moduleRect.size().width();
    surfaceConfig.height = moduleRect.size().height();

    std::unique_ptr<CYISurface> pSurface = CYISurface::New(&surfaceConfig, CYISurface::WindowOwnership::GrabsWindow);

    glm::vec2 DPI = GetScreenDensity();

    // Create and initialize the You.i Engine application.
    s_pApp = AppFactory::Create();

    s_pApp->SetScreenProperties(pSurface->GetWidth(),
                                pSurface->GetHeight(),
                                DPI.x,
                                DPI.y);
    s_pApp->SetSurface(pSurface.get());

#    ifndef YI_TIZEN_NACL_STORAGE_QUOTA
#        error YI_TIZEN_NACL_STORAGE_QUOTA not defined. This is the size in bytes that will be allocated in persistent storage for the application. This can be defined by setting the YI_TIZEN_NACL_STORAGE_QUOTA variable to a valid number.
#    endif

    umount("/");
    mount(
        "", /* source */
        "/", /* target */
        "httpfs", /* filesystemtype */
        0, /* mountflags */
        ""); /* data specific to the html5fs type */

    umount("/persistent");
    mount(
        "", /* source */
        "/persistent", /* target */
        "html5fs", /* filesystemtype */
        0, /* mountflags */
        "type=PERSISTENT,expected_size=" YI_STRINGIFY(YI_TIZEN_NACL_STORAGE_QUOTA));

    s_pApp->SetAssetsPath("/assets/");
    s_pApp->SetDataPath("/persistent/");
    s_pApp->SetExternalPath("/persistent/");

    if (!s_pApp->Init())
    {
        s_pApp.reset();
        pSurface.reset();

        YI_LOGE(LOG_TAG, "Failed to initialize application.");

        return 1;
    }

    HideCursorHandler hideCursorHandler;
    s_mouseActivityTimer.TimedOut.Connect(hideCursorHandler, &HideCursorHandler::OnMouseInactive);
    AppVisibilityHandler appVisibilityHandler;

    // Set the filter to accept all events before heading into the main application loop.
    PSEventSetFilter(PSE_ALL);

    // NaCl does not have the TZ environment variable set which prevents localtime from working. The TimezoneHandler will update the TZ environment variable to match what is in Javascript.
    TimezoneHandler timezoneHandler;

    // Hide the splash screen.
    CYIWebMessagingBridge::FutureResponse futureResponse = CallTizenApplicationFunction(yi::rapidjson::Document(), "hideSplashScreen");

    bool valueAssigned = false;
    CYIWebMessagingBridge::Response response = std::move(futureResponse.Take(CYIWebMessagingBridge::DEFAULT_RESPONSE_TIMEOUT_MS, &valueAssigned));

    if (!valueAssigned)
    {
        YI_LOGE(LOG_TAG, "hideSplashScreen did not receive a response from the web messaging bridge!");
    }
    else if (response.HasError())
    {
        YI_LOGE(LOG_TAG, "%s", response.GetError()->GetStacktrace().GetData());
    }

    // Main application loop.
    while (true)
    {
        ProcessEvents();

        s_pApp->Update();
        s_pApp->Draw();
        s_pApp->Swap();
    }

    s_pApp.reset();
    pSurface.reset();

    return 0;
}

PPAPI_SIMPLE_REGISTER_MAIN(main);

#endif
