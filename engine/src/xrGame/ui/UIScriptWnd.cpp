#include "pch_script.h"
#include "UIScriptWnd.h"
#include "Common/object_broker.h"
#include "xrUICore/Callbacks/callback_info.h"

#include <cctype>
#include <cstring>

namespace
{
bool UICallbackTraceEnabled()
{
    constexpr char flag[] = "-ui_callback_trace";
    if (!Core.Params)
        return false;

    for (pcstr arg = Core.Params; *arg;)
    {
        while (*arg && std::isspace(static_cast<unsigned char>(*arg)))
            ++arg;
        pcstr end = arg;
        while (*end && !std::isspace(static_cast<unsigned char>(*end)))
            ++end;
        if (end - arg == sizeof(flag) - 1 && std::strncmp(arg, flag, sizeof(flag) - 1) == 0)
            return true;
        arg = end;
    }
    return false;
}
}

CUIDialogWndEx::CUIDialogWndEx() : CUIDialogWnd("CUIDialogWndEx") {}
CUIDialogWndEx::~CUIDialogWndEx() { delete_data(m_callbacks); }

void CUIDialogWndEx::Register(CUIWindow* pChild) { pChild->SetMessageTarget(this); }
void CUIDialogWndEx::Register(CUIWindow* pChild, pcstr name)
{
    pChild->SetWindowName(name);
    pChild->SetMessageTarget(this);
}

void CUIDialogWndEx::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    // UI event bursts should not format and synchronously write verbose traces
    // unless requested. Callback selection and dispatch are unchanged.
    const bool trace = UICallbackTraceEnabled();
    if (trace)
        Msg(">>> [CUIDialogWndEx] SendMessage: pWnd=%p ('%s'), msg=%d, callbacks_count=%u",
            pWnd, pWnd ? pWnd->WindowName().c_str() : "<null>", (int)msg, (u32)m_callbacks.size());

    const auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(), event_comparer{ pWnd, msg });
    if (it == m_callbacks.end())
    {
        if (trace)
            Msg(">>> [CUIDialogWndEx] SendMessage: NO MATCHING CALLBACK FOUND for '%s' msg=%d!",
                pWnd ? pWnd->WindowName().c_str() : "<null>", (int)msg);
        return inherited::SendMessage(pWnd, msg, pData);
    }

    if (trace)
        Msg(">>> [CUIDialogWndEx] SendMessage: Invoking callback for '%s'...", (*it)->m_control_name.c_str());
    (*it)->m_callback();
    if (trace)
        Msg(">>> [CUIDialogWndEx] SendMessage: Callback for '%s' completed!", (*it)->m_control_name.c_str());
}

bool CUIDialogWndEx::Load(pcstr /*xml_name*/) { return true; }

SCallbackInfo* CUIDialogWndEx::NewCallback()
{
    m_callbacks.push_back(xr_new<SCallbackInfo>());
    return m_callbacks.back();
}

void CUIDialogWndEx::AddCallback(LPCSTR control_id, s16 evt, const luabind::functor<void> &lua_function)
{
    if (UICallbackTraceEnabled())
        Msg(">>> [CUIDialogWndEx] AddCallback(1): control_id='%s', evt=%d", control_id, (int)evt);
    SCallbackInfo* c = NewCallback ();
    c->m_callback.set(lua_function);
    c->m_control_name = control_id;
    c->m_event = evt;
}

void CUIDialogWndEx::AddCallback(pcstr control_id, s16 evt, const luabind::functor<void>& functor, const luabind::object& object)
{
    if (UICallbackTraceEnabled())
        Msg(">>> [CUIDialogWndEx] AddCallback(2): control_id='%s', evt=%d", control_id, (int)evt);
    SCallbackInfo* c = NewCallback();
    c->m_callback.set(functor, object);
    c->m_control_name = control_id;
    c->m_event = evt;
}
