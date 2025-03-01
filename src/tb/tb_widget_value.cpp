// ================================================================================
// ==      This file is a part of Turbo Badger. (C) 2011-2014, Emil Segerås      ==
// ==                     See tb_core.h for more information.                    ==
// ================================================================================

#include "tb_widget_value.h"
#include "tb_widgets.h"

namespace tb {

// == TBWidgetValueConnection ===========================================================

void TBWidgetValueConnection::Connect(TBWidgetValue *value, TBWidget *widget)
{
	Unconnect();
	m_widget = widget;
	m_value = value;
	m_value->m_connections.AddLast(this);
	m_value->SyncToWidget(m_widget);
}

void TBWidgetValueConnection::Unconnect()
{
	if (m_value)
		m_value->m_connections.Remove(this);
	m_value = nullptr;
	m_widget = nullptr;
}

TBID TBWidgetValueConnection::GetName() const
{
	if (m_value)
		return m_value->GetName();
	return TBID();
}

void TBWidgetValueConnection::SyncFromWidget(TBWidget *source_widget)
{
	if (m_value)
		m_value->SetFromWidget(source_widget);
}

// == TBWidgetValue =====================================================================

TBWidgetValue::TBWidgetValue(const TBID &name, TBValue::TYPE type)
	: m_name(name)
	, m_value(type)
	, m_syncing(false)
{
}

TBWidgetValue::~TBWidgetValue()
{
	while (m_connections.GetFirst())
		m_connections.GetFirst()->Unconnect();
}

void TBWidgetValue::SetFromWidget(TBWidget *source_widget)
{
	if (m_syncing)
		return; // We ended up here because syncing is in progress.

	// Get the value in the format
	TBStr text;
	switch (m_value.GetType())
	{
	case TBValue::TYPE_STRING:
		if (!source_widget->GetText(text))
			return;
		m_value.SetString(text);
		break;
	case TBValue::TYPE_NULL:
	case TBValue::TYPE_INT:
		m_value.SetInt(source_widget->GetValue());
		break;
	case TBValue::TYPE_FLOAT:
		m_value.SetFloat(source_widget->GetValueDouble());
		break;
	case TBValue::TYPE_OBJECT:
		m_value.SetObject(source_widget->data.GetObject(), TBValue::SET_AS_STATIC);
		break;
	default:
		assert(!"Unsupported value type!");
	}

	SyncToWidgets(source_widget);
}

bool TBWidgetValue::SyncToWidgets(TBWidget *exclude_widget)
{
	// FIX: Assign group to each value. Currently we only have one global group.
	g_value_group.InvokeOnValueChanged(this);

	bool fail = false;
	TBLinkListOf<TBWidgetValueConnection>::Iterator iter = m_connections.IterateForward();
	while (TBWidgetValueConnection *connection = iter.GetAndStep())
	{
		if (connection->m_widget != exclude_widget)
			fail |= !SyncToWidget(connection->m_widget);
	}
	return !fail;
}

bool TBWidgetValue::SyncToWidget(TBWidget *dst_widget)
{
	if (m_syncing)
		return true; // We ended up here because syncing is in progress.

	m_syncing = true;
	bool ret = true;
	//switch (m_value.GetType())
	switch (dst_widget->m_sync_type)
	{
		case TBValue::TYPE_STRING:
			ret = dst_widget->SetText(m_value.GetString());
			break;
		case TBValue::TYPE_NULL:
		case TBValue::TYPE_INT:
			dst_widget->SetValue(m_value.GetInt());
			break;
		case TBValue::TYPE_FLOAT:
			dst_widget->SetValueDouble(m_value.GetFloat());
			break;
		case TBValue::TYPE_OBJECT:
		default:
			dst_widget->SetValue(m_value);
			break;
	}
	m_syncing = false;
	return ret;
}

void TBWidgetValue::SetInt(long value)
{
	if (m_value.GetType() != TBValue::TYPE_INT || !m_value.Equals(value)) {
		m_value.SetInt(value);
		SyncToWidgets(nullptr);
	}
}

bool TBWidgetValue::SetText(const TBStr & text)
{
	if (m_value.GetType() != TBValue::TYPE_STRING || !m_value.Equals(text)) {
		m_value.SetString(text);
		return SyncToWidgets(nullptr);
	}
	return false;
}

void TBWidgetValue::SetDouble(double value)
{
	if (m_value.GetType() != TBValue::TYPE_FLOAT || !m_value.Equals(value)) {
		m_value.SetFloat(value);
		SyncToWidgets(nullptr);
	}
}

void TBWidgetValue::SetValue(const TBValue & value)
{
	if (!m_value.Equals(value)) {
		m_value = value;
		SyncToWidgets(nullptr);
	}
}

// == TBValueGroup ================================================================================

/*extern*/ TBValueGroup g_value_group;

TBWidgetValue *TBValueGroup::CreateValueIfNeeded(const TBID &name, TBValue::TYPE type)
{
	if (TBWidgetValue *val = GetValue(name))
		return val;
	if (TBWidgetValue *val = new TBWidgetValue(name, type))
	{
		if (m_values.Add(name, val))
			return val;
		else
			delete val;
	}
	return nullptr;
}

void TBValueGroup::InvokeOnValueChanged(const TBWidgetValue *value)
{
	TBLinkListOf<TBValueGroupListener>::Iterator iter = m_listeners.IterateForward();
	while (TBValueGroupListener *listener = iter.GetAndStep())
		listener->OnValueChanged(this, value);
}

} // namespace tb
